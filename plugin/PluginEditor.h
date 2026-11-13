#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/BackingPanel.h"
#include "ui/ChordPicker.h"
#include "ui/Controls.h"
#include "ui/KeyboardView.h"
#include "ui/LeadSheetView.h"
#include "ui/Panels.h"
#include "ui/ProgressionStrip.h"

class ChordsearchEditor final : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit ChordsearchEditor (ChordsearchProcessor&);
    ~ChordsearchEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Pulls the latest analysis into the panels (called by the timer). */
    void refreshFromProcessor();

private:
    void timerCallback() override { refreshFromProcessor(); }
    void syncBackingPanel();
    void updateFromSnapshot();
    void openChordPicker (std::size_t index, juce::Rectangle<int> anchorInStrip);
    int chordIndexAt (double beat) const;

    ChordsearchProcessor& chordProcessor;
    ChordsearchLookAndFeel lookAndFeel;

    ChordPanel chordPanel;
    KeyPanel keyPanel;
    ScalePanel scalePanel;
    NextPanel nextPanel;
    ProgressionStrip strip;
    BackingPanel backingPanel;
    LeadSheetView leadSheet;
    KeyboardView keyboard;
    FlatButton resetButton { "RESET" };

    std::uint64_t lastVersion = ~std::uint64_t (0);
    bool sustainOn = false;
    std::vector<chordsearch::HistoryEntry> history;

    juce::Rectangle<int> headerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordsearchEditor)
};
