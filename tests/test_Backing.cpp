#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "chordsearch/AnalysisEngine.h"
#include "chordsearch/backing/Synth.h"

#include <cmath>

using namespace chordsearch;
using namespace chordsearch::backing;
using testing::chord;

namespace
{
    std::vector<ChartChord> twoFiveOne()
    {
        return { { chord (2, "m7"), 4 }, { chord (7, "7"), 4 }, { chord (0, "maj7"), 8 } };
    }

    std::vector<NoteEvent> of (const Sequence& s, Instrument inst)
    {
        std::vector<NoteEvent> out;
        for (const auto& e : s.events)
            if (e.instrument == inst)
                out.push_back (e);
        return out;
    }
} // namespace

TEST_CASE ("Sequence basics for every genre")
{
    for (int g = 0; g < numGenres; ++g)
    {
        const auto genre = (Genre) g;
        INFO (genreName (genre));
        const auto seq = generateSequence (twoFiveOne(), genre, 7);

        CHECK (seq.chartBeats == 16.0);
        CHECK (std::fmod (seq.lengthBeats, 4.0) == 0.0);
        REQUIRE (! seq.events.empty());

        for (std::size_t i = 1; i < seq.events.size(); ++i)
            CHECK (seq.events[i - 1].start <= seq.events[i].start);

        for (const auto& e : seq.events)
        {
            CHECK (e.start >= 0.0);
            CHECK (e.start < seq.lengthBeats);
            CHECK (e.velocity > 0.0f);
            CHECK (e.velocity <= 1.0f);
        }

        for (auto inst : { Instrument::bass, Instrument::drums, Instrument::guitar, Instrument::piano, Instrument::click })
            CHECK (! of (seq, inst).empty());

        for (const auto& e : of (seq, Instrument::bass))
        {
            CHECK (e.note >= 21);
            CHECK (e.note <= 55);
        }
    }
}

TEST_CASE ("Swing bass walks quarter notes and lands on roots")
{
    const auto seq = generateSequence (twoFiveOne(), Genre::swing, 3, 1);
    const auto bass = of (seq, Instrument::bass);

    int onBeats = 0;
    for (const auto& e : bass)
        if (std::abs (e.start - std::round (e.start)) < 0.02)
            ++onBeats;
    CHECK (onBeats >= 15); // one per beat over 4 bars

    // Downbeat of each chord is its root
    for (double t : { 0.0, 4.0, 8.0 })
    {
        const auto it = std::find_if (bass.begin(), bass.end(), [&] (const NoteEvent& e) { return std::abs (e.start - t) < 0.02; });
        REQUIRE (it != bass.end());
        const int expectedRoot = t == 0.0 ? 2 : (t == 4.0 ? 7 : 0);
        CHECK (it->note % 12 == expectedRoot);
    }
}

TEST_CASE ("Rock drums: backbeat on 2 and 4")
{
    const auto seq = generateSequence (twoFiveOne(), Genre::rock, 1, 1);
    int backbeats = 0;
    for (const auto& e : of (seq, Instrument::drums))
        if (e.note == Drum::snare && std::abs (std::fmod (e.start, 2.0) - 1.0) < 0.02)
            ++backbeats;
    CHECK (backbeats >= 7);
}

TEST_CASE ("Voice leading keeps movement small")
{
    const auto c = voiceLead ({ 4, 7, 11, 2 }, 50, 74, {});       // Cmaj9 rootless
    const auto f = voiceLead ({ 9, 0, 4, 7 }, 50, 74, c);          // Fmaj9 rootless
    REQUIRE (c.size() == 4);
    REQUIRE (f.size() == 4);
    int movement = 0;
    for (std::size_t i = 0; i < 4; ++i)
        movement += std::abs (c[i] - f[i]);
    CHECK (movement <= 8);
    for (int n : f)
    {
        CHECK (n >= 50);
        CHECK (n <= 74);
    }
}

TEST_CASE ("Player renders finite audio and loops")
{
    const auto seq = generateSequence (twoFiveOne(), Genre::funk, 5);
    BackingPlayer player;
    player.prepare (48000.0);
    player.setSequence (&seq);

    BackingPlayer::Params p;
    p.playing = true;
    p.bpm = 240.0;
    p.metronome = true;

    std::vector<float> l (512), r (512);
    double peak = 0.0;
    const int blocks = (int) (seq.lengthBeats * 60.0 / p.bpm * 48000.0 / 512.0) + 20; // a full loop and a bit
    for (int b = 0; b < blocks; ++b)
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        player.process (l.data(), r.data(), 512, p);
        for (int i = 0; i < 512; ++i)
        {
            REQUIRE (std::isfinite (l[(std::size_t) i]));
            REQUIRE (std::isfinite (r[(std::size_t) i]));
            peak = std::max (peak, (double) std::abs (l[(std::size_t) i]));
        }
    }
    CHECK (peak > 0.05);
    CHECK (peak <= 1.0);
    CHECK (player.positionBeats() < seq.lengthBeats); // wrapped around

    // Stopping releases everything
    p.playing = false;
    for (int b = 0; b < 400; ++b)
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        player.process (l.data(), r.data(), 512, p);
    }
    CHECK (std::abs (l[511]) < 1e-3);
}

TEST_CASE ("Engine progression editing")
{
    AnalysisEngine engine;
    engine.insertChord (0, chord (2, "m7"));
    engine.insertChord (1, chord (0, "maj7"));
    engine.insertChord (1, chord (7, "7"), 2);

    auto chart = engine.chart();
    REQUIRE (chart.size() == 3);
    CHECK (chart[1].chord.root == 7);
    CHECK (chart[1].beats == 2);
    CHECK (engine.snapshot().history.size() == 3);
    CHECK (engine.snapshot().key == Key { 0, Mode::major });

    engine.removeChord (0);
    CHECK (engine.chart().size() == 2);

    // Locked: playing does not append
    engine.setRecording (false);
    double t = 0.0;
    for (int i = 0; i < 10; ++i, t += 0.1)
        engine.update (testing::notes ({ 50, 65, 69, 72 }), t);
    CHECK (engine.chart().size() == 2);
    CHECK (engine.snapshot().chordName == "Dm7");
}
