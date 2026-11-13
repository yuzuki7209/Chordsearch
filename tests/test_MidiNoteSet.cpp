#include <catch2/catch_test_macros.hpp>

#include "chordsearch/MidiNoteSet.h"

using namespace chordsearch;

TEST_CASE ("MidiNoteSet set/clear across both words")
{
    MidiNoteSet s;
    REQUIRE (s.empty());
    REQUIRE (s.lowest() == -1);
    REQUIRE (s.highest() == -1);

    s.set (48, true);
    s.set (64, true);
    s.set (127, true);
    s.set (200, true); // ignored

    REQUIRE (s.size() == 3);
    REQUIRE (s.lowest() == 48);
    REQUIRE (s.highest() == 127);

    s.set (48, false);
    REQUIRE (s.lowest() == 64);
}

TEST_CASE ("MidiNoteSet pitchClasses folds octaves")
{
    MidiNoteSet s;
    s.set (36, true); // C2
    s.set (52, true); // E3
    s.set (67, true); // G4
    s.set (72, true); // C5

    const auto pcs = s.pitchClasses();
    REQUIRE (pcs.size() == 3);
    REQUIRE (pcs.contains (0));
    REQUIRE (pcs.contains (4));
    REQUIRE (pcs.contains (7));
}
