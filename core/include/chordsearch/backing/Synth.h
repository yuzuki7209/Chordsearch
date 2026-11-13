#pragma once

#include "Backing.h"

#include <array>
#include <cstdint>
#include <vector>

namespace chordsearch::backing
{

/** Small sample-free synthesizer for the backing band.

    - Bass:   sine + band-limited saw through an enveloped 2-pole low-pass
              (upright-like for swing/bossa/ballad, electric otherwise)
    - Piano:  2-operator FM electric piano with a decaying "tine" partial
    - Guitar: Karplus–Strong plucked string (nylon / clean / overdriven, muted scratches)
    - Drums:  synthesized kick, snare, cross-stick, hats, ride, crash, toms
    - Click:  short sine blip

    All memory is allocated in prepare(); trigger()/render() are real-time safe. */
class BackingSynth
{
public:
    void prepare (double sampleRate);
    void setGenre (Genre g) noexcept { genre = g; }

    void trigger (const NoteEvent& e, double samplesPerBeat) noexcept;

    /** Adds the band into L/R. */
    void render (float* left, float* right, int numSamples) noexcept;

    void releaseAll() noexcept;
    void kill (Instrument inst) noexcept;

    int activeVoices() const noexcept;

private:
    enum class Kind : std::uint8_t
    {
        none, bass, piano, guitar, kick, snare, crossStick, hatClosed, hatPedal, hatOpen, ride, crash, tom, click
    };

    struct Voice
    {
        Kind kind = Kind::none;
        Instrument instrument = Instrument::piano;
        int note = 0;
        float velocity = 0.0f;
        std::uint64_t serial = 0;

        int t = 0;              // samples since start
        int gate = 0;           // samples until release
        bool released = false;
        float releaseGain = 1.0f;
        float releaseCoef = 0.999f;

        float gainL = 0.7f, gainR = 0.7f;

        double phase1 = 0.0, phase2 = 0.0, phase3 = 0.0;
        double freq = 0.0;
        std::array<double, 6> metal {};

        float lp1 = 0.0f, lp2 = 0.0f, hpX = 0.0f, hpY = 0.0f, hpX2 = 0.0f, hpY2 = 0.0f;
        std::uint32_t noise = 0x12345678u;

        // Karplus–Strong
        float* ks = nullptr;
        int ksLen = 0;
        int ksPos = 0;
        float ksFrac = 0.0f;
        float ksPrev = 0.0f;
        float ksLoss = 0.996f;
        float ksWeight = 0.5f;
        bool overdrive = false;
        bool muted = false;

        float tomFreq = 100.0f;
    };

    float renderVoice (Voice& v) noexcept;
    Voice& allocate (Instrument inst) noexcept;
    void setPan (Voice& v, float pan, float gain) noexcept;

    static float whiteNoise (std::uint32_t& s) noexcept
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (float) (int) s * (1.0f / 2147483648.0f);
    }

    double sr = 48000.0;
    Genre genre = Genre::swing;
    std::array<Voice, 64> voices;
    std::vector<float> ksMemory;
    int ksStride = 0;
    std::uint64_t serialCounter = 0;
};

/** Plays a Sequence in a loop through a BackingSynth (audio thread). */
class BackingPlayer
{
public:
    struct Params
    {
        bool playing = false;
        double bpm = 120.0;
        bool metronome = false;
        std::array<bool, 4> enabled { true, true, true, true }; // bass, drums, guitar, piano
        float masterGain = 0.8f;
    };

    void prepare (double sampleRate);

    /** Call when the active sequence pointer may have changed (cheap if unchanged). */
    void setSequence (const Sequence* s) noexcept;

    /** Renders into (cleared or not) stereo buffers, adding the band. */
    void process (float* left, float* right, int numSamples, const Params& params) noexcept;

    double positionBeats() const noexcept { return pos; }
    bool isRunning() const noexcept { return wasPlaying; }

private:
    void triggerDue (const Params& params, double samplesPerBeat) noexcept;

    BackingSynth synth;
    const Sequence* seq = nullptr;
    double sr = 48000.0;
    double pos = 0.0;
    std::size_t cursor = 0;
    bool wasPlaying = false;
    std::array<bool, 4> lastEnabled { true, true, true, true };
};

} // namespace chordsearch::backing
