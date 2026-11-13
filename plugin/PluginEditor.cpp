#include "PluginEditor.h"
#include "ui/Theme.h"

using namespace chordsearch;

ChordsearchEditor::ChordsearchEditor (ChordsearchProcessor& p)
    : AudioProcessorEditor (p), chordProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* c : std::initializer_list<juce::Component*> { &chordPanel, &keyPanel, &scalePanel, &nextPanel, &strip,
                                                             &backingPanel, &leadSheet, &keyboard, &resetButton })
        addAndMakeVisible (c);

    resetButton.onClick = [this]
    {
        chordProcessor.resetAnalysis();
        lastVersion = ~std::uint64_t (0);
    };

    // Progression editing
    strip.onDelete = [this] (std::size_t i) { chordProcessor.removeChord (i); };
    strip.onInsert = [this] (std::size_t i, juce::Rectangle<int> anchor) { openChordPicker (i, anchor); };
    strip.onConfirm = [this] (bool confirm)
    {
        chordProcessor.confirmProgression (confirm);
        syncBackingPanel();
    };

    // Backing band
    backingPanel.onPlay = [this] (bool play)
    {
        chordProcessor.setPlaying (play);
        syncBackingPanel();
    };
    backingPanel.onTempo = [this] (double bpm)
    {
        chordProcessor.setTempo (bpm);
        lastVersion = ~std::uint64_t (0);
    };
    backingPanel.onGenre = [this] (backing::Genre g)
    {
        chordProcessor.setGenre (g);
        syncBackingPanel();
        lastVersion = ~std::uint64_t (0);
    };
    backingPanel.onMetronome = [this] (bool on) { chordProcessor.setMetronome (on); };
    backingPanel.onInstrument = [this] (int i, bool on) { chordProcessor.setInstrumentEnabled (i, on); };

    setResizable (true, true);
    setResizeLimits (1240, 720, 2200, 1400);
    setSize (1400, 790);

    syncBackingPanel();
    startTimerHz (30);
}

ChordsearchEditor::~ChordsearchEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ChordsearchEditor::syncBackingPanel()
{
    BackingPanel::State s;
    s.confirmed = chordProcessor.isConfirmed();
    s.playing = chordProcessor.isPlaying();
    s.tempo = chordProcessor.getTempo();
    s.genre = chordProcessor.getGenre();
    s.metronome = chordProcessor.getMetronome();
    for (int i = 0; i < 4; ++i)
        s.instruments[(std::size_t) i] = chordProcessor.isInstrumentEnabled (i);
    backingPanel.setState (s);
}

void ChordsearchEditor::openChordPicker (std::size_t index, juce::Rectangle<int> anchorInStrip)
{
    const auto& engine = chordProcessor.getEngine();
    const auto& snap = engine.snapshot();

    std::vector<Suggestion> suggestions;
    if (snap.hasKey)
    {
        const auto context = engine.historyBefore (index);
        if (! context.empty())
            suggestions = ChordSuggester::suggest (context, snap.key, 6);
    }

    auto picker = std::make_unique<ChordPicker> (std::move (suggestions),
                                                 snap.hasKey ? snap.key.accidentals() : Accidentals::flats);
    picker->onPick = [this, index] (const Chord& c, int beats) { chordProcessor.insertChord (index, c, beats); };

    juce::CallOutBox::launchAsynchronously (std::move (picker), getLocalArea (&strip, anchorInStrip), this);
}

int ChordsearchEditor::chordIndexAt (double beat) const
{
    if (beat < 0.0)
        return -1;
    double t = 0.0;
    for (std::size_t i = 0; i < history.size(); ++i)
    {
        t += history[i].beats;
        if (beat < t)
            return (int) i;
    }
    return history.empty() ? -1 : (int) history.size() - 1;
}

void ChordsearchEditor::refreshFromProcessor()
{
    const auto& tracker = chordProcessor.getMidiTracker();
    const auto sounding = tracker.getSounding();
    keyboard.setNotes (tracker.getHeld(), sounding);
    chordPanel.setSoundingNotes (sounding);

    if (tracker.isSustainOn() != sustainOn)
    {
        sustainOn = tracker.isSustainOn();
        repaint (headerArea);
    }

    const auto& engine = chordProcessor.getEngine();
    const bool changed = engine.version() != lastVersion;
    if (changed)
        updateFromSnapshot();

    // Playhead (after the progression is up to date)
    const double beat = chordProcessor.getChartPlayhead();
    leadSheet.setPlayhead (beat);
    strip.setPlayingIndex (chordIndexAt (beat));
}

void ChordsearchEditor::updateFromSnapshot()
{
    const auto& engine = chordProcessor.getEngine();

    lastVersion = engine.version();
    const auto& snap = engine.snapshot();
    history = snap.history;

    chordPanel.setSnapshot (snap);
    keyPanel.setSnapshot (snap);
    scalePanel.setSnapshot (snap);
    nextPanel.setSnapshot (snap);
    strip.setSnapshot (snap);

    LeadSheetView::Header header;
    header.key = snap.hasKey ? Theme::pretty (snap.key.name()) : juce::String();
    header.style = backing::genreName (chordProcessor.getGenre());
    header.tempo = chordProcessor.getTempo();
    header.confirmed = chordProcessor.isConfirmed();
    leadSheet.setChart (snap.history, header);

    if (! snap.scales.empty())
        keyboard.setScale (snap.scales.front().notes, snap.scales.front().root);
    else
        keyboard.setScale ({}, -1);

    syncBackingPanel();
}

void ChordsearchEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);

    auto r = headerArea.reduced (Theme::pad, 0);

    // Logo mark: two stacked squares
    auto mark = r.removeFromLeft (18).withSizeKeepingCentre (14, 14).toFloat();
    g.setColour (Theme::accent);
    g.fillRect (mark.removeFromLeft (6.0f));
    g.setColour (Theme::accentDim);
    g.fillRect (mark.withTrimmedLeft (2.0f));

    r.removeFromLeft (8);
    g.setColour (Theme::text);
    g.setFont (Theme::label (13.0f));
    g.drawText ("CHORDSEARCH", r, juce::Justification::centredLeft, false);

    // Sustain indicator: square LED + label (left of reset button)
    auto right = r.removeFromRight (190).withTrimmedRight (80);
    auto led = right.removeFromLeft (10).withSizeKeepingCentre (8, 8).toFloat();
    g.setColour (sustainOn ? Theme::warm : Theme::textFaint);
    g.fillRect (led);
    g.setColour (sustainOn ? Theme::text : Theme::textDim);
    g.setFont (Theme::label (10.0f));
    g.drawText ("SUSTAIN", right.withTrimmedLeft (8), juce::Justification::centredLeft, false);

    g.setColour (Theme::border);
    g.fillRect (headerArea.getX(), headerArea.getBottom() - 1, headerArea.getWidth(), 1);
}

void ChordsearchEditor::resized()
{
    auto r = getLocalBounds();

    headerArea = r.removeFromTop (46);
    resetButton.setBounds (headerArea.reduced (Theme::pad, 0).removeFromRight (64).withSizeKeepingCentre (64, 24));

    keyboard.setBounds (r.removeFromBottom (96));

    // Right sidebar: backing controls + chart
    auto side = r.removeFromRight (440).reduced (0, Theme::gap).withTrimmedRight (Theme::pad - 4);
    backingPanel.setBounds (side.removeFromTop (246));
    side.removeFromTop (Theme::gap);
    leadSheet.setBounds (side);

    // Left: analysis
    strip.setBounds (r.removeFromBottom (62));
    r = r.reduced (Theme::pad - 4, Theme::gap);

    auto top = r.removeFromTop (juce::jlimit (210, 260, r.getHeight() * 42 / 100));
    r.removeFromTop (Theme::gap);

    const int leftW = r.getWidth() * 56 / 100;
    keyPanel.setBounds (top.removeFromRight (top.getWidth() - leftW - Theme::gap));
    top.removeFromRight (Theme::gap);
    chordPanel.setBounds (top);

    scalePanel.setBounds (r.removeFromLeft (r.getWidth() * 46 / 100));
    r.removeFromLeft (Theme::gap);
    nextPanel.setBounds (r);
}
