#include "MidiInputTracker.h"

void MidiInputTracker::process (const juce::MidiBuffer& midi) noexcept
{
    if (midi.isEmpty())
        return;

    for (const auto metadata : midi)
        handle (metadata.getMessage());

    publish();
}

void MidiInputTracker::reset() noexcept
{
    held.clear();
    sustained.clear();
    sustainOn = false;
    publish();
}

void MidiInputTracker::handle (const juce::MidiMessage& m) noexcept
{
    if (m.isNoteOn())
    {
        held.set (m.getNoteNumber(), true);
        sustained.set (m.getNoteNumber(), false);
    }
    else if (m.isNoteOff())
    {
        held.set (m.getNoteNumber(), false);
        if (sustainOn)
            sustained.set (m.getNoteNumber(), true);
    }
    else if (m.isSustainPedalOn())
    {
        sustainOn = true;
    }
    else if (m.isSustainPedalOff())
    {
        sustainOn = false;
        sustained.clear();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        held.clear();
        sustained.clear();
    }
}

void MidiInputTracker::publish() noexcept
{
    const auto sounding = held | sustained;
    heldLo.store (held.lo, std::memory_order_relaxed);
    heldHi.store (held.hi, std::memory_order_relaxed);
    soundLo.store (sounding.lo, std::memory_order_relaxed);
    soundHi.store (sounding.hi, std::memory_order_relaxed);
    publishedSustain.store (sustainOn, std::memory_order_relaxed);
}

chordsearch::MidiNoteSet MidiInputTracker::getHeld() const noexcept
{
    return { heldLo.load (std::memory_order_relaxed), heldHi.load (std::memory_order_relaxed) };
}

chordsearch::MidiNoteSet MidiInputTracker::getSounding() const noexcept
{
    return { soundLo.load (std::memory_order_relaxed), soundHi.load (std::memory_order_relaxed) };
}
