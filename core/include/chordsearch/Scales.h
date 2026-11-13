#pragma once

#include "Chord.h"
#include "Harmony.h"
#include "Key.h"

#include <string>
#include <vector>

namespace chordsearch
{

struct ScaleType
{
    const char* name;       // English name
    const char* koreanName; // Korean name
    std::uint16_t mask;     // intervals relative to root
};

const std::vector<ScaleType>& allScales();

/** Scale degree (1..7) of an interval within a scale type (7-note scales: one letter per note). */
int scaleDegree (const ScaleType& type, int interval) noexcept;

/** Letter-correct note name of an absolute pitch class within a scale on `root`. */
std::string spellScaleNote (const ScaleType& type, int root, int pc, Accidentals rootAcc);

struct ScaleSuggestion
{
    const ScaleType* type = nullptr;
    int root = 0;
    PitchClassSet notes;             // absolute pitch classes
    std::uint8_t availableTensions = 0;
    float score = 0.0f;
};

/** Chord-scale theory: which scales fit a chord in its harmonic context.

    Candidates must contain every played pitch class. Ranking combines
      - a prior for the chord quality (e.g. m7 → Dorian, m7b5 → Locrian),
      - closeness to the key's parent scale (diatonic chords → modes of the key),
      - functional rules (V7 → minor target: Phrygian dominant / Altered,
        non-resolving dominants (bVII7, IV7, subV) → Lydian dominant). */
class ChordScaleAdvisor
{
public:
    static std::vector<ScaleSuggestion> suggest (const Chord& chord,
                                                 PitchClassSet played,
                                                 const Key* key,
                                                 std::size_t maxResults = 4);

    /** Available tensions of a chord over a scale (avoid-note rule: a tension a half step
        above a chord tone is avoided, except b9/b13 on dominant chords). */
    static std::uint8_t availableTensions (const Chord& chord, PitchClassSet scaleAbsolute);
};

} // namespace chordsearch
