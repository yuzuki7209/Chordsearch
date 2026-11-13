#pragma once

#include "NoteNames.h"
#include "PitchClassSet.h"

#include <string>

namespace chordsearch
{

enum class Mode : std::uint8_t { major, minor };

struct Key
{
    int tonic = 0;
    Mode mode = Mode::major;

    /** "C Major", "F# Minor" */
    std::string name() const;

    /** Spelling preference for chords and scales in this key. */
    Accidentals accidentals() const noexcept;

    /** Ionian for major, natural minor (Aeolian) for minor. */
    PitchClassSet scale() const noexcept;

    int index() const noexcept { return tonic + (mode == Mode::minor ? 12 : 0); }
    static Key fromIndex (int i) noexcept { return { mod12 (i), i >= 12 ? Mode::minor : Mode::major }; }

    bool operator== (const Key&) const noexcept = default;
};

} // namespace chordsearch
