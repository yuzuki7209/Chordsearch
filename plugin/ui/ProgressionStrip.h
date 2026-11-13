#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Controls.h"
#include "chordsearch/AnalysisEngine.h"

/** Editable chord progression.

    Hover a chord  -> an X appears in its top-right corner (delete).
    Hover a gap    -> a + appears between chords (insert).
    CONFIRM locks the progression and enables the backing band; EDIT unlocks it. */
class ProgressionStrip final : public juce::Component
{
public:
    std::function<void (std::size_t index)> onDelete;
    std::function<void (std::size_t index, juce::Rectangle<int> anchorInThis)> onInsert;
    std::function<void (bool confirm)> onConfirm;

    ProgressionStrip();

    void setSnapshot (const chordsearch::AnalysisSnapshot& s);
    void setPlayingIndex (int index);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    static constexpr int boxW = 84;
    static constexpr int gapW = 20;

    juce::Rectangle<int> lane() const;
    juce::Rectangle<int> boxRect (std::size_t i) const;
    juce::Rectangle<int> deleteRect (std::size_t i) const;
    juce::Rectangle<int> plusRect (std::size_t gap) const;
    int contentWidth() const;
    void clampScroll();
    void updateHover (juce::Point<int> p);

    std::vector<chordsearch::HistoryEntry> history;
    bool locked = false;
    int playingIndex = -1;

    int scroll = 0;
    int hoverBox = -1;
    int hoverGap = -1;
    bool hoverDelete = false;

    FlatButton confirmButton { "CONFIRM", 10.0f };
};
