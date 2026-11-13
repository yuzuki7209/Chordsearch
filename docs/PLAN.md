# Chordsearch 개발 계획

건반 MIDI 입력을 실시간으로 받아 **코드 인식 → 키 감지 → 화성 분석 → 다음 코드 추천**을 수행하는 플러그인.

## 확정된 결정사항

| 항목 | 결정 |
|---|---|
| 타깃 DAW | **Ableton Live 12** (macOS, Apple Silicon) |
| 플러그인 형태 | **악기(Instrument) 타입**: MIDI 입력, 무음 출력. Ableton은 VST/AU MIDI 이펙트를 지원하지 않으므로 이 방식을 씀 |
| 포맷 | VST3, AU, Standalone(테스트용) |
| 화성 분석 깊이 | **재즈 화성**: 텐션(9/11/13, b9/#9/#11/b13), alt, sus, rootless 보이싱까지 |
| MIDI 미리듣기 | 현재 범위에서 제외 |
| 기술 스택 | JUCE 8.0.15 (git submodule), CMake + Ninja, C++20, Catch2 v3 |
| 디자인 | 다크 톤의 모던 플랫 디자인. 각진 형태, corner radius 0~3px. **둥근 알약형(pill/capsule) 형태 금지** |

## Ableton에서 사용하는 방법
- 방법 A: 피아노 트랙과 별도로 MIDI 트랙을 하나 만들고, Chordsearch를 넣은 뒤 같은 키보드를 입력으로 설정하고 Monitor를 `In`으로 둔다.
- 방법 B: Instrument Rack 안에 체인을 두 개 만들어 하나는 실제 악기, 하나는 Chordsearch로 병렬 구성한다.

## 아키텍처

```
[건반] → MIDI → PluginProcessor (오디오 스레드)
                 └ MidiInputTracker: Note On/Off, 서스테인 페달(CC64)
                   → 눌린 음/울리는 음 비트셋 → atomic으로 공개 (lock-free)
                                  ▼  (메시지 스레드 타이머, 약 30Hz)
              core (순수 C++, JUCE에 의존하지 않음)
                 ├ ChordRecognizer  음 집합 → 코드 후보 + 점수
                 ├ ChordNamer       재즈 표기 (C7(b9,#11), Cm11, C7alt, C13sus …)
                 ├ KeyDetector      전체 키(global) + 국소 조성 중심(local, ii-V-I 기반)
                 ├ HarmonyAnalyzer  로마숫자, 기능, 세컨더리/대리/차용 해석, 코드 스케일
                 └ ChordSuggester   다음 코드 추천 + 이유 + 텐션 보이싱
                                  ▼
              PluginEditor (GUI)
```

오디오 스레드에서는 메모리 할당, 락, 문자열 생성을 하지 않는다.

## 재즈 화성 엔진 설계

### 코드 인식 (`ChordRecognizer`)
1. 음 집합을 12비트 피치클래스 마스크로 바꾼다. 최저음(bass)은 따로 보관한다.
2. 12개 근음 후보마다 마스크를 회전시켜 **음도를 분해**한다.
   - 3도 자리: m3 / M3 / sus2 / sus4
   - 5도 자리: b5 / 5 / #5 (5도는 생략 가능)
   - 7도 자리: 6 / b7 / M7 / dim7
   - 텐션: b9 / 9 / #9 / 11 / #11 / b13 / 13
3. **가이드톤(3도+7도)**을 중심으로 점수를 매긴다.
   - 근음이 실제로 있으면 가점, 최저음이 근음이면 추가 가점
   - **Rootless 보이싱**(근음 없이 3-5-7-9 같은 형태)도 후보로 인정하되 감점하고, 키 문맥으로 보정
   - 불협 조합(같은 코드 안의 M3와 11 등)은 감점
4. 결과는 후보 목록(근음, 퀄리티, 텐션, 베이스, 점수)이다. 이후 키 문맥으로 재정렬한다.
   - 예: `C6`와 `Am7`, `Cm7b5`와 `Ebm6`, dim7의 이름 4가지

### 코드 표기 (`ChordNamer`)
- 퀄리티: maj7, 7, m7, m7b5, dim7, m(maj7), 6, m6, 6/9, sus4, 7sus4, aug, 7#5
- 텐션: 가장 높은 자연 텐션으로 이름 짓기(9→11→13)와 변화 텐션 괄호 표기 `C7(b9,#11)`
- 변화 텐션이 3개 이상(b9 #9 b5/#11 b13)이면 `C7alt`로 축약(옵션)
- 슬래시 코드 `C/E`, 상부구조(UST)는 이후에 고려
- 이명동음(C# / Db)은 현재 키 기준으로 표기

### 키 감지 (`KeyDetector`)
- **음 분포:** 시간이 지날수록 비중이 줄어드는 피치클래스 히스토그램과 키 프로파일(Temperley/Krumhansl)의 상관계수를 24개 키에 대해 계산한다.
- **코드 진행:** 다이어토닉 적합도와 종지 패턴을 본다. ii-V-I, V-I, ii-V(해결 전), 백도어 iv-bVII7-I, 트라이톤 대리 bII7-I가 나오면 가중한다.
- 재즈 곡은 전조가 잦으므로 **global key**(전체 조)와 **local center**(최근 ii-V가 가리키는 일시적 토닉)를 따로 보여준다.
- 히스테리시스로 판정이 깜빡이지 않게 하고, 키를 수동으로 고정할 수 있게 한다.

### 화성 분석 (`HarmonyAnalyzer`)
- 로마숫자(Imaj7, ii7, V7, bVII7 …)와 기능(T / SD / D / SD-minor)을 붙인다.
- 해석 종류: 세컨더리 도미넌트(V7/ii), 연관 ii(related ii), 트라이톤 대리(subV7), 차용화음(iv7, bVImaj7), 경과 감7(#idim7), 백도어 도미넌트
- **코드 스케일:** Ionian / Dorian / Mixolydian / Lydian dominant / Altered / HW dim / Locrian #2 …

### 다음 코드 추천 (`ChordSuggester`)
```
점수 = w1·규칙점수 + w2·통계점수 + w3·보이스리딩점수
```
- **규칙:** ii→V, V→I, 세컨더리 도미넌트 체인, 트라이톤 대리, 백도어, 해결 대상이 단조이면 V7(b9)/alt, 차용화음 등
- **통계:** 로마숫자 기반 Markov 모델(바이그램/트라이그램). JSON 테이블로 플러그인에 내장한다. 재즈 스탠더드 진행을 직접 정리한 데이터로 시작한다.
- **보이스 리딩:** 현재 보이싱에서 이동이 적은 텐션 보이싱을 제안한다(가이드톤 3↔7 연결).
- 표시 예: `D7(b9) — V7/V, G로 가는 세컨더리 도미넌트` 형태로 이유를 함께 보여준다.

## 폴더 구조

```
CMakeLists.txt
external/JUCE/                 (submodule, 8.0.15)
core/                          순수 C++ 엔진 (chordsearch_core)
  include/chordsearch/*.h
  src/*.cpp
plugin/                        JUCE 플러그인
  PluginProcessor / PluginEditor / MidiInputTracker
  ui/ (Theme, KeyboardView …)
tests/                         Catch2 단위 테스트
resources/                     추천용 전이 테이블 (JSON)
docs/PLAN.md
```

## 마일스톤

| 단계 | 내용 | 상태 |
|---|---|---|
| M0 | 빌드 환경, 빈 플러그인 | 완료 |
| M1 | MIDI 입력, 건반 시각화 | 완료 |
| M2 | 코드 인식과 재즈 표기 (텐션, rootless, 슬래시, 도수 기반 이명동음) | 완료 |
| M3 | 키 감지 (KK 프로파일, 다이아토닉 적합도, 종지, 히스테리시스, 일시적 조성 중심) | 완료 |
| M4 | 로마숫자, 기능, 세컨더리/subV/related ii/차용/경과 감화음, 코드 스케일 | 완료 |
| M5 | 규칙 기반 추천과 이유 | 완료 |
| M6 | 진행 라이브러리 n-gram 매칭, 보이스 리딩 보너스, 텐션 추천 | 완료 |
| M7 | 진행 편집(X 삭제, + 삽입, 길이), CONFIRM, 장르별 백킹 밴드, 메트로놈, 악기 on/off, 리얼북 차트 | 완료 |
| M8 | 상태 저장(진행, 장르, 템포), 호스트 템포/트랜스포트 동기화, 키 고정, 사운드 다듬기 | 다음 |

## 백킹 밴드 (core/backing)
- `generateSequence()`: 차트(코드, 박 수)와 장르를 받아 노트 이벤트를 만든다. 차트가 짧으면 2~4회 반복하면서 변주한다.
- 장르별 연주법:

| 장르 | Bass | Drums | Piano | Guitar |
|---|---|---|---|---|
| Swing | 워킹(스케일/코드톤 경로, 반음/도미넌트 어프로치) | 라이드 spang-a-lang, 2·4 하이햇, 페더링, 스네어 컴핑 | 루트리스 A/B 보이싱, 찰스턴/앤티시페이션 | Freddie Green 4비트 쉘 |
| Bossa Nova | 루트–5도 점음표 | 크로스스틱 보사 클라베 | 싱코페이션 쉘 | 엄지 베이스 + 핑거 플럭 |
| Rock | 8비트 루트, 프레이즈 끝 5도→옥타브 | 8비트, 4마디 필인, 크래시 | 4분 트라이어드 + LH 옥타브 | 팜뮤트 파워코드 (오버드라이브) |
| Funk | 16비트 싱코페이션, 옥타브 팝 | 16비트 햇, 고스트 노트, 오픈햇 | 16분 스탭 (3-7-9) | 치킨 스크래치 (뮤트 + 액센트) |
| Ballad | 2분음표 루트–5도 + 어프로치 | 소프트 8비트 + 크로스스틱 | 서스테인 블록 코드 + LH 루트 | 오픈 보이싱 아르페지오 |
| Blues Shuffle | 부기 라인 (R-3-5-6) | 셔플, 트리플렛 필 | 스윙 컴핑 | 부기 더블스톱 (5-6-b7) |

- 보이싱은 `voiceLead()`로 모든 전위를 시도해 이전 보이싱과의 이동이 최소인 것을 고른다.
- `BackingSynth`: 샘플 없이 합성한다. 베이스는 감산 합성, 피아노는 FM EP, 기타는 Karplus–Strong, 드럼은 합성 드럼이다. prepare() 이후에는 메모리를 할당하지 않는다.
- `BackingPlayer`: 오디오 스레드에서 루프 재생하고, 이벤트를 샘플 단위 위치에 정확히 트리거한다.
- 시퀀스는 hazard pointer 방식으로 오디오 스레드에 넘긴다(메시지 스레드가 소유하고, 오디오 스레드가 사용 중이면 해제하지 않는다).

## 알고리즘 참고 자료
- 키 프로파일: Krumhansl & Kessler (1982), probe-tone key profiles
- 코드-스케일 이론과 사용 가능 텐션, 어보이드 노트 규칙: Berklee 화성학 (Nettles & Graf, *The Chord Scale Theory & Jazz Harmony*)
- 기능 화성, 세컨더리 도미넌트, 모달 인터체인지: Levine, *The Jazz Theory Book*
- 진행 라이브러리(`core/src/ProgressionLibrary.cpp`): 재즈 스탠더드 턴어라운드와 사이클, 블루스 폼, 팝/J-Pop/네오소울 대표 진행 50여 개

## 개발 도구
- `ChordsearchUISnapshot`: 정해 둔 진행을 연주시킨 상태로 에디터를 PNG로 렌더링한다(UI 확인용).
  `build/tools/ui_snapshot/ChordsearchUISnapshot_artefacts/Release/ChordsearchUISnapshot out.png [width height] [--wav dir]`
  `--wav`를 주면 6개 장르의 백킹을 WAV로 출력한다(사운드 확인용).

## 빌드

```bash
export PATH="$HOME/Library/Python/3.9/bin:$PATH"   # pip로 설치한 cmake, ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```
빌드가 끝나면 VST3/AU가 `~/Library/Audio/Plug-Ins/`에 자동으로 복사된다.
