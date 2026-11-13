#include "ChordPicker.h"

using namespace chordsearch;

namespace
{
    // Preset suffix -> button label
    const std::vector<std::pair<const char*, const char*>> qualities {
        { "maj7", "maj7" }, { "m7", "m7" },     { "7", "7" },         { "m7b5", "m7b5" },   { "dim7", "dim7" },
        { "6", "6" },       { "m6", "m6" },     { "m(maj7)", "mMaj7" }, { "7sus4", "7sus4" }, { "7alt", "7alt" },
        { "maj9", "maj9" }, { "m9", "m9" },     { "9", "9" },         { "13", "13" },       { "7b9", "7b9" },
        { "", "Major" },    { "m", "minor" },   { "sus4", "sus4" },   { "+", "aug" },       { "dim", "dim" },
    };

    const int beatOptions[] = { 2, 4, 8 };
} // namespace

ChordPicker::ChordPicker (std::vector<Suggestion> s, Accidentals acc)
    : suggestions (std::move (s)), accidentals (acc)
{
    for (std::size_t i = 0; i < suggestions.size() && i < 6; ++i)
    {
        auto* b = suggestionButtons.add (new FlatButton (Theme::pretty (suggestions[i].chord.name (acc)), 12.0f));
        b->setAccent (Theme::warm);
        b->onClick = [this, i] { pick (suggestions[i].chord, 4); };
        addAndMakeVisible (b);
    }

    for (int pc = 0; pc < 12; ++pc)
    {
        auto* b = rootButtons.add (new FlatButton (Theme::pretty (pitchClassName (pc, acc)), 12.0f));
        b->setRadioGroupId (1);
        b->setClickingTogglesState (true);
        b->onClick = [this, pc] { root = pc; updatePreview(); };
        addAndMakeVisible (b);
    }

    for (std::size_t q = 0; q < qualities.size(); ++q)
    {
        auto* b = qualityButtons.add (new FlatButton (Theme::pretty (qualities[q].second), 10.0f));
        b->setRadioGroupId (2);
        b->setClickingTogglesState (true);
        b->onClick = [this, q] { quality = (int) q; updatePreview(); };
        addAndMakeVisible (b);
    }

    for (int bt : beatOptions)
    {
        auto* b = beatButtons.add (new FlatButton (bt == 2 ? "2 BEATS" : (bt == 4 ? "1 BAR" : "2 BARS"), 9.0f));
        b->setRadioGroupId (3);
        b->setClickingTogglesState (true);
        b->onClick = [this, bt] { beats = bt; };
        addAndMakeVisible (b);
    }

    // Defaults: first suggestion's root, maj7, one bar
    if (! suggestions.empty())
        root = suggestions.front().chord.root;
    rootButtons[root]->setToggleState (true, juce::dontSendNotification);
    qualityButtons[0]->setToggleState (true, juce::dontSendNotification);
    beatButtons[1]->setToggleState (true, juce::dontSendNotification);

    addButton.setAccent (Theme::accent);
    addButton.onClick = [this] { pick (currentChord(), beats); };
    addAndMakeVisible (addButton);

    setSize (440, suggestions.empty() ? 302 : 354);
    updatePreview();
}

Chord ChordPicker::currentChord() const
{
    if (auto c = Chord::fromSuffix (root, qualities[(std::size_t) quality].first))
        return *c;
    return *Chord::fromSuffix (root, "");
}

void ChordPicker::updatePreview()
{
    repaint();
}

void ChordPicker::pick (const Chord& c, int numBeats)
{
    if (onPick)
        onPick (c, numBeats);
    if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
        box->dismiss();
}

void ChordPicker::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();

    auto section = [&] (const char* title, int y)
    {
        g.setColour (Theme::textFaint);
        g.setFont (Theme::label (9.0f));
        g.drawText (Theme::utf8 (title), r.getX(), y, r.getWidth(), 14, juce::Justification::centredLeft, false);
    };

    if (! suggestionButtons.isEmpty())
        section ("추천 코드  (클릭하면 바로 추가)", 0);

    const int base = suggestionButtons.isEmpty() ? 0 : 52;
    section ("ROOT", base);
    section ("QUALITY", base + 82);
    section ("LENGTH", base + 222);

    // Preview of the chord being built
    g.setColour (Theme::text);
    g.setFont (Theme::display (24.0f));
    g.drawText (Theme::pretty (currentChord().name (accidentals)),
                juce::Rectangle<int> (r.getX(), base + 268, r.getWidth(), 32), juce::Justification::centredLeft, false);
}

void ChordPicker::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth();

    int y = 0;
    if (! suggestionButtons.isEmpty())
    {
        const int n = suggestionButtons.size();
        const int bw = (w - (n - 1) * 4) / n;
        for (int i = 0; i < n; ++i)
            suggestionButtons[i]->setBounds (i * (bw + 4), 16, bw, 28);
        y = 52;
    }

    // Roots: 2 rows of 6
    const int rw = (w - 5 * 4) / 6;
    for (int i = 0; i < 12; ++i)
        rootButtons[i]->setBounds ((i % 6) * (rw + 4), y + 16 + (i / 6) * 30, rw, 26);

    // Qualities: 4 rows of 5
    const int qy = y + 98;
    const int qw = (w - 4 * 4) / 5;
    for (int i = 0; i < qualityButtons.size(); ++i)
        qualityButtons[i]->setBounds ((i % 5) * (qw + 4), qy + (i / 5) * 30, qw, 26);

    // Length
    const int ly = y + 238;
    const int lw = (w - 100 - 3 * 4) / 3;
    for (int i = 0; i < beatButtons.size(); ++i)
        beatButtons[i]->setBounds (i * (lw + 4), ly, lw, 26);

    addButton.setBounds (w - 96, ly, 96, 26);
}
