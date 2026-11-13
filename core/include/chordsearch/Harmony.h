#pragma once

#include "Chord.h"
#include "Key.h"

#include <optional>
#include <string>
#include <string_view>

namespace chordsearch
{

enum class HarmonicFunction : std::uint8_t
{
    tonic,
    subdominant,
    dominant,
    minorSubdominant, // borrowed iv, bVI, bVII7 ...
    other
};

enum class ChordRole : std::uint8_t
{
    diatonic,
    secondaryDominant, // V7/x
    tritoneSub,        // subV7/x
    relatedTwo,        // ii7/x
    borrowed,          // modal interchange
    passingDiminished, // vii°7/x, #I°7 ...
    chromatic
};

struct HarmonyAnalysis
{
    std::string roman;        // "V7/ii", "subV7", "bVImaj7", "ii7"
    HarmonicFunction function = HarmonicFunction::other;
    ChordRole role = ChordRole::chromatic;
    int targetDegree = -1;    // applied chords: degree they resolve to (0..11, relative to tonic)
    std::string description;  // short Korean explanation
    float fit = -0.5f;        // how well the chord belongs to the key (-0.5 .. 1)
};

/** A chord described relative to a tonic (root = semitones above tonic). */
struct RomanToken
{
    int degree = 0;
    Chord chord; // root == degree
};

/** Parses jazz-style roman numerals: "ii7", "V7", "bVImaj7", "iiø7", "#iv°7", "I6", "bVII7", "V7alt", "iv6".
    Upper case = major-third chords, lower case = minor-third chords. */
std::optional<RomanToken> parseRoman (std::string_view text);

/** Roman numeral of a chord in a key (major-scale based numerals, jazz convention). */
std::string romanNumeral (const Chord& chord, const Key& key);

/** Functional analysis of a chord in a key. */
HarmonyAnalysis analyzeHarmony (const Chord& chord, const Key& key);

/** Typical 7th-chord built on a scale degree of the key, or nullopt if the degree is not diatonic. */
std::optional<Chord> diatonicSeventh (const Key& key, int degree);

/** Korean label for a function. */
const char* functionLabel (HarmonicFunction f) noexcept;

} // namespace chordsearch
