#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Controls.h"
#include "chordsearch/backing/Backing.h"

/** Transport, tempo, genre, metronome and instrument on/off for the backing band. */
class BackingPanel final : public juce::Component
{
public:
    std::function<void (bool)> onPlay;
    std::function<void (double)> onTempo;
    std::function<void (chordsearch::backing::Genre)> onGenre;
    std::function<void (bool)> onMetronome;
    std::function<void (int, bool)> onInstrument;

    BackingPanel();

    struct State
    {
        bool confirmed = false;
        bool playing = false;
        double tempo = 120.0;
        chordsearch::backing::Genre genre = chordsearch::backing::Genre::swing;
        bool metronome = false;
        std::array<bool, 4> instruments { true, true, true, true };
    };

    void setState (const State& s);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    State state;
    TransportButton play;
    TempoControl tempo;
    juce::OwnedArray<FlatButton> genreButtons, instrumentButtons;
    FlatButton metronome { "METRONOME", 10.0f };
    int genreLabelY = 0, mixLabelY = 0, descY = 0;
};
