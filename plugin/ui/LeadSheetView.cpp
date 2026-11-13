#include "LeadSheetView.h"
#include "Theme.h"

namespace
{
    const juce::Colour paper { 0xffece7dc };
    const juce::Colour ink { 0xff1d1c1a };

    constexpr int barsPerRow = 4;
    constexpr int rowHeight = 74;
} // namespace

juce::Font LeadSheetView::chartFont (float size) const
{
    // Hand-lettered look similar to classic fake books (falls back to the system font)
    return juce::Font (juce::FontOptions ("Marker Felt", size, juce::Font::plain));
}

void LeadSheetView::setChart (const std::vector<chordsearch::HistoryEntry>& history, const Header& h)
{
    entries = history;
    header = h;
    rebuild();
    repaint();
}

void LeadSheetView::setPlayhead (double beat)
{
    if (std::abs (beat - playhead) < 1e-3)
        return;
    playhead = beat;
    repaint();
}

void LeadSheetView::rebuild()
{
    bars.clear();
    double t = 0.0;
    for (const auto& e : entries)
    {
        const int bar = (int) std::floor (t / 4.0 + 1e-9);
        while ((int) bars.size() <= bar)
            bars.push_back ({});
        bars[(std::size_t) bar].chords.push_back ({ t - bar * 4.0, juce::String::fromUTF8 (e.name.c_str()) }); // ASCII b/# like a hand-written fake book
        t += std::max (1, e.beats);
    }

    const int total = (int) std::ceil (t / 4.0 - 1e-9);
    while ((int) bars.size() < total)
        bars.push_back ({});

    for (auto& b : bars)
        b.simile = b.chords.empty();
}

void LeadSheetView::paint (juce::Graphics& g)
{
    Theme::drawPanel (g, getLocalBounds().toFloat(), "Chart");

    auto paperArea = getLocalBounds().reduced (Theme::pad - 4, 0).withTrimmedTop (36).withTrimmedBottom (Theme::pad - 4);
    g.setColour (paper);
    g.fillRect (paperArea);

    auto area = paperArea.reduced (18, 12);

    // ---- Header (title, style, key, tempo)
    auto head = area.removeFromTop (56);
    g.setColour (ink);
    g.setFont (chartFont (26.0f));
    g.drawText ("Chordsearch Session", head.removeFromTop (30), juce::Justification::centred, false);

    g.setFont (chartFont (15.0f));
    g.drawText ("(" + header.style + ")", head, juce::Justification::centredLeft, false);
    juce::String right;
    if (header.key.isNotEmpty())
        right << header.key << "    ";
    right << juce::String::fromUTF8 ("\xe2\x99\xa9 = ") << juce::String ((int) header.tempo);
    g.drawText (right, head, juce::Justification::centredRight, false);

    area.removeFromTop (6);

    if (bars.empty())
    {
        g.setColour (ink.withAlpha (0.45f));
        g.setFont (chartFont (17.0f));
        g.drawText (Theme::utf8 ("코드를 연주하면 차트가 그려집니다"), area, juce::Justification::centred, false);
        return;
    }

    const int rows = (int) ((bars.size() + barsPerRow - 1) / barsPerRow);
    const int visibleRows = juce::jmax (1, area.getHeight() / rowHeight);

    const int currentBar = playhead >= 0.0 ? (int) std::floor (playhead / 4.0) : -1;
    if (currentBar >= 0)
    {
        const int currentRow = currentBar / barsPerRow;
        if (currentRow < firstVisibleRow || currentRow >= firstVisibleRow + visibleRows)
            firstVisibleRow = juce::jmax (0, currentRow - (visibleRows > 2 ? 1 : 0));
    }
    else
    {
        firstVisibleRow = header.confirmed ? 0 : juce::jmax (0, rows - visibleRows);
    }
    firstVisibleRow = juce::jlimit (0, juce::jmax (0, rows - 1), firstVisibleRow);

    const float barW = (float) area.getWidth() / (float) barsPerRow;

    for (int row = firstVisibleRow; row < rows && row < firstVisibleRow + visibleRows; ++row)
    {
        const float y0 = (float) area.getY() + (float) ((row - firstVisibleRow) * rowHeight);
        const float staffTop = y0 + 36.0f;
        const float lineGap = 5.0f;
        const float staffBottom = staffTop + 4.0f * lineGap;
        const float x0 = (float) area.getX();

        // Staff lines (only as far as this row has bars)
        const int barsInRow = juce::jmin (barsPerRow, (int) bars.size() - row * barsPerRow);
        g.setColour (ink.withAlpha (0.35f));
        for (int l = 0; l < 5; ++l)
            g.fillRect (x0, staffTop + (float) l * lineGap, barW * (float) barsInRow, 0.8f);

        for (int c = 0; c < barsPerRow; ++c)
        {
            const int barIndex = row * barsPerRow + c;
            if (barIndex >= (int) bars.size())
                break;

            const auto& bar = bars[(std::size_t) barIndex];
            const float bx = x0 + (float) c * barW;
            const bool current = barIndex == currentBar;

            if (current)
            {
                g.setColour (Theme::accent.withAlpha (0.22f));
                g.fillRect (juce::Rectangle<float> (bx, y0 + 2.0f, barW, staffBottom - y0 + 8.0f));
            }

            // Chord symbols
            g.setColour (ink);
            for (const auto& ch : bar.chords)
            {
                const float cx = bx + 8.0f + (float) (ch.offset / 4.0) * (barW - 8.0f);
                g.setFont (chartFont (21.0f));
                g.drawText (ch.name, juce::Rectangle<float> (cx, y0 + 4.0f, barW, 28.0f), juce::Justification::centredLeft, false);
            }

            // Rhythm slashes or simile mark
            const float mid = (staffTop + staffBottom) * 0.5f;
            if (bar.simile)
            {
                const float cx = bx + barW * 0.5f;
                g.setColour (ink);
                g.drawLine (cx - 7.0f, mid + 7.0f, cx + 7.0f, mid - 7.0f, 2.6f);
                g.fillRect (juce::Rectangle<float> (3.4f, 3.4f).withCentre ({ cx - 6.0f, mid - 4.5f }));
                g.fillRect (juce::Rectangle<float> (3.4f, 3.4f).withCentre ({ cx + 6.0f, mid + 4.5f }));
            }
            else
            {
                for (int beat = 0; beat < 4; ++beat)
                {
                    const float sx = bx + barW * (0.16f + 0.22f * (float) beat);
                    const bool now = current && (int) std::floor (playhead) % 4 == beat;
                    g.setColour (now ? Theme::accent.darker (0.3f) : ink);
                    g.drawLine (sx - 4.0f, mid + 5.0f, sx + 4.0f, mid - 5.0f, now ? 4.0f : 3.0f);
                }
            }

            // Barlines
            g.setColour (ink);
            if (barIndex == 0)
            {
                // Start repeat: thick, thin, dots
                g.fillRect (bx, staffTop, 3.0f, staffBottom - staffTop);
                g.fillRect (bx + 5.0f, staffTop, 1.0f, staffBottom - staffTop);
                g.fillRect (juce::Rectangle<float> (2.6f, 2.6f).withCentre ({ bx + 9.0f, staffTop + lineGap * 1.5f }));
                g.fillRect (juce::Rectangle<float> (2.6f, 2.6f).withCentre ({ bx + 9.0f, staffTop + lineGap * 2.5f }));
            }
            else if (c == 0)
            {
                g.fillRect (bx, staffTop, 1.2f, staffBottom - staffTop);
            }

            const float ex = bx + barW;
            if (barIndex == (int) bars.size() - 1)
            {
                // End repeat: dots, thin, thick
                g.fillRect (juce::Rectangle<float> (2.6f, 2.6f).withCentre ({ ex - 10.0f, staffTop + lineGap * 1.5f }));
                g.fillRect (juce::Rectangle<float> (2.6f, 2.6f).withCentre ({ ex - 10.0f, staffTop + lineGap * 2.5f }));
                g.fillRect (ex - 7.0f, staffTop, 1.0f, staffBottom - staffTop);
                g.fillRect (ex - 4.0f, staffTop, 3.0f, staffBottom - staffTop);
            }
            else
            {
                g.fillRect (ex - 1.2f, staffTop, 1.2f, staffBottom - staffTop);
            }

            // Playhead line
            if (current)
            {
                const float px = bx + (float) (std::fmod (playhead, 4.0) / 4.0) * barW;
                g.setColour (Theme::accent.darker (0.2f));
                g.fillRect (px - 1.0f, y0 + 2.0f, 2.0f, staffBottom - y0 + 8.0f);
            }
        }

        // Bar numbers at line starts (Real Book style)
        g.setColour (ink.withAlpha (0.5f));
        g.setFont (Theme::body (9.0f));
        g.drawText (juce::String (row * barsPerRow + 1), juce::Rectangle<float> (x0 - 16.0f, staffTop - 14.0f, 14.0f, 12.0f),
                    juce::Justification::centredRight, false);
    }

    if (rows > firstVisibleRow + visibleRows)
    {
        g.setColour (ink.withAlpha (0.45f));
        g.setFont (Theme::label (9.0f));
        g.drawText ("MORE " + juce::String::fromUTF8 ("\xe2\x86\x93"), area.removeFromBottom (14), juce::Justification::centredRight, false);
    }

    if (! header.confirmed)
    {
        g.setColour (ink.withAlpha (0.55f));
        g.setFont (Theme::label (9.0f));
        g.drawText (Theme::utf8 ("미확정 · CONFIRM 후 백킹 재생"), paperArea.reduced (10, 6).removeFromBottom (14),
                    juce::Justification::centredLeft, false);
    }
}
