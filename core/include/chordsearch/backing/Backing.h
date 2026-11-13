#pragma once

#include "../Chord.h"

#include <cstdint>
#include <vector>

namespace chordsearch::backing
{

enum class Genre : std::uint8_t { swing, bossa, rock, funk, ballad, shuffle };
constexpr int numGenres = 6;

const char* genreName (Genre g) noexcept;        // "Swing"
const char* genreDescription (Genre g) noexcept; // Korean: what each instrument does
double defaultTempo (Genre g) noexcept;

enum class Instrument : std::uint8_t { bass, drums, guitar, piano, click };
constexpr int numInstruments = 5;

/** General-MIDI drum numbers used by the drum synth. */
namespace Drum
{
    constexpr std::uint8_t kick = 36, crossStick = 37, snare = 38, hatClosed = 42, tomLow = 45,
                           hatPedal = 44, hatOpen = 46, tomMid = 47, crash = 49, tomHigh = 50, ride = 51;
}

namespace NoteFlag
{
    constexpr std::uint8_t muted = 1; // guitar: dead/scratch note
}

struct NoteEvent
{
    double start = 0.0;  // beats from sequence start
    double length = 0.0; // beats
    Instrument instrument = Instrument::piano;
    std::uint8_t note = 60;
    float velocity = 0.8f;
    std::uint8_t flags = 0;
};

struct ChartChord
{
    Chord chord;
    int beats = 4;
};

/** A rendered backing track: sorted note events over a looping length. */
struct Sequence
{
    std::vector<NoteEvent> events; // sorted by start
    double lengthBeats = 0.0;      // loop length (a whole number of bars)
    double chartBeats = 0.0;       // length of one pass through the chart
    int beatsPerBar = 4;
    Genre genre = Genre::swing;
};

/** Generates the backing track for a chord chart.

    Each instrument follows a genre-specific playing algorithm:
      Bass   – swing: walking quarter notes with chromatic/dominant approach tones;
               bossa: root–fifth dotted pattern; rock: driving 8ths; funk: syncopated 16ths
               with octave pops; ballad: half notes with approaches; shuffle: boogie line.
      Drums  – swing: ride "spang-a-lang", feathered kick, hi-hat on 2&4, snare comping;
               bossa: cross-stick clave; rock: 8-beat; funk: 16th hats + ghost notes;
               ballad: soft 8ths + cross-stick; shuffle: triplet shuffle. Fills every 4 bars.
      Piano  – swing: rootless voicings with Charleston / anticipation comping;
               bossa: syncopated shells; rock: pumping triads; funk: 16th stabs;
               ballad: sustained block chords; shuffle: swung comp.
      Guitar – swing: Freddie-Green 4-to-the-bar shells; bossa: thumb bass + finger plucks;
               rock: palm-muted power chords; funk: 16th "chicken scratch";
               ballad: arpeggios; shuffle: boogie dyads.
    Voicings are voice-led (minimum movement) within each instrument's register.
    The chart is played `passes` times with different random variations. */
Sequence generateSequence (const std::vector<ChartChord>& chart, Genre genre, std::uint32_t seed = 1, int passes = 0);

/** Voice-leading helper (exposed for tests): places pitch classes (bottom-up order is a
    preference, all rotations are tried) inside [low, high], minimising movement from `previous`. */
std::vector<int> voiceLead (const std::vector<int>& pitchClasses, int low, int high,
                            const std::vector<int>& previous, bool firstIsBottom = false);

} // namespace chordsearch::backing
