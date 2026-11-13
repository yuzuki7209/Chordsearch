#include "chordsearch/Scales.h"

#include <algorithm>
#include <bit>
#include <initializer_list>
#include <string_view>

namespace chordsearch
{

namespace
{
    constexpr std::uint16_t mask (std::initializer_list<int> intervals)
    {
        std::uint16_t m = 0;
        for (int i : intervals)
            m = std::uint16_t (m | (1u << i));
        return m;
    }

    enum ScaleId
    {
        ionian, dorian, phrygian, lydian, mixolydian, aeolian, locrian,
        melodicMinor, locrianSharp2, lydianDominant, altered, lydianAugmented,
        phrygianDominant, harmonicMinor, halfWholeDim, wholeHalfDim, wholeTone,
        majorPentatonic, minorPentatonic, blues, mixolydianFlat6, dorianFlat2, numScales
    };

    enum class ChordClass { maj, dom, sus7, min7, minTriad, min6, minMaj7, halfDim, dim, aug, augMaj7, susTriad, power };

    ChordClass classify (const Chord& c)
    {
        switch (c.family())
        {
            case Family::major:          return ChordClass::maj;
            case Family::dominant:
                if (c.isSus()) return ChordClass::sus7;
                return ChordClass::dom;
            case Family::minor:
                if (c.seventh == Seventh::minor) return ChordClass::min7;
                if (c.seventh == Seventh::sixth) return ChordClass::min6;
                if (c.seventh == Seventh::major) return ChordClass::minMaj7;
                return ChordClass::minTriad;
            case Family::halfDiminished: return ChordClass::halfDim;
            case Family::diminished:     return ChordClass::dim;
            case Family::augmented:      return c.seventh == Seventh::major ? ChordClass::augMaj7 : ChordClass::aug;
            case Family::suspended:      return ChordClass::susTriad;
            case Family::power:          return ChordClass::power;
        }
        return ChordClass::power;
    }

    struct Prior { int scale; float weight; };

    std::vector<Prior> priorsFor (ChordClass cls)
    {
        switch (cls)
        {
            case ChordClass::maj:      return { { ionian, 1.0f }, { lydian, 0.9f }, { majorPentatonic, 0.55f }, { mixolydian, 0.3f } };
            case ChordClass::dom:      return { { mixolydian, 1.0f }, { lydianDominant, 0.8f }, { altered, 0.8f }, { halfWholeDim, 0.7f },
                                                { phrygianDominant, 0.7f }, { mixolydianFlat6, 0.6f }, { wholeTone, 0.5f }, { blues, 0.4f } };
            case ChordClass::sus7:     return { { mixolydian, 1.0f }, { dorian, 0.5f }, { phrygian, 0.5f }, { dorianFlat2, 0.5f } };
            case ChordClass::min7:     return { { dorian, 1.0f }, { aeolian, 0.9f }, { phrygian, 0.7f }, { minorPentatonic, 0.55f }, { blues, 0.4f } };
            case ChordClass::minTriad: return { { dorian, 1.0f }, { aeolian, 0.95f }, { melodicMinor, 0.6f }, { harmonicMinor, 0.6f },
                                                { phrygian, 0.5f }, { minorPentatonic, 0.55f } };
            case ChordClass::min6:     return { { melodicMinor, 1.0f }, { dorian, 0.9f } };
            case ChordClass::minMaj7:  return { { melodicMinor, 1.0f }, { harmonicMinor, 0.85f } };
            case ChordClass::halfDim:  return { { locrian, 1.0f }, { locrianSharp2, 0.9f } };
            case ChordClass::dim:      return { { wholeHalfDim, 1.0f }, { locrian, 0.5f }, { harmonicMinor, 0.3f } };
            case ChordClass::aug:      return { { wholeTone, 1.0f }, { lydianAugmented, 0.8f } };
            case ChordClass::augMaj7:  return { { lydianAugmented, 1.0f } };
            case ChordClass::susTriad: return { { mixolydian, 0.8f }, { ionian, 0.75f }, { dorian, 0.6f }, { majorPentatonic, 0.5f } };
            case ChordClass::power:    return { { ionian, 0.6f }, { aeolian, 0.6f }, { mixolydian, 0.55f }, { dorian, 0.55f },
                                                { minorPentatonic, 0.5f }, { blues, 0.45f } };
        }
        return {};
    }

    /** Extra weight from harmonic function. */
    float functionalBoost (int scale, const Chord& c, const HarmonyAnalysis& h, const Key& key)
    {
        if (c.family() != Family::dominant || c.isSus())
            return 0.0f;

        // Where does this dominant resolve, and is the target minor?
        bool resolvesToMinor = false;
        bool resolves = false;
        if (h.role == ChordRole::secondaryDominant || (h.role == ChordRole::diatonic && mod12 (c.root - key.tonic) == 7))
        {
            resolves = true;
            const int target = h.role == ChordRole::secondaryDominant ? h.targetDegree : 0;
            if (const auto t = diatonicSeventh (key, target))
                resolvesToMinor = t->family() == Family::minor || t->family() == Family::halfDiminished;
        }

        const bool nonResolving = h.role == ChordRole::tritoneSub || h.role == ChordRole::borrowed
                               || (h.role == ChordRole::diatonic && mod12 (c.root - key.tonic) == 10);

        if (resolves && resolvesToMinor)
        {
            if (scale == phrygianDominant) return 0.35f;
            if (scale == altered)          return 0.25f;
            if (scale == mixolydianFlat6)  return 0.15f;
            if (scale == halfWholeDim)     return 0.1f;
        }
        else if (resolves)
        {
            if (scale == mixolydian)   return 0.2f;
            if (scale == altered)      return 0.1f;
            if (scale == halfWholeDim) return 0.05f;
        }
        else if (nonResolving)
        {
            if (scale == lydianDominant) return 0.4f;
        }
        return 0.0f;
    }
} // namespace

const std::vector<ScaleType>& allScales()
{
    static const std::vector<ScaleType> scales {
        { "Ionian",             "이오니안 (장음계)",     mask ({ 0, 2, 4, 5, 7, 9, 11 }) },
        { "Dorian",             "도리안",                mask ({ 0, 2, 3, 5, 7, 9, 10 }) },
        { "Phrygian",           "프리지안",              mask ({ 0, 1, 3, 5, 7, 8, 10 }) },
        { "Lydian",             "리디안",                mask ({ 0, 2, 4, 6, 7, 9, 11 }) },
        { "Mixolydian",         "믹솔리디안",            mask ({ 0, 2, 4, 5, 7, 9, 10 }) },
        { "Aeolian",            "에올리안 (자연단음계)", mask ({ 0, 2, 3, 5, 7, 8, 10 }) },
        { "Locrian",            "로크리안",              mask ({ 0, 1, 3, 5, 6, 8, 10 }) },
        { "Melodic Minor",      "멜로딕 마이너",         mask ({ 0, 2, 3, 5, 7, 9, 11 }) },
        { "Locrian #2",         "로크리안 #2",           mask ({ 0, 2, 3, 5, 6, 8, 10 }) },
        { "Lydian Dominant",    "리디안 도미넌트",       mask ({ 0, 2, 4, 6, 7, 9, 10 }) },
        { "Altered",            "얼터드",                mask ({ 0, 1, 3, 4, 6, 8, 10 }) },
        { "Lydian Augmented",   "리디안 어그먼티드",     mask ({ 0, 2, 4, 6, 8, 9, 11 }) },
        { "Phrygian Dominant",  "프리지안 도미넌트",     mask ({ 0, 1, 4, 5, 7, 8, 10 }) },
        { "Harmonic Minor",     "하모닉 마이너",         mask ({ 0, 2, 3, 5, 7, 8, 11 }) },
        { "Half-Whole Dim",     "H-W 디미니시드",        mask ({ 0, 1, 3, 4, 6, 7, 9, 10 }) },
        { "Whole-Half Dim",     "W-H 디미니시드",        mask ({ 0, 2, 3, 5, 6, 8, 9, 11 }) },
        { "Whole Tone",         "홀톤",                  mask ({ 0, 2, 4, 6, 8, 10 }) },
        { "Major Pentatonic",   "메이저 펜타토닉",       mask ({ 0, 2, 4, 7, 9 }) },
        { "Minor Pentatonic",   "마이너 펜타토닉",       mask ({ 0, 3, 5, 7, 10 }) },
        { "Blues",              "블루스",                mask ({ 0, 3, 5, 6, 7, 10 }) },
        { "Mixolydian b6",      "믹솔리디안 b6",         mask ({ 0, 2, 4, 5, 7, 8, 10 }) },
        { "Dorian b2",          "도리안 b2",             mask ({ 0, 1, 3, 5, 7, 9, 10 }) },
    };
    return scales;
}

int scaleDegree (const ScaleType& type, int interval) noexcept
{
    interval = mod12 (interval);
    if (std::popcount (unsigned (type.mask)) == 7)
    {
        int degree = 0;
        for (int i = 0; i <= interval; ++i)
            if ((type.mask >> i) & 1u)
                ++degree;
        return degree;
    }

    const std::string_view name (type.name);
    if (name == "Half-Whole Dim")
    {
        static constexpr int m[12] = { 1, 2, 2, 3, 3, 4, 4, 5, 6, 6, 7, 7 }; // conventional: b9 b3 3 #11
        return m[interval];
    }
    if (name == "Whole Tone")
    {
        static constexpr int m[12] = { 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 7, 7 };
        return m[interval];
    }
    static constexpr int m[12] = { 1, 2, 2, 3, 3, 4, 5, 5, 6, 6, 7, 7 };
    return m[interval];
}

std::string spellScaleNote (const ScaleType& type, int root, int pc, Accidentals rootAcc)
{
    const int interval = mod12 (pc - root);
    return spellDegree (root, rootAcc, interval, scaleDegree (type, interval));
}

std::uint8_t ChordScaleAdvisor::availableTensions (const Chord& chord, PitchClassSet scaleAbsolute)
{
    const auto scaleRel = scaleAbsolute.relativeTo (chord.root);
    const auto basic = [&]
    {
        Chord c = chord;
        c.tensions = 0;
        return c.intervals();
    }();

    const bool dominant = chord.family() == Family::dominant;
    std::uint8_t result = 0;

    for (std::uint8_t flag : { Tension::flat9, Tension::nine, Tension::sharp9, Tension::eleven,
                               Tension::sharp11, Tension::flat13, Tension::thirteen })
    {
        const int iv = Tension::interval (flag);
        if (! scaleRel.contains (iv) || basic.contains (iv))
            continue;

        if (flag == Tension::sharp9 && chord.third != Third::major)
            continue; // #9 only meaningful over a major third
        if (flag == Tension::eleven && chord.third == Third::sus4)
            continue;

        const bool halfStepAboveChordTone = basic.contains (iv - 1);
        const bool exempt = dominant && (flag == Tension::flat9 || flag == Tension::flat13);
        if (halfStepAboveChordTone && ! exempt)
            continue;

        result |= flag;
    }
    return result;
}

std::vector<ScaleSuggestion> ChordScaleAdvisor::suggest (const Chord& chord, PitchClassSet played,
                                                         const Key* key, std::size_t maxResults)
{
    const auto& scales = allScales();
    const auto playedRel = played.relativeTo (chord.root).bits() | chord.intervals().bits();
    const auto cls = classify (chord);

    std::optional<HarmonyAnalysis> harmony;
    if (key != nullptr)
        harmony = analyzeHarmony (chord, *key);

    std::vector<ScaleSuggestion> out;

    for (const auto& prior : priorsFor (cls))
    {
        const auto& type = scales[(std::size_t) prior.scale];
        if ((playedRel & ~type.mask) != 0)
            continue; // scale must contain every played note

        ScaleSuggestion s;
        s.type = &type;
        s.root = chord.root;
        s.notes = PitchClassSet (type.mask).transposed (chord.root);
        s.score = prior.weight;

        if (key != nullptr)
        {
            const auto keyScale = key->scale();
            int inKey = 0;
            for (int pc = 0; pc < 12; ++pc)
                if (s.notes.contains (pc) && keyScale.contains (pc))
                    ++inKey;

            const float affinity = (float) inKey / (float) s.notes.size();
            s.score += affinity;

            if (harmony->role == ChordRole::diatonic && inKey == s.notes.size() && s.notes.size() == 7)
                s.score += 0.5f; // the mode of the parent scale

            s.score += functionalBoost (prior.scale, chord, *harmony, *key);
        }

        s.availableTensions = availableTensions (chord, s.notes);
        out.push_back (s);
    }

    std::stable_sort (out.begin(), out.end(), [] (const auto& a, const auto& b) { return a.score > b.score; });
    if (out.size() > maxResults)
        out.resize (maxResults);
    return out;
}

} // namespace chordsearch
