#include "chordsearch/Key.h"

#include <array>

namespace chordsearch
{

namespace
{
    constexpr std::array<const char*, 12> majorNames { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    constexpr std::array<const char*, 12> minorNames { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
} // namespace

std::string Key::name() const
{
    const auto i = static_cast<std::size_t> (mod12 (tonic));
    return mode == Mode::major ? std::string (majorNames[i]) + " Major"
                               : std::string (minorNames[i]) + " Minor";
}

Accidentals Key::accidentals() const noexcept
{
    const int t = mod12 (tonic);
    if (mode == Mode::major)
    {
        switch (t)
        {
            case 7: case 2: case 9: case 4: case 11: case 6: return Accidentals::sharps;
            default: return Accidentals::flats;
        }
    }

    switch (t)
    {
        case 9: case 4: case 11: case 6: case 1: case 8: return Accidentals::sharps;
        default: return Accidentals::flats;
    }
}

PitchClassSet Key::scale() const noexcept
{
    constexpr std::uint16_t ionian = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11);
    constexpr std::uint16_t aeolian = (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10);
    return PitchClassSet (mode == Mode::major ? ionian : aeolian).transposed (tonic);
}

} // namespace chordsearch
