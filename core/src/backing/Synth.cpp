#include "chordsearch/backing/Synth.h"

#include <algorithm>
#include <cmath>

namespace chordsearch::backing
{

namespace
{
    constexpr double twoPi = 6.283185307179586;

    double mtof (int note) { return 440.0 * std::pow (2.0, (note - 69) / 12.0); }

    float onePoleCoef (double cutoff, double sr)
    {
        return (float) (1.0 - std::exp (-twoPi * std::min (cutoff, sr * 0.45) / sr));
    }

    float hpCoef (double cutoff, double sr)
    {
        const double rc = 1.0 / (twoPi * cutoff);
        const double dt = 1.0 / sr;
        return (float) (rc / (rc + dt));
    }

    /** PolyBLEP saw. */
    float polySaw (double phase, double inc)
    {
        float v = (float) (2.0 * phase - 1.0);
        if (phase < inc)
        {
            const double t = phase / inc;
            v -= (float) (t + t - t * t - 1.0);
        }
        else if (phase > 1.0 - inc)
        {
            const double t = (phase - 1.0) / inc;
            v -= (float) (t * t + t + t + 1.0);
        }
        return v;
    }

    // 808-style metallic square partials
    constexpr std::array<double, 6> metalFreqs { 263.0, 400.0, 421.0, 474.0, 587.0, 845.0 };

    bool uprightBass (Genre g) { return g == Genre::swing || g == Genre::bossa || g == Genre::ballad; }
} // namespace

// ============================================================================
void BackingSynth::prepare (double sampleRate)
{
    sr = sampleRate;
    ksStride = (int) (sr / 30.0) + 4;
    ksMemory.assign ((std::size_t) ksStride * voices.size(), 0.0f);
    for (std::size_t i = 0; i < voices.size(); ++i)
    {
        voices[i] = Voice {};
        voices[i].ks = ksMemory.data() + i * (std::size_t) ksStride;
        voices[i].noise = 0x9e3779b9u + (std::uint32_t) i * 7919u;
    }
}

int BackingSynth::activeVoices() const noexcept
{
    return (int) std::count_if (voices.begin(), voices.end(), [] (const Voice& v) { return v.kind != Kind::none; });
}

void BackingSynth::setPan (Voice& v, float pan, float gain) noexcept
{
    const float a = (pan + 1.0f) * 0.25f * 3.14159265f;
    v.gainL = std::cos (a) * gain;
    v.gainR = std::sin (a) * gain;
}

BackingSynth::Voice& BackingSynth::allocate (Instrument inst) noexcept
{
    for (auto& v : voices)
        if (v.kind == Kind::none)
            return v;

    // Steal: oldest voice of the same instrument, else the oldest overall
    Voice* oldest = nullptr;
    for (auto& v : voices)
        if (v.instrument == inst && (oldest == nullptr || v.serial < oldest->serial))
            oldest = &v;
    if (oldest == nullptr)
        for (auto& v : voices)
            if (oldest == nullptr || v.serial < oldest->serial)
                oldest = &v;
    return *oldest;
}

void BackingSynth::releaseAll() noexcept
{
    for (auto& v : voices)
        if (v.kind != Kind::none && ! v.released)
            v.gate = std::min (v.gate, v.t);
}

void BackingSynth::kill (Instrument inst) noexcept
{
    for (auto& v : voices)
    {
        if (v.kind == Kind::none || v.instrument != inst)
            continue;
        v.gate = std::min (v.gate, v.t);
        v.releaseCoef = (float) std::exp (-1.0 / (0.01 * sr));
    }
}

void BackingSynth::trigger (const NoteEvent& e, double samplesPerBeat) noexcept
{
    // Monophonic bass; open hi-hat choked by closed/pedal hats
    if (e.instrument == Instrument::bass)
        for (auto& v : voices)
            if (v.kind == Kind::bass && ! v.released)
            {
                v.gate = std::min (v.gate, v.t);
                v.releaseCoef = (float) std::exp (-1.0 / (0.012 * sr));
            }

    if (e.instrument == Instrument::drums && (e.note == Drum::hatClosed || e.note == Drum::hatPedal))
        for (auto& v : voices)
            if (v.kind == Kind::hatOpen)
            {
                v.gate = std::min (v.gate, v.t);
                v.releaseCoef = (float) std::exp (-1.0 / (0.01 * sr));
            }

    Voice& v = allocate (e.instrument);
    auto* ks = v.ks;
    const auto noiseSeed = v.noise;
    v = Voice {};
    v.ks = ks;
    v.noise = noiseSeed;

    v.instrument = e.instrument;
    v.note = e.note;
    v.velocity = e.velocity;
    v.serial = ++serialCounter;
    v.gate = std::max (1, (int) (e.length * samplesPerBeat));
    v.releaseCoef = (float) std::exp (-1.0 / (0.08 * sr));
    v.freq = mtof (e.note);

    const float vel = e.velocity;

    switch (e.instrument)
    {
        case Instrument::bass:
            v.kind = Kind::bass;
            v.releaseCoef = (float) std::exp (-1.0 / (0.05 * sr));
            setPan (v, 0.0f, 0.62f * vel);
            break;

        case Instrument::piano:
            v.kind = Kind::piano;
            v.releaseCoef = (float) std::exp (-1.0 / (0.18 * sr));
            setPan (v, -0.3f + (float) (e.note - 60) * 0.01f, 0.2f * vel);
            break;

        case Instrument::guitar:
        {
            v.kind = Kind::guitar;
            v.muted = (e.flags & NoteFlag::muted) != 0;
            v.overdrive = genre == Genre::rock && ! v.muted;

            const double period = sr / v.freq;
            v.ksLen = std::clamp ((int) period, 2, ksStride - 2);
            v.ksFrac = (float) (period - (double) v.ksLen);
            v.ksPos = 0;

            // Excitation: filtered noise burst (brightness from velocity / genre)
            float brightness = 0.35f + 0.55f * vel;
            if (genre == Genre::bossa) brightness *= 0.55f; // nylon
            if (v.muted) brightness = 0.25f;
            float lp = 0.0f;
            for (int i = 0; i <= v.ksLen; ++i)
            {
                lp += brightness * (whiteNoise (v.noise) - lp);
                v.ks[i] = lp;
            }

            v.ksWeight = genre == Genre::bossa ? 0.42f : 0.5f;
            v.ksLoss = v.muted ? 0.6f : (genre == Genre::rock ? 0.994f : 0.9965f);
            v.releaseCoef = (float) std::exp (-1.0 / ((v.muted ? 0.01 : 0.06) * sr));
            setPan (v, 0.35f, (v.overdrive ? 0.42f : (v.muted ? 0.55f : 0.7f)) * vel);
            break;
        }

        case Instrument::click:
            v.kind = Kind::click;
            v.freq = e.note == 1 ? 2200.0 : 1600.0;
            setPan (v, 0.0f, 0.32f * vel);
            break;

        case Instrument::drums:
        {
            switch (e.note)
            {
                case Drum::kick:       v.kind = Kind::kick;       setPan (v, 0.0f, 0.95f * vel); break;
                case Drum::snare:      v.kind = Kind::snare;      setPan (v, 0.05f, 0.55f * vel); break;
                case Drum::crossStick: v.kind = Kind::crossStick; setPan (v, 0.05f, 0.45f * vel); break;
                case Drum::hatClosed:  v.kind = Kind::hatClosed;  setPan (v, 0.3f, 0.32f * vel); break;
                case Drum::hatPedal:   v.kind = Kind::hatPedal;   setPan (v, 0.3f, 0.22f * vel); break;
                case Drum::hatOpen:    v.kind = Kind::hatOpen;    setPan (v, 0.3f, 0.26f * vel); v.gate = (int) (sr * 2.0); break;
                case Drum::ride:       v.kind = Kind::ride;       setPan (v, -0.35f, 0.22f * vel); v.gate = (int) (sr * 3.0); break;
                case Drum::crash:      v.kind = Kind::crash;      setPan (v, -0.25f, 0.3f * vel); v.gate = (int) (sr * 3.0); break;
                case Drum::tomHigh:    v.kind = Kind::tom; v.tomFreq = 210.0f; setPan (v, 0.2f, 0.6f * vel); break;
                case Drum::tomMid:     v.kind = Kind::tom; v.tomFreq = 155.0f; setPan (v, 0.0f, 0.6f * vel); break;
                case Drum::tomLow:     v.kind = Kind::tom; v.tomFreq = 110.0f; setPan (v, -0.2f, 0.65f * vel); break;
                default:               v.kind = Kind::none; return;
            }
            v.gate = std::max (v.gate, (int) (sr * 1.0)); // drums ring out naturally
            break;
        }
    }
}

float BackingSynth::renderVoice (Voice& v) noexcept
{
    const double t = (double) v.t / sr;
    float out = 0.0f;
    float amp = 1.0f;

    switch (v.kind)
    {
        case Kind::bass:
        {
            const bool upright = uprightBass (genre);
            const double inc = v.freq / sr;
            v.phase1 += inc;
            if (v.phase1 >= 1.0) v.phase1 -= 1.0;

            const float sine = (float) std::sin (twoPi * v.phase1);
            const float saw = polySaw (v.phase1, inc);
            const float raw = upright ? 0.85f * sine + 0.22f * saw : 0.6f * sine + 0.45f * saw;

            const double cutoff = (upright ? 260.0 : 420.0) + (upright ? 900.0 : 1600.0) * v.velocity * std::exp (-t / 0.09);
            const float a = onePoleCoef (cutoff, sr);
            v.lp1 += a * (raw - v.lp1);
            v.lp2 += a * (v.lp1 - v.lp2);

            const float attack = (float) std::min (1.0, t / 0.004);
            const float sustain = upright ? 0.32f : 0.62f;
            const float decayT = upright ? 0.28f : 0.45f;
            amp = attack * (sustain + (1.0f - sustain) * (float) std::exp (-t / decayT));
            out = v.lp2 * 1.6f;
            break;
        }

        case Kind::piano:
        {
            const double inc = v.freq / sr;
            v.phase1 += inc;          // carrier
            v.phase2 += inc;          // modulator 1:1
            v.phase3 += inc * 14.0;   // tine
            v.phase1 -= std::floor (v.phase1);
            v.phase2 -= std::floor (v.phase2);
            v.phase3 -= std::floor (v.phase3);

            const double index = v.velocity * (2.0 * std::exp (-t / 0.35) + 0.25);
            const double tine = 0.9 * v.velocity * std::exp (-t / 0.018);
            out = (float) std::sin (twoPi * v.phase1 + index * std::sin (twoPi * v.phase2) + tine * std::sin (twoPi * v.phase3));

            const double decay = std::clamp (2.6 - (v.note - 48) * 0.035, 0.6, 2.8);
            amp = (float) (std::min (1.0, t / 0.002) * std::exp (-t / decay));
            break;
        }

        case Kind::guitar:
        {
            // Read with fractional delay (linear interpolation)
            const int len = v.ksLen;
            const int i0 = v.ksPos;
            const int i1 = (i0 + 1) % (len + 1);
            const float cur = v.ks[i0] + v.ksFrac * (v.ks[i1] - v.ks[i0]);
            const float filtered = v.ksLoss * (v.ksWeight * cur + (1.0f - v.ksWeight) * v.ksPrev);
            v.ksPrev = cur;
            v.ks[i0] = filtered;
            v.ksPos = i1;

            out = cur;
            if (v.overdrive)
            {
                out = std::tanh (out * 4.0f) * 0.6f;
                const float a = onePoleCoef (2800.0, sr);
                v.lp1 += a * (out - v.lp1);
                out = v.lp1;
            }
            amp = (float) std::min (1.0, t / 0.001);
            break;
        }

        case Kind::kick:
        {
            const double f = 50.0 + 110.0 * std::exp (-t / 0.03);
            v.phase1 += f / sr;
            out = (float) std::sin (twoPi * v.phase1);
            if (t < 0.003)
                out += 0.4f * whiteNoise (v.noise);
            amp = (float) std::exp (-t / 0.28);
            break;
        }

        case Kind::snare:
        {
            v.phase1 += 185.0 / sr;
            const float tone = 0.45f * (float) std::sin (twoPi * v.phase1) * (float) std::exp (-t / 0.07);
            const float n = whiteNoise (v.noise);
            const float a = hpCoef (1200.0, sr);
            v.hpY = a * (v.hpY + n - v.hpX);
            v.hpX = n;
            out = tone + 0.8f * v.hpY * (float) std::exp (-t / 0.15);
            amp = 1.0f;
            break;
        }

        case Kind::crossStick:
        {
            v.phase1 += 420.0 / sr;
            v.phase2 += 1050.0 / sr;
            out = (float) (0.6 * std::sin (twoPi * v.phase1) + 0.3 * std::sin (twoPi * v.phase2)) * (float) std::exp (-t / 0.03)
                + 0.5f * whiteNoise (v.noise) * (float) std::exp (-t / 0.006);
            amp = 1.0f;
            break;
        }

        case Kind::hatClosed:
        case Kind::hatPedal:
        case Kind::hatOpen:
        case Kind::ride:
        case Kind::crash:
        {
            const double scale = v.kind == Kind::ride ? 1.25 : (v.kind == Kind::crash ? 1.1 : 1.0);
            float metal = 0.0f;
            for (std::size_t k = 0; k < metalFreqs.size(); ++k)
            {
                v.metal[k] += metalFreqs[k] * scale * 2.0 / sr;
                v.metal[k] -= std::floor (v.metal[k]);
                metal += v.metal[k] < 0.5 ? 1.0f : -1.0f;
            }
            metal *= 0.16f;
            const float n = whiteNoise (v.noise);
            const float src = v.kind == Kind::crash ? 0.5f * metal + 0.7f * n : (v.kind == Kind::ride ? 0.8f * metal + 0.25f * n : 0.6f * metal + 0.5f * n);

            const float a = hpCoef (v.kind == Kind::ride ? 3500.0 : 6500.0, sr);
            v.hpY = a * (v.hpY + src - v.hpX);
            v.hpX = src;
            v.hpY2 = a * (v.hpY2 + v.hpY - v.hpX2);
            v.hpX2 = v.hpY;
            out = v.hpY2;

            double decay = 0.045;
            if (v.kind == Kind::hatPedal) decay = 0.03;
            if (v.kind == Kind::hatOpen)  decay = 0.35;
            if (v.kind == Kind::ride)     decay = 0.9;
            if (v.kind == Kind::crash)    decay = 1.4;
            amp = (float) std::exp (-t / decay);
            if (v.kind == Kind::ride)
                amp = 0.6f * amp + 0.4f * (float) std::exp (-t / 0.05); // stick "ping"
            break;
        }

        case Kind::tom:
        {
            const double f = v.tomFreq * (1.0 + 0.35 * std::exp (-t / 0.06));
            v.phase1 += f / sr;
            out = (float) std::sin (twoPi * v.phase1);
            if (t < 0.004)
                out += 0.3f * whiteNoise (v.noise);
            amp = (float) std::exp (-t / 0.3);
            break;
        }

        case Kind::click:
        {
            v.phase1 += v.freq / sr;
            out = (float) std::sin (twoPi * v.phase1);
            amp = (float) std::exp (-t / 0.015);
            break;
        }

        case Kind::none:
            return 0.0f;
    }

    if (v.t >= v.gate)
    {
        if (! v.released)
        {
            v.released = true;
            if (v.kind == Kind::guitar)
                v.ksLoss = std::min (v.ksLoss, 0.9f);
        }
        v.releaseGain *= v.releaseCoef;
    }

    ++v.t;
    const float level = amp * v.releaseGain;

    // Voice end: silent after release, or drum envelope finished
    if ((v.released && v.releaseGain < 1.0e-4f) || (v.t > (int) (sr * 0.25) && level < 1.0e-5f && v.kind != Kind::guitar)
        || v.t > (int) (sr * 8.0))
        v.kind = Kind::none;

    return out * level;
}

void BackingSynth::render (float* left, float* right, int numSamples) noexcept
{
    for (auto& v : voices)
    {
        if (v.kind == Kind::none)
            continue;
        for (int i = 0; i < numSamples && v.kind != Kind::none; ++i)
        {
            const float s = renderVoice (v);
            left[i] += s * v.gainL;
            right[i] += s * v.gainR;
        }
    }
}

// ============================================================================
void BackingPlayer::prepare (double sampleRate)
{
    sr = sampleRate;
    synth.prepare (sampleRate);
    pos = 0.0;
    cursor = 0;
    wasPlaying = false;
}

void BackingPlayer::setSequence (const Sequence* s) noexcept
{
    if (s == seq)
        return;

    seq = s;
    if (seq == nullptr || seq->lengthBeats <= 0.0)
    {
        cursor = 0;
        return;
    }

    synth.setGenre (seq->genre);
    pos = std::fmod (pos, seq->lengthBeats);
    const auto& ev = seq->events;
    cursor = (std::size_t) (std::lower_bound (ev.begin(), ev.end(), pos,
                                              [] (const NoteEvent& e, double p) { return e.start < p; }) - ev.begin());
}

void BackingPlayer::triggerDue (const Params& params, double samplesPerBeat) noexcept
{
    const auto& ev = seq->events;
    while (cursor < ev.size() && ev[cursor].start <= pos + 1e-9)
    {
        const auto& e = ev[cursor++];
        if (e.instrument == Instrument::click)
        {
            if (params.metronome)
                synth.trigger (e, samplesPerBeat);
        }
        else if (params.enabled[(std::size_t) e.instrument])
        {
            synth.trigger (e, samplesPerBeat);
        }
    }
}

void BackingPlayer::process (float* left, float* right, int numSamples, const Params& params) noexcept
{
    for (std::size_t i = 0; i < 4; ++i)
    {
        if (lastEnabled[i] && ! params.enabled[i])
            synth.kill ((Instrument) i);
        lastEnabled[i] = params.enabled[i];
    }

    const bool canPlay = params.playing && seq != nullptr && seq->lengthBeats > 0.0;

    if (! canPlay)
    {
        if (wasPlaying)
        {
            synth.releaseAll();
            wasPlaying = false;
        }
        pos = 0.0;
        cursor = 0;
    }
    else
    {
        if (! wasPlaying)
        {
            pos = 0.0;
            cursor = 0;
            wasPlaying = true;
        }

        const double spb = sr * 60.0 / std::clamp (params.bpm, 20.0, 400.0);
        int done = 0;
        int guard = 0;

        while (done < numSamples && ++guard < 100000)
        {
            if (cursor >= seq->events.size() && pos >= seq->lengthBeats - 1e-9)
            {
                pos -= seq->lengthBeats;
                if (pos < 0.0) pos = 0.0;
                cursor = 0;
            }

            triggerDue (params, spb);

            const double nextBeat = cursor < seq->events.size() ? seq->events[cursor].start : seq->lengthBeats;
            const int remaining = numSamples - done;
            const int untilNext = (int) std::floor ((nextBeat - pos) * spb);

            if (untilNext >= remaining)
            {
                synth.render (left + done, right + done, remaining);
                pos += remaining / spb;
                done = numSamples;
            }
            else
            {
                const int n = std::max (0, untilNext);
                if (n > 0)
                    synth.render (left + done, right + done, n);
                done += n;
                pos = std::max (pos + n / spb, nextBeat);
            }
        }
    }

    if (! canPlay)
        synth.render (left, right, numSamples); // let tails ring out

    for (int i = 0; i < numSamples; ++i)
    {
        left[i] = std::tanh (left[i] * params.masterGain);
        right[i] = std::tanh (right[i] * params.masterGain);
    }
}

} // namespace chordsearch::backing
