//==============================================================================
// ReplicantEdgy headless regression suite.
//
// Covers the DSP voice (oscillators, noise, ladder filter, envelopes), the
// 8-step sequencer and its clock modes, the 24-point patch bay, the preset
// system (factory, user, import/export) and the editor / drawer wiring.
//==============================================================================

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GUI/TempoDisplay.h"
#include "DSP/StepSequencer.h"
#include "DSP/SynthEngine.h"
#include "Presets/PresetManager.h"

#include <iostream>
#include <vector>

namespace
{
    int checks = 0;
    int failures = 0;

    void check (bool condition, const juce::String& what)
    {
        ++checks;

        if (condition)
            std::cout << "  [ok]   " << what << std::endl;
        else
        {
            ++failures;
            std::cout << "  [FAIL] " << what << std::endl;
        }
    }

    void section (const char* name)
    {
        std::cout << "\n== " << name << " ==" << std::endl;
    }

    struct Rendered
    {
        float peak = 0.0f;
        double energy = 0.0;
        bool finite = true;
        int firstSoundSample = -1;
    };

    Rendered render (ReplicantEdgy::SynthEngine& engine, int blocks, int blockSize = 256,
                     float* capture = nullptr, int captureCapacity = 0)
    {
        Rendered result;
        int sampleIndex = 0;

        for (int b = 0; b < blocks; ++b)
        {
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            engine.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
            {
                const auto* data = buffer.getReadPointer (ch);

                for (int i = 0; i < blockSize; ++i)
                {
                    const float value = data[i];
                    result.finite = result.finite && std::isfinite (value);
                    result.peak = juce::jmax (result.peak, std::abs (value));
                    result.energy += (double) value * value;

                    if (result.firstSoundSample < 0 && std::abs (value) > 0.01f)
                        result.firstSoundSample = sampleIndex;

                    if (capture != nullptr && sampleIndex < captureCapacity)
                        capture[sampleIndex] = value;

                    ++sampleIndex;
                }
            }
        }

        return result;
    }

    ReplicantEdgy::EngineParams makeParams()
    {
        ReplicantEdgy::EngineParams params;
        params.osc1FreqNorm = 0.40f;
        params.osc1Level = 0.9f;
        params.osc2Level = 0.5f;
        params.cutoffHz = 2000.0f;
        params.resonance = 0.2f;
        params.egDecay = 0.25f;
        params.vcfDecay = 0.3f;
        params.vcaDecay = 0.3f;
        params.seqPitchMod = 0.5f;
        params.tempo = 120.0f;
        params.scale = 1; // 1/16
        params.volumeGain = 0.8f;
        return params;
    }

    //==========================================================================
    void testOscillators()
    {
        section ("Oscillators & noise");

        ReplicantEdgy::OscillatorEdgy osc;
        osc.prepare (48000.0);
        osc.setFrequency (220.0f);

        for (int shape = 0; shape < 3; ++shape)
        {
            osc.setShape ((ReplicantEdgy::OscillatorEdgy::Shape) shape);
            osc.reset();

            float peak = 0.0f;
            bool finite = true;
            double sum = 0.0;

            for (int i = 0; i < 48000; ++i)
            {
                const float value = osc.process();
                finite = finite && std::isfinite (value);
                peak = juce::jmax (peak, std::abs (value));
                sum += value;
            }

            check (finite && peak > 0.5f && peak < 3.0f,
                   "oscillator shape " + juce::String (shape) + " is finite and audible");
            check (std::abs (sum / 48000.0) < 0.25,
                   "oscillator shape " + juce::String (shape) + " has no DC offset");
        }

        // Frequency: count zero crossings on the saw.
        osc.setShape (ReplicantEdgy::OscillatorEdgy::Shape::Saw);
        osc.setFrequency (100.0f);
        osc.reset();

        int crossings = 0;
        float previous = osc.process();

        for (int i = 1; i < 48000; ++i)
        {
            const float value = osc.process();

            if (previous < 0.0f && value >= 0.0f)
                ++crossings;

            previous = value;
        }

        check (std::abs (crossings - 100) <= 2,
               "100 Hz saw crosses zero ~100 times per second (got " + juce::String (crossings) + ")");

        // Noise: pink is quieter and smoother than white.
        ReplicantEdgy::NoiseEdgy noise;
        noise.prepare (48000.0);
        noise.setColour (ReplicantEdgy::NoiseEdgy::Colour::White);

        float whitePeak = 0.0f;

        for (int i = 0; i < 24000; ++i)
            whitePeak = juce::jmax (whitePeak, std::abs (noise.process()));

        noise.setColour (ReplicantEdgy::NoiseEdgy::Colour::Pink);
        noise.reset();

        float pinkPeak = 0.0f;

        for (int i = 0; i < 24000; ++i)
            pinkPeak = juce::jmax (pinkPeak, std::abs (noise.process()));

        check (whitePeak > 0.5f, "white noise is audible");
        check (pinkPeak > 0.01f && pinkPeak <= 1.0f, "pink noise is bounded and audible");
    }

    //==========================================================================
    void testFilter()
    {
        section ("Ladder filter");

        auto response = [] (bool highpass, float cutoff, float inputHz)
        {
            ReplicantEdgy::LadderFilter filter;
            filter.prepare (48000.0);
            filter.setCutoff (cutoff);
            filter.setResonance (0.1f);
            filter.setHighpass (highpass);

            // Warm-up, then RMS of the second half.
            double energy = 0.0;
            const int total = 24000;

            for (int i = 0; i < total; ++i)
            {
                const float input = std::sin (juce::MathConstants<float>::twoPi
                                              * inputHz * (float) i / 48000.0f);
                const float out = filter.process (input);

                if (i > total / 2)
                    energy += (double) out * out;
            }

            return std::sqrt (energy / (total / 2));
        };

        const float lpLow = response (false, 500.0f, 100.0f);
        const float lpHigh = response (false, 500.0f, 8000.0f);
        const float hpLow = response (true, 1000.0f, 100.0f);
        const float hpHigh = response (true, 1000.0f, 8000.0f);

        check (lpLow > lpHigh * 4.0f, "low-pass keeps lows and rejects highs");
        check (hpHigh > hpLow * 4.0f, "high-pass keeps highs and rejects lows");

        // Stability at maximum resonance.
        ReplicantEdgy::LadderFilter hot;
        hot.prepare (48000.0);
        hot.setCutoff (900.0f);
        hot.setResonance (1.0f);

        bool finite = true;
        float peak = 0.0f;

        for (int i = 0; i < 48000; ++i)
        {
            const float value = hot.process (std::sin (0.01f * (float) i)
                                             + (i % 1000 == 0 ? 1.0f : 0.0f));
            finite = finite && std::isfinite (value);
            peak = juce::jmax (peak, std::abs (value));
        }

        check (finite && peak < 8.0f, "the ladder stays finite at full resonance");
    }

    //==========================================================================
    void testEnvelope()
    {
        section ("Decay envelopes");

        ReplicantEdgy::DecayEnvelope envelope;
        envelope.prepare (48000.0);
        envelope.setDecay (0.2f);
        envelope.trigger (1.0f);

        check (envelope.getValue() > 0.99f, "trigger jumps the envelope to the velocity");

        float previous = envelope.getValue();

        for (int i = 0; i < 2400; ++i)
            envelope.process();

        check (envelope.getValue() < previous, "the envelope decays");

        // Time to -60 dB should be close to the decay knob.
        int samples = 0;

        while (envelope.isActive() && samples < 48000)
        {
            envelope.process();
            ++samples;
        }

        const float seconds = (float) samples / 48000.0f;
        check (std::abs (seconds - 0.2f) < 0.09f,
               "decay time tracks the knob (got " + juce::String (seconds, 3) + " s)");

        // FAST mode is shorter than SLOW for the same decay.
        ReplicantEdgy::DecayEnvelope fast;
        fast.prepare (48000.0);
        fast.setDecay (0.4f);
        fast.setFastMode (true);
        fast.trigger (1.0f);

        int fastSamples = 0;

        while (fast.isActive() && fastSamples < 96000)
        {
            fast.process();
            ++fastSamples;
        }

        ReplicantEdgy::DecayEnvelope slow;
        slow.prepare (48000.0);
        slow.setDecay (0.4f);
        slow.setFastMode (false);
        slow.trigger (1.0f);

        int slowSamples = 0;

        while (slow.isActive() && slowSamples < 96000)
        {
            slow.process();
            ++slowSamples;
        }

        check (fastSamples < slowSamples, "VCA FAST mode decays faster than SLOW");
    }

    //==========================================================================
    void testSequencer()
    {
        section ("Step sequencer");

        ReplicantEdgy::StepSequencer sequencer;
        sequencer.prepare (48000.0);
        sequencer.setTempo (120.0f);
        sequencer.setScale (ReplicantEdgy::ClockScale::Div16);
        sequencer.setPlaying (true);

        for (int i = 0; i < ReplicantEdgy::StepSequencer::numSteps; ++i)
            sequencer.setStep (i, (float) i / 7.0f, 0.5f + 0.05f * (float) i);

        // 120 BPM, 1/16: 6000 samples per step.
        int advancedAt[4] = { -1, -1, -1, -1 };
        int advances = 0;

        for (int i = 0; i < 24000 && advances < 4; ++i)
        {
            const auto event = sequencer.process (0.0f, 0.0f, 0.0f);

            if (event.advanced)
                advancedAt[advances++] = i;
        }

        check (advancedAt[0] == 6000, "first 1/16 step lands at sample 6000 (got "
              + juce::String (advancedAt[0]) + ")");
        check (advancedAt[1] == 12000, "second step at 12000");
        check (advancedAt[2] == 18000, "third step at 18000");
        check (sequencer.getCurrentStep() == 3, "telemetry follows the step index");

        // Stop freezes the clock.
        sequencer.setPlaying (false);
        int stepAfterStop = sequencer.getCurrentStep();
        int advancesWhileStopped = 0;

        for (int i = 0; i < 24000; ++i)
            if (sequencer.process (0.0f, 0.0f, 0.0f).advanced)
                ++advancesWhileStopped;

        check (advancesWhileStopped == 0, "stopped clock does not advance");
        check (sequencer.getCurrentStep() == stepAfterStop, "stopped clock keeps its step");

        // Scale: 1/4 at 120 BPM = 24000 samples.
        ReplicantEdgy::StepSequencer slow;
        slow.prepare (48000.0);
        slow.setTempo (120.0f);
        slow.setScale (ReplicantEdgy::ClockScale::Div4);
        slow.setPlaying (true);

        int slowAdvance = -1;

        for (int i = 0; i < 30000 && slowAdvance < 0; ++i)
            if (slow.process (0.0f, 0.0f, 0.0f).advanced)
                slowAdvance = i;

        check (slowAdvance == 24000, "1/4 scale steps every 24000 samples at 120 BPM");

        // External clock: a rising edge on ADV/CLOCK advances.
        ReplicantEdgy::StepSequencer external;
        external.prepare (48000.0);
        external.setClockSource (ReplicantEdgy::ClockSource::External);
        external.setPlaying (true);
        const int before = external.getCurrentStep();
        bool clockEdgeFired = false;

        for (int i = 0; i < 1000; ++i)
        {
            const float gate = (i >= 10 && i < 20) ? 1.0f : 0.0f;

            if (external.process (0.0f, 0.0f, gate).advanced)
                clockEdgeFired = true;
        }

        check (clockEdgeFired && external.getCurrentStep() == (before + 1) % 8,
               "a pulse on ADV/CLOCK advances one step");

        // Manual TRIGGER fires without moving the step; ADVANCE moves.
        ReplicantEdgy::StepSequencer manual;
        manual.prepare (48000.0);
        const int manualStart = manual.getCurrentStep();
        manual.triggerNow();
        auto triggerEvent = manual.process (0.0f, 0.0f, 0.0f);
        check (triggerEvent.triggered && ! triggerEvent.advanced
                   && manual.getCurrentStep() == manualStart,
               "TRIGGER fires the envelopes without advancing");

        manual.advanceNow();
        auto advanceEvent = manual.process (0.0f, 0.0f, 0.0f);
        check (advanceEvent.advanced && advanceEvent.triggered
                   && manual.getCurrentStep() == (manualStart + 1) % 8,
               "ADVANCE moves exactly one step");

        // PLAY/STOP gate.
        ReplicantEdgy::StepSequencer gated;
        gated.prepare (48000.0);
        gated.setPlaying (false);
        bool started = false;

        for (int i = 0; i < 6000; ++i)
        {
            gated.process (0.0f, 1.0f, 0.0f);

            if (gated.isPlaying())
                started = true;
        }

        check (started, "a gate on PLAY/STOP starts the transport");

        // Host sync (MIDI clock source).
        ReplicantEdgy::StepSequencer host;
        host.prepare (48000.0);
        host.setClockSource (ReplicantEdgy::ClockSource::Midi);
        host.setPlaying (true);
        host.setHostState (false, 120.0);
        int hostAdvances = 0;

        for (int i = 0; i < 24000; ++i)
            if (host.process (0.0f, 0.0f, 0.0f).advanced)
                ++hostAdvances;

        check (hostAdvances == 0, "MIDI clock waits for the host transport");

        host.setHostState (true, 120.0);

        for (int i = 0; i < 18000; ++i)
            host.process (0.0f, 0.0f, 0.0f);

        check (host.getCurrentStep() == 2, "MIDI clock follows the host transport at 120 BPM");
    }

    //==========================================================================
    void testPatchBay()
    {
        section ("Patch bay");

        using namespace ReplicantEdgy;
        PatchBay bay;

        check (bay.getNumCables() == 0, "the bay starts empty");

        check (bay.connect (Jack::Osc1, Jack::Osc1Cv), "output -> input connects");
        check (! bay.connect (Jack::Osc1, Jack::Osc2), "output -> output is rejected");
        check (! bay.connect (Jack::Osc1Cv, Jack::Osc2Cv), "input -> input is rejected");
        check (! bay.connect (Jack::Osc1, Jack::Osc1Cv), "duplicate cables are rejected");
        check (bay.getNumCables() == 1, "only the valid cable was stored");

        PatchOutputs outputs;
        outputs.osc1 = 0.5f;
        outputs.vcfEg = 0.75f;

        auto inputs = bay.resolve (outputs);
        check (std::abs (inputs.osc1Cv - 0.5f) < 1.0e-6f, "audio/CV reaches its destination");

        bay.connect (Jack::VcfEg, Jack::VcfMod);
        bay.connect (Jack::VcaEg, Jack::VcaCv);
        inputs = bay.resolve (outputs);
        check (std::abs (inputs.vcfMod - 0.75f) < 1.0e-6f, "the second cable also resolves");
        check (std::abs (inputs.vcaCv) < 1.0e-6f, "unrelated inputs stay silent");

        // Two cables into one input sum.
        bay.connect (Jack::Osc2, Jack::Osc1Cv);
        outputs.osc2 = 0.25f;
        inputs = bay.resolve (outputs);
        check (std::abs (inputs.osc1Cv - 0.75f) < 1.0e-6f, "cables into one input sum");

        const auto snapshot = bay.snapshot();
        check (snapshot.count == 4, "snapshot copies every cable");

        auto tree = bay.toValueTree();
        PatchBay restored;
        restored.restoreFromValueTree (tree);
        check (restored.getNumCables() == 4, "cables survive a ValueTree round-trip");

        const int removed = bay.disconnect (Jack::Osc1Cv);
        check (removed == 2 && bay.getNumCables() == 2,
               "disconnect removes every cable on a jack");

        // A feedback patch is legal and resolves to a one-sample-delayed signal.
        PatchBay feedback;
        check (feedback.connect (Jack::Vca, Jack::ExtAudio), "VCA -> EXT AUDIO feedback connects");

        PatchOutputs loopOutputs;
        loopOutputs.vca = 0.4f;
        check (std::abs (feedback.resolve (loopOutputs).extAudio - 0.4f) < 1.0e-6f,
               "the feedback cable carries audio into EXT AUDIO");
    }

    //==========================================================================
    void testEngine()
    {
        section ("Synth engine");

        ReplicantEdgy::SynthEngine engine;
        engine.prepare (48000.0);
        engine.setParams (makeParams());

        auto silence = render (engine, 4);
        check (silence.peak < 1.0e-6f, "the engine is silent before a trigger");

        engine.noteOn (48, 0.9f);
        auto note = render (engine, 8);
        check (note.peak > 0.02f, "MIDI note-on makes sound");
        check (note.finite && note.peak < 4.0f, "note audio is finite and bounded");
        check (note.firstSoundSample <= 256, "the note starts in the first block");

        // The sequencer drives the envelopes too.
        ReplicantEdgy::SynthEngine sequencerEngine;
        sequencerEngine.prepare (48000.0);
        sequencerEngine.setParams (makeParams());
        sequencerEngine.getSequencer().setPlaying (true);

        auto sequenced = render (sequencerEngine, 48);
        check (sequenced.peak > 0.02f, "the internal sequencer triggers the voice");
        check (sequenced.finite && sequenced.peak < 4.0f, "sequenced audio stays bounded");

        // Stopping the sequence lets the envelopes decay to silence.
        sequencerEngine.getSequencer().setPlaying (false);
        render (sequencerEngine, 40);
        auto tail = render (sequencerEngine, 20);
        check (tail.peak < 0.02f, "audio decays after the sequence stops");

        // Velocity changes the level.
        ReplicantEdgy::SynthEngine loud;
        loud.prepare (48000.0);
        auto loudParams = makeParams();
        loudParams.vcaDecay = 1.0f;
        loud.setParams (loudParams);
        loud.noteOn (48, 1.0f);
        auto loudRender = render (loud, 4);

        ReplicantEdgy::SynthEngine quiet;
        quiet.prepare (48000.0);
        quiet.setParams (loudParams);
        quiet.noteOn (48, 0.15f);
        auto quietRender = render (quiet, 4);

        check (loudRender.peak > quietRender.peak * 1.5f,
               "velocity scales the VCA level (loud " + juce::String (loudRender.peak, 3)
                   + " vs quiet " + juce::String (quietRender.peak, 3) + ")");

        // Hard sync changes the waveform but stays bounded.
        ReplicantEdgy::SynthEngine sync;
        sync.prepare (48000.0);
        auto syncParams = makeParams();
        syncParams.hardSync = true;
        syncParams.osc2Level = 0.8f;
        sync.setParams (syncParams);
        sync.noteOn (48, 1.0f);
        auto syncRender = render (sync, 8);
        check (syncRender.finite && syncRender.peak > 0.02f, "hard sync stays stable");

        // FM.
        ReplicantEdgy::SynthEngine fm;
        fm.prepare (48000.0);
        auto fmParams = makeParams();
        fmParams.fmAmount = 0.8f;
        fm.setParams (fmParams);
        fm.noteOn (48, 1.0f);
        auto fmRender = render (fm, 8);
        check (fmRender.finite && fmRender.peak < 4.0f, "audio-rate FM stays bounded");

        // VCF envelope closes/opens the filter: compare a dark and a bright patch.
        auto measureBrightness = [] (float vcfEnvAmount)
        {
            ReplicantEdgy::SynthEngine e;
            e.prepare (48000.0);
            auto params = makeParams();
            params.cutoffHz = 300.0f;
            params.vcfEnv = vcfEnvAmount;
            params.vcaDecay = 0.8f;
            params.vcfDecay = 0.8f;
            e.setParams (params);
            e.noteOn (48, 1.0f);

            std::vector<float> captured (4096, 0.0f);
            render (e, 8, 256, captured.data(), (int) captured.size());

            double difference = 0.0;

            for (int i = 1; i < (int) captured.size(); ++i)
                difference += std::abs (captured[(size_t) i] - captured[(size_t) (i - 1)]);

            return difference;
        };

        const double closed = measureBrightness (-0.8f);
        const double open = measureBrightness (0.9f);
        check (open > closed * 1.1, "the VCF envelope opens the filter");

        // Noise level adds noise.
        ReplicantEdgy::SynthEngine noisy;
        noisy.prepare (48000.0);
        auto noiseParams = makeParams();
        noiseParams.noiseLevel = 0.8f;
        noiseParams.osc1Level = 0.0f;
        noiseParams.osc2Level = 0.0f;
        noisy.setParams (noiseParams);
        noisy.noteOn (48, 1.0f);
        auto noiseRender = render (noisy, 8);
        check (noiseRender.peak > 0.005f, "the noise generator reaches the output");

        // Patch cable VELOCITY -> VCF MOD makes the filter follow the step velocity.
        ReplicantEdgy::SynthEngine patched;
        patched.prepare (48000.0);
        patched.setParams (makeParams());
        patched.getPatchBay().connect (ReplicantEdgy::Jack::VelocityOut,
                                       ReplicantEdgy::Jack::VcfMod);
        patched.getSequencer().setPlaying (true);

        auto patchedRender = render (patched, 24);
        check (patchedRender.peak > 0.02f && patchedRender.finite,
               "a patched CV destination is rendered safely");
    }

    //==========================================================================
    void testPresetManager()
    {
        section ("Preset manager");

        using namespace ReplicantEdgy;

        auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("redg_presets", "", false);
        temp.createDirectory();

        PresetManager manager;
        manager.setUserPresetDirectory (temp);
        manager.initialise();

        check (manager.getNumFactory() >= 24,
               "the factory bank ships at least 24 patches (got "
                   + juce::String (manager.getNumFactory()) + ")");
        check (manager.getNumUser() == 0, "the user bank starts empty");

        bool namesUnique = true;
        bool categoriesPresent = false;
        juce::StringArray names;

        for (int i = 0; i < manager.getNumFactory(); ++i)
        {
            const auto name = manager.getName (i);
            names.addIfNotAlreadyThere (name);
            categoriesPresent = categoriesPresent || manager.getCategory (i) == "Perc"
                                || manager.getCategory (i) == "Bass"
                                || manager.getCategory (i) == "Lead"
                                || manager.getCategory (i) == "Noise"
                                || manager.getCategory (i) == "Drone"
                                || manager.getCategory (i) == "FX";
        }

        namesUnique = names.size() == manager.getNumFactory();
        check (namesUnique, "factory preset names are unique");
        check (categoriesPresent, "factory presets use the documented categories");

        // Every factory preset must be loadable and inside parameter ranges.
        bool sane = true;

        for (int i = 0; i < manager.getNumFactory(); ++i)
        {
            const auto preset = manager.getPreset (i);
            sane = sane && preset.get (ParamID::cutoff, -1.0f) >= 20.0f
                         && preset.get (ParamID::cutoff, 1.0f) <= 20000.0f
                         && preset.get (ParamID::tempo, -1.0f) >= 20.0f
                         && preset.get (ParamID::tempo, 999.0f) <= 300.0f
                         && preset.get (ParamID::volume, -100.0f) >= -60.0f;
        }

        check (sane, "factory preset values stay inside the parameter ranges");

        // User preset save / load / delete.
        auto preset = Preset::makeInit();
        preset.name = "Unit Test Patch";
        preset.set (ParamID::cutoff, 1234.0f);
        preset.set (ParamID::tempo, 133.0f);
        preset.cables.push_back ({ Jack::Osc2, Jack::FmAmount, 3 });
        preset.pitch[2] = 0.5f;
        preset.velocity[2] = 0.9f;

        const int index = manager.saveUserPreset (preset);
        check (index >= manager.getNumFactory(), "saving a user preset appends it to the bank");
        check (manager.getNumUser() == 1, "the user bank now has one patch");
        check (manager.isUserPreset (index), "the saved patch is marked as user content");

        const auto loaded = manager.getPreset (index);
        check (loaded.name == "Unit Test Patch", "the user preset keeps its name");
        check (std::abs (loaded.get (ParamID::cutoff) - 1234.0f) < 0.01f,
               "the user preset keeps modified parameters");
        check (loaded.cables.size() == 1
                   && loaded.cables[0].source == Jack::Osc2,
               "the user preset keeps its patch cables");

        // Export / import.
        auto exportDir = temp.getChildFile ("exports");
        exportDir.createDirectory();
        auto exported = exportDir.getChildFile ("exported.edgy");
        check (manager.exportPreset (index, exported), "export writes an .edgy file");
        check (exported.existsAsFile(), "the exported file exists");

        Preset imported;
        check (manager.importPreset (exported, imported), "import reads the .edgy file");
        check (imported.name == "Unit Test Patch"
                   && std::abs (imported.get (ParamID::cutoff) - 1234.0f) < 0.01f,
               "the imported preset matches the exported one");

        check (manager.deleteUserPreset (index), "deleting a user preset works");
        check (manager.getNumUser() == 0, "the user bank is empty again");

        temp.deleteRecursively();
    }

    //==========================================================================
    struct StepProbe
    {
        float peak = 0.0f;
        int zeroCrossings = 0;
        bool finite = true;
    };

    /** Renders one step of the sequencer with the given pitch and velocity.
        The patch is stripped down to VCO 1 through an open filter so the pitch
        can be counted reliably. */
    StepProbe renderSingleStep (float pitchNorm, float velocity, int samples = 21600)
    {
        using namespace ReplicantEdgy;

        ReplicantEdgyAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);

        processor.setParameterValue (ParamID::osc1Level, 1.0f);
        processor.setParameterValue (ParamID::osc2Level, 0.0f);
        processor.setParameterValue (ParamID::noiseLevel, 0.0f);
        processor.setParameterValue (ParamID::osc1Env, 0.0f);
        processor.setParameterValue (ParamID::osc2Env, 0.0f);
        processor.setParameterValue (ParamID::fmAmount, 0.0f);
        processor.setParameterValue (ParamID::hardSync, 0.0f);
        processor.setParameterValue (ParamID::cutoff, 14000.0f);
        processor.setParameterValue (ParamID::resonance, 0.0f);
        processor.setParameterValue (ParamID::vcfEnv, 0.0f);
        processor.setParameterValue (ParamID::noiseVcfMod, 0.0f);
        processor.setParameterValue (ParamID::egDecay, 3.0f);
        processor.setParameterValue (ParamID::vcfDecay, 3.0f);
        processor.setParameterValue (ParamID::vcaDecay, 3.0f);
        processor.setParameterValue (ParamID::vcaMode, 0.0f);
        processor.setParameterValue (ParamID::volume, 0.0f);
        processor.setParameterValue (ParamID::seqPitchMod, 1.0f);
        processor.setParameterValue (ParamID::seqPitchTarget, 2.0f);
        processor.setParameterValue (ParamID::tempo, 60.0f);
        processor.setParameterValue (ParamID::scale, 3.0f);   // 1/4 = one step per second

        for (int i = 0; i < StepSequencer::numSteps; ++i)
        {
            processor.setParameterValue (ParamID::stepPitch (i), pitchNorm);
            processor.setParameterValue (ParamID::stepVelocity (i), velocity);
        }

        processor.getEngine().getSequencer().setPlaying (true);

        StepProbe probe;
        std::vector<float> audio;
        audio.reserve ((size_t) samples);

        while ((int) audio.size() < samples)
        {
            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                audio.push_back (buffer.getSample (0, i));
        }

        float previous = 0.0f;

        for (int i = 0; i < (int) audio.size(); ++i)
        {
            const float value = audio[(size_t) i];
            probe.finite = probe.finite && std::isfinite (value);
            probe.peak = juce::jmax (probe.peak, std::abs (value));

            // Skip the attack, then count rising zero crossings.
            if (i > 2000 && previous <= 0.0f && value > 0.0f)
                ++probe.zeroCrossings;

            previous = value;
        }

        return probe;
    }

    void testStepKnobs()
    {
        section ("Step knobs");

        using namespace ReplicantEdgy;

        ReplicantEdgyAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        processor.applyPreset (0);

        // The knobs are APVTS parameters; a block must publish them to the
        // sequencer, otherwise the whole row is decorative.
        processor.setParameterValue (ParamID::stepPitch (0), 0.75f);
        processor.setParameterValue (ParamID::stepVelocity (0), 0.25f);

        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        check (std::abs (processor.getEngine().getSequencer().getPitch (0) - 0.75f) < 1.0e-3f,
               "step pitch knobs reach the sequencer");
        check (std::abs (processor.getEngine().getSequencer().getVelocity (0) - 0.25f) < 1.0e-3f,
               "step velocity knobs reach the sequencer");

        // And they are audible: +12 semitones nearly doubles the pitch.
        const auto low = renderSingleStep (0.0f, 0.9f);
        const auto high = renderSingleStep (1.0f, 0.9f);

        check (low.finite && high.finite, "step renders stay finite");
        check (low.peak > 0.01f, "a step with pitch 0 makes sound");

        const float ratio = (float) high.zeroCrossings / juce::jmax (1.0f, (float) low.zeroCrossings);
        check (ratio > 1.7f && ratio < 2.3f,
               "pitch +12 semitones doubles the oscillator frequency (ratio "
                   + juce::String (ratio, 2) + ")");

        // Velocity drives the VCA level.
        const auto loud = renderSingleStep (0.0f, 1.0f);
        const auto quiet = renderSingleStep (0.0f, 0.20f);

        check (loud.peak > quiet.peak * 2.0f,
               "step velocity changes the level (loud " + juce::String (loud.peak, 3)
                   + " vs quiet " + juce::String (quiet.peak, 3) + ")");

        // The parameter tree stays the source of truth after a preset load.
        processor.applyPreset (0);
        processor.processBlock (buffer, midi);

        const auto preset = processor.getPresetManager().getPreset (0);
        check (std::abs (processor.getEngine().getSequencer().getPitch (2)
                             - preset.pitch[2]) < 1.0e-3f,
               "loading a preset still restores every step value");
    }

    //==========================================================================
    void testTempoDisplay()
    {
        section ("Tempo display");

        using namespace ReplicantEdgy;

        ReplicantEdgyAudioProcessor processor;   // keep it alive: the display uses its parameter
        auto* tempoParameter = processor.apvts.getParameter (ParamID::tempo);
        check (tempoParameter != nullptr, "the tempo parameter exists");

        if (tempoParameter == nullptr)
            return;

        TempoDisplay display (*tempoParameter);
        display.setSize (110, 54);

        display.setValue (143.5f);
        check (std::abs (processor.apvts.getRawParameterValue (ParamID::tempo)->load() - 143.5f) < 0.01f,
               "typing a tempo writes the parameter");

        display.setValue (9999.0f);
        check (std::abs (processor.apvts.getRawParameterValue (ParamID::tempo)->load() - 300.0f) < 0.01f,
               "out-of-range tempos clamp to the top of the range");
        check (std::abs (display.getValue() - 300.0f) < 0.01f,
               "the display shows the clamped value");

        // Drive the parameter itself, not the display, so the attachment round
        // trip is what is actually being tested.
        tempoParameter->setValueNotifyingHost (tempoParameter->convertTo0to1 (72.0f));
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        check (std::abs (display.getValue() - 72.0f) < 0.01f,
               "the display follows external tempo changes");
    }

    //==========================================================================
    void testRandomPatch()
    {
        section ("Random patch");

        using namespace ReplicantEdgy;

        ReplicantEdgyAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        processor.applyPreset (0);

        const auto beforeCutoff = processor.apvts.getRawParameterValue (ParamID::cutoff)->load();
        const auto beforePitch = processor.apvts.getRawParameterValue (ParamID::stepPitch (3))->load();

        processor.randomisePatch();

        const auto afterCutoff = processor.apvts.getRawParameterValue (ParamID::cutoff)->load();
        const auto afterPitch = processor.apvts.getRawParameterValue (ParamID::stepPitch (3))->load();

        check (std::abs (afterPitch - beforePitch) > 1.0e-3f,
               "RANDOM writes a new step line");
        check (std::abs (afterCutoff - beforeCutoff) > 0.5f,
               "RANDOM moves the filter");
        check (processor.getEngine().getPatchBay().getNumCables() <= 2,
               "RANDOM patches at most two cables");
        check (processor.getCurrentPresetName().contains ("Mutation"),
               "RANDOM names the mutation");

        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        check (std::abs (processor.getEngine().getSequencer().getPitch (3) - afterPitch) < 1.0e-3f,
               "the random line reaches the sequencer");

        // E1: a SAVE with no audio block in between must capture the knobs, not
        // the one-block-old sequencer cache.
        processor.setParameterValue (ParamID::stepPitch (1), 0.66f);

        const auto captured = processor.captureCurrentPreset ("Stale Check", "User");
        check (std::abs (captured.get (ParamID::stepPitch (1)) - 0.66f) < 1.0e-3f,
               "capture reads step pitch from the parameter, not a stale cache");
        check (std::abs (captured.pitch[1] - 0.66f) < 1.0e-3f,
               "the captured step array matches the parameter");

        // Repeated mutation must never leave the engine in a silent or
        // unstable state (cables can sum, parameters can land on extremes).
        bool allFinite = true;
        bool allAudible = true;
        bool cablesValid = true;

        for (int i = 0; i < 12; ++i)
        {
            processor.randomisePatch();

            // Restart the transport so step 0 fires immediately: at the slowest
            // clock division the next step can be 0.6 s away, and a short VCA
            // decay would leave a silent window that is not a silent patch.
            processor.getEngine().getSequencer().setPlaying (false);
            juce::AudioBuffer<float> silence (2, 256);
            juce::MidiBuffer noEvents;
            processor.processBlock (silence, noEvents);
            processor.getEngine().getSequencer().setPlaying (true);

            float peak = 0.0f;

            for (int block = 0; block < 48; ++block)
            {
                juce::AudioBuffer<float> audio (2, 256);
                juce::MidiBuffer events;
                processor.processBlock (audio, events);

                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < audio.getNumSamples(); ++n)
                    {
                        const float value = audio.getSample (ch, n);
                        allFinite = allFinite && std::isfinite (value);
                        peak = juce::jmax (peak, std::abs (value));
                    }
            }

            allAudible = allAudible && peak > 0.001f && peak < 4.0f;

            for (const auto& cable : processor.getEngine().getPatchBay().getCables())
                cablesValid = cablesValid && jackIsOutput (cable.source)
                              && jackIsInput (cable.destination);
        }

        check (allFinite, "repeated mutations stay finite");
        check (allAudible, "repeated mutations always produce a playable patch");
        check (cablesValid, "random cables are always output -> input");

        processor.getEngine().getSequencer().setPlaying (false);
    }

    //==========================================================================
    void testProcessor()
    {
        section ("Plugin processor");

        using namespace ReplicantEdgy;

        ReplicantEdgyAudioProcessor processor;

        // Never touch the real user preset folder from a test run.
        auto testPresetDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getNonexistentChildFile ("redg_processor_presets", "", false);
        testPresetDir.createDirectory();
        processor.getPresetManager().setUserPresetDirectory (testPresetDir);

        processor.prepareToPlay (48000.0, 256);

        bool allPresent = true;

        for (const auto& id : ReplicantEdgyAudioProcessor::getAllParameterIds())
            allPresent = allPresent && processor.apvts.getParameter (id) != nullptr;

        check (allPresent, "every parameter is registered");

        processor.applyPreset (1);
        check (processor.getCurrentPresetIndex() == 1, "applying a preset updates the index");

        const auto preset = processor.getPresetManager().getPreset (1);
        const auto cutoff = processor.apvts.getRawParameterValue (ParamID::cutoff)->load();
        check (std::abs (cutoff - preset.get (ParamID::cutoff)) < 1.0f,
               "applying a preset writes its cutoff into the parameter tree");

        // Cable application.
        auto cablePresetIndex = -1;

        for (int i = 0; i < processor.getPresetManager().getNumFactory(); ++i)
            if (! processor.getPresetManager().getPreset (i).cables.empty())
            {
                cablePresetIndex = i;
                break;
            }

        check (cablePresetIndex >= 0, "at least one factory preset ships with patch cables");

        if (cablePresetIndex >= 0)
        {
            processor.applyPreset (cablePresetIndex);
            check (processor.getEngine().getPatchBay().getNumCables()
                       == (int) processor.getPresetManager().getPreset (cablePresetIndex).cables.size(),
                   "applying a preset restores its cables");
        }

        // Capture -> save -> state round-trip.
        processor.setParameterValue (ParamID::cutoff, 2222.0f);
        processor.setParameterValue (ParamID::tempo, 141.0f);
        processor.getEngine().getPatchBay().connect (Jack::Osc1, Jack::Osc2Cv);

        const auto captured = processor.captureCurrentPreset ("Round Trip", "User");
        check (std::abs (captured.get (ParamID::cutoff) - 2222.0f) < 1.0f,
               "capture stores the live cutoff");
        check (captured.cables.size() == 2, "capture stores the live cables");

        juce::MemoryBlock state;
        processor.getStateInformation (state);
        check (state.getSize() > 0, "state serialises");

        ReplicantEdgyAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (std::abs (restored.apvts.getRawParameterValue (ParamID::cutoff)->load() - 2222.0f) < 1.0f,
               "the cutoff survives the state round-trip");
        check (restored.getEngine().getPatchBay().getNumCables() == 2,
               "patch cables survive the state round-trip");
        check (restored.getCurrentPresetIndex() == processor.getCurrentPresetIndex(),
               "the active preset survives the state round-trip");

        // Import/export through the processor.
        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("redg_export", ".edgy", false);
        check (processor.exportPreset (1, file), "EXPORT writes a preset file");
        check (processor.importPreset (file), "IMPORT loads the preset into the user bank");
        file.deleteFile();

        // Host program interface.
        check (processor.getNumPrograms() >= 24, "the host sees the whole bank");
        check (! processor.getProgramName (0).isEmpty(), "program names are exposed");

        // Audio rendering with the preset.
        processor.applyPreset (0);
        processor.prepareToPlay (48000.0, 256);
        processor.getEngine().getSequencer().setPlaying (true);

        float peak = 0.0f;
        bool finite = true;

        for (int block = 0; block < 80; ++block)
        {
            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    const float value = buffer.getSample (ch, i);
                    finite = finite && std::isfinite (value);
                    peak = juce::jmax (peak, std::abs (value));
                }
        }

        check (finite && peak > 0.02f && peak < 4.0f,
               "the processor renders the sequencer through the full chain");

        testPresetDir.deleteRecursively();
    }

    //==========================================================================
    void testEditor()
    {
        section ("Editor & preset drawer");

        ReplicantEdgyAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        check (editor != nullptr, "the editor is created");

        if (editor == nullptr)
            return;

        auto* concrete = dynamic_cast<ReplicantEdgyAudioProcessorEditor*> (editor.get());
        check (concrete != nullptr, "the editor has the expected type");

        if (concrete == nullptr)
            return;

        check (editor->getWidth() == ReplicantEdgy::Layout::panelWidth
                   && editor->getHeight() == ReplicantEdgy::Layout::panelHeight,
               "the panel opens at its hardware size");

        concrete->setPresetDrawerOpen (true, false);
        check (editor->getHeight() == ReplicantEdgy::Layout::panelHeight
                                         + ReplicantEdgy::Layout::drawerHeight,
               "the preset drawer extends the window");

        concrete->setPresetDrawerOpen (false, false);
        check (editor->getHeight() == ReplicantEdgy::Layout::panelHeight,
               "closing the drawer restores the panel size");

        // Cable drag path used by the mouse, exercised directly.
        processor.applyPreset (0);
        const int before = processor.getEngine().getPatchBay().getNumCables();
        check (concrete->connectJacks (ReplicantEdgy::Jack::OscEg,
                                       ReplicantEdgy::Jack::Osc1Cv),
               "the editor can patch a cable");
        check (processor.getEngine().getPatchBay().getNumCables() == before + 1,
               "the cable reaches the engine");
        check (! concrete->connectJacks (ReplicantEdgy::Jack::Osc1Cv,
                                         ReplicantEdgy::Jack::Osc2Cv),
               "invalid cable directions are refused");

        // Animated drawer.
        concrete->setPresetDrawerOpen (true, true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
        check (editor->getHeight() == ReplicantEdgy::Layout::panelHeight
                                         + ReplicantEdgy::Layout::drawerHeight,
               "the animated drawer reaches its full height");
        concrete->setPresetDrawerOpen (false, true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
        check (editor->getHeight() == ReplicantEdgy::Layout::panelHeight,
               "the animated drawer closes completely");
    }
}

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::cout << "ReplicantEdgy test suite" << std::endl;

    testOscillators();
    testFilter();
    testEnvelope();
    testSequencer();
    testPatchBay();
    testStepKnobs();
    testTempoDisplay();
    testRandomPatch();
    testEngine();
    testPresetManager();
    testProcessor();
    testEditor();

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << checks - failures << " / " << checks << " checks passed" << std::endl;

    if (failures != 0)
    {
        std::cout << failures << " FAILURE(S)" << std::endl;
        return 1;
    }

    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
}
