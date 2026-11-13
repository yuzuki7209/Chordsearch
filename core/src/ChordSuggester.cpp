#include "chordsearch/ChordSuggester.h"
#include "chordsearch/Harmony.h"
#include "chordsearch/ProgressionLibrary.h"
#include "chordsearch/Scales.h"

#include <algorithm>
#include <array>

namespace chordsearch
{

namespace
{
    struct Part
    {
        float score;
        std::string reason;
        std::string source;
    };

    struct Entry
    {
        Chord chord;
        std::vector<Part> parts;

        float best() const
        {
            float b = 0.0f;
            for (const auto& p : parts)
                b = std::max (b, p.score);
            return b;
        }

        float combined() const
        {
            float sum = 0.0f;
            for (const auto& p : parts)
                sum += p.score;
            const float b = best();
            return b + std::min (0.3f, 0.25f * (sum - b));
        }

        const Part& top() const
        {
            return *std::max_element (parts.begin(), parts.end(), [] (const auto& a, const auto& b) { return a.score < b.score; });
        }
    };

    /** Triad version of a chord for players who don't use 7ths. */
    Chord toTriad (Chord c)
    {
        c.tensions = 0;
        switch (c.family())
        {
            case Family::dominant:
                if (! c.isSus()) c.seventh = Seventh::none;
                break;
            case Family::halfDiminished:
            case Family::diminished:
                c.seventh = Seventh::none;
                c.fifth = Fifth::flat;
                break;
            case Family::major:
            case Family::minor:
                c.seventh = Seventh::none;
                if (c.fifth == Fifth::none) c.fifth = Fifth::perfect;
                break;
            default:
                break;
        }
        return c;
    }

    class Pool
    {
    public:
        Pool (const Chord& currentChord, bool useSevenths) : current (currentChord), sevenths (useSevenths) {}

        void add (Chord c, float score, std::string reason, std::string source)
        {
            if (score <= 0.0f)
                return;

            c.root = mod12 (c.root);
            c.bass = c.root;
            c.rootless = false;
            if (! sevenths)
                c = toTriad (c);

            if (c.sameHarmony (current))
                return;

            for (auto& e : entries)
            {
                if (e.chord.root == c.root && e.chord.family() == c.family())
                {
                    e.parts.push_back ({ score, std::move (reason), std::move (source) });
                    return;
                }
            }
            entries.push_back ({ c, { { score, std::move (reason), std::move (source) } } });
        }

        std::vector<Entry> entries;

    private:
        Chord current;
        bool sevenths;
    };

    float familyCompat (Family a, Family b)
    {
        if (a == b) return 1.0f;
        auto pair = [&] (Family x, Family y) { return (a == x && b == y) || (a == y && b == x); };
        if (pair (Family::major, Family::dominant))             return 0.8f;
        if (pair (Family::halfDiminished, Family::diminished))  return 0.8f;
        if (pair (Family::suspended, Family::major))            return 0.7f;
        if (pair (Family::suspended, Family::dominant))         return 0.7f;
        if (pair (Family::power, Family::major))                return 0.6f;
        if (pair (Family::power, Family::minor))                return 0.6f;
        if (pair (Family::augmented, Family::major))            return 0.5f;
        return 0.0f;
    }

    void addRules (Pool& pool, const Chord& cur, const Key& key)
    {
        const int deg = mod12 (cur.root - key.tonic);
        const auto fam = cur.family();
        const bool major = key.mode == Mode::major;
        const auto harmony = analyzeHarmony (cur, key);

        auto at = [&] (int degree, const char* preset) { return *Chord::fromSuffix (key.tonic + degree, preset); };
        auto diat = [&] (int degree, const char* fallback = "maj7")
        {
            if (auto c = diatonicSeventh (key, mod12 (degree)))
                return *c;
            return at (degree, fallback);
        };
        auto rn = [&] (const Chord& c) { return romanNumeral (c, key); };

        const std::string functional = "기능 화성";

        if (fam == Family::dominant)
        {
            const int t = mod12 (deg + 5);
            const bool altered = cur.hasTension (Tension::flat9 | Tension::sharp9 | Tension::flat13);
            const Chord target = diatonicSeventh (key, t) ? *diatonicSeventh (key, t) : at (t, altered ? "m7" : "maj7");
            pool.add (target, 1.0f, t == 0 ? "정격 종지: V7 → I" : "도미넌트 해결: 완전5도 아래로 (" + rn (target) + ")", functional);

            if (deg == 7)
            {
                const auto d = diat (major ? 9 : 8);
                pool.add (d, 0.55f, "거짓 종지: V7 → " + rn (d), functional);
            }
            if (harmony.role == ChordRole::tritoneSub && harmony.targetDegree >= 0)
                pool.add (diat (harmony.targetDegree), 0.95f, "트라이톤 대리 해결: 반음 아래로", functional);
            if (major && deg == 10)
                pool.add (diat (0), 0.9f, "백도어 종지: bVII7 → I", functional);
            if (major && deg == 0)
                pool.add (diat (5), 0.85f, "I7 → IV (블루스 / V7/IV)", functional);
            if (major && deg == 5)
                pool.add (at (0, "7"), 0.6f, "블루스: IV7 → I7", functional);

            // Generic dominant options (apply to secondary dominants too)
            if (deg != 7)
            {
                // Deceptive resolution relative to the expected target:
                // major target -> its vi (whole step above the dominant root),
                // minor target -> its bVI (half step above the dominant root).
                const bool minorTarget = target.family() == Family::minor || target.family() == Family::halfDiminished;
                const int up = mod12 (deg + (minorTarget ? 1 : 2));
                const Chord deceptive = diatonicSeventh (key, up) ? *diatonicSeventh (key, up)
                                                                  : at (up, minorTarget ? "maj7" : "m7");
                pool.add (deceptive, 0.38f, "거짓 해결: " + rn (target) + " 대신 " + rn (deceptive), functional);
            }
            if (t != 0)
                pool.add (at (t, "7"), 0.45f, "도미넌트 체인: 다음 도미넌트로 (5도권)", functional);
            pool.add (diat (0), 0.25f, "토닉으로 복귀", functional);
        }

        if (fam == Family::minor || fam == Family::halfDiminished)
        {
            const bool tonicMinor = ! major && deg == 0;
            const bool borrowedIv = major && deg == 5 && fam == Family::minor;

            if (borrowedIv)
            {
                pool.add (diat (0), 0.8f, "마이너 플라갈 종지: iv → I", functional);
                pool.add (at (10, "7"), 0.7f, "백도어 진행: iv7 → bVII7 → I", functional);
            }
            else if (! tonicMinor)
            {
                if (fam == Family::halfDiminished)
                    pool.add (at (deg + 5, "7b9"), 0.95f, "단조 ii–V: iiø7 → V7(b9)", functional);
                else
                    pool.add (at (deg + 5, "7"), 0.9f, "ii–V: 완전4도 위 도미넌트로", functional);
            }
        }

        if (fam == Family::diminished)
        {
            pool.add (diat (deg + 1), 0.85f, "경과 감7화음 해결: 반음 위로", functional);
            if (cur.seventh == Seventh::diminished)
                for (int k : { 4, 7, 10 }) // dim7 is symmetrical: other resolutions
                    if (diatonicSeventh (key, deg + k + 1))
                        pool.add (diat (deg + k + 1), 0.35f, "감7화음의 대칭 해결", functional);
        }

        if (fam == Family::augmented)
            pool.add (diat (deg + 5), 0.7f, "증화음(V+) → 완전4도 위로 해결", functional);

        if (fam == Family::suspended)
        {
            Chord resolved = cur;
            resolved.third = Third::major;
            resolved.tensions = 0;
            pool.add (resolved, 0.7f, "서스펜션 해결 (4 → 3)", functional);
        }

        // Typical continuations from stable degrees
        if (major && deg == 0 && fam == Family::major)
        {
            pool.add (diat (9), 0.6f, "I → vi (토닉 대리)", functional);
            pool.add (diat (2), 0.6f, "I → ii (서브도미넌트로)", functional);
            pool.add (diat (5), 0.6f, "I → IV (서브도미넌트로)", functional);
            pool.add (diat (7), 0.45f, "I → V (도미넌트로)", functional);
            pool.add (at (9, "7"), 0.5f, "I → VI7 (V7/ii 세컨더리 도미넌트)", functional);
            pool.add (at (1, "dim7"), 0.45f, "I → #I°7 → ii (경과 감화음)", functional);
            pool.add (diat (4), 0.35f, "I → iii", functional);
            pool.add (at (5, "m7"), 0.35f, "모달 인터체인지: iv", functional);
            pool.add (at (0, "7"), 0.35f, "I → I7 (IV로 가는 V7/IV)", functional);
        }
        if (! major && deg == 0 && fam == Family::minor)
        {
            pool.add (diat (5), 0.6f, "i → iv (서브도미넌트)", functional);
            pool.add (diat (2), 0.6f, "i → iiø7 (단조 ii–V 시작)", functional);
            pool.add (diat (8), 0.55f, "i → bVI", functional);
            pool.add (diat (10), 0.5f, "i → bVII", functional);
            pool.add (diat (7), 0.5f, "i → V7", functional);
            pool.add (diat (3), 0.4f, "i → bIII (관계장조)", functional);
        }
        if (major && deg == 5 && fam == Family::major)
        {
            pool.add (diat (7), 0.6f, "IV → V", functional);
            pool.add (diat (0), 0.5f, "변격(플라갈) 종지: IV → I", functional);
            pool.add (at (5, "m6"), 0.6f, "IV → iv (마이너 서브도미넌트)", functional);
            pool.add (diat (4), 0.45f, "IV → iii (하행 진행)", functional);
            pool.add (at (6, "dim7"), 0.45f, "IV → #IV°7 (경과 감화음)", functional);
            pool.add (diat (2), 0.35f, "IV → ii", functional);
        }
        if (major && deg == 9 && fam == Family::minor)
        {
            pool.add (diat (2), 0.65f, "vi → ii (5도권 진행)", functional);
            pool.add (diat (5), 0.55f, "vi → IV", functional);
        }
        if (major && deg == 4 && fam == Family::minor)
        {
            pool.add (diat (9), 0.65f, "iii → vi (5도권 진행)", functional);
            pool.add (at (9, "7"), 0.5f, "iii → VI7 (V7/ii)", functional);
            pool.add (diat (5), 0.4f, "iii → IV", functional);
        }
        if (major && deg == 2 && fam == Family::minor)
            pool.add (at (1, "7"), 0.55f, "ii → subV7 (트라이톤 대리 도미넌트)", functional);
        if (! major && deg == 8 && fam == Family::major)
        {
            pool.add (diat (7), 0.55f, "bVI → V7", functional);
            pool.add (diat (10), 0.55f, "bVI → bVII", functional);
            pool.add (diat (2), 0.45f, "bVI → iiø7", functional);
        }
        if (! major && deg == 10)
        {
            pool.add (diat (3), 0.6f, "bVII → bIII (관계장조의 V–I)", functional);
            pool.add (diat (0), 0.5f, "bVII → i", functional);
        }
        if (! major && deg == 3 && fam == Family::major)
        {
            pool.add (diat (8), 0.55f, "bIII → bVI (5도권 진행)", functional);
            pool.add (diat (5), 0.45f, "bIII → iv", functional);
        }
        if (! major && deg == 5 && fam == Family::minor)
        {
            pool.add (diat (7), 0.6f, "iv → V7", functional);
            pool.add (diat (10), 0.55f, "iv → bVII7 (5도권 진행)", functional);
        }
    }

    void addLibraryMatches (Pool& pool, const std::vector<Chord>& history, const Key& key)
    {
        constexpr std::array<float, 5> lengthFactor { 0.0f, 0.3f, 0.6f, 0.85f, 1.0f };

        for (const auto& p : progressionLibrary())
        {
            if (p.mode != key.mode || p.tokens.size() < 2)
                continue;

            for (std::size_t j = 0; j + 1 < p.tokens.size(); ++j)
            {
                std::size_t len = 0;
                float quality = 1.0f;
                while (len < 4 && len <= j && len < history.size())
                {
                    const auto& tok = p.tokens[j - len];
                    const auto& h = history[history.size() - 1 - len];
                    if (mod12 (h.root - key.tonic) != tok.degree)
                        break;
                    const float m = familyCompat (h.family(), tok.chord.family());
                    if (m <= 0.0f)
                        break;
                    quality *= m;
                    ++len;
                }
                if (len == 0)
                    continue;

                Chord next = p.tokens[j + 1].chord;
                next.root = mod12 (key.tonic + p.tokens[j + 1].degree);
                next.bass = next.root;

                const float s = 0.95f * p.weight * lengthFactor[len] * quality;
                pool.add (next, s, p.name + " · " + p.genre, p.display);
            }
        }
    }
} // namespace

std::vector<Suggestion> ChordSuggester::suggest (const std::vector<Chord>& history, const Key& key, std::size_t maxResults)
{
    std::vector<Suggestion> out;
    if (history.empty())
        return out;

    const Chord& cur = history.back();

    bool useSevenths = false;
    for (std::size_t i = history.size() > 4 ? history.size() - 4 : 0; i < history.size(); ++i)
        if (history[i].seventh != Seventh::none || history[i].tensions != 0)
            useSevenths = true;

    Pool pool (cur, useSevenths);
    addRules (pool, cur, key);
    addLibraryMatches (pool, history, key);

    // Derived: tritone substitutes and secondary dominants of strong candidates
    const auto snapshot = pool.entries;
    for (const auto& e : snapshot)
    {
        const float s = e.best();
        const auto name = e.chord.name (key.accidentals());

        if (e.chord.family() == Family::dominant && s >= 0.5f)
            pool.add (*Chord::fromSuffix (e.chord.root + 6, "7"), s * 0.5f,
                      "트라이톤 대리: " + name + " 대신", "Tritone substitution");

        const auto f = e.chord.family();
        if ((f == Family::major || f == Family::minor) && s >= 0.55f && mod12 (e.chord.root - key.tonic) != 0)
            pool.add (*Chord::fromSuffix (e.chord.root + 7, "7"), s * 0.4f,
                      name + "로 가는 세컨더리 도미넌트", "Secondary dominant");
    }

    // Fallback for chromatic chords with no rule/library match
    if (pool.entries.empty())
    {
        pool.add (*Chord::fromSuffix (cur.root + 5, cur.family() == Family::minor ? "7" : "maj7"), 0.5f, "완전4도 위로 진행", "기능 화성");
        if (auto t = diatonicSeventh (key, 0))
            pool.add (*t, 0.4f, "토닉으로 복귀", "기능 화성");
    }

    const auto curTones = cur.pitchClasses();
    float maxScore = 0.0f;

    for (const auto& e : pool.entries)
    {
        Suggestion s;
        s.chord = e.chord;
        s.score = e.combined();

        // Voice-leading: shared tones make smooth connections
        const auto cand = e.chord.pitchClasses();
        int common = 0;
        for (int pc = 0; pc < 12; ++pc)
            if (cand.contains (pc) && curTones.contains (pc))
                ++common;
        s.score += 0.03f * (float) std::min (common, 3);

        const auto& top = e.top();
        s.reason = top.reason;
        s.source = top.source;
        s.roman = analyzeHarmony (e.chord, key).roman;

        const auto scales = ChordScaleAdvisor::suggest (e.chord, e.chord.pitchClasses(), &key, 1);
        if (! scales.empty())
            s.tensions = scales.front().availableTensions;

        maxScore = std::max (maxScore, s.score);
        out.push_back (std::move (s));
    }

    std::stable_sort (out.begin(), out.end(), [] (const auto& a, const auto& b) { return a.score > b.score; });
    if (out.size() > maxResults)
        out.resize (maxResults);

    if (maxScore > 0.0f)
        for (auto& s : out)
            s.score /= maxScore;

    return out;
}

} // namespace chordsearch
