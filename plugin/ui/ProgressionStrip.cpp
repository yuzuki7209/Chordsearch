#include "ProgressionStrip.h"

ProgressionStrip::ProgressionStrip()
{
    addAndMakeVisible (confirmButton);
    confirmButton.onClick = [this]
    {
        if (onConfirm)
            onConfirm (! locked);
    };
}

void ProgressionStrip::setSnapshot (const chordsearch::AnalysisSnapshot& s)
{
    const bool grew = s.history.size() > history.size();
    history = s.history;
    locked = ! s.recording;

    confirmButton.setButtonText (locked ? "EDIT" : "CONFIRM");
    confirmButton.setToggleState (locked, juce::dontSendNotification);
    confirmButton.setEnabled (locked || ! history.empty());

    if (locked)
    {
        hoverBox = hoverGap = -1;
        hoverDelete = false;
    }

    if (grew) // follow new chords
        scroll = contentWidth();
    clampScroll();
    repaint();
}

void ProgressionStrip::setPlayingIndex (int index)
{
    if (index == playingIndex)
        return;
    playingIndex = index;

    if (index >= 0) // keep the playing chord visible
    {
        const auto r = boxRect ((std::size_t) index);
        const auto l = lane();
        if (r.getRight() > l.getRight())  scroll += r.getRight() - l.getRight() + gapW;
        if (r.getX() < l.getX())          scroll -= l.getX() - r.getX() + gapW;
        clampScroll();
    }
    repaint();
}

juce::Rectangle<int> ProgressionStrip::lane() const
{
    return getLocalBounds().reduced (Theme::pad - 4, 6).withTrimmedLeft (100).withTrimmedRight (confirmButton.getWidth() + 12);
}

int ProgressionStrip::contentWidth() const
{
    return gapW + (int) history.size() * (boxW + gapW);
}

void ProgressionStrip::clampScroll()
{
    scroll = juce::jlimit (0, juce::jmax (0, contentWidth() - lane().getWidth()), scroll);
}

juce::Rectangle<int> ProgressionStrip::boxRect (std::size_t i) const
{
    const auto l = lane();
    return { l.getX() + gapW + (int) i * (boxW + gapW) - scroll, l.getY(), boxW, l.getHeight() };
}

juce::Rectangle<int> ProgressionStrip::deleteRect (std::size_t i) const
{
    const auto b = boxRect (i);
    return { b.getRight() - 19, b.getY() + 3, 16, 16 };
}

juce::Rectangle<int> ProgressionStrip::plusRect (std::size_t gap) const
{
    const auto l = lane();
    const int centreX = l.getX() + gapW / 2 + (int) gap * (boxW + gapW) - scroll;
    return juce::Rectangle<int> (18, 18).withCentre ({ centreX, l.getCentreY() });
}

void ProgressionStrip::resized()
{
    auto r = getLocalBounds().reduced (Theme::pad - 4, 6);
    confirmButton.setBounds (r.removeFromRight (96).withSizeKeepingCentre (96, 30));
    clampScroll();
}

void ProgressionStrip::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().reduced (Theme::pad - 4, 6);

    g.setColour (Theme::textFaint);
    g.setFont (Theme::label (10.0f));
    g.drawText ("PROGRESSION", area.removeFromLeft (100), juce::Justification::centredLeft, false);

    const auto l = lane();
    g.saveState();
    g.reduceClipRegion (l.expanded (0, 6));

    if (history.empty())
    {
        g.setColour (Theme::textFaint);
        g.setFont (Theme::body (12.0f));
        g.drawText (Theme::utf8 ("건반으로 코드를 연주하거나 + 를 눌러 추가하세요"),
                    l.withTrimmedLeft (gapW + 8), juce::Justification::centredLeft, false);
    }

    for (std::size_t i = 0; i < history.size(); ++i)
    {
        const auto box = boxRect (i).toFloat();
        if (box.getRight() < (float) l.getX() || box.getX() > (float) l.getRight())
            continue;

        const bool isLast = i + 1 == history.size();
        const bool playing = (int) i == playingIndex;
        const bool hovered = (int) i == hoverBox && ! locked;
        const bool highlight = playing || (! locked && isLast && playingIndex < 0);

        g.setColour (playing ? Theme::accent.withAlpha (0.18f) : (hovered || highlight ? Theme::panelRaise : Theme::panel));
        g.fillRoundedRectangle (box, Theme::radius);
        g.setColour (playing || highlight ? Theme::accent : (hovered ? Theme::textDim : Theme::border));
        g.drawRoundedRectangle (box.reduced (0.5f), Theme::radius, 1.0f);

        auto inner = box.reduced (8.0f, 4.0f);
        g.setColour (highlight || hovered ? Theme::text : Theme::textDim);
        g.setFont (Theme::strong (13.0f));
        g.drawFittedText (Theme::pretty (history[i].name), inner.removeFromTop (inner.getHeight() * 0.56f).toNearestInt()
                              .withTrimmedRight (hovered ? 14 : 0),
                          juce::Justification::centredLeft, 1, 0.7f);
        g.setColour (highlight ? Theme::accent : Theme::textFaint);
        g.setFont (Theme::body (11.0f));
        g.drawText (Theme::pretty (history[i].roman), inner, juce::Justification::centredLeft, false);

        // Duration (beats) when not a full bar
        if (history[i].beats != 4)
        {
            g.setColour (Theme::warm);
            g.setFont (Theme::label (9.0f));
            g.drawText (juce::String (history[i].beats) + "b", inner, juce::Justification::centredRight, false);
        }

        if (hovered)
        {
            const auto x = deleteRect (i).toFloat();
            g.setColour (hoverDelete ? Theme::warm.withAlpha (0.25f) : Theme::panel);
            g.fillRect (x);
            g.setColour (hoverDelete ? Theme::warm : Theme::textDim);
            g.drawRect (x, 1.0f);
            const auto c = x.reduced (4.5f);
            g.drawLine (c.getX(), c.getY(), c.getRight(), c.getBottom(), 1.4f);
            g.drawLine (c.getRight(), c.getY(), c.getX(), c.getBottom(), 1.4f);
        }
    }

    // Insert buttons
    if (! locked)
    {
        auto drawPlus = [&] (std::size_t gap, bool strong)
        {
            const auto p = plusRect (gap).toFloat();
            g.setColour (strong ? Theme::accent.withAlpha (0.2f) : Theme::panel);
            g.fillRect (p);
            g.setColour (strong ? Theme::accent : Theme::textDim);
            g.drawRect (p, 1.0f);
            const auto c = p.getCentre();
            g.fillRect (juce::Rectangle<float> (9.0f, 1.6f).withCentre (c));
            g.fillRect (juce::Rectangle<float> (1.6f, 9.0f).withCentre (c));
        };

        if (hoverGap >= 0)
            drawPlus ((std::size_t) hoverGap, true);
        else if (history.empty())
            drawPlus (0, false);
    }

    g.restoreState();

    // Scroll hints
    if (scroll > 0)
    {
        g.setColour (Theme::textDim);
        g.fillRect (l.getX(), l.getY(), 2, l.getHeight());
    }
    if (scroll < contentWidth() - l.getWidth())
    {
        g.setColour (Theme::textDim);
        g.fillRect (l.getRight() - 2, l.getY(), 2, l.getHeight());
    }
}

void ProgressionStrip::updateHover (juce::Point<int> p)
{
    int box = -1, gap = -1;
    bool del = false;

    if (! locked && lane().expanded (0, 6).contains (p))
    {
        for (std::size_t i = 0; i < history.size(); ++i)
        {
            if (boxRect (i).contains (p))
            {
                box = (int) i;
                del = deleteRect (i).expanded (2).contains (p);
                break;
            }
        }
        if (box < 0)
        {
            for (std::size_t k = 0; k <= history.size(); ++k)
            {
                const auto pr = plusRect (k);
                if (std::abs (p.x - pr.getCentreX()) <= gapW / 2 + 4)
                {
                    gap = (int) k;
                    break;
                }
            }
        }
    }

    if (box != hoverBox || gap != hoverGap || del != hoverDelete)
    {
        hoverBox = box;
        hoverGap = gap;
        hoverDelete = del;
        setMouseCursor (del || gap >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ProgressionStrip::mouseMove (const juce::MouseEvent& e) { updateHover (e.getPosition()); }

void ProgressionStrip::mouseExit (const juce::MouseEvent&)
{
    hoverBox = hoverGap = -1;
    hoverDelete = false;
    repaint();
}

void ProgressionStrip::mouseDown (const juce::MouseEvent& e)
{
    updateHover (e.getPosition());
    if (locked)
        return;

    if (hoverBox >= 0 && hoverDelete)
    {
        const auto index = (std::size_t) hoverBox;
        hoverBox = -1;
        hoverDelete = false;
        if (onDelete)
            onDelete (index);
        return;
    }

    if (hoverGap >= 0 && onInsert)
        onInsert ((std::size_t) hoverGap, plusRect ((std::size_t) hoverGap));
}

void ProgressionStrip::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float delta = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
    scroll -= juce::roundToInt (delta * 240.0f);
    clampScroll();
    repaint();
}
