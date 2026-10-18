//==============================================================================
// Renders the ReplicantEdgy demo track through the real plugin DSP.
//
//   render_audio_demo [output.wav]
//
// The plugin runs standalone: the internal sequencer drives the voice while the
// arrangement steps through the factory bank and sweeps the filter, so the file
// is exactly what the instrument sounds like.
//==============================================================================

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

#include <iostream>
#include <vector>

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;
    constexpr double kDuration = 74.0;
    constexpr float kTempo = 132.0f;

    struct Section
    {
        double start;
        int preset;
        float cutoffScale;
        float volumeDb;
    };

    // Arrangement: intro, percussion, acid, lead, noise, hard, outro.
    const Section sections[] =
    {
        {  0.0, 0,  0.55f, -10.0f },   // Neon Pulse, filtered intro
        {  6.0, 0,  1.00f,  -4.0f },   // full percussion
        { 18.0, 5,  0.75f,  -4.0f },   // Acid Protocol
        { 28.0, 7,  1.00f,  -5.0f },   // Chrome Sub
        { 38.0, 10, 1.00f,  -6.0f },   // Ghost Wire
        { 46.0, 13, 1.00f,  -5.0f },   // Static Rain
        { 52.0, 22, 1.00f,  -7.0f },   // Feedback Grid
        { 58.0, 3,  0.90f,  -5.0f },   // Voltage Drop
        { 65.0, 0,  0.60f, -12.0f }    // tail
    };

    void setParam (ReplicantEdgyAudioProcessor& processor, const juce::String& id, float value)
    {
        processor.setParameterValue (id, value);
    }
}

int main (int argc, char* argv[])
{
    const juce::File output = (argc > 1)
        ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
        : juce::File::getCurrentWorkingDirectory().getChildFile (
              "assets/replicantedgy_demo.wav");

    juce::ScopedJuceInitialiser_GUI gui;

    ReplicantEdgyAudioProcessor processor;
    processor.prepareToPlay (kSampleRate, kBlockSize);

    auto& sequencer = processor.getEngine().getSequencer();
    sequencer.setPlaying (true);

    processor.applyPreset (sections[0].preset);
    setParam (processor, ReplicantEdgy::ParamID::tempo, kTempo);

    const int totalBlocks = (int) std::ceil (kDuration * kSampleRate / kBlockSize);
    std::vector<float> left ((size_t) totalBlocks * kBlockSize, 0.0f);
    std::vector<float> right (left.size(), 0.0f);

    int sectionIndex = 0;
    float baseCutoff = 900.0f;

    for (int block = 0; block < totalBlocks; ++block)
    {
        const double t = block * kBlockSize / kSampleRate;

        while (sectionIndex + 1 < (int) (sizeof (sections) / sizeof (sections[0]))
               && t >= sections[sectionIndex + 1].start)
        {
            ++sectionIndex;
            const auto& section = sections[sectionIndex];
            processor.applyPreset (section.preset);
            setParam (processor, ReplicantEdgy::ParamID::tempo, kTempo);
            setParam (processor, ReplicantEdgy::ParamID::volume, section.volumeDb);
            baseCutoff = processor.apvts.getRawParameterValue (
                             ReplicantEdgy::ParamID::cutoff)->load();
        }

        const auto& section = sections[sectionIndex];

        // Slow filter movement so the loop never sits still.
        const float movement = 1.0f + 0.35f * std::sin ((float) t * 0.55f);
        setParam (processor, ReplicantEdgy::ParamID::cutoff,
                  juce::jlimit (30.0f, 12000.0f,
                                baseCutoff * section.cutoffScale * movement));

        // Accent the last bar of every section with a little more decay.
        const float phase = (float) std::fmod (t, 10.0);
        setParam (processor, ReplicantEdgy::ParamID::vcaDecay,
                  phase > 9.0f ? 0.55f : 0.32f);

        juce::AudioBuffer<float> buffer (2, kBlockSize);
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
        {
            auto* destination = (ch == 0 ? left.data() : right.data());
            const auto* source = buffer.getReadPointer (ch);
            std::copy (source, source + kBlockSize,
                       destination + (size_t) block * kBlockSize);
        }
    }

    // Normalise to -1 dBFS.
    float peak = 0.0f;

    for (float value : left)
        peak = juce::jmax (peak, std::abs (value));

    for (float value : right)
        peak = juce::jmax (peak, std::abs (value));

    const float gain = peak > 0.0f ? 0.891f / peak : 1.0f;

    juce::AudioBuffer<float> song (2, (int) left.size());
    std::copy (left.begin(), left.end(), song.getWritePointer (0));
    std::copy (right.begin(), right.end(), song.getWritePointer (1));
    song.applyGain (gain);

    // Short fades so the demo does not click.
    song.applyGainRamp (0, (int) (0.25 * kSampleRate), 0.0f, 1.0f);
    song.applyGainRamp (song.getNumSamples() - (int) (1.5 * kSampleRate),
                        (int) (1.5 * kSampleRate), 1.0f, 0.0f);

    juce::WavAudioFormat wav;
    output.getParentDirectory().createDirectory();
    output.deleteFile();

    std::unique_ptr<juce::FileOutputStream> stream (output.createOutputStream());

    if (stream == nullptr)
    {
        std::cerr << "Cannot write " << output.getFullPathName().toRawUTF8() << std::endl;
        return 1;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (stream.get(), kSampleRate, 2, 24, {}, 0));

    if (writer == nullptr)
    {
        std::cerr << "Cannot create the WAV writer" << std::endl;
        return 1;
    }

    stream.release();
    writer->writeFromAudioSampleBuffer (song, 0, song.getNumSamples());
    writer.reset();

    std::cout << "[SUCCESS] Wrote " << (kDuration) << " s to "
              << output.getFullPathName().toRawUTF8()
              << " (peak " << peak << " -> normalised)" << std::endl;
    return 0;
}
