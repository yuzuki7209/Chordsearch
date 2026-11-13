#pragma once

#include "Chord.h"
#include "ChordRecognizer.h"
#include "ChordSuggester.h"
#include "Harmony.h"
#include "KeyDetector.h"
#include "MidiNoteSet.h"
#include "Scales.h"
#include "backing/Backing.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace chordsearch
{

struct HistoryEntry
{
    Chord chord;
    std::string name;
    std::string roman;
    int beats = 4;
};

struct AnalysisSnapshot
{
    // Current chord
    bool hasChord = false;
    bool chordSounding = false;
    Chord chord;
    std::string chordName;
    std::vector<std::string> alternatives;
    HarmonyAnalysis harmony;
    PitchClassSet playedPitchClasses;

    // Key
    bool hasKey = false;
    Key key;
    float keyProbability = 0.0f;
    std::vector<KeyEstimate> keyRanking; // top 4
    std::optional<Key> localCenter;

    // Chord scales for the current chord
    std::vector<ScaleSuggestion> scales;

    // Next-chord recommendations
    std::vector<Suggestion> suggestions;

    // Progression so far (oldest first)
    std::vector<HistoryEntry> history;
    bool recording = true; // false = progression confirmed (locked)
};

/** Real-time pipeline: notes → chord → key → harmony/scales → suggestions.

    Call update() regularly (e.g. 50–100 Hz) on a non-audio thread with the
    currently sounding notes. A chord is committed once the note set has been
    stable for a short time; releasing notes never creates a new chord. */
class AnalysisEngine
{
public:
    AnalysisEngine();

    void update (const MidiNoteSet& sounding, double nowSeconds);
    void reset();

    const AnalysisSnapshot& snapshot() const noexcept { return snap; }
    std::uint64_t version() const noexcept { return snapshotVersion; }

    void setStabilizeTime (double seconds) noexcept { stabilizeTime = seconds; }

    // --- Progression editing (message thread)
    void removeChord (std::size_t index);
    void insertChord (std::size_t index, const Chord& chord, int beats = 4);
    void setChordBeats (std::size_t index, int beats);

    /** When false the progression is locked: playing still updates the analysis
        but no longer appends chords. */
    void setRecording (bool shouldRecord);
    bool isRecording() const noexcept { return recording; }

    /** The progression as a chart for the backing generator. */
    std::vector<backing::ChartChord> chart() const;

    /** Chords before position `index` (used to suggest chords for an insertion). */
    std::vector<Chord> historyBefore (std::size_t index) const;

private:
    void commit (const MidiNoteSet& notes, double now);
    void refresh();
    void rebuildFromHistory();

    KeyDetector keyDetector;
    std::vector<Chord> chordHistory;
    std::vector<Chord> altCandidates;
    std::vector<int> chordBeats;
    bool recording = true;

    MidiNoteSet pending, committed;
    double pendingSince = 0.0;
    double lastTime = -1.0;
    double lastEvaluate = 0.0;
    double stabilizeTime = 0.06;

    AnalysisSnapshot snap;
    std::uint64_t snapshotVersion = 0;

    static constexpr std::size_t maxHistory = 64;
};

} // namespace chordsearch
