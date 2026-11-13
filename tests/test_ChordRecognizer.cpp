#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "chordsearch/Scales.h"

using testing::chordName;

// MIDI: C3 = 48, C4 = 60
TEST_CASE ("Triads")
{
    CHECK (chordName ({ 60, 64, 67 }) == "C");
    CHECK (chordName ({ 57, 60, 64 }) == "Am");
    CHECK (chordName ({ 59, 62, 65 }) == "Bdim");
    CHECK (chordName ({ 60, 64, 68 }) == "C+");
    CHECK (chordName ({ 60, 65, 67 }) == "Csus4");
    CHECK (chordName ({ 60, 62, 67 }) == "Csus2");
}

TEST_CASE ("Inversions become slash chords")
{
    CHECK (chordName ({ 52, 55, 60 }) == "C/E");
    CHECK (chordName ({ 55, 60, 64 }) == "C/G");
}

TEST_CASE ("Seventh chords")
{
    CHECK (chordName ({ 60, 64, 67, 71 }) == "Cmaj7");
    CHECK (chordName ({ 62, 65, 69, 72 }) == "Dm7");
    CHECK (chordName ({ 55, 59, 62, 65 }) == "G7");
    CHECK (chordName ({ 59, 62, 65, 69 }) == "Bm7b5");
    CHECK (chordName ({ 60, 63, 66, 69 }) == "Cdim7");
    CHECK (chordName ({ 60, 63, 67, 71 }) == "Cm(maj7)");
    CHECK (chordName ({ 60, 65, 67, 70 }) == "C7sus4");
}

TEST_CASE ("Sixth chords vs minor seventh depend on bass")
{
    CHECK (chordName ({ 48, 64, 67, 69 }) == "C6");
    CHECK (chordName ({ 45, 60, 64, 67 }) == "Am7");
    CHECK (chordName ({ 48, 52, 57, 62 }) == "C6/9");
}

TEST_CASE ("Jazz tensions")
{
    CHECK (chordName ({ 48, 64, 70, 74 }) == "C9");                    // C E Bb D
    CHECK (chordName ({ 43, 59, 65, 69, 76 }) == "G13");               // G B F A E
    CHECK (chordName ({ 48, 64, 70, 73 }) == "C7(b9)");                // C E Bb Db
    CHECK (chordName ({ 48, 52, 58, 61, 63, 68 }) == "C7alt");         // C E Bb Db D# Ab
    CHECK (chordName ({ 48, 64, 71, 74, 78 }) == "Cmaj9(#11)");        // C E B D F#
    CHECK (chordName ({ 50, 65, 72, 76 }) == "Dm9");                   // D F C E
    CHECK (chordName ({ 50, 65, 67, 72, 76 }) == "Dm11");              // D F G C E
    CHECK (chordName ({ 55, 65, 69, 72 }) == "G9sus4");                // G F A C
    CHECK (chordName ({ 48, 64, 67, 74 }) == "Cadd9");
}

TEST_CASE ("Shell voicings without fifth")
{
    CHECK (chordName ({ 48, 64, 70 }) == "C7");  // root-3-7
    CHECK (chordName ({ 50, 65, 72 }) == "Dm7"); // root-b3-b7
    CHECK (chordName ({ 48, 64, 71 }) == "Cmaj7");
}

TEST_CASE ("Sharps spelling")
{
    CHECK (chordName ({ 54, 58, 61 }, chordsearch::Accidentals::sharps) == "F#");
    CHECK (chordName ({ 54, 58, 61 }, chordsearch::Accidentals::flats) == "Gb");
}

TEST_CASE ("Letter-correct spelling")
{
    using namespace chordsearch;
    const auto a7b9 = testing::chord (9, "7b9");
    CHECK (a7b9.spell (1, Accidentals::sharps) == "C#");  // 3rd
    CHECK (a7b9.spell (10, Accidentals::sharps) == "Bb"); // b9
    CHECK (testing::chord (0, "dim7").spell (9, Accidentals::flats) == "Bbb");
    CHECK (testing::chord (0, "7#11").spell (6, Accidentals::flats) == "F#");
    CHECK (testing::chord (0, "m7b5").spell (6, Accidentals::flats) == "Gb");

    const auto& scales = allScales();
    auto find = [&] (const char* n) -> const ScaleType& {
        for (const auto& s : scales) if (std::string (s.name) == n) return s;
        return scales.front();
    };
    // G Altered: G Ab Bb B Db Eb F
    std::string alt;
    for (int k = 0; k < 12; ++k)
        if ((find ("Altered").mask >> k) & 1)
            alt += spellScaleNote (find ("Altered"), 7, 7 + k, Accidentals::sharps) + " ";
    CHECK (alt == "G Ab Bb Cb Db Eb F ");
    // F# Lydian: F# G# A# B# C# D# E#
    std::string lyd;
    for (int k = 0; k < 12; ++k)
        if ((find ("Lydian").mask >> k) & 1)
            lyd += spellScaleNote (find ("Lydian"), 6, 6 + k, Accidentals::sharps) + " ";
    CHECK (lyd == "F# G# A# B# C# D# E# ");
}
