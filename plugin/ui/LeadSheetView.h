#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "chordsearch/AnalysisEngine.h"

/** Real Book–style chord chart: ink on paper, four bars per line, rhythm slashes,
    simile marks for repeated bars, repeat barlines, and a moving playhead. */
class LeadSheetView final : public juce::Component
{
public:
    struct Header
    {
        juce::String key;
        juce::String style;
        double tempo = 120.0;
        bool confirmed = false;
    };

    void setChart (const std::vector<chordsearch::HistoryEntry>& history, const Header& header);

    /** Beat position within one pass of the chart (or < 0 when stopped). */
    void setPlayhead (double beat);

    void paint (juce::Graphics&) override;

private:
    struct BarChord
    {
        double offset; // beats within bar
        juce::String name;
    };

    struct Bar
    {
        std::vector<BarChord> chords;
        bool simile = false;
    };

    void rebuild();
    juce::Font chartFont (float size) const;

    std::vector<chordsearch::HistoryEntry> entries;
    std::vector<Bar> bars;
    Header header;
    double playhead = -1.0;
    int firstVisibleRow = 0;
};
