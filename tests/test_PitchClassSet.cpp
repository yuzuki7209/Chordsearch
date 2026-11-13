#include <catch2/catch_test_macros.hpp>

#include "chordsearch/NoteNames.h"
#include "chordsearch/PitchClassSet.h"

using namespace chordsearch;

TEST_CASE ("PitchClassSet add/contains/size")
{
    PitchClassSet s;
    REQUIRE (s.empty());
    s.add (0);
    s.add (4);
    s.add (19); // G, wrapped
    s.add (-5); // G again
    REQUIRE (s.size() == 3);
    REQUIRE (s.contains (0));
    REQUIRE (s.contains (4));
    REQUIRE (s.contains (7));
    REQUIRE_FALSE (s.contains (1));
}

TEST_CASE ("PitchClassSet relativeTo rotates root to 0")
{
    PitchClassSet g; // G B D
    g.add (7);
    g.add (11);
    g.add (2);

    PitchClassSet major; // C E G
    major.add (0);
    major.add (4);
    major.add (7);

    REQUIRE (g.relativeTo (7) == major);
    REQUIRE (major.transposed (7) == g);
    REQUIRE (g.relativeTo (0) == g);
}

TEST_CASE ("Note names")
{
    REQUIRE (pitchClassName (1, Accidentals::sharps) == "C#");
    REQUIRE (pitchClassName (1, Accidentals::flats) == "Db");
    REQUIRE (midiNoteName (60) == "C4");
    REQUIRE (midiNoteName (21) == "A0");
    REQUIRE (midiNoteName (0) == "C-1");
}
