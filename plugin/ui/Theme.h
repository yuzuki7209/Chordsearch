#pragma once

#include <juce_graphics/juce_graphics.h>

#include <string>

/** Visual language: dark, flat, sharp-edged.
    Rule: corner radius never exceeds Theme::radius. No pill / capsule shapes anywhere. */
namespace Theme
{
    inline const juce::Colour background { 0xff0d0e11 };
    inline const juce::Colour panel      { 0xff15171c };
    inline const juce::Colour panelRaise { 0xff1c1f26 };
    inline const juce::Colour border     { 0xff262a32 };
    inline const juce::Colour text       { 0xffe9ebef };
    inline const juce::Colour textDim    { 0xff8a909b };
    inline const juce::Colour textFaint  { 0xff4a4f58 };
    inline const juce::Colour accent     { 0xff3ee0c4 }; // primary highlight
    inline const juce::Colour accentDim  { 0xff1f7f70 };
    inline const juce::Colour warm       { 0xffffb547 }; // tonicization, sustain, tensions
    inline const juce::Colour warmDim    { 0xff7a5a2a };

    inline const juce::Colour whiteKey   { 0xffd9dce2 };
    inline const juce::Colour blackKey   { 0xff1a1c21 };

    constexpr float radius = 2.0f;
    constexpr int gap = 10;
    constexpr int pad = 16;

    inline juce::Font label (float size = 11.0f)
    {
        return juce::Font (juce::FontOptions (size, juce::Font::bold).withKerningFactor (0.14f));
    }

    inline juce::Font body (float size = 14.0f)
    {
        return juce::Font (juce::FontOptions (size));
    }

    inline juce::Font strong (float size = 14.0f)
    {
        return juce::Font (juce::FontOptions (size, juce::Font::bold));
    }

    inline juce::Font display (float size)
    {
        return juce::Font (juce::FontOptions (size, juce::Font::bold).withKerningFactor (-0.02f));
    }

    /** Converts engine text (UTF-8, ASCII accidentals) to display text with ♭ / ♯. */
    inline juce::String pretty (const std::string& s)
    {
        juce::String out;
        const auto isNoteLetter = [] (char c) { return c >= 'A' && c <= 'G'; };
        const auto isDigit = [] (char c) { return c >= '0' && c <= '9'; };
        const auto isLetter = [] (char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };

        std::string buf;
        for (std::size_t i = 0; i < s.size(); ++i)
        {
            const char c = s[i];
            const char prev = i > 0 ? s[i - 1] : '\0';
            const char next = i + 1 < s.size() ? s[i + 1] : '\0';

            if (c == '#')
            {
                buf += "\xe2\x99\xaf"; // ♯
                continue;
            }
            if (c == 'b')
            {
                const bool afterNote = isNoteLetter (prev) && ! isLetter (next);
                const bool beforeDigit = isDigit (next);
                const bool beforeRoman = (next == 'I' || next == 'V') && ! isLetter (prev);
                if (afterNote || beforeDigit || beforeRoman)
                {
                    buf += "\xe2\x99\xad"; // ♭
                    continue;
                }
            }
            buf += c;
        }
        out = juce::String::fromUTF8 (buf.c_str());
        return out;
    }

    inline juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }
    inline juce::String utf8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

    /** Flat panel with a hairline border and a small accent tick in the top-left corner. */
    inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r, radius);
        g.setColour (border);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);

        g.setColour (accent);
        g.fillRect (r.getX(), r.getY(), 3.0f, 14.0f);

        g.setColour (textDim);
        g.setFont (label());
        g.drawText (title.toUpperCase(), r.reduced ((float) pad, 0).withHeight (34.0f).toNearestInt(),
                    juce::Justification::centredLeft, false);
    }

    /** Small rectangular tag (sharp corners, outlined). Returns its width. */
    inline float drawTag (juce::Graphics& g, const juce::String& tagText, float x, float y, float h,
                          juce::Colour colour, bool filled = false)
    {
        const auto font = label (10.0f);
        const float w = juce::GlyphArrangement::getStringWidth (font, tagText) + 12.0f;
        const juce::Rectangle<float> r (x, y, w, h);

        if (filled)
        {
            g.setColour (colour.withAlpha (0.16f));
            g.fillRoundedRectangle (r, radius);
        }
        g.setColour (colour.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        g.setColour (colour);
        g.setFont (font);
        g.drawText (tagText, r, juce::Justification::centred, false);
        return w;
    }

    /** Thin horizontal meter (rectangular). */
    inline void drawMeter (juce::Graphics& g, juce::Rectangle<float> r, float value, juce::Colour colour)
    {
        g.setColour (border);
        g.fillRect (r);
        g.setColour (colour);
        g.fillRect (r.withWidth (r.getWidth() * juce::jlimit (0.0f, 1.0f, value)));
    }
} // namespace Theme
