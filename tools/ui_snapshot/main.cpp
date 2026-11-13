// Developer tool: renders the editor (and the chord picker) to PNG with a scripted
// progression, and optionally bounces every backing genre to WAV.
//
//   ChordsearchUISnapshot out.png [width height] [--wav <dir>]

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/ChordPicker.h"

#include <vector>

namespace
{
    bool writePng (const juce::Image& image, const juce::File& file)
    {
        file.deleteFile();
        juce::FileOutputStream stream (file);
        return stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream);
    }

    void bounceGenres (const juce::File& dir, const std::vector<chordsearch::backing::ChartChord>& chart)
    {
        using namespace chordsearch::backing;
        dir.createDirectory();
        constexpr double sr = 44100.0;

        for (int g = 0; g < numGenres; ++g)
        {
            const auto genre = (Genre) g;
            const auto seq = generateSequence (chart, genre, 11);
            BackingPlayer player;
            player.prepare (sr);
            player.setSequence (&seq);

            BackingPlayer::Params p;
            p.playing = true;
            p.bpm = defaultTempo (genre);

            const int total = (int) (seq.lengthBeats * 60.0 / p.bpm * sr);
            juce::AudioBuffer<float> buffer (2, total);
            buffer.clear();
            double sumSq = 0.0;
            for (int start = 0; start < total; start += 512)
            {
                const int n = juce::jmin (512, total - start);
                player.process (buffer.getWritePointer (0, start), buffer.getWritePointer (1, start), n, p);
            }
            for (int i = 0; i < total; ++i)
                sumSq += buffer.getSample (0, i) * buffer.getSample (0, i);

            auto file = dir.getChildFile (juce::String (genreName (genre)).replaceCharacter (' ', '_') + ".wav");
            file.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> os (file.createOutputStream().release());
            const auto options = juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (16);
            if (auto writer = wav.createWriterFor (os, options))
                writer->writeFromAudioSampleBuffer (buffer, 0, total);
            std::printf ("%-14s %5.1fs  rms %.3f  peak %.3f  events %zu\n", genreName (genre), total / sr,
                         std::sqrt (sumSq / total), buffer.getMagnitude (0, total), seq.events.size());
        }
    }
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (argv[i]);

    juce::File wavDir;
    if (const int w = args.indexOf ("--wav"); w >= 0 && w + 1 < args.size())
    {
        wavDir = juce::File::getCurrentWorkingDirectory().getChildFile (args[w + 1]);
        args.removeRange (w, 2);
    }

    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (args.isEmpty() ? "chordsearch_ui.png" : args[0]);
    const int width = args.size() > 2 ? args[1].getIntValue() : 1400;
    const int height = args.size() > 2 ? args[2].getIntValue() : 790;

    ChordsearchProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);

    std::vector<int> held;
    auto play = [&] (std::vector<int> notes)
    {
        juce::MidiBuffer midi;
        for (int n : held)  midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        for (int n : notes) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
        processor.processBlock (buffer, midi);
        held = notes;

        for (int i = 0; i < 6; ++i)
        {
            processor.pumpAnalysis();
            juce::Thread::sleep (50);
        }
        processor.pumpAnalysis();
    };

    // Dm9 → G13 → Cmaj9 → A7(b9)  (ii–V–I–VI7 turnaround), then a 2-beat chord inserted
    play ({ 38, 53, 57, 60, 64 });
    play ({ 31, 53, 59, 64, 69 });
    play ({ 36, 52, 59, 62, 67 });
    play ({ 33, 55, 61, 64, 70 });
    processor.insertChord (3, *chordsearch::Chord::fromSuffix (4, "m7"), 2);
    processor.insertChord (4, *chordsearch::Chord::fromSuffix (3, "dim7"), 2);
    processor.pumpAnalysis();

    if (wavDir != juce::File())
        bounceGenres (wavDir, processor.getEngine().chart());

    // Confirm and play ~ 6 beats of the backing band
    processor.confirmProgression (true);
    processor.setPlaying (true);
    const int blocks = (int) (6.0 * 60.0 / processor.getTempo() * 48000.0 / 512.0);
    for (int i = 0; i < blocks; ++i)
    {
        juce::MidiBuffer none;
        processor.processBlock (buffer, none);
    }
    processor.pumpAnalysis();

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (width, height);
    if (auto* e = dynamic_cast<ChordsearchEditor*> (editor.get()))
        e->refreshFromProcessor();

    if (! writePng (editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f), out))
    {
        std::fprintf (stderr, "failed to write %s\n", out.getFullPathName().toRawUTF8());
        return 1;
    }
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());

    // Chord picker popup (rendered on its own)
    {
        const auto& snap = processor.getEngine().snapshot();
        ChordPicker picker (chordsearch::ChordSuggester::suggest (processor.getEngine().historyBefore (2), snap.key, 6),
                            snap.key.accidentals());
        juce::Component frame;
        frame.setOpaque (true);
        frame.setSize (picker.getWidth() + 28, picker.getHeight() + 28);
        frame.addAndMakeVisible (picker);
        picker.setTopLeftPosition (14, 14);

        struct Bg : juce::Component
        {
            void paint (juce::Graphics& g) override { g.fillAll (Theme::panel); }
        } bg;
        bg.setBounds (frame.getLocalBounds());
        frame.addAndMakeVisible (bg);
        bg.toBack();

        const auto pickerFile = out.getSiblingFile (out.getFileNameWithoutExtension() + "_picker.png");
        writePng (frame.createComponentSnapshot (frame.getLocalBounds(), true, 2.0f), pickerFile);
        std::printf ("wrote %s\n", pickerFile.getFullPathName().toRawUTF8());
    }
    return 0;
}
