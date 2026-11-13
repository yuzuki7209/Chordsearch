#pragma once

#include <string>

namespace chordsearch
{

enum class Accidentals
{
    sharps,
    flats
};

/** "C", "C#"/"Db", ... for a pitch class 0..11 (any int is wrapped). */
std::string pitchClassName (int pc, Accidentals acc = Accidentals::flats);

/** Scientific pitch name, MIDI 60 = "C4". */
std::string midiNoteName (int note, Accidentals acc = Accidentals::flats);

} // namespace chordsearch

namespace chordsearch
{

/** Letter-correct spelling of the note `interval` semitones above a root, as the given
    scale degree (1..7). E.g. root A, interval 1, degree 2 -> "Bb"; interval 3, degree 2 -> "B#".
    The root itself is spelled with `rootAcc`. */
std::string spellDegree (int rootPc, Accidentals rootAcc, int interval, int degree);

} // namespace chordsearch
