#pragma once

#include "Chord.h"
#include "Key.h"

#include <array>
#include <deque>
#include <optional>
#include <vector>

namespace chordsearch
{

struct KeyEstimate
{
    Key key;
    float score = 0.0f;
    float probability = 0.0f;
};

/** Real-time key finding from chord events.

    Evidence for each of the 24 keys:
      1. Pitch-class distribution (time-weighted, decaying) correlated with
         Krumhansl–Kessler key profiles.
      2. Diatonic fit of recent chords (functional analysis per key).
      3. Cadences: V7→I, ii–V, iiø–V, subV→I, backdoor bVII7→I, vii°7→I, IV→I.
    A hysteresis prevents the displayed key from flickering. */
class KeyDetector
{
public:
    void reset();

    /** Called when a new chord is committed. */
    void addChordEvent (const Chord& chord);

    /** Called when the current chord is re-voiced (same harmony): replaces the last event. */
    void replaceLastChord (const Chord& chord);

    /** Called continuously while a chord sounds. */
    void accumulate (const Chord& chord, double seconds);

    /** Exponential forgetting of old pitch content. */
    void decay (double seconds);

    /** Re-scores all keys. Returns true if the current key changed. */
    bool evaluate (double now);

    std::optional<Key> current() const noexcept { return currentKey; }
    float currentProbability() const noexcept;
    const std::vector<KeyEstimate>& ranking() const noexcept { return sorted; }

    /** Temporary tonal centre implied by the latest ii–V / V–I (if different from the key). */
    std::optional<Key> localCenter() const;

    /** Manual override (nullopt = automatic). */
    void setLockedKey (std::optional<Key> k) { locked = k; if (k) currentKey = k; }

    const std::deque<Chord>& recentChords() const noexcept { return recent; }

private:
    void addWeights (const Chord& chord, double amount);

    std::array<double, 12> histogram {};
    std::deque<Chord> recent;
    std::vector<KeyEstimate> sorted;

    std::optional<Key> currentKey, locked;
    std::optional<Key> challenger;
    double challengerSince = 0.0;

    static constexpr std::size_t maxRecent = 12;
};

} // namespace chordsearch
