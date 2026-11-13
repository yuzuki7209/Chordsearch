#include "chordsearch/NoteNames.h"
#include "chordsearch/PitchClassSet.h"

#include <array>

namespace chordsearch
{

namespace
{
    constexpr std::array<const char*, 12> sharpNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    constexpr std::array<const char*, 12> flatNames  { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
} // namespace

std::string pitchClassName (int pc, Accidentals acc)
{
    const auto i = static_cast<std::size_t> (mod12 (pc));
    return acc == Accidentals::sharps ? sharpNames[i] : flatNames[i];
}

std::string midiNoteName (int note, Accidentals acc)
{
    const int octave = (note >= 0 ? note / 12 : (note - 11) / 12) - 1;
    return pitchClassName (note, acc) + std::to_string (octave);
}

} // namespace chordsearch

namespace chordsearch
{

std::string spellDegree (int rootPc, Accidentals rootAcc, int interval, int degree)
{
    static constexpr char letters[7] = { 'C', 'D', 'E', 'F', 'G', 'A', 'B' };
    static constexpr int naturals[7] = { 0, 2, 4, 5, 7, 9, 11 };

    const auto rootName = pitchClassName (rootPc, rootAcc);
    int rootLetter = 0;
    for (int i = 0; i < 7; ++i)
        if (letters[i] == rootName[0])
            rootLetter = i;

    const int letterIndex = (rootLetter + degree - 1 + 70) % 7;
    const int target = mod12 (rootPc + interval);
    int diff = mod12 (target - naturals[letterIndex]);
    if (diff > 6)
        diff -= 12;

    if (diff < -2 || diff > 2) // unspellable on this letter: fall back
        return pitchClassName (target, rootAcc);

    std::string s (1, letters[letterIndex]);
    if (diff < 0) s += std::string ((std::size_t) -diff, 'b');
    if (diff > 0) s += std::string ((std::size_t) diff, '#');
    return s;
}

} // namespace chordsearch
