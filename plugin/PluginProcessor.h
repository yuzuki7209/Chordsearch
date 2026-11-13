#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "MidiInputTracker.h"
#include "chordsearch/AnalysisEngine.h"
#include "chordsearch/backing/Synth.h"

#include <array>
#include <atomic>
#include <memory>
#include <vector>

class ChordsearchProcessor final : public juce::AudioProcessor,
                                   private juce::Timer
{
public:
    ChordsearchProcessor();
    ~ChordsearchProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    const MidiInputTracker& getMidiTracker() const noexcept { return midiTracker; }

    /** Runs one analysis step immediately (used by tools/tests; normally driven by the timer). */
    void pumpAnalysis() { timerCallback(); }

    // ---- Analysis (message thread)
    const chordsearch::AnalysisEngine& getEngine() const noexcept { return engine; }
    void resetAnalysis();

    // ---- Progression editing (message thread)
    void removeChord (std::size_t index);
    void insertChord (std::size_t index, const chordsearch::Chord& chord, int beats);
    void confirmProgression (bool confirm);
    bool isConfirmed() const noexcept { return ! engine.isRecording(); }

    // ---- Backing band (message thread setters, thread-safe)
    void setPlaying (bool shouldPlay);
    bool isPlaying() const noexcept { return playing.load(); }
    void setTempo (double bpm) { tempo.store (juce::jlimit (40.0, 300.0, bpm)); }
    double getTempo() const noexcept { return tempo.load(); }
    void setGenre (chordsearch::backing::Genre g);
    chordsearch::backing::Genre getGenre() const noexcept { return genre; }
    void setMetronome (bool on) { metronome.store (on); }
    bool getMetronome() const noexcept { return metronome.load(); }
    void setInstrumentEnabled (int index, bool on);
    bool isInstrumentEnabled (int index) const noexcept { return instrumentOn[(std::size_t) index].load(); }

    /** Beat position inside one pass of the chart (-1 when stopped). */
    double getChartPlayhead() const noexcept;

private:
    void timerCallback() override;
    void regenerateBacking();
    void publishSequence (std::unique_ptr<chordsearch::backing::Sequence> s);
    void collectGarbage();
    const chordsearch::backing::Sequence* acquireSequence() noexcept;

    MidiInputTracker midiTracker;
    chordsearch::AnalysisEngine engine; // owned by the message thread

    // Backing band
    chordsearch::backing::BackingPlayer player; // audio thread
    std::vector<float> scratchL, scratchR;
    chordsearch::backing::Genre genre = chordsearch::backing::Genre::swing;
    std::uint32_t seed = 1;
    double chartBeats = 0.0;

    std::atomic<bool> playing { false }, metronome { false };
    std::atomic<double> tempo { 160.0 };
    std::array<std::atomic<bool>, 4> instrumentOn { true, true, true, true };
    std::atomic<double> playbackBeat { -1.0 };

    // Sequence hand-off (hazard pointer): message thread owns, audio thread borrows
    std::vector<std::unique_ptr<chordsearch::backing::Sequence>> ownedSequences;
    std::atomic<const chordsearch::backing::Sequence*> activeSequence { nullptr };
    std::atomic<const chordsearch::backing::Sequence*> sequenceInUse { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordsearchProcessor)
};
