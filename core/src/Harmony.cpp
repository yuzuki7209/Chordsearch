#include "chordsearch/Harmony.h"

#include <array>
#include <cctype>

namespace chordsearch
{

namespace
{
    using HF = HarmonicFunction;

    struct DiatonicEntry
    {
        int degree;
        Family family;
        float fit;
        HF function;
        const char* seventhPreset; // nullptr = not the primary chord on this degree
    };

    // Major key: Ionian harmony.
    constexpr std::array<DiatonicEntry, 10> majorTable {{
        { 0,  Family::major,          1.0f, HF::tonic,       "maj7" },
        { 2,  Family::minor,          1.0f, HF::subdominant, "m7" },
        { 4,  Family::minor,          1.0f, HF::tonic,       "m7" },
        { 5,  Family::major,          1.0f, HF::subdominant, "maj7" },
        { 7,  Family::dominant,       1.0f, HF::dominant,    "7" },
        { 7,  Family::major,          0.9f, HF::dominant,    nullptr },
        { 7,  Family::suspended,      0.8f, HF::dominant,    nullptr },
        { 9,  Family::minor,          1.0f, HF::tonic,       "m7" },
        { 11, Family::halfDiminished, 1.0f, HF::dominant,    "m7b5" },
        { 11, Family::diminished,     0.8f, HF::dominant,    nullptr },
    }};

    // Minor key: natural + harmonic + melodic minor as used in jazz.
    constexpr std::array<DiatonicEntry, 17> minorTable {{
        { 0,  Family::minor,          1.0f, HF::tonic,       "m7" },
        { 2,  Family::halfDiminished, 1.0f, HF::subdominant, "m7b5" },
        { 2,  Family::diminished,     0.8f, HF::subdominant, nullptr },
        { 3,  Family::major,          1.0f, HF::tonic,       "maj7" },
        { 3,  Family::augmented,      0.7f, HF::tonic,       nullptr },
        { 5,  Family::minor,          1.0f, HF::subdominant, "m7" },
        { 5,  Family::dominant,       0.6f, HF::subdominant, nullptr },
        { 7,  Family::dominant,       1.0f, HF::dominant,    "7" },
        { 7,  Family::major,          0.9f, HF::dominant,    nullptr },
        { 7,  Family::minor,          0.7f, HF::dominant,    nullptr },
        { 7,  Family::suspended,      0.8f, HF::dominant,    nullptr },
        { 8,  Family::major,          1.0f, HF::subdominant, "maj7" },
        { 9,  Family::halfDiminished, 0.6f, HF::subdominant, nullptr },
        { 10, Family::dominant,       0.9f, HF::subdominant, "7" },
        { 10, Family::major,          0.9f, HF::subdominant, nullptr },
        { 11, Family::diminished,     0.9f, HF::dominant,    "dim7" },
        { 11, Family::halfDiminished, 0.6f, HF::dominant,    nullptr },
    }};

    template <typename Fn>
    void forEachEntry (Mode mode, Fn&& fn)
    {
        if (mode == Mode::major)
            for (const auto& e : majorTable) fn (e);
        else
            for (const auto& e : minorTable) fn (e);
    }

    float entryMatch (const DiatonicEntry& e, Family fam)
    {
        if (e.family == fam)
            return e.fit;
        if (fam == Family::suspended && (e.family == Family::major || e.family == Family::minor || e.family == Family::dominant))
            return 0.7f * e.fit;
        if (fam == Family::power && (e.family == Family::major || e.family == Family::minor))
            return 0.6f * e.fit;
        return 0.0f;
    }

    /** Best diatonic entry for degree/family, or nullptr. */
    const DiatonicEntry* findDiatonic (Mode mode, int degree, Family fam, float& fitOut)
    {
        const DiatonicEntry* best = nullptr;
        fitOut = 0.0f;
        forEachEntry (mode, [&] (const DiatonicEntry& e)
        {
            if (e.degree != degree)
                return;
            const float f = entryMatch (e, fam);
            if (f > fitOut)
            {
                fitOut = f;
                best = &e;
            }
        });
        return best;
    }

    /** Primary (stable, non-diminished) diatonic chord on a degree, used as resolution target. */
    const DiatonicEntry* primaryTarget (Mode mode, int degree)
    {
        const DiatonicEntry* found = nullptr;
        forEachEntry (mode, [&] (const DiatonicEntry& e)
        {
            if (found != nullptr || e.degree != degree || e.seventhPreset == nullptr)
                return;
            if (e.family == Family::major || e.family == Family::minor || e.family == Family::dominant)
                found = &e;
        });
        return found;
    }

    bool isLowerCase (Family f)
    {
        return f == Family::minor || f == Family::halfDiminished || f == Family::diminished;
    }

    std::string degreeName (int degree, Family fam)
    {
        static constexpr std::array<const char*, 12> names { "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII" };

        std::string n;
        const bool dimLike = fam == Family::diminished || fam == Family::halfDiminished;
        if (dimLike)
        {
            switch (degree)
            {
                case 1: n = "#I"; break;
                case 3: n = "#II"; break;
                case 6: n = "#IV"; break;
                case 8: n = "#V"; break;
                default: n = names[(std::size_t) degree]; break;
            }
        }
        else
        {
            n = degree == 6 ? "bV" : names[(std::size_t) degree];
        }

        if (isLowerCase (fam))
            for (auto& ch : n)
                if (ch == 'I' || ch == 'V')
                    ch = (char) std::tolower ((unsigned char) ch);
        return n;
    }

    std::string romanSuffix (const Chord& c)
    {
        switch (c.family())
        {
            case Family::major:
                if (c.seventh == Seventh::major) return "maj7";
                if (c.seventh == Seventh::sixth) return "6";
                return "";
            case Family::augmented:      return c.seventh == Seventh::major ? "+maj7" : "+";
            case Family::dominant:       return c.isSus() ? "7sus" : "7";
            case Family::minor:
                if (c.seventh == Seventh::minor) return "7";
                if (c.seventh == Seventh::sixth) return "6";
                if (c.seventh == Seventh::major) return "(maj7)";
                return "";
            case Family::halfDiminished: return "ø7";
            case Family::diminished:     return c.seventh == Seventh::diminished ? "°7" : "°";
            case Family::suspended:      return "sus";
            case Family::power:          return "5";
        }
        return "";
    }

    std::string targetName (Mode mode, int degree)
    {
        const auto* t = primaryTarget (mode, degree);
        return degreeName (degree, t != nullptr ? t->family : Family::major);
    }

    bool isStableTarget (Mode mode, int degree)
    {
        return primaryTarget (mode, degree) != nullptr;
    }
} // namespace

const char* functionLabel (HarmonicFunction f) noexcept
{
    switch (f)
    {
        case HF::tonic:            return "토닉";
        case HF::subdominant:      return "서브도미넌트";
        case HF::dominant:         return "도미넌트";
        case HF::minorSubdominant: return "마이너 서브도미넌트";
        case HF::other:            return "반음계";
    }
    return "";
}

std::string romanNumeral (const Chord& chord, const Key& key)
{
    const int degree = mod12 (chord.root - key.tonic);
    return degreeName (degree, chord.family()) + romanSuffix (chord);
}

std::optional<Chord> diatonicSeventh (const Key& key, int degree)
{
    std::optional<Chord> result;
    degree = mod12 (degree);
    forEachEntry (key.mode, [&] (const DiatonicEntry& e)
    {
        if (! result && e.degree == degree && e.seventhPreset != nullptr)
            result = Chord::fromSuffix (key.tonic + degree, e.seventhPreset);
    });
    return result;
}

HarmonyAnalysis analyzeHarmony (const Chord& chord, const Key& key)
{
    HarmonyAnalysis a;
    const int deg = mod12 (chord.root - key.tonic);
    const Family fam = chord.family();
    const bool major = key.mode == Mode::major;

    a.roman = romanNumeral (chord, key);

    // 1. Diatonic
    float fit = 0.0f;
    if (const auto* e = findDiatonic (key.mode, deg, fam, fit))
    {
        a.role = ChordRole::diatonic;
        a.function = e->function;
        a.fit = fit;
        a.description = std::string ("다이아토닉 · ") + functionLabel (e->function);
        return a;
    }

    auto set = [&] (ChordRole role, HarmonicFunction fn, float f, std::string roman, std::string desc, int target = -1)
    {
        a.role = role;
        a.function = fn;
        a.fit = f;
        if (! roman.empty())
            a.roman = std::move (roman);
        a.description = std::move (desc);
        a.targetDegree = target;
        return a;
    };

    // 2. Dominant-type chords
    if (fam == Family::dominant)
    {
        if (major && deg == 10)
            return set (ChordRole::borrowed, HF::minorSubdominant, 0.45f, {}, "백도어 도미넌트 (bVII7 → I)", 0);

        if (major && deg == 5)
            return set (ChordRole::borrowed, HF::subdominant, 0.35f, {}, "블루스 IV7 (믹솔리디안 차용)");

        const int fifthBelow = mod12 (deg + 5);
        if (isStableTarget (key.mode, fifthBelow))
        {
            const auto t = targetName (key.mode, fifthBelow);
            std::string desc = "세컨더리 도미넌트 → " + t;
            if (major && deg == 0)
                desc += " (블루스 I7)";
            return set (ChordRole::secondaryDominant, HF::dominant, 0.45f,
                        std::string (chord.isSus() ? "V7sus/" : "V7/") + t, desc, fifthBelow);
        }

        const int halfBelow = mod12 (deg + 11);
        if (isStableTarget (key.mode, halfBelow))
        {
            const auto t = targetName (key.mode, halfBelow);
            const bool toTonic = halfBelow == 0;
            return set (ChordRole::tritoneSub, HF::dominant, 0.35f,
                        toTonic ? std::string ("subV7") : "subV7/" + t,
                        "트라이톤 대리 → " + t + " (반음 아래로 해결)", halfBelow);
        }

        return set (ChordRole::chromatic, HF::other, -0.3f, {}, "반음계적 도미넌트");
    }

    // 3. Minor / half-diminished
    if (fam == Family::minor || fam == Family::halfDiminished)
    {
        if (major)
        {
            if (deg == 5 && fam == Family::minor)
                return set (ChordRole::borrowed, HF::minorSubdominant, 0.5f, {}, "모달 인터체인지: 평행단조의 iv");
            if (deg == 2 && fam == Family::halfDiminished)
                return set (ChordRole::borrowed, HF::minorSubdominant, 0.5f, {}, "모달 인터체인지: 평행단조의 iiø7");
            if (deg == 0 && fam == Family::minor)
                return set (ChordRole::borrowed, HF::tonic, 0.3f, {}, "평행단조 토닉 (i)");
            if (deg == 7 && fam == Family::minor)
                return set (ChordRole::borrowed, HF::subdominant, 0.4f, {}, "믹솔리디안 차용 v7 (IV로 가는 ii)", 5);
        }
        else
        {
            if (deg == 2 && fam == Family::minor)
                return set (ChordRole::borrowed, HF::subdominant, 0.5f, {}, "도리안 ii7 (멜로딕 마이너)");
        }

        const int target = mod12 (deg + 10); // ii of x => x is a whole step below
        if (target != 0 && isStableTarget (key.mode, target))
        {
            const auto t = targetName (key.mode, target);
            return set (ChordRole::relatedTwo, HF::subdominant, 0.35f,
                        std::string (fam == Family::halfDiminished ? "iiø7/" : "ii7/") + t,
                        "Related ii: " + t + "로 가는 ii–V의 ii", target);
        }

        return set (ChordRole::chromatic, HF::other, -0.4f, {}, "반음계적 마이너 화음");
    }

    // 4. Major / augmented: modal interchange
    if (fam == Family::major || fam == Family::augmented || fam == Family::suspended)
    {
        if (major)
        {
            if (deg == 8)  return set (ChordRole::borrowed, HF::minorSubdominant, 0.5f, {}, "모달 인터체인지: 평행단조의 bVI");
            if (deg == 10) return set (ChordRole::borrowed, HF::minorSubdominant, 0.5f, {}, "모달 인터체인지: 평행단조의 bVII");
            if (deg == 3)  return set (ChordRole::borrowed, HF::tonic, 0.45f, {}, "모달 인터체인지: 평행단조의 bIII");
            if (deg == 1)  return set (ChordRole::borrowed, HF::subdominant, 0.35f, {}, "나폴리 bII (프리지안 차용)");
        }
        else
        {
            if (deg == 0)  return set (ChordRole::borrowed, HF::tonic, 0.4f, {}, "평행장조 I (피카르디 3도)");
            if (deg == 5)  return set (ChordRole::borrowed, HF::subdominant, 0.4f, {}, "도리안 IV (멜로딕 마이너)");
            if (deg == 1)  return set (ChordRole::borrowed, HF::subdominant, 0.4f, {}, "나폴리 bII");
        }
        return set (ChordRole::chromatic, HF::other, -0.4f, {}, "반음계적 화음");
    }

    // 5. Diminished
    if (fam == Family::diminished)
    {
        const int target = mod12 (deg + 1);
        if (isStableTarget (key.mode, target))
        {
            const auto t = targetName (key.mode, target);
            return set (ChordRole::passingDiminished, HF::dominant, 0.4f,
                        target == 0 ? std::string ("vii°7") : "vii°7/" + t,
                        "경과 감7화음 → " + t + " (반음 위로 해결)", target);
        }
        if (deg == 0)
            return set (ChordRole::passingDiminished, HF::tonic, 0.3f, {}, "공통음 감화음 (I°7)");
        return set (ChordRole::chromatic, HF::other, -0.3f, {}, "반음계적 감화음");
    }

    return set (ChordRole::chromatic, HF::other, -0.5f, {}, "반음계적 화음");
}

std::optional<RomanToken> parseRoman (std::string_view text)
{
    std::size_t i = 0;
    int accidental = 0;
    if (i < text.size() && text[i] == 'b') { accidental = -1; ++i; }
    else if (i < text.size() && text[i] == '#') { accidental = 1; ++i; }

    const std::size_t numStart = i;
    while (i < text.size() && (text[i] == 'I' || text[i] == 'V' || text[i] == 'i' || text[i] == 'v'))
        ++i;

    if (i == numStart)
        return std::nullopt;

    std::string numeral (text.substr (numStart, i - numStart));
    const bool lower = std::islower ((unsigned char) numeral[0]) != 0;
    for (auto& ch : numeral)
        ch = (char) std::toupper ((unsigned char) ch);

    static constexpr std::array<std::pair<std::string_view, int>, 7> numerals {{
        { "I", 0 }, { "II", 2 }, { "III", 4 }, { "IV", 5 }, { "V", 7 }, { "VI", 9 }, { "VII", 11 } }};

    int degree = -1;
    for (const auto& [n, d] : numerals)
        if (n == numeral)
            degree = d;
    if (degree < 0)
        return std::nullopt;

    degree = mod12 (degree + accidental);
    const std::string_view suffix = text.substr (i);

    std::string preset;
    if (lower)
    {
        if (suffix.empty())                         preset = "m";
        else if (suffix == "7")                     preset = "m7";
        else if (suffix == "6")                     preset = "m6";
        else if (suffix == "9")                     preset = "m9";
        else if (suffix == "11")                    preset = "m11";
        else if (suffix == "(maj7)")                preset = "m(maj7)";
        else if (suffix == "ø7" || suffix == "ø")   preset = "m7b5";
        else if (suffix == "°7")                    preset = "dim7";
        else if (suffix == "°")                     preset = "dim";
        else return std::nullopt;
    }
    else
    {
        if (suffix == "7sus")      preset = "7sus4";
        else if (suffix == "sus")  preset = "sus4";
        else                       preset = std::string (suffix);
    }

    auto chord = Chord::fromSuffix (degree, preset);
    if (! chord)
        return std::nullopt;

    return RomanToken { degree, *chord };
}

} // namespace chordsearch
