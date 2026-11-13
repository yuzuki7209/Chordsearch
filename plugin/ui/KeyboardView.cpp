#include "KeyboardView.h"
#include "Theme.h"

KeyboardView::KeyboardView (int lowestNote, int highestNote)
    : lowNote (lowestNote), highNote (highestNote)
{
    setOpaque (false);
}

void KeyboardView::setNotes (chordsearch::MidiNoteSet newHeld, chordsearch::MidiNoteSet newSounding)
{
    if (newHeld == held && newSounding == sounding)
        return;

    held = newHeld;
    sounding = newSounding;
    repaint();
}

void KeyboardView::setScale (chordsearch::PitchClassSet scaleTones, int scaleRoot)
{
    if (scaleTones == scale && scaleRoot == root)
        return;

    scale = scaleTones;
    root = scaleRoot;
    repaint();
}

bool KeyboardView::isBlack (int note) noexcept
{
    switch (note % 12)
    {
        case 1: case 3: case 6: case 8: case 10: return true;
        default: return false;
    }
}

juce::Colour KeyboardView::fillFor (int note, juce::Colour base) const
{
    if (held.contains (note))     return Theme::accent;
    if (sounding.contains (note)) return Theme::accentDim;
    return base;
}

void KeyboardView::drawMarker (juce::Graphics& g, int note, juce::Rectangle<float> key) const
{
    if (! scale.contains (note) || sounding.contains (note))
        return;

    const float size = juce::jmin (7.0f, key.getWidth() - 4.0f);
    const auto marker = juce::Rectangle<float> (size, size)
                            .withCentre ({ key.getCentreX(), key.getBottom() - size - 4.0f });

    const bool isRoot = (note % 12) == root;
    g.setColour (isRoot ? Theme::warm : Theme::warm.withAlpha (isBlack (note) ? 0.75f : 0.55f));
    g.fillRect (marker);
}

void KeyboardView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    int numWhite = 0;
    for (int n = lowNote; n <= highNote; ++n)
        if (! isBlack (n))
            ++numWhite;

    if (numWhite == 0)
        return;

    const float whiteW = bounds.getWidth() / (float) numWhite;
    const float blackW = whiteW * 0.62f;
    const float blackH = bounds.getHeight() * 0.62f;

    // White keys
    int whiteIndex = 0;
    for (int n = lowNote; n <= highNote; ++n)
    {
        if (isBlack (n))
            continue;

        const juce::Rectangle<float> key (bounds.getX() + (float) whiteIndex * whiteW, bounds.getY(),
                                          whiteW - 1.0f, bounds.getHeight());
        g.setColour (fillFor (n, Theme::whiteKey));
        g.fillRect (key);

        if (n % 12 == 0) // octave label on C
        {
            g.setColour (sounding.contains (n) ? Theme::background : Theme::textDim);
            g.setFont (Theme::label (9.0f));
            g.drawText ("C" + juce::String (n / 12 - 1), key.withTrimmedBottom (14.0f).withTrimmedTop (key.getHeight() - 30.0f).toNearestInt(),
                        juce::Justification::centred, false);
        }

        drawMarker (g, n, key);
        ++whiteIndex;
    }

    // Black keys on top
    whiteIndex = 0;
    for (int n = lowNote; n <= highNote; ++n)
    {
        if (! isBlack (n))
        {
            ++whiteIndex;
            continue;
        }

        const float x = bounds.getX() + (float) whiteIndex * whiteW - blackW * 0.5f - 0.5f;
        const juce::Rectangle<float> key (x, bounds.getY(), blackW, blackH);
        g.setColour (fillFor (n, Theme::blackKey));
        g.fillRect (key);
        drawMarker (g, n, key);
    }

    // Top edge line
    g.setColour (Theme::border);
    g.fillRect (bounds.withHeight (2.0f));
}
