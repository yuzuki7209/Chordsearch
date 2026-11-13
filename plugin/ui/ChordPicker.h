#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Controls.h"
#include "chordsearch/ChordSuggester.h"

/** Popup for inserting a chord: suggested chords (one click), or root + quality + length. */
class ChordPicker final : public juce::Component
{
public:
    std::function<void (const chordsearch::Chord&, int beats)> onPick;

    ChordPicker (std::vector<chordsearch::Suggestion> suggestions, chordsearch::Accidentals acc);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    chordsearch::Chord currentChord() const;
    void updatePreview();
    void pick (const chordsearch::Chord& c, int beats);

    std::vector<chordsearch::Suggestion> suggestions;
    chordsearch::Accidentals accidentals;

    juce::OwnedArray<FlatButton> suggestionButtons, rootButtons, qualityButtons, beatButtons;
    FlatButton addButton { "ADD", 11.0f };

    int root = 0;
    int quality = 0;
    int beats = 4;
};
