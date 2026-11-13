#pragma once

#include <bit>
#include <cstdint>

namespace chordsearch
{

constexpr int mod12 (int x) noexcept { return ((x % 12) + 12) % 12; }

/** A set of pitch classes (0 = C ... 11 = B) stored as a 12-bit mask. */
class PitchClassSet
{
public:
    constexpr PitchClassSet() noexcept = default;
    constexpr explicit PitchClassSet (std::uint16_t bits) noexcept : mask (std::uint16_t (bits & 0xFFFu)) {}

    constexpr void add (int pc) noexcept    { mask = std::uint16_t (mask | (1u << mod12 (pc))); }
    constexpr void remove (int pc) noexcept { mask = std::uint16_t (mask & ~(1u << mod12 (pc))); }

    constexpr bool contains (int pc) const noexcept { return ((mask >> mod12 (pc)) & 1u) != 0; }
    constexpr int size() const noexcept             { return std::popcount (mask); }
    constexpr bool empty() const noexcept           { return mask == 0; }
    constexpr std::uint16_t bits() const noexcept   { return mask; }

    /** Returns this set transposed so that pitch class `root` becomes 0.
        Used to compare a set against chord templates for every root candidate. */
    constexpr PitchClassSet relativeTo (int root) const noexcept
    {
        const int s = mod12 (root);
        if (s == 0)
            return *this;
        return PitchClassSet (std::uint16_t ((mask >> s) | (mask << (12 - s))));
    }

    constexpr PitchClassSet transposed (int semitones) const noexcept { return relativeTo (-semitones); }

    friend constexpr bool operator== (PitchClassSet, PitchClassSet) noexcept = default;

private:
    std::uint16_t mask = 0;
};

} // namespace chordsearch
