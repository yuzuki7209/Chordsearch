#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

/** Sharp-cornered LookAndFeel overrides (call-out boxes etc.). */
class ChordsearchLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ChordsearchLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, Theme::panel);
        setColour (juce::TooltipWindow::backgroundColourId, Theme::panelRaise);
        setColour (juce::TooltipWindow::textColourId, Theme::text);
        setColour (juce::TooltipWindow::outlineColourId, Theme::border);
    }

    float getCallOutBoxCornerSize (const juce::CallOutBox&) override { return Theme::radius; }
    int getCallOutBoxBorderSize (const juce::CallOutBox&) override { return 14; }

    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&) override
    {
        g.setColour (Theme::panel);
        g.fillPath (path);
        g.setColour (Theme::border.brighter (0.3f));
        g.strokePath (path, juce::PathStrokeType (1.0f));
    }
};

/** Rectangular flat button; supports toggle state (accent tint when on). */
class FlatButton : public juce::Button
{
public:
    explicit FlatButton (const juce::String& text, float fontSize = 10.0f)
        : juce::Button (text), size (fontSize) {}

    void setAccent (juce::Colour c) { accent = c; repaint(); }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = getToggleState();
        const bool enabled = isEnabled();

        juce::Colour bg = Theme::panel;
        if (on)               bg = accent.withAlpha (0.16f);
        else if (down)        bg = Theme::panelRaise.brighter (0.1f);
        else if (highlighted) bg = Theme::panelRaise;

        g.setColour (bg);
        g.fillRoundedRectangle (r, Theme::radius);
        g.setColour (on ? accent : (highlighted && enabled ? Theme::textDim : Theme::border));
        g.drawRoundedRectangle (r, Theme::radius, 1.0f);

        g.setColour (! enabled ? Theme::textFaint : on ? accent : (highlighted ? Theme::text : Theme::textDim));
        g.setFont (Theme::label (size));
        g.drawText (getButtonText(), getLocalBounds().reduced (4, 0), juce::Justification::centred, true);
    }

private:
    float size;
    juce::Colour accent = Theme::accent;
};

/** Play / stop button with a drawn icon. */
class TransportButton final : public juce::Button
{
public:
    TransportButton() : juce::Button ("Play") { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool playing = getToggleState();
        const bool enabled = isEnabled();

        g.setColour (playing ? Theme::accent : (highlighted && enabled ? Theme::panelRaise.brighter (0.05f) : Theme::panelRaise));
        g.fillRoundedRectangle (r, Theme::radius);
        g.setColour (playing ? Theme::accent : Theme::border);
        g.drawRoundedRectangle (r, Theme::radius, 1.0f);

        const auto ink = playing ? Theme::background : (enabled ? Theme::text : Theme::textFaint);
        auto iconArea = r.withWidth (r.getHeight()).reduced (r.getHeight() * 0.32f);
        g.setColour (ink);
        if (playing)
        {
            g.fillRect (iconArea.reduced (1.0f));
        }
        else
        {
            juce::Path tri;
            tri.addTriangle (iconArea.getX(), iconArea.getY(), iconArea.getX(), iconArea.getBottom(),
                             iconArea.getRight(), iconArea.getCentreY());
            g.fillPath (tri);
        }

        g.setFont (Theme::label (11.0f));
        g.drawText (playing ? "STOP" : "PLAY", r.withTrimmedLeft (r.getHeight() * 0.8f).toNearestInt(),
                    juce::Justification::centredLeft, false);
    }
};

/** BPM control: drag vertically / scroll / ± buttons. */
class TempoControl final : public juce::Component
{
public:
    std::function<void (double)> onChange;

    TempoControl()
    {
        addAndMakeVisible (minus);
        addAndMakeVisible (plus);
        minus.onClick = [this] { setTempo (tempo - 1.0, true); };
        plus.onClick = [this] { setTempo (tempo + 1.0, true); };
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void setTempo (double bpm, bool notify)
    {
        bpm = juce::jlimit (40.0, 300.0, std::round (bpm));
        if (std::abs (bpm - tempo) < 0.5)
            return;
        tempo = bpm;
        repaint();
        if (notify && onChange)
            onChange (tempo);
    }

    double getTempo() const noexcept { return tempo; }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Theme::panelRaise);
        g.fillRoundedRectangle (r, Theme::radius);
        g.setColour (Theme::border);
        g.drawRoundedRectangle (r, Theme::radius, 1.0f);

        auto text = getLocalBounds().reduced (10, 0).withTrimmedRight (plus.getWidth() * 2 + 8);
        g.setColour (Theme::textFaint);
        g.setFont (Theme::label (9.0f));
        g.drawText ("TEMPO", text.removeFromLeft (46), juce::Justification::centredLeft, false);

        g.setColour (isEnabled() ? Theme::text : Theme::textFaint);
        g.setFont (Theme::display (22.0f));
        g.drawText (juce::String ((int) tempo), text.removeFromLeft (52), juce::Justification::centredLeft, false);
        g.setColour (Theme::textDim);
        g.setFont (Theme::label (9.0f));
        g.drawText ("BPM", text, juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        plus.setBounds (r.removeFromRight (r.getHeight()));
        r.removeFromRight (4);
        minus.setBounds (r.removeFromRight (r.getHeight()));
    }

    void mouseDown (const juce::MouseEvent&) override { dragStart = tempo; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        setTempo (dragStart - e.getDistanceFromDragStartY() * 0.5, true);
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        setTempo (tempo + (w.deltaY > 0 ? 1.0 : -1.0) * (std::abs (w.deltaY) > 0.2f ? 5.0 : 1.0), true);
    }

private:
    double tempo = 120.0;
    double dragStart = 120.0;
    FlatButton minus { "-", 13.0f }, plus { "+", 13.0f };
};
