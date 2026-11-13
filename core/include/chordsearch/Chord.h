#pragma once

#include "NoteNames.h"
#include "PitchClassSet.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace chordsearch
{

enum class Third : std::uint8_t { none, minor, major, sus2, sus4 };
enum class Fifth : std::uint8_t { none, perfect, flat, sharp };
enum class Seventh : std::uint8_t { none, sixth, minor, major, diminished };

/** Tension bit flags (relative to the chord root). */
namespace Tension
{
    constexpr std::uint8_t flat9    = 1 << 0; // 1 semitone
    constexpr std::uint8_t nine     = 1 << 1; // 2
    constexpr std::uint8_t sharp9   = 1 << 2; // 3
    constexpr std::uint8_t eleven   = 1 << 3; // 5
    constexpr std::uint8_t sharp11  = 1 << 4; // 6
    constexpr std::uint8_t flat13   = 1 << 5; // 8
    constexpr std::uint8_t thirteen = 1 << 6; // 9

    constexpr std::uint8_t altered = flat9 | sharp9 | sharp11 | flat13;

    /** Semitone interval above the root for a single tension flag. */
    int interval (std::uint8_t flag) noexcept;

    /** "b9, #11, 13" */
    std::string toString (std::uint8_t flags);
} // namespace Tension

/** Broad chord class used for functional analysis and progression matching. */
enum class Family : std::uint8_t
{
    major,          // triad, 6, maj7, add9 ...
    minor,          // m, m7, m6, m(maj7) ...
    dominant,       // 7, 9, 13, 7alt, 7sus4 ...
    halfDiminished, // m7b5
    diminished,     // dim, dim7
    augmented,      // +, maj7#5
    suspended,      // sus2, sus4 triads
    power           // no third
};

struct Chord
{
    int root = 0; // pitch class
    int bass = 0; // pitch class
    Third third = Third::none;
    Fifth fifth = Fifth::none;
    Seventh seventh = Seventh::none;
    std::uint8_t tensions = 0;
    bool rootless = false;

    Family family() const noexcept;
    bool hasTension (std::uint8_t t) const noexcept { return (tensions & t) != 0; }
    bool isSus() const noexcept { return third == Third::sus2 || third == Third::sus4; }

    /** Chord tones + tensions, relative to the root (root always included). */
    PitchClassSet intervals() const noexcept;
    PitchClassSet pitchClasses() const noexcept { return intervals().transposed (root); }

    /** Quality text after the root, e.g. "m7b5", "7(b9,#11)", "maj9", "13sus4". */
    std::string suffix() const;

    /** Full name including slash bass, e.g. "Dm7", "C/E", "G7alt". */
    std::string name (Accidentals acc) const;

    /** Same root and family (used to merge re-voicings of the same harmony). */
    bool sameHarmony (const Chord& o) const noexcept { return root == o.root && family() == o.family(); }

    bool operator== (const Chord&) const noexcept = default;

    /** Builds a chord from a preset suffix ("maj7", "m7", "7", "m7b5", "dim7", "7alt", "" ...). */
    static std::optional<Chord> fromSuffix (int root, std::string_view suffix);

    /** Scale degree (1..7) a pitch class plays in this chord (b9 -> 2, #11 -> 4, b5 -> 5 ...). */
    int degreeOf (int pc) const noexcept;

    /** Letter-correct name of a pitch class within this chord (A7(b9): Bb, not A#). */
    std::string spell (int pc, Accidentals rootAcc) const;
};

} // namespace chordsearch
