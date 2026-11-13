Chordsearch - 테스트 빌드 (Windows)
=====================================

포함 파일
  Chordsearch.exe      단독 실행 앱 (DAW 없이 바로 실행)
  Chordsearch.vst3     Ableton 등 DAW용 플러그인

1) 단독 실행 (Chordsearch.exe)
  - 처음 실행할 때 "Windows의 PC 보호" 창이 뜨면
    [추가 정보] -> [실행]을 누르세요. (서명되지 않은 테스트 빌드라서 그렇습니다)
  - 왼쪽 위 [Options] -> Audio/MIDI Settings 에서
      * Output: 사용할 오디오 장치
      * Active MIDI inputs: 건반(MIDI 키보드)을 체크
  - 건반으로 코드를 치면 코드, 키, 스케일, 다음 코드 추천이 표시됩니다.

2) Ableton Live 플러그인 (Chordsearch.vst3)
  - Chordsearch.vst3 폴더를 통째로 아래 위치에 복사하세요 (관리자 권한 필요):
      C:\Program Files\Common Files\VST3\
  - Ableton: 설정 -> Plug-Ins -> "Use VST3 Plug-In System Folders" 켜기 -> Rescan
  - MIDI 트랙에 Chordsearch를 넣고 건반 입력을 받도록 설정하세요.

사용법
  - 코드를 연주하면 아래 PROGRESSION에 진행이 쌓입니다.
  - 코드 위에 마우스: X = 삭제 / 코드 사이에 마우스: + = 코드 추가
  - CONFIRM -> 장르/템포 선택 -> PLAY 로 백킹 밴드 재생
  - METRONOME, BASS / DRUMS / GUITAR / PIANO 버튼으로 각각 켜고 끄기
  - EDIT 을 누르면 진행을 다시 편집할 수 있습니다.

알려진 제한
  - 진행/설정은 아직 저장되지 않습니다.
  - 백킹 템포는 플러그인 자체 템포를 사용합니다 (DAW 템포 동기화 미지원).
