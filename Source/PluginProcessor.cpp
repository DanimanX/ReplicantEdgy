#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GUI/EdgyTheme.h"

ReplicantEdgyAudioProcessor::ReplicantEdgyAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    cacheParameterPointers();
    presetManager.initialise();

    if (presetManager.getNumPresets() > 0)
    {
        currentPresetIndex.store (0);
        currentPresetName = presetManager.getName (0);
    }
}

ReplicantEdgyAudioProcessor::~ReplicantEdgyAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ReplicantEdgyAudioProcessor::createParameterLayout()
{
    using namespace ReplicantEdgy;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addFloat = [&params] (const juce::String& id, const juce::String& name,
                               juce::NormalisableRange<float> range, float defaultValue,
                               const juce::String& label = {})
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, defaultValue,
            juce::AudioParameterFloatAttributes().withLabel (label)));
    };

    auto addChoice = [&params] (const juce::String& id, const juce::String& name,
                                const juce::StringArray& choices, int defaultIndex)
    {
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id, 1 }, name, choices, defaultIndex));
    };

    auto addBool = [&params] (const juce::String& id, const juce::String& name, bool defaultValue)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { id, 1 }, name, defaultValue));
    };

    // --- oscillators --------------------------------------------------------
    addFloat (ParamID::osc1Freq, "VCO1 Frequency", { 0.0f, 1.0f, 0.001f }, Defaults::osc1Freq);
    addChoice (ParamID::osc1Shape, "VCO1 Shape", { "Triangle", "Square" }, Defaults::osc1Shape);
    addFloat (ParamID::osc1Env, "VCO1 Envelope", { -1.0f, 1.0f, 0.001f }, Defaults::osc1Env);
    addFloat (ParamID::osc1Level, "VCO1 Level", { 0.0f, 1.0f, 0.001f }, Defaults::osc1Level);

    addFloat (ParamID::osc2Freq, "VCO2 Frequency", { 0.0f, 1.0f, 0.001f }, Defaults::osc2Freq);
    addChoice (ParamID::osc2Shape, "VCO2 Shape", { "Saw", "Square" }, Defaults::osc2Shape);
    addFloat (ParamID::osc2Env, "VCO2 Envelope", { -1.0f, 1.0f, 0.001f }, Defaults::osc2Env);
    addFloat (ParamID::osc2Level, "VCO2 Level", { 0.0f, 1.0f, 0.001f }, Defaults::osc2Level);
    addBool (ParamID::hardSync, "Hard Sync", Defaults::hardSync != 0);
    addFloat (ParamID::fmAmount, "1-2 FM Amount", { 0.0f, 1.0f, 0.001f }, Defaults::fmAmount);

    addFloat (ParamID::seqPitchMod, "Seq Pitch Mod", { 0.0f, 1.0f, 0.001f }, Defaults::seqPitchMod);
    addChoice (ParamID::seqPitchTarget, "Seq Pitch Target",
               { "OFF", "OSC2", "OSC1+2" }, Defaults::seqPitchTarget);

    addFloat (ParamID::egDecay, "VCO EG Decay",
              { 0.01f, 6.0f, 0.001f, 0.35f }, Defaults::egDecay, "s");

    // --- noise --------------------------------------------------------------
    addChoice (ParamID::noiseColour, "Noise Colour", { "Pink", "White" }, Defaults::noiseColour);
    addFloat (ParamID::noiseLevel, "Noise / Ext Level", { 0.0f, 1.0f, 0.001f }, Defaults::noiseLevel);

    // --- filter -------------------------------------------------------------
    addChoice (ParamID::vcfMode, "VCF Mode", { "LP", "HP" }, Defaults::vcfMode);
    addFloat (ParamID::cutoff, "Cutoff", { 20.0f, 20000.0f, 0.1f, 0.28f }, Defaults::cutoff, "Hz");
    addFloat (ParamID::resonance, "Resonance", { 0.0f, 1.0f, 0.001f }, Defaults::resonance);
    addFloat (ParamID::vcfDecay, "VCF Decay", { 0.01f, 6.0f, 0.001f, 0.35f }, Defaults::vcfDecay, "s");
    addFloat (ParamID::vcfEnv, "VCF Envelope", { -1.0f, 1.0f, 0.001f }, Defaults::vcfEnv);
    addFloat (ParamID::noiseVcfMod, "Noise / VCF Mod", { 0.0f, 1.0f, 0.001f }, Defaults::noiseVcfMod);

    // --- VCA ----------------------------------------------------------------
    addChoice (ParamID::vcaMode, "VCA Mode", { "Fast", "Slow" }, Defaults::vcaMode);
    addFloat (ParamID::vcaDecay, "VCA Decay", { 0.01f, 6.0f, 0.001f, 0.35f }, Defaults::vcaDecay, "s");
    addFloat (ParamID::volume, "Volume", { -60.0f, 6.0f, 0.1f, 2.0f }, Defaults::volume, "dB");

    // --- sequencer ----------------------------------------------------------
    addFloat (ParamID::tempo, "Tempo", { 20.0f, 300.0f, 0.5f }, Defaults::tempo, "BPM");
    addChoice (ParamID::clockSource, "Clock Source", getClockSourceNames(), Defaults::clockSource);
    addChoice (ParamID::scale, "Clock Scale", getScaleNames(), Defaults::scale);

    for (int i = 0; i < StepSequencer::numSteps; ++i)
    {
        addFloat (ParamID::stepPitch (i), "Step " + juce::String (i + 1) + " Pitch",
                  { -1.0f, 1.0f, 0.001f }, 0.0f);
        addFloat (ParamID::stepVelocity (i), "Step " + juce::String (i + 1) + " Velocity",
                  { 0.0f, 1.0f, 0.001f }, 0.65f);
    }

    // --- UI -----------------------------------------------------------------
    addChoice (ParamID::uiTheme, "UI Theme", ThemeManager::getThemeNames(), Defaults::uiTheme);

    return { params.begin(), params.end() };
}

//==============================================================================
juce::StringArray ReplicantEdgyAudioProcessor::getAllParameterIds()
{
    juce::StringArray ids
    {
        ReplicantEdgy::ParamID::osc1Freq, ReplicantEdgy::ParamID::osc1Shape,
        ReplicantEdgy::ParamID::osc1Env, ReplicantEdgy::ParamID::osc1Level,
        ReplicantEdgy::ParamID::osc2Freq, ReplicantEdgy::ParamID::osc2Shape,
        ReplicantEdgy::ParamID::osc2Env, ReplicantEdgy::ParamID::osc2Level,
        ReplicantEdgy::ParamID::hardSync, ReplicantEdgy::ParamID::fmAmount,
        ReplicantEdgy::ParamID::seqPitchMod, ReplicantEdgy::ParamID::seqPitchTarget,
        ReplicantEdgy::ParamID::egDecay,
        ReplicantEdgy::ParamID::noiseColour, ReplicantEdgy::ParamID::noiseLevel,
        ReplicantEdgy::ParamID::vcfMode, ReplicantEdgy::ParamID::cutoff,
        ReplicantEdgy::ParamID::resonance, ReplicantEdgy::ParamID::vcfDecay,
        ReplicantEdgy::ParamID::vcfEnv, ReplicantEdgy::ParamID::noiseVcfMod,
        ReplicantEdgy::ParamID::vcaMode, ReplicantEdgy::ParamID::vcaDecay,
        ReplicantEdgy::ParamID::volume,
        ReplicantEdgy::ParamID::tempo, ReplicantEdgy::ParamID::clockSource,
        ReplicantEdgy::ParamID::scale,
        ReplicantEdgy::ParamID::uiTheme
    };

    for (int i = 0; i < ReplicantEdgy::StepSequencer::numSteps; ++i)
    {
        ids.add (ReplicantEdgy::ParamID::stepPitch (i));
        ids.add (ReplicantEdgy::ParamID::stepVelocity (i));
    }

    return ids;
}

//==============================================================================
void ReplicantEdgyAudioProcessor::cacheParameterPointers()
{
    using namespace ReplicantEdgy::ParamID;

    pOsc1Freq = apvts.getRawParameterValue (osc1Freq);
    pOsc1Shape = apvts.getRawParameterValue (osc1Shape);
    pOsc1Env = apvts.getRawParameterValue (osc1Env);
    pOsc1Level = apvts.getRawParameterValue (osc1Level);
    pOsc2Freq = apvts.getRawParameterValue (osc2Freq);
    pOsc2Shape = apvts.getRawParameterValue (osc2Shape);
    pOsc2Env = apvts.getRawParameterValue (osc2Env);
    pOsc2Level = apvts.getRawParameterValue (osc2Level);
    pHardSync = apvts.getRawParameterValue (hardSync);
    pFmAmount = apvts.getRawParameterValue (fmAmount);
    pSeqPitchMod = apvts.getRawParameterValue (seqPitchMod);
    pSeqPitchTarget = apvts.getRawParameterValue (seqPitchTarget);
    pEgDecay = apvts.getRawParameterValue (egDecay);
    pNoiseColour = apvts.getRawParameterValue (noiseColour);
    pNoiseLevel = apvts.getRawParameterValue (noiseLevel);
    pVcfMode = apvts.getRawParameterValue (vcfMode);
    pCutoff = apvts.getRawParameterValue (cutoff);
    pResonance = apvts.getRawParameterValue (resonance);
    pVcfDecay = apvts.getRawParameterValue (vcfDecay);
    pVcfEnv = apvts.getRawParameterValue (vcfEnv);
    pNoiseVcfMod = apvts.getRawParameterValue (noiseVcfMod);
    pVcaMode = apvts.getRawParameterValue (vcaMode);
    pVcaDecay = apvts.getRawParameterValue (vcaDecay);
    pVolume = apvts.getRawParameterValue (volume);
    pTempo = apvts.getRawParameterValue (tempo);
    pClockSource = apvts.getRawParameterValue (clockSource);
    pScale = apvts.getRawParameterValue (scale);
}

//==============================================================================
void ReplicantEdgyAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    engine.prepare (sampleRate);
}

void ReplicantEdgyAudioProcessor::releaseResources()
{
    engine.reset();
}

bool ReplicantEdgyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ReplicantEdgyAudioProcessor::updateEngineParams()
{
    ReplicantEdgy::EngineParams params;

    auto value = [] (const std::atomic<float>* pointer, float fallback)
    {
        return pointer != nullptr ? pointer->load() : fallback;
    };

    params.osc1FreqNorm = value (pOsc1Freq, ReplicantEdgy::Defaults::osc1Freq);
    params.osc1Shape = juce::roundToInt (value (pOsc1Shape, 0.0f));
    params.osc1Env = value (pOsc1Env, ReplicantEdgy::Defaults::osc1Env);
    params.osc1Level = value (pOsc1Level, ReplicantEdgy::Defaults::osc1Level);

    params.osc2FreqNorm = value (pOsc2Freq, ReplicantEdgy::Defaults::osc2Freq);
    params.osc2Shape = juce::roundToInt (value (pOsc2Shape, 0.0f));
    params.osc2Env = value (pOsc2Env, ReplicantEdgy::Defaults::osc2Env);
    params.osc2Level = value (pOsc2Level, ReplicantEdgy::Defaults::osc2Level);
    params.hardSync = value (pHardSync, 0.0f) > 0.5f;
    params.fmAmount = value (pFmAmount, 0.0f);

    params.seqPitchMod = value (pSeqPitchMod, ReplicantEdgy::Defaults::seqPitchMod);
    params.seqPitchTarget = juce::roundToInt (value (pSeqPitchTarget, 2.0f));
    params.egDecay = value (pEgDecay, ReplicantEdgy::Defaults::egDecay);

    params.noiseColour = juce::roundToInt (value (pNoiseColour, 0.0f));
    params.noiseLevel = value (pNoiseLevel, 0.0f);

    params.vcfHighpass = value (pVcfMode, 0.0f) > 0.5f;
    params.cutoffHz = value (pCutoff, ReplicantEdgy::Defaults::cutoff);
    params.resonance = value (pResonance, ReplicantEdgy::Defaults::resonance);
    params.vcfDecay = value (pVcfDecay, ReplicantEdgy::Defaults::vcfDecay);
    params.vcfEnv = value (pVcfEnv, ReplicantEdgy::Defaults::vcfEnv);
    params.noiseVcfMod = value (pNoiseVcfMod, 0.0f);

    params.vcaFast = value (pVcaMode, 0.0f) < 0.5f;
    params.vcaDecay = value (pVcaDecay, ReplicantEdgy::Defaults::vcaDecay);
    params.volumeGain = juce::Decibels::decibelsToGain (value (pVolume, ReplicantEdgy::Defaults::volume),
                                                        -60.0f);

    params.tempo = value (pTempo, ReplicantEdgy::Defaults::tempo);
    params.clockSource = juce::roundToInt (value (pClockSource, 0.0f));
    params.scale = juce::roundToInt (value (pScale, 1.0f));

    engine.setParams (params);
}

void ReplicantEdgyAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    updateEngineParams();

    double hostBpm = 0.0;
    bool hostPlaying = false;

    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto bpm = position->getBpm())
                hostBpm = *bpm;

            hostPlaying = position->getIsPlaying();
        }
    }

    engine.setHostState (hostPlaying, hostBpm);
    engine.processBlock (buffer, midi);
}

//==============================================================================
juce::AudioProcessorEditor* ReplicantEdgyAudioProcessor::createEditor()
{
    return new ReplicantEdgyAudioProcessorEditor (*this);
}

//==============================================================================
// Presets
//==============================================================================
void ReplicantEdgyAudioProcessor::setParameterValue (const juce::String& parameterId, float plainValue)
{
    if (auto* parameter = apvts.getParameter (parameterId))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
}

void ReplicantEdgyAudioProcessor::applyPreset (int index)
{
    if (presetManager.getNumPresets() == 0)
        return;

    const int clamped = juce::jlimit (0, presetManager.getNumPresets() - 1, index);
    const auto preset = presetManager.getPreset (clamped);

    for (const auto& entry : preset.values)
        setParameterValue (entry.first, entry.second);

    engine.getPatchBay().setCables (preset.cables);

    for (int i = 0; i < ReplicantEdgy::StepSequencer::numSteps; ++i)
        engine.getSequencer().setStep (i, preset.pitch[(size_t) i], preset.velocity[(size_t) i]);

    currentPresetIndex.store (clamped);
    currentPresetName = preset.name;
}

void ReplicantEdgyAudioProcessor::nextPreset()
{
    applyPreset ((getCurrentPresetIndex() + 1) % juce::jmax (1, presetManager.getNumPresets()));
}

void ReplicantEdgyAudioProcessor::previousPreset()
{
    const int count = juce::jmax (1, presetManager.getNumPresets());
    applyPreset ((getCurrentPresetIndex() + count - 1) % count);
}

juce::String ReplicantEdgyAudioProcessor::getCurrentPresetName() const
{
    return currentPresetName;
}

ReplicantEdgy::Preset ReplicantEdgyAudioProcessor::captureCurrentPreset (const juce::String& name,
                                                                         const juce::String& category) const
{
    ReplicantEdgy::Preset preset;
    preset.name = name;
    preset.category = category;

    for (const auto& id : getAllParameterIds())
    {
        // The skin is a user preference, not part of the sound: loading a
        // preset must never repaint the panel.
        if (id == ReplicantEdgy::ParamID::uiTheme)
            continue;

        if (auto* value = apvts.getRawParameterValue (id))
            preset.values[id] = value->load();
    }

    const auto& sequencer = engine.getSequencer();

    for (int i = 0; i < ReplicantEdgy::StepSequencer::numSteps; ++i)
    {
        preset.pitch[(size_t) i] = sequencer.getPitch (i);
        preset.velocity[(size_t) i] = sequencer.getVelocity (i);
        preset.values[ReplicantEdgy::ParamID::stepPitch (i)] = preset.pitch[(size_t) i];
        preset.values[ReplicantEdgy::ParamID::stepVelocity (i)] = preset.velocity[(size_t) i];
    }

    preset.cables = engine.getPatchBay().getCables();
    return preset;
}

int ReplicantEdgyAudioProcessor::saveUserPreset (const juce::String& name)
{
    auto preset = captureCurrentPreset (name, "User");
    const int index = presetManager.saveUserPreset (preset);

    if (index >= 0)
    {
        currentPresetIndex.store (index);
        currentPresetName = presetManager.getName (index);
    }

    return index;
}

bool ReplicantEdgyAudioProcessor::deleteUserPreset (int index)
{
    if (! presetManager.isUserPreset (index))
        return false;

    const bool removed = presetManager.deleteUserPreset (index);
    applyPreset (juce::jlimit (0, juce::jmax (0, presetManager.getNumPresets() - 1),
                               index - 1));
    return removed;
}

bool ReplicantEdgyAudioProcessor::deleteCurrentUserPreset()
{
    return deleteUserPreset (getCurrentPresetIndex());
}

bool ReplicantEdgyAudioProcessor::exportPreset (int index, const juce::File& file) const
{
    // Export what is on screen, not the stored factory version: the user
    // expects EXPORT to capture their current tweaks.
    auto preset = captureCurrentPreset (presetManager.getName (index),
                                        presetManager.getCategory (index));
    auto xml = preset.toValueTree().createXml();

    if (xml == nullptr)
        return false;

    file.getParentDirectory().createDirectory();
    return xml->writeTo (file);
}

bool ReplicantEdgyAudioProcessor::importPreset (const juce::File& file)
{
    ReplicantEdgy::Preset imported;

    if (! presetManager.importPreset (file, imported))
        return false;

    if (imported.name.isEmpty())
        imported.name = file.getFileNameWithoutExtension();

    const int index = presetManager.saveUserPreset (imported);

    if (index < 0)
        return false;

    applyPreset (index);
    return true;
}

//==============================================================================
// Host program interface
//==============================================================================
int ReplicantEdgyAudioProcessor::getNumPrograms()
{
    return juce::jmax (1, presetManager.getNumPresets());
}

int ReplicantEdgyAudioProcessor::getCurrentProgram()
{
    return getCurrentPresetIndex();
}

void ReplicantEdgyAudioProcessor::setCurrentProgram (int index)
{
    applyPreset (index);
}

const juce::String ReplicantEdgyAudioProcessor::getProgramName (int index)
{
    if (presetManager.getNumPresets() == 0)
        return "Init";

    return presetManager.getName (juce::jlimit (0, presetManager.getNumPresets() - 1, index));
}

void ReplicantEdgyAudioProcessor::changeProgramName (int, const juce::String&)
{
    // User presets are renamed through the browser, not the host program list.
}

//==============================================================================
void ReplicantEdgyAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("PATCH"), nullptr);
    state.appendChild (engine.getPatchBay().toValueTree(), nullptr);
    state.setProperty ("presetIndex", getCurrentPresetIndex(), nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("stateVersion", 1, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ReplicantEdgyAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    engine.getPatchBay().restoreFromValueTree (tree.getChildWithName ("PATCH"));

    const int presetIndex = (int) tree.getProperty ("presetIndex", 0);
    currentPresetIndex.store (juce::jlimit (0, juce::jmax (0, presetManager.getNumPresets() - 1),
                                            presetIndex));
    currentPresetName = tree.getProperty ("presetName", "Init").toString();

    auto parameterState = tree.createCopy();
    parameterState.removeChild (parameterState.getChildWithName ("PATCH"), nullptr);
    apvts.replaceState (parameterState);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReplicantEdgyAudioProcessor();
}
