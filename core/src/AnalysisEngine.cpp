#include "chordsearch/AnalysisEngine.h"

#include <algorithm>

namespace chordsearch
{

AnalysisEngine::AnalysisEngine() = default;

void AnalysisEngine::reset()
{
    keyDetector.reset();
    chordHistory.clear();
    chordBeats.clear();
    recording = true;
    pending.clear();
    committed.clear();
    lastTime = -1.0;
    snap = {};
    ++snapshotVersion;
}

void AnalysisEngine::update (const MidiNoteSet& sounding, double now)
{
    const double dt = lastTime < 0.0 ? 0.0 : std::clamp (now - lastTime, 0.0, 1.0);
    lastTime = now;

    keyDetector.decay (dt);
    if (snap.hasChord && snap.chordSounding)
        keyDetector.accumulate (snap.chord, dt);

    if (sounding != pending)
    {
        pending = sounding;
        pendingSince = now;
    }

    bool changed = false;

    if (now - pendingSince >= stabilizeTime && pending != committed)
    {
        const auto previous = committed;
        committed = pending;

        if (committed.empty())
        {
            if (snap.chordSounding)
            {
                snap.chordSounding = false;
                changed = true;
            }
        }
        else
        {
            const bool onlyReleased = ! previous.empty() && (committed & ~previous).empty();
            if (onlyReleased && snap.chordSounding)
            {
                // Lifting fingers one by one must not produce new chords.
            }
            else if (committed.pitchClasses().size() >= 3)
            {
                commit (committed, now);
                changed = true;
            }
        }
    }

    if (now - lastEvaluate > 0.25)
    {
        lastEvaluate = now;
        if (keyDetector.evaluate (now))
        {
            refresh();
            changed = true;
        }
    }

    if (changed)
        ++snapshotVersion;
}

void AnalysisEngine::commit (const MidiNoteSet& notes, double now)
{
    const auto pcs = notes.pitchClasses();
    const int bass = mod12 (notes.lowest());

    auto candidates = ChordRecognizer::recognize (pcs, bass, 6);
    if (candidates.empty())
        return;

    // Re-rank with key context (e.g. Am7 vs C6, rootless voicings)
    if (const auto key = keyDetector.current())
    {
        for (auto& c : candidates)
            c.score += 0.8f * analyzeHarmony (c.chord, *key).fit;
        std::stable_sort (candidates.begin(), candidates.end(), [] (const auto& a, const auto& b) { return a.score > b.score; });
    }

    const Chord chord = candidates.front().chord;

    if (! recording)
    {
        // Progression is locked: analyse live playing without changing the chart.
        keyDetector.addChordEvent (chord);
    }
    else if (! chordHistory.empty() && chordHistory.back().sameHarmony (chord))
    {
        chordHistory.back() = chord; // same harmony re-voiced / extended
        keyDetector.replaceLastChord (chord);
    }
    else
    {
        chordHistory.push_back (chord);
        chordBeats.push_back (4);
        if (chordHistory.size() > maxHistory)
        {
            chordHistory.erase (chordHistory.begin());
            chordBeats.erase (chordBeats.begin());
        }
        keyDetector.addChordEvent (chord);
    }

    keyDetector.evaluate (now);
    lastEvaluate = now;

    snap.hasChord = true;
    snap.chordSounding = true;
    snap.chord = chord;
    snap.playedPitchClasses = pcs;

    altCandidates.clear();
    for (std::size_t i = 1; i < candidates.size() && altCandidates.size() < 2; ++i)
        if (candidates[i].score > candidates.front().score - 2.5f)
            altCandidates.push_back (candidates[i].chord);

    refresh();
}

void AnalysisEngine::refresh()
{
    const auto key = keyDetector.current();
    const auto acc = key ? key->accidentals() : Accidentals::flats;

    snap.hasKey = key.has_value();
    if (key)
    {
        snap.key = *key;
        snap.keyProbability = keyDetector.currentProbability();
        const auto& r = keyDetector.ranking();
        snap.keyRanking.assign (r.begin(), r.begin() + (std::ptrdiff_t) std::min<std::size_t> (4, r.size()));
        snap.localCenter = keyDetector.localCenter();
    }

    if (snap.hasChord)
    {
        snap.chordName = snap.chord.name (acc);

        snap.alternatives.clear();
        for (const auto& alt : altCandidates)
            snap.alternatives.push_back (alt.name (acc));

        if (key)
            snap.harmony = analyzeHarmony (snap.chord, *key);

        snap.scales = ChordScaleAdvisor::suggest (snap.chord, snap.playedPitchClasses, key ? &*key : nullptr, 4);
        auto context = chordHistory;
        if (! recording && (context.empty() || ! context.back().sameHarmony (snap.chord)))
            context.push_back (snap.chord);
        snap.suggestions = key ? ChordSuggester::suggest (context, *key, 6) : std::vector<Suggestion> {};
    }

    snap.recording = recording;
    snap.history.clear();
    for (std::size_t i = 0; i < chordHistory.size(); ++i)
    {
        const auto& c = chordHistory[i];
        snap.history.push_back ({ c, c.name (acc), key ? analyzeHarmony (c, *key).roman : std::string(), chordBeats[i] });
    }
}

// ---------------------------------------------------------------------------
// Progression editing

void AnalysisEngine::rebuildFromHistory()
{
    keyDetector.reset();
    for (const auto& c : chordHistory)
    {
        keyDetector.addChordEvent (c);
        keyDetector.accumulate (c, 0.5);
    }
    keyDetector.evaluate (lastTime + 1000.0);
    refresh();
    ++snapshotVersion;
}

void AnalysisEngine::removeChord (std::size_t index)
{
    if (index >= chordHistory.size())
        return;
    chordHistory.erase (chordHistory.begin() + (std::ptrdiff_t) index);
    chordBeats.erase (chordBeats.begin() + (std::ptrdiff_t) index);
    rebuildFromHistory();
}

void AnalysisEngine::insertChord (std::size_t index, const Chord& chord, int beats)
{
    index = std::min (index, chordHistory.size());
    Chord c = chord;
    c.root = mod12 (c.root);
    c.bass = mod12 (c.bass);
    chordHistory.insert (chordHistory.begin() + (std::ptrdiff_t) index, c);
    chordBeats.insert (chordBeats.begin() + (std::ptrdiff_t) index, std::clamp (beats, 1, 16));
    rebuildFromHistory();
}

void AnalysisEngine::setChordBeats (std::size_t index, int beats)
{
    if (index >= chordBeats.size())
        return;
    chordBeats[index] = std::clamp (beats, 1, 16);
    refresh();
    ++snapshotVersion;
}

void AnalysisEngine::setRecording (bool shouldRecord)
{
    if (recording == shouldRecord)
        return;
    recording = shouldRecord;
    refresh();
    ++snapshotVersion;
}

std::vector<backing::ChartChord> AnalysisEngine::chart() const
{
    std::vector<backing::ChartChord> out;
    for (std::size_t i = 0; i < chordHistory.size(); ++i)
        out.push_back ({ chordHistory[i], chordBeats[i] });
    return out;
}

std::vector<Chord> AnalysisEngine::historyBefore (std::size_t index) const
{
    index = std::min (index, chordHistory.size());
    return { chordHistory.begin(), chordHistory.begin() + (std::ptrdiff_t) index };
}

} // namespace chordsearch
