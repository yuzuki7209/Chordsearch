#pragma once

#include "PitchClassSet.h"

#include <bit>
#include <cstdint>

namespace chordsearch
{

/** A set of MIDI note numbers (0..127) stored as a 128-bit mask. Trivially copyable. */
struct MidiNoteSet
{
    std::uint64_t lo = 0; // notes 0..63
    std::uint64_t hi = 0; // notes 64..127

    constexpr void set (int note, bool on) noexcept
    {
        if (note < 0 || note > 127)
            return;
        auto& word = note < 64 ? lo : hi;
        const auto bit = std::uint64_t (1) << (note & 63);
        word = on ? (word | bit) : (word & ~bit);
    }

    constexpr bool contains (int note) const noexcept
    {
        if (note < 0 || note > 127)
            return false;
        const auto word = note < 64 ? lo : hi;
        return ((word >> (note & 63)) & 1u) != 0;
    }

    constexpr bool empty() const noexcept { return lo == 0 && hi == 0; }
    constexpr int size() const noexcept   { return std::popcount (lo) + std::popcount (hi); }
    constexpr void clear() noexcept       { lo = hi = 0; }

    /** Lowest note number, or -1 if empty. */
    constexpr int lowest() const noexcept
    {
        if (lo != 0) return std::countr_zero (lo);
        if (hi != 0) return 64 + std::countr_zero (hi);
        return -1;
    }

    /** Highest note number, or -1 if empty. */
    constexpr int highest() const noexcept
    {
        if (hi != 0) return 127 - std::countl_zero (hi);
        if (lo != 0) return 63 - std::countl_zero (lo);
        return -1;
    }

    constexpr PitchClassSet pitchClasses() const noexcept
    {
        PitchClassSet pcs;
        for (int n = 0; n < 128; ++n)
            if (contains (n))
                pcs.add (n);
        return pcs;
    }

    constexpr MidiNoteSet operator| (MidiNoteSet o) const noexcept { return { lo | o.lo, hi | o.hi }; }
    constexpr MidiNoteSet operator& (MidiNoteSet o) const noexcept { return { lo & o.lo, hi & o.hi }; }
    constexpr MidiNoteSet operator~() const noexcept               { return { ~lo, ~hi }; }

    friend constexpr bool operator== (MidiNoteSet, MidiNoteSet) noexcept = default;
};

} // namespace chordsearch
