#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "chordsearch/MidiNoteSet.h"

#include <atomic>

/** Tracks which notes are held / sounding from incoming MIDI.

    process() and reset() run on the audio thread (no allocation, no locks).
    getHeld() / getSounding() / isSustainOn() can be called from any thread.
*/
class MidiInputTracker
{
public:
    void process (const juce::MidiBuffer& midi) noexcept;
    void reset() noexcept;

    /** Keys physically pressed. */
    chordsearch::MidiNoteSet getHeld() const noexcept;

    /** Keys pressed plus keys released while the sustain pedal is down. */
    chordsearch::MidiNoteSet getSounding() const noexcept;

    bool isSustainOn() const noexcept { return publishedSustain.load (std::memory_order_relaxed); }

private:
    void handle (const juce::MidiMessage& m) noexcept;
    void publish() noexcept;

    // Audio-thread state
    chordsearch::MidiNoteSet held;
    chordsearch::MidiNoteSet sustained; // released while pedal down
    bool sustainOn = false;

    // Published snapshot
    std::atomic<std::uint64_t> heldLo { 0 }, heldHi { 0 };
    std::atomic<std::uint64_t> soundLo { 0 }, soundHi { 0 };
    std::atomic<bool> publishedSustain { false };
};
