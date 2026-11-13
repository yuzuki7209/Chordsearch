#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace chordsearch;

ChordsearchProcessor::ChordsearchProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    scratchL.assign (4096, 0.0f);
    scratchR.assign (4096, 0.0f);
    player.prepare (48000.0);
    tempo.store (backing::defaultTempo (genre));

    // Analysis runs on the message thread so it keeps following the music
    // even while the editor is closed. The audio thread only publishes note bitsets.
    startTimerHz (60);
}

ChordsearchProcessor::~ChordsearchProcessor()
{
    stopTimer();
}

void ChordsearchProcessor::timerCallback()
{
    engine.update (midiTracker.getSounding(), juce::Time::getMillisecondCounterHiRes() * 0.001);
    collectGarbage();
}

void ChordsearchProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    midiTracker.reset();
    player.prepare (sampleRate);
    const auto size = (std::size_t) juce::jmax (4096, samplesPerBlock);
    scratchL.assign (size, 0.0f);
    scratchR.assign (size, 0.0f);
}

bool ChordsearchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

const backing::Sequence* ChordsearchProcessor::acquireSequence() noexcept
{
    const backing::Sequence* p = nullptr;
    do
    {
        p = activeSequence.load();
        sequenceInUse.store (p);
    } while (p != activeSequence.load());
    return p;
}

void ChordsearchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    midiTracker.process (midi);
    buffer.clear();

    player.setSequence (acquireSequence());

    backing::BackingPlayer::Params params;
    params.playing = playing.load();
    params.bpm = tempo.load();
    params.metronome = metronome.load();
    for (std::size_t i = 0; i < 4; ++i)
        params.enabled[i] = instrumentOn[i].load();

    const int numChannels = buffer.getNumChannels();
    const int total = buffer.getNumSamples();
    const int chunk = (int) scratchL.size();

    for (int start = 0; start < total; start += chunk)
    {
        const int n = juce::jmin (chunk, total - start);
        std::fill (scratchL.begin(), scratchL.begin() + n, 0.0f);
        std::fill (scratchR.begin(), scratchR.begin() + n, 0.0f);
        player.process (scratchL.data(), scratchR.data(), n, params);

        if (numChannels >= 2)
        {
            buffer.copyFrom (0, start, scratchL.data(), n);
            buffer.copyFrom (1, start, scratchR.data(), n);
        }
        else if (numChannels == 1)
        {
            buffer.copyFrom (0, start, scratchL.data(), n, 0.5f);
            buffer.addFrom (0, start, scratchR.data(), n, 0.5f);
        }
    }

    playbackBeat.store (player.isRunning() ? player.positionBeats() : -1.0);
}

// ---------------------------------------------------------------------------
void ChordsearchProcessor::publishSequence (std::unique_ptr<backing::Sequence> s)
{
    const backing::Sequence* raw = s.get();
    if (s)
        ownedSequences.push_back (std::move (s));
    activeSequence.store (raw);
    collectGarbage();
}

void ChordsearchProcessor::collectGarbage()
{
    const auto* inUse = sequenceInUse.load();
    const auto* active = activeSequence.load();
    ownedSequences.erase (std::remove_if (ownedSequences.begin(), ownedSequences.end(),
                                          [&] (const auto& p) { return p.get() != inUse && p.get() != active; }),
                          ownedSequences.end());
}

void ChordsearchProcessor::regenerateBacking()
{
    const auto chart = engine.chart();
    if (chart.empty() || ! isConfirmed())
    {
        chartBeats = 0.0;
        publishSequence (nullptr);
        return;
    }

    auto seq = std::make_unique<backing::Sequence> (backing::generateSequence (chart, genre, ++seed));
    chartBeats = seq->chartBeats;
    publishSequence (std::move (seq));
}

double ChordsearchProcessor::getChartPlayhead() const noexcept
{
    const double b = playbackBeat.load();
    if (b < 0.0 || chartBeats <= 0.0)
        return -1.0;
    return std::fmod (b, chartBeats);
}

void ChordsearchProcessor::resetAnalysis()
{
    setPlaying (false);
    engine.reset();
    regenerateBacking();
}

void ChordsearchProcessor::removeChord (std::size_t index)
{
    engine.removeChord (index);
}

void ChordsearchProcessor::insertChord (std::size_t index, const Chord& chord, int beats)
{
    engine.insertChord (index, chord, beats);
}

void ChordsearchProcessor::confirmProgression (bool confirm)
{
    if (confirm && engine.chart().empty())
        return;

    engine.setRecording (! confirm);
    if (! confirm)
        setPlaying (false);
    regenerateBacking();
}

void ChordsearchProcessor::setPlaying (bool shouldPlay)
{
    playing.store (shouldPlay && isConfirmed() && activeSequence.load() != nullptr);
}

void ChordsearchProcessor::setGenre (backing::Genre g)
{
    if (g == genre)
        return;
    genre = g;
    tempo.store (backing::defaultTempo (g));
    regenerateBacking();
}

void ChordsearchProcessor::setInstrumentEnabled (int index, bool on)
{
    if (index >= 0 && index < 4)
        instrumentOn[(std::size_t) index].store (on);
}

juce::AudioProcessorEditor* ChordsearchProcessor::createEditor()
{
    return new ChordsearchEditor (*this);
}

void ChordsearchProcessor::getStateInformation (juce::MemoryBlock&)
{
    // Settings (key lock, style, progression) will be stored here in M7.
}

void ChordsearchProcessor::setStateInformation (const void*, int)
{
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordsearchProcessor();
}
