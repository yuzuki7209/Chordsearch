#include "Panels.h"
#include "Theme.h"

#include <array>

using namespace chordsearch;

namespace
{
    juce::String spell (int pc, Accidentals acc) { return Theme::pretty (pitchClassName (pc, acc)); }

    juce::StringArray tensionList (std::uint8_t flags)
    {
        juce::StringArray out;
        for (std::uint8_t f : { Tension::flat9, Tension::nine, Tension::sharp9, Tension::eleven,
                                Tension::sharp11, Tension::flat13, Tension::thirteen })
            if ((flags & f) != 0)
                out.add (Theme::pretty (Tension::toString (f)));
        return out;
    }

    Accidentals accFor (const AnalysisSnapshot& s)
    {
        return s.hasKey ? s.key.accidentals() : Accidentals::flats;
    }

    /** Spelling for notes inside a chord/scale: follow the chord root's own key
        (A7 → C#, not Db), falling back to the song key when no chord is known. */
    Accidentals chordSpelling (const AnalysisSnapshot& s)
    {
        if (! s.hasChord)
            return accFor (s);
        const auto f = s.chord.family();
        const bool minorish = f == Family::minor || f == Family::halfDiminished || f == Family::diminished;
        return Key { s.chord.root, minorish ? Mode::minor : Mode::major }.accidentals();
    }

    void emptyState (juce::Graphics& g, juce::Rectangle<int> area, const char* message)
    {
        g.setColour (Theme::textFaint);
        g.setFont (Theme::body (13.0f));
        g.drawText (Theme::utf8 (message), area, juce::Justification::centred, true);
    }
} // namespace

// ---------------------------------------------------------------------------
void SnapshotPanel::paint (juce::Graphics& g)
{
    Theme::drawPanel (g, getLocalBounds().toFloat(), title);
    paintContent (g, content());
}

juce::Rectangle<int> SnapshotPanel::content() const
{
    return getLocalBounds().reduced (Theme::pad, 0).withTrimmedTop (36).withTrimmedBottom (Theme::pad - 4);
}

// ---------------------------------------------------------------------------
void ChordPanel::setSoundingNotes (const MidiNoteSet& n)
{
    if (n == sounding)
        return;
    sounding = n;
    repaint();
}

void ChordPanel::paintContent (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto acc = chordSpelling (snap);

    // Notes line at the bottom
    auto notesLine = area.removeFromBottom (18);
    {
        g.setColour (Theme::textFaint);
        g.setFont (Theme::label (10.0f));
        g.drawText ("NOTES", notesLine.removeFromLeft (52), juce::Justification::centredLeft, false);

        juce::StringArray names;
        for (int n = sounding.lowest(); n >= 0 && n <= sounding.highest(); ++n)
        {
            if (! sounding.contains (n))
                continue;
            const auto pcName = snap.hasChord ? snap.chord.spell (n, accFor (snap)) : pitchClassName (n, acc);
            names.add (Theme::pretty (pcName + std::to_string (n / 12 - 1)));
        }

        g.setColour (Theme::textDim);
        g.setFont (Theme::body (12.0f));
        g.drawText (names.isEmpty() ? juce::String ("-") : names.joinIntoString ("  "), notesLine,
                    juce::Justification::centredLeft, true);
    }

    if (! snap.hasChord)
    {
        g.setColour (Theme::textFaint);
        g.setFont (Theme::display (64.0f));
        g.drawText (Theme::utf8 ("\xe2\x80\x94"), area.removeFromTop (84), juce::Justification::centredLeft, false);
        g.setColour (Theme::textDim);
        g.setFont (Theme::body (13.0f));
        g.drawText (Theme::utf8 ("건반에서 3음 이상을 눌러 코드를 연주하세요"), area.removeFromTop (22),
                    juce::Justification::centredLeft, false);
        return;
    }

    // Chord name
    g.setColour (snap.chordSounding ? Theme::text : Theme::textDim);
    g.setFont (Theme::display (66.0f));
    g.drawFittedText (Theme::pretty (snap.chordName), area.removeFromTop (80), juce::Justification::centredLeft, 1, 0.5f);

    // Roman numeral + function tag
    if (snap.hasKey)
    {
        auto line = area.removeFromTop (30);
        const auto roman = Theme::pretty (snap.harmony.roman);
        const auto romanFont = Theme::strong (22.0f);
        const float romanW = juce::GlyphArrangement::getStringWidth (romanFont, roman);

        g.setColour (Theme::accent);
        g.setFont (romanFont);
        g.drawText (roman, line, juce::Justification::centredLeft, false);

        const auto fnColour = snap.harmony.function == HarmonicFunction::dominant ? Theme::warm : Theme::accent;
        Theme::drawTag (g, Theme::utf8 (functionLabel (snap.harmony.function)),
                        (float) line.getX() + romanW + 14.0f, (float) line.getCentreY() - 10.0f, 20.0f, fnColour, true);

        area.removeFromTop (4);
        g.setColour (Theme::textDim);
        g.setFont (Theme::body (13.0f));
        g.drawText (Theme::pretty (snap.harmony.description), area.removeFromTop (20), juce::Justification::centredLeft, true);
    }

    if (! snap.alternatives.empty())
    {
        juce::StringArray alts;
        for (const auto& a : snap.alternatives)
            alts.add (Theme::pretty (a));

        auto line = area.removeFromTop (20);
        g.setColour (Theme::textFaint);
        g.setFont (Theme::label (10.0f));
        g.drawText (Theme::utf8 ("다른 해석"), line.removeFromLeft (64), juce::Justification::centredLeft, false);
        g.setFont (Theme::body (12.0f));
        g.setColour (Theme::textDim);
        g.drawText (alts.joinIntoString ("   "), line, juce::Justification::centredLeft, true);
    }
}

// ---------------------------------------------------------------------------
void KeyPanel::paintContent (juce::Graphics& g, juce::Rectangle<int> area)
{
    if (! snap.hasKey)
    {
        emptyState (g, area, "코드를 연주하면 키를 분석합니다");
        return;
    }

    auto top = area.removeFromTop (42);
    g.setColour (Theme::text);
    g.setFont (Theme::display (30.0f));
    g.drawText (Theme::pretty (snap.key.name()), top, juce::Justification::centredLeft, false);

    g.setColour (Theme::accent);
    g.setFont (Theme::strong (16.0f));
    g.drawText (juce::String (juce::roundToInt (snap.keyProbability * 100.0f)) + "%", top, juce::Justification::centredRight, false);

    Theme::drawMeter (g, area.removeFromTop (4).toFloat(), snap.keyProbability, Theme::accent);
    area.removeFromTop (10);

    // Diatonic 7th chords of the key (bottom); the current chord's degree is highlighted
    {
        auto block = area.removeFromBottom (44);
        g.setColour (Theme::textFaint);
        g.setFont (Theme::label (10.0f));
        g.drawText (Theme::utf8 ("다이아토닉 코드"), block.removeFromTop (16), juce::Justification::centredLeft, false);

        const std::array<int, 7> degrees = snap.key.mode == Mode::major ? std::array<int, 7> { 0, 2, 4, 5, 7, 9, 11 }
                                                                        : std::array<int, 7> { 0, 2, 3, 5, 7, 8, 10 };
        const float cellW = (float) block.getWidth() / 7.0f;
        const auto keyAcc = snap.key.accidentals();

        for (std::size_t i = 0; i < degrees.size(); ++i)
        {
            const auto chord = diatonicSeventh (snap.key, degrees[i]);
            if (! chord)
                continue;

            const juce::Rectangle<float> cell ((float) block.getX() + cellW * (float) i, (float) block.getY() + 2.0f,
                                               cellW - 4.0f, (float) block.getHeight() - 4.0f);
            const bool current = snap.hasChord && snap.chord.root == chord->root
                              && snap.harmony.role == ChordRole::diatonic;

            g.setColour (current ? Theme::accent.withAlpha (0.16f) : Theme::panelRaise);
            g.fillRoundedRectangle (cell, Theme::radius);
            if (current)
            {
                g.setColour (Theme::accent);
                g.drawRoundedRectangle (cell.reduced (0.5f), Theme::radius, 1.0f);
            }

            g.setColour (current ? Theme::text : Theme::textDim);
            g.setFont (Theme::strong (12.0f));
            g.drawFittedText (Theme::pretty (chord->name (keyAcc)), cell.reduced (3.0f, 0.0f).toNearestInt(),
                              juce::Justification::centred, 1, 0.6f);
        }
        area.removeFromBottom (6);
    }

    if (snap.localCenter)
    {
        auto line = area.removeFromTop (22);
        g.setColour (Theme::warm);
        g.setFont (Theme::label (10.0f));
        g.drawText (Theme::utf8 ("일시적 조성 중심"), line.removeFromLeft (100), juce::Justification::centredLeft, false);
        g.setFont (Theme::strong (13.0f));
        g.drawText (Theme::pretty (snap.localCenter->name()), line, juce::Justification::centredLeft, false);
        area.removeFromTop (4);
    }

    g.setColour (Theme::textFaint);
    g.setFont (Theme::label (10.0f));
    g.drawText (Theme::utf8 ("후보"), area.removeFromTop (18), juce::Justification::centredLeft, false);

    int shown = 0;
    for (const auto& e : snap.keyRanking)
    {
        if (e.key == snap.key || shown >= 3 || area.getHeight() < 18)
            continue;
        ++shown;

        auto row = area.removeFromTop (22);
        auto meter = row.removeFromRight (row.getWidth() / 3).toFloat();
        auto pct = row.removeFromRight (44);

        g.setColour (Theme::textDim);
        g.setFont (Theme::body (13.0f));
        g.drawText (Theme::pretty (e.key.name()), row, juce::Justification::centredLeft, false);
        g.setColour (Theme::textFaint);
        g.drawText (juce::String (juce::roundToInt (e.probability * 100.0f)) + "%", pct, juce::Justification::centredRight, false);
        Theme::drawMeter (g, meter.withSizeKeepingCentre (meter.getWidth() - 10.0f, 2.0f), e.probability, Theme::textDim);
    }
}

// ---------------------------------------------------------------------------
void ScalePanel::paintContent (juce::Graphics& g, juce::Rectangle<int> area)
{
    if (! snap.hasChord || snap.scales.empty())
    {
        emptyState (g, area, "현재 코드에 어울리는 스케일이 여기에 표시됩니다");
        return;
    }

    const auto acc = chordSpelling (snap);
    const int rowH = juce::jlimit (54, 62, area.getHeight() / (int) juce::jmax<std::size_t> (1, snap.scales.size()));

    for (std::size_t i = 0; i < snap.scales.size() && area.getHeight() >= rowH - 4; ++i)
    {
        const auto& sc = snap.scales[i];
        auto row = area.removeFromTop (rowH).withTrimmedBottom (4);
        const bool primary = i == 0;

        if (primary)
        {
            g.setColour (Theme::panelRaise);
            g.fillRoundedRectangle (row.toFloat(), Theme::radius);
            g.setColour (Theme::accent);
            g.fillRect (row.getX(), row.getY(), 2, row.getHeight());
        }

        auto inner = row.reduced (10, 6);
        auto line1 = inner.removeFromTop (inner.getHeight() / 2);
        auto line2 = inner;

        const juce::String name = spell (sc.root, acc) + " " + Theme::pretty (sc.type->name);
        const auto nameFont = Theme::strong (primary ? 15.0f : 14.0f);
        g.setColour (primary ? Theme::text : Theme::textDim);
        g.setFont (nameFont);
        g.drawText (name, line1, juce::Justification::centredLeft, false);

        const float nameW = juce::GlyphArrangement::getStringWidth (nameFont, name);
        g.setColour (Theme::textFaint);
        g.setFont (Theme::body (12.0f));
        g.drawText (Theme::pretty (sc.type->koreanName), line1.withTrimmedLeft ((int) nameW + 10),
                    juce::Justification::centredLeft, true);

        // Notes spelled from the root; chord tones brighter
        const auto chordTones = snap.chord.pitchClasses();
        float x = (float) line2.getX();
        g.setFont (Theme::body (13.0f));
        for (int k = 0; k < 12; ++k)
        {
            const int pc = mod12 (sc.root + k);
            if (! sc.notes.contains (pc))
                continue;
            const auto txt = Theme::pretty (spellScaleNote (*sc.type, sc.root, pc, accFor (snap)));
            g.setColour (chordTones.contains (pc) ? (primary ? Theme::accent : Theme::text) : Theme::textDim);
            g.drawText (txt, juce::Rectangle<float> (x, (float) line2.getY(), 30.0f, (float) line2.getHeight()),
                        juce::Justification::centredLeft, false);
            x += juce::GlyphArrangement::getStringWidth (Theme::body (13.0f), txt) + 9.0f;
        }

        // Available tension tags on the right
        const auto tensions = tensionList (sc.availableTensions);
        float tx = (float) line1.getRight();
        for (int t = tensions.size(); --t >= 0;)
        {
            const float w = juce::GlyphArrangement::getStringWidth (Theme::label (10.0f), tensions[t]) + 12.0f;
            tx -= w;
            Theme::drawTag (g, tensions[t], tx, (float) line1.getCentreY() - 9.0f, 18.0f, Theme::warm);
            tx -= 4.0f;
        }
    }
}

// ---------------------------------------------------------------------------
void NextPanel::paintContent (juce::Graphics& g, juce::Rectangle<int> area)
{
    if (! snap.hasChord || snap.suggestions.empty())
    {
        emptyState (g, area, "다음에 올 코드 추천이 여기에 표시됩니다");
        return;
    }

    const auto acc = accFor (snap);
    const int rowH = juce::jlimit (54, 60, area.getHeight() / (int) juce::jmax<std::size_t> (1, snap.suggestions.size()));

    for (std::size_t i = 0; i < snap.suggestions.size() && area.getHeight() >= rowH - 4; ++i)
    {
        const auto& s = snap.suggestions[i];
        auto row = area.removeFromTop (rowH).withTrimmedBottom (4);
        const bool primary = i == 0;

        if (primary)
        {
            g.setColour (Theme::panelRaise);
            g.fillRoundedRectangle (row.toFloat(), Theme::radius);
            g.setColour (Theme::accent);
            g.fillRect (row.getX(), row.getY(), 2, row.getHeight());
        }

        auto inner = row.reduced (10, 5);
        auto line1 = inner.removeFromTop (inner.getHeight() * 55 / 100);
        auto line2 = inner;

        // Score meter on the right of line 1
        auto meterArea = line1.removeFromRight (64).toFloat();
        Theme::drawMeter (g, meterArea.withSizeKeepingCentre (meterArea.getWidth(), 3.0f), s.score,
                          primary ? Theme::accent : Theme::textDim);

        const auto name = Theme::pretty (s.chord.name (acc));
        const auto nameFont = Theme::strong (primary ? 20.0f : 18.0f);
        g.setColour (primary ? Theme::text : Theme::text.withAlpha (0.85f));
        g.setFont (nameFont);
        g.drawText (name, line1, juce::Justification::centredLeft, false);

        const float nameW = juce::GlyphArrangement::getStringWidth (nameFont, name);
        g.setColour (Theme::accent.withAlpha (primary ? 1.0f : 0.7f));
        g.setFont (Theme::strong (13.0f));
        g.drawText (Theme::pretty (s.roman), line1.withTrimmedLeft ((int) nameW + 10), juce::Justification::centredLeft, false);

        // Reason + tensions
        const auto tensions = tensionList (s.tensions);
        auto tensionArea = line2.removeFromRight (juce::jmin (130, line2.getWidth() / 3));
        if (! tensions.isEmpty())
        {
            g.setColour (Theme::warm.withAlpha (0.85f));
            g.setFont (Theme::body (11.0f));
            g.drawText ("+" + tensions.joinIntoString (" "), tensionArea, juce::Justification::centredRight, false);
        }

        g.setColour (Theme::textDim);
        g.setFont (Theme::body (12.0f));
        g.drawText (Theme::pretty (s.reason), line2, juce::Justification::centredLeft, true);
    }
}
