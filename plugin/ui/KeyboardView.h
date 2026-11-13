#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "chordsearch/MidiNoteSet.h"

/** Read-only piano display.
    Held notes: accent. Sustained-only notes: dim accent.
    Scale tones of the recommended scale: small square markers. */
class KeyboardView final : public juce::Component
{
public:
    KeyboardView (int lowestNote = 21, int highestNote = 108);

    void setNotes (chordsearch::MidiNoteSet held, chordsearch::MidiNoteSet sounding);
    void setScale (chordsearch::PitchClassSet scaleTones, int scaleRoot);

    void paint (juce::Graphics&) override;

private:
    static bool isBlack (int note) noexcept;
    juce::Colour fillFor (int note, juce::Colour base) const;
    void drawMarker (juce::Graphics& g, int note, juce::Rectangle<float> key) const;

    int lowNote, highNote;
    chordsearch::MidiNoteSet held, sounding;
    chordsearch::PitchClassSet scale;
    int root = -1;
};
