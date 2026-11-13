#pragma once

#include "Chord.h"
#include "Key.h"

#include <string>
#include <vector>

namespace chordsearch
{

struct Suggestion
{
    Chord chord;
    std::string roman;
    std::string reason;   // why (Korean)
    std::string source;   // matched progression / rule family
    std::uint8_t tensions = 0; // recommended colour tones
    float score = 0.0f;   // 0..1 (relative to best)
};

/** Next-chord recommendation.

    Hybrid scoring:
      - functional-harmony rules (dominant resolution, ii–V, tritone subs, backdoor,
        modal interchange, passing diminished, secondary dominants, deceptive cadence)
      - n-gram matching of the recent progression against a library of well-known
        progressions (longest suffix match wins)
      - a small voice-leading bonus (common tones). */
class ChordSuggester
{
public:
    static std::vector<Suggestion> suggest (const std::vector<Chord>& history, const Key& key, std::size_t maxResults = 6);
};

} // namespace chordsearch
