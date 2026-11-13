#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "chordsearch/AnalysisEngine.h"
#include "chordsearch/Harmony.h"
#include "chordsearch/KeyDetector.h"
#include "chordsearch/ProgressionLibrary.h"
#include "chordsearch/Scales.h"

#include <vector>

using namespace chordsearch;
using testing::chord;

namespace
{
    const Key cMajor { 0, Mode::major };
    const Key aMinor { 9, Mode::minor };

    Key detect (const std::vector<Chord>& progression)
    {
        KeyDetector kd;
        double t = 0.0;
        for (const auto& c : progression)
        {
            kd.addChordEvent (c);
            kd.accumulate (c, 1.0);
            t += 1.0;
            kd.evaluate (t);
        }
        // allow hysteresis to settle
        kd.evaluate (t + 5.0);
        return kd.ranking().front().key;
    }
} // namespace

TEST_CASE ("Roman numerals and functions")
{
    CHECK (analyzeHarmony (chord (2, "m7"), cMajor).roman == "ii7");
    CHECK (analyzeHarmony (chord (7, "7"), cMajor).roman == "V7");
    CHECK (analyzeHarmony (chord (0, "maj7"), cMajor).roman == "Imaj7");
    CHECK (analyzeHarmony (chord (11, "m7b5"), cMajor).roman == "viiø7");

    const auto a7 = analyzeHarmony (chord (9, "7"), cMajor);
    CHECK (a7.roman == "V7/ii");
    CHECK (a7.role == ChordRole::secondaryDominant);

    CHECK (analyzeHarmony (chord (2, "7"), cMajor).roman == "V7/V");
    CHECK (analyzeHarmony (chord (1, "7"), cMajor).roman == "subV7");
    CHECK (analyzeHarmony (chord (10, "7"), cMajor).role == ChordRole::borrowed);
    CHECK (analyzeHarmony (chord (5, "m7"), cMajor).role == ChordRole::borrowed);
    CHECK (analyzeHarmony (chord (1, "dim7"), cMajor).roman == "vii°7/ii");

    CHECK (analyzeHarmony (chord (11, "m7b5"), aMinor).roman == "iiø7");
    CHECK (analyzeHarmony (chord (4, "7"), aMinor).roman == "V7");
}

TEST_CASE ("Roman parser round-trip")
{
    for (const char* r : { "ii7", "V7", "Imaj7", "bVImaj7", "iiø7", "#iv°7", "bVII7", "iv6", "I", "vi" })
    {
        const auto t = parseRoman (r);
        REQUIRE (t.has_value());
        CHECK (romanNumeral (t->chord, cMajor) == std::string (r));
    }
}

TEST_CASE ("Progression library parses fully")
{
    for (const auto& p : progressionLibrary())
    {
        INFO (p.name << " : " << p.display);
        std::size_t words = 0;
        for (char ch : p.display)
            if (ch == ' ')
                ++words;
        CHECK (p.tokens.size() >= 2);
        CHECK (p.tokens.size() <= words + 1);
    }
}

TEST_CASE ("Key detection")
{
    CHECK (detect ({ chord (2, "m7"), chord (7, "7"), chord (0, "maj7") }) == cMajor);
    CHECK (detect ({ chord (11, "m7b5"), chord (4, "7"), chord (9, "m7") }) == aMinor);
    CHECK (detect ({ chord (0, ""), chord (7, ""), chord (9, "m"), chord (5, "") }) == cMajor);
    CHECK (detect ({ chord (9, "m"), chord (7, ""), chord (5, ""), chord (4, "7"), chord (9, "m") }) == aMinor);

    const Key fMajor { 5, Mode::major };
    CHECK (detect ({ chord (7, "m7"), chord (0, "7"), chord (5, "maj7"), chord (2, "m7"), chord (7, "m7"), chord (0, "7") }) == fMajor);
}

TEST_CASE ("Chord scales")
{
    auto top = [] (const Chord& c, const Key& k)
    {
        const auto s = ChordScaleAdvisor::suggest (c, c.pitchClasses(), &k, 4);
        REQUIRE (! s.empty());
        return std::string (s.front().type->name);
    };

    CHECK (top (chord (0, "maj7"), cMajor) == "Ionian");
    CHECK (top (chord (5, "maj7"), cMajor) == "Lydian");
    CHECK (top (chord (2, "m7"), cMajor) == "Dorian");
    CHECK (top (chord (4, "m7"), cMajor) == "Phrygian");
    CHECK (top (chord (9, "m7"), cMajor) == "Aeolian");
    CHECK (top (chord (7, "7"), cMajor) == "Mixolydian");
    CHECK (top (chord (11, "m7b5"), cMajor) == "Locrian");
    CHECK (top (chord (10, "7"), cMajor) == "Lydian Dominant");
    CHECK (top (chord (4, "7"), aMinor) == "Phrygian Dominant");
    CHECK (top (chord (7, "7alt"), cMajor) == "Altered");
    CHECK (top (chord (1, "dim7"), cMajor) == "Whole-Half Dim");

    // Available tensions: Cmaj7 in C -> 9, 13 (11 is an avoid note)
    const auto s = ChordScaleAdvisor::suggest (chord (0, "maj7"), chord (0, "maj7").pitchClasses(), &cMajor, 1);
    CHECK (s.front().availableTensions == (Tension::nine | Tension::thirteen));
}

TEST_CASE ("Next chord suggestions")
{
    auto first = [] (const std::vector<Chord>& h, const Key& k)
    {
        const auto s = ChordSuggester::suggest (h, k);
        REQUIRE (! s.empty());
        return s.front().chord.name (k.accidentals());
    };

    CHECK (first ({ chord (2, "m7") }, cMajor) == "G7");
    CHECK (first ({ chord (2, "m7"), chord (7, "7") }, cMajor) == "Cmaj7");
    CHECK (first ({ chord (11, "m7b5") }, aMinor) == "E7(b9)");
    CHECK (first ({ chord (11, "m7b5"), chord (4, "7") }, aMinor) == "Am7");
    CHECK (first ({ chord (9, "7") }, cMajor) == "Dm7");

    // Triad players get triads back
    const auto pop = ChordSuggester::suggest ({ chord (0, ""), chord (7, "") }, cMajor);
    REQUIRE (! pop.empty());
    for (const auto& s : pop)
        CHECK (s.chord.seventh == Seventh::none);
}

TEST_CASE ("Engine: commit, release, key")
{
    AnalysisEngine engine;
    double t = 0.0;
    auto play = [&] (std::initializer_list<int> midi)
    {
        engine.update (testing::notes (midi), t);
        t += 0.1;
        engine.update (testing::notes (midi), t);
        t += 0.6;
        engine.update (testing::notes (midi), t);
    };

    play ({ 50, 65, 69, 72 }); // Dm7
    CHECK (engine.snapshot().chordName == "Dm7");

    // releasing notes one by one keeps Dm7
    engine.update (testing::notes ({ 50, 65 }), t);
    t += 0.2;
    engine.update (testing::notes ({ 50, 65 }), t);
    CHECK (engine.snapshot().chordName == "Dm7");

    play ({ 43, 59, 65, 69 }); // G9 shell-ish
    play ({ 48, 64, 67, 71 }); // Cmaj7

    const auto& s = engine.snapshot();
    CHECK (s.chordName == "Cmaj7");
    REQUIRE (s.hasKey);
    CHECK (s.key == cMajor);
    CHECK (s.history.size() == 3);
    CHECK (s.harmony.roman == "Imaj7");
    CHECK (! s.scales.empty());
    CHECK (! s.suggestions.empty());
}
