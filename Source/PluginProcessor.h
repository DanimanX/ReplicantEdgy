#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

#include "DSP/SynthEngine.h"
#include "Presets/PresetManager.h"

class ReplicantEdgyAudioProcessor : public juce::AudioProcessor
{
public:
    ReplicantEdgyAudioProcessor();
    ~ReplicantEdgyAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

#ifndef JucePlugin_Name
 #define JucePlugin_Name "ReplicantEdgy"
#endif

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    // The host program list is the preset browser: factory bank + user bank.
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static juce::StringArray getAllParameterIds();

    ReplicantEdgy::SynthEngine& getEngine() noexcept { return engine; }
    const ReplicantEdgy::SynthEngine& getEngine() const noexcept { return engine; }

    ReplicantEdgy::PresetManager& getPresetManager() noexcept { return presetManager; }
    const ReplicantEdgy::PresetManager& getPresetManager() const noexcept { return presetManager; }

    //==========================================================================
    // Preset workflow
    void applyPreset (int index);

    void nextPreset();
    void previousPreset();

    int getCurrentPresetIndex() const noexcept { return currentPresetIndex.load(); }
    juce::String getCurrentPresetName() const;

    /** Editor zoom, 0.5 .. 3.0. UI state: saved with the plugin state so a
        reopened instance keeps the size the user chose. */
    float getUiZoom() const noexcept { return uiZoom.load(); }
    void setUiZoom (float newZoom) noexcept
    {
        uiZoom.store (juce::jlimit (0.5f, 3.0f, newZoom));
    }

    /** Captures the panel, the eight steps and the cables into a preset. */
    ReplicantEdgy::Preset captureCurrentPreset (const juce::String& name,
                                                const juce::String& category) const;

    /** Saves the live patch as a user preset. Returns its browser index or -1. */
    int saveUserPreset (const juce::String& name);
    bool deleteUserPreset (int index);
    bool deleteCurrentUserPreset();

    bool exportPreset (int index, const juce::File& file) const;
    bool importPreset (const juce::File& file);

    /** Plain (not normalised) parameter write, used by presets and tests. */
    void setParameterValue (const juce::String& parameterId, float plainValue);

    /** Same, wrapped in a begin/end change gesture so hosts record it. */
    void setParameterValueAsGesture (const juce::String& parameterId, float plainValue);

    /** Builds a fresh cyberpunk patch: random voice, random line and a couple
        of random cables. Shared by the header RANDOM button and the archive. */
    void randomisePatch();

private:
    ReplicantEdgy::SynthEngine engine;
    ReplicantEdgy::PresetManager presetManager;

    std::atomic<int> currentPresetIndex { 0 };
    juce::String currentPresetName { "Init" };

    std::atomic<float> uiZoom { 1.0f };

    // Latched host transport. A host may omit timing information on some
    // callbacks; the last known state is what the sequencer must keep using.
    std::atomic<double> lastHostBpm { 0.0 };
    std::atomic<bool> lastHostPlaying { false };

    std::atomic<float>* pOsc1Freq = nullptr;
    std::atomic<float>* pOsc1Shape = nullptr;
    std::atomic<float>* pOsc1Env = nullptr;
    std::atomic<float>* pOsc1Level = nullptr;
    std::atomic<float>* pOsc2Freq = nullptr;
    std::atomic<float>* pOsc2Shape = nullptr;
    std::atomic<float>* pOsc2Env = nullptr;
    std::atomic<float>* pOsc2Level = nullptr;
    std::atomic<float>* pHardSync = nullptr;
    std::atomic<float>* pFmAmount = nullptr;
    std::atomic<float>* pSeqPitchMod = nullptr;
    std::atomic<float>* pSeqPitchTarget = nullptr;
    std::atomic<float>* pEgDecay = nullptr;
    std::atomic<float>* pNoiseColour = nullptr;
    std::atomic<float>* pNoiseLevel = nullptr;
    std::atomic<float>* pVcfMode = nullptr;
    std::atomic<float>* pCutoff = nullptr;
    std::atomic<float>* pResonance = nullptr;
    std::atomic<float>* pVcfDecay = nullptr;
    std::atomic<float>* pVcfEnv = nullptr;
    std::atomic<float>* pNoiseVcfMod = nullptr;
    std::atomic<float>* pVcaMode = nullptr;
    std::atomic<float>* pVcaDecay = nullptr;
    std::atomic<float>* pVolume = nullptr;
    std::atomic<float>* pTempo = nullptr;
    std::atomic<float>* pClockSource = nullptr;
    std::atomic<float>* pScale = nullptr;

    std::array<std::atomic<float>*, ReplicantEdgy::StepSequencer::numSteps> pStepPitch {};
    std::array<std::atomic<float>*, ReplicantEdgy::StepSequencer::numSteps> pStepVelocity {};

    void cacheParameterPointers();
    void updateEngineParams();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReplicantEdgyAudioProcessor)
};
