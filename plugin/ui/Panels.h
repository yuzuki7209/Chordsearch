#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "chordsearch/AnalysisEngine.h"

/** Base for panels that render a piece of the analysis snapshot. */
class SnapshotPanel : public juce::Component
{
public:
    explicit SnapshotPanel (juce::String panelTitle) : title (std::move (panelTitle)) {}

    void setSnapshot (const chordsearch::AnalysisSnapshot& s)
    {
        snap = s;
        repaint();
    }

    void paint (juce::Graphics& g) override;

protected:
    /** Area below the panel title. */
    juce::Rectangle<int> content() const;
    virtual void paintContent (juce::Graphics& g, juce::Rectangle<int> area) = 0;

    chordsearch::AnalysisSnapshot snap;
    juce::String title;
};

class ChordPanel final : public SnapshotPanel
{
public:
    ChordPanel() : SnapshotPanel ("Chord") {}

    void setSoundingNotes (const chordsearch::MidiNoteSet& n);

private:
    void paintContent (juce::Graphics& g, juce::Rectangle<int> area) override;
    chordsearch::MidiNoteSet sounding;
};

class KeyPanel final : public SnapshotPanel
{
public:
    KeyPanel() : SnapshotPanel ("Key") {}

private:
    void paintContent (juce::Graphics& g, juce::Rectangle<int> area) override;
};

class ScalePanel final : public SnapshotPanel
{
public:
    ScalePanel() : SnapshotPanel ("Scales") {}

private:
    void paintContent (juce::Graphics& g, juce::Rectangle<int> area) override;
};

class NextPanel final : public SnapshotPanel
{
public:
    NextPanel() : SnapshotPanel ("Next Chord") {}

private:
    void paintContent (juce::Graphics& g, juce::Rectangle<int> area) override;
};
