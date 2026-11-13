#pragma once

#include "Harmony.h"
#include "Key.h"

#include <string>
#include <vector>

namespace chordsearch
{

struct Progression
{
    std::string name;        // e.g. "ii–V–I"
    std::string genre;       // "Jazz", "Pop", "Blues", "Classical", "Neo-Soul", "J-Pop"
    Mode mode;
    float weight;            // how common / canonical (0..1)
    std::string display;     // roman text as written
    std::vector<RomanToken> tokens;
};

/** Curated library of well-known progressions (jazz standards, pop, blues, classical cadences). */
const std::vector<Progression>& progressionLibrary();

} // namespace chordsearch
