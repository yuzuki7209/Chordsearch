#include "BackingPanel.h"

using namespace chordsearch::backing;

BackingPanel::BackingPanel()
{
    addAndMakeVisible (play);
    play.onClick = [this] { if (onPlay) onPlay (play.getToggleState()); };

    addAndMakeVisible (tempo);
    tempo.onChange = [this] (double bpm) { if (onTempo) onTempo (bpm); };

    for (int g = 0; g < numGenres; ++g)
    {
        auto* b = genreButtons.add (new FlatButton (juce::String (genreName ((Genre) g)).toUpperCase(), 9.5f));
        b->setRadioGroupId (10);
        b->setClickingTogglesState (true);
        b->onClick = [this, g] { if (onGenre) onGenre ((Genre) g); };
        addAndMakeVisible (b);
    }

    const char* names[] = { "BASS", "DRUMS", "GUITAR", "PIANO" };
    for (int i = 0; i < 4; ++i)
    {
        auto* b = instrumentButtons.add (new FlatButton (names[i], 10.0f));
        b->setClickingTogglesState (true);
        b->setToggleState (true, juce::dontSendNotification);
        b->onClick = [this, i, b] { if (onInstrument) onInstrument (i, b->getToggleState()); };
        addAndMakeVisible (b);
    }

    metronome.setClickingTogglesState (true);
    metronome.setAccent (Theme::warm);
    metronome.onClick = [this] { if (onMetronome) onMetronome (metronome.getToggleState()); };
    addAndMakeVisible (metronome);
}

void BackingPanel::setState (const State& s)
{
    state = s;
    play.setEnabled (s.confirmed);
    play.setToggleState (s.playing, juce::dontSendNotification);
    tempo.setTempo (s.tempo, false);
    genreButtons[(int) s.genre]->setToggleState (true, juce::dontSendNotification);
    metronome.setToggleState (s.metronome, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i)
        instrumentButtons[i]->setToggleState (s.instruments[(std::size_t) i], juce::dontSendNotification);
    repaint();
}

void BackingPanel::paint (juce::Graphics& g)
{
    Theme::drawPanel (g, getLocalBounds().toFloat(), "Backing");

    auto r = getLocalBounds().reduced (Theme::pad, 0);
    if (! state.confirmed)
    {
        g.setColour (Theme::warm);
        g.setFont (Theme::label (9.0f));
        g.drawText (Theme::utf8 ("진행을 CONFIRM 하면 재생할 수 있습니다"), r.withHeight (34), juce::Justification::centredRight, false);
    }

    g.setColour (Theme::textFaint);
    g.setFont (Theme::label (9.0f));
    g.drawText ("GENRE", r.getX(), genreLabelY, r.getWidth(), 14, juce::Justification::centredLeft, false);
    g.drawText ("BAND", r.getX(), mixLabelY, r.getWidth(), 14, juce::Justification::centredLeft, false);

    g.setColour (Theme::textDim);
    g.setFont (Theme::body (11.0f));
    g.drawText (Theme::utf8 (genreDescription (state.genre)), r.getX(), descY, r.getWidth(), 16,
                juce::Justification::centredLeft, true);
}

void BackingPanel::resized()
{
    auto r = getLocalBounds().reduced (Theme::pad, 0).withTrimmedTop (38).withTrimmedBottom (Theme::pad - 4);

    auto row = r.removeFromTop (40);
    play.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (8);
    tempo.setBounds (row);

    r.removeFromTop (10);
    genreLabelY = r.getY();
    r.removeFromTop (16);
    const int gw = (r.getWidth() - 2 * 4) / 3;
    for (int i = 0; i < genreButtons.size(); ++i)
        genreButtons[i]->setBounds (r.getX() + (i % 3) * (gw + 4), r.getY() + (i / 3) * 30, gw, 26);
    r.removeFromTop (60);
    descY = r.getY();
    r.removeFromTop (22);

    mixLabelY = r.getY();
    r.removeFromTop (16);
    auto mix = r.removeFromTop (28);
    metronome.setBounds (mix.removeFromRight (96));
    mix.removeFromRight (6);
    const int iw = (mix.getWidth() - 3 * 4) / 4;
    for (int i = 0; i < 4; ++i)
        instrumentButtons[i]->setBounds (mix.getX() + i * (iw + 4), mix.getY(), iw, mix.getHeight());
}
