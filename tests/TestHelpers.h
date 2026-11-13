#pragma once

#include "chordsearch/ChordRecognizer.h"
#include "chordsearch/MidiNoteSet.h"

#include <initializer_list>
#include <string>

namespace testing
{
    inline chordsearch::MidiNoteSet notes (std::initializer_list<int> midi)
    {
        chordsearch::MidiNoteSet s;
        for (int n : midi)
            s.set (n, true);
        return s;
    }

    /** Best chord name for MIDI notes (lowest note = bass). */
    inline std::string chordName (std::initializer_list<int> midi,
                                  chordsearch::Accidentals acc = chordsearch::Accidentals::flats)
    {
        const auto s = notes (midi);
        const auto c = chordsearch::ChordRecognizer::recognize (s.pitchClasses(), s.lowest() % 12);
        return c.empty() ? std::string() : c.front().chord.name (acc);
    }

    inline chordsearch::Chord chord (int root, const char* suffix)
    {
        return *chordsearch::Chord::fromSuffix (root, suffix);
    }
} // namespace testing
