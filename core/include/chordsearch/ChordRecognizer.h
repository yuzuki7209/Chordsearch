#pragma once

#include "Chord.h"

#include <vector>

namespace chordsearch
{

struct ChordCandidate
{
    Chord chord;
    float score = 0.0f;
};

/** Turns a set of pitch classes (+ bass) into ranked chord interpretations.

    For each of the 12 possible roots the set is decomposed into third / fifth /
    seventh / tensions; each interpretation is scored by how "tertian" and
    simple it is (guide tones present, root in bass, few or no avoid combinations).
    Rootless interpretations (root not played) are allowed with a penalty. */
class ChordRecognizer
{
public:
    static std::vector<ChordCandidate> recognize (PitchClassSet pcs, int bassPc, std::size_t maxResults = 4);

    /** Interprets `pcs` with the given root. */
    static ChordCandidate evaluate (PitchClassSet pcs, int root, int bassPc);
};

} // namespace chordsearch
