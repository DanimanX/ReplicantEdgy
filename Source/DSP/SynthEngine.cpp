#include "SynthEngine.h"

namespace ReplicantEdgy
{
    namespace
    {
        float freqFromNorm (float norm) noexcept
        {
            // 8 Hz .. 2 kHz, the range the hardware oscillators cover.
            return Math::logMap (norm, 8.0f, 2000.0f);
        }

        float scaledDecay (float seconds, float cv) noexcept
        {
            return seconds * std::pow (2.0f, juce::jlimit (-1.0f, 1.0f, cv) * 2.0f);
        }
    }

    //==============================================================================
    SynthEngine::SynthEngine()
    {
        for (int i = 0; i < StepSequencer::numSteps; ++i)
            sequencer.setStep (i, 0.0f, 0.65f);
    }

    void SynthEngine::prepare (double newSampleRate)
    {
        sampleRate = juce::jmax (1000.0, newSampleRate);

        osc1.prepare (sampleRate);
        osc2.prepare (sampleRate);
        noise.prepare (sampleRate);
        filter.prepare (sampleRate);
        oscEnv.prepare (sampleRate);
        vcfEnv.prepare (sampleRate);
        vcaEnv.prepare (sampleRate);
        sequencer.prepare (sampleRate);

        // A trigger pulse lasts roughly 8 ms; the value doubles as the gate the
        // TRIGGER output jack carries.
        triggerDecay = (float) std::exp (-1.0 / (0.008 * sampleRate));

        reset();
    }

    void SynthEngine::reset()
    {
        osc1.reset();
        osc2.reset();
        noise.reset();
        filter.reset();
        oscEnv.reset();
        vcfEnv.reset();
        vcaEnv.reset();
        sequencer.reset();

        patchInputs = {};
        patchOutputs = {};
        triggerPulse = 0.0f;
        lastTriggerInput = false;
        lastOsc2 = 0.0f;
        midiHeld = false;
    }

    void SynthEngine::setHostState (bool hostIsPlaying, double hostBpm) noexcept
    {
        sequencer.setHostState (hostIsPlaying, hostBpm);
    }

    void SynthEngine::noteOn (int note, float velocity) noexcept
    {
        midiNote = juce::jlimit (0, 127, note);
        midiHeld = true;

        const float amount = juce::jlimit (0.05f, 1.0f, velocity);
        currentVelocity = amount;

        oscEnv.trigger (1.0f);
        vcfEnv.trigger (amount);
        vcaEnv.trigger (amount);
        triggerPulse = 1.0f;
    }

    void SynthEngine::allNotesOff() noexcept
    {
        midiHeld = false;
    }

    //==============================================================================
    void SynthEngine::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        const int numSamples = buffer.getNumSamples();

        if (numSamples <= 0)
            return;

        // One lock-free copy of the patch cables per block. A feedback patch is
        // then just a one-sample delay, never an algebraic loop.
        cableSnapshot = patchBay.snapshot();

        sequencer.setTempo (params.tempo);
        sequencer.setClockSource ((ClockSource) juce::jlimit (0, 2, params.clockSource));
        sequencer.setScale ((ClockScale) juce::jlimit (0, 7, params.scale));

        osc1.setShape (params.osc1Shape == 1 ? OscillatorEdgy::Shape::Square
                                             : OscillatorEdgy::Shape::Triangle);
        osc2.setShape (params.osc2Shape == 1 ? OscillatorEdgy::Shape::Square
                                             : OscillatorEdgy::Shape::Saw);
        noise.setColour (params.noiseColour == 1 ? NoiseEdgy::Colour::White
                                                  : NoiseEdgy::Colour::Pink);
        filter.setHighpass (params.vcfHighpass);
        filter.setResonance (params.resonance);
        oscEnv.setFastMode (false);
        vcfEnv.setFastMode (false);
        vcaEnv.setFastMode (params.vcaFast);

        auto midiIterator = midi.begin();
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            while (midiIterator != midi.end() && (*midiIterator).samplePosition <= i)
            {
                const auto message = (*midiIterator).getMessage();
                ++midiIterator;

                if (message.isNoteOn())
                    noteOn (message.getNoteNumber(), message.getFloatVelocity());
                else if (message.isNoteOff())
                    allNotesOff();
                else if (message.isAllNotesOff() || message.isAllSoundOff())
                    allNotesOff();
            }

            float l = 0.0f, r = 0.0f;
            processSample (l, r);

            left[i] = l;

            if (right != nullptr)
                right[i] = r;
        }
    }

    //==============================================================================
    void SynthEngine::processSample (float& left, float& right)
    {
        // --- patched modulation (one sample old, by design) -------------------
        patchInputs = PatchBay::resolve (cableSnapshot, patchOutputs);

        // --- sequencer --------------------------------------------------------
        const auto event = sequencer.process (patchInputs.tempo,
                                              patchInputs.playStop,
                                              patchInputs.advClock);

        const float stepVelocity = juce::jlimit (
            0.0f, 1.0f, sequencer.getVelocity (sequencer.getCurrentStep())
                            + patchInputs.velocity);

        currentVelocity = juce::jmax (0.0f, stepVelocity);

        const bool triggerEdge = patchInputs.trigger > 0.5f && ! lastTriggerInput;
        lastTriggerInput = patchInputs.trigger > 0.5f;

        if (event.triggered || triggerEdge)
        {
            const float amount = juce::jlimit (0.05f, 1.0f, currentVelocity);
            oscEnv.trigger (1.0f);
            vcfEnv.trigger (amount);
            vcaEnv.trigger (amount);
            triggerPulse = 1.0f;
        }

        triggerPulse *= triggerDecay;

        // --- envelope decay times, CV controlled ------------------------------
        oscEnv.setDecay (scaledDecay (params.egDecay, patchInputs.oscDecay));
        vcfEnv.setDecay (scaledDecay (params.vcfDecay, patchInputs.vcfDecay));
        vcaEnv.setDecay (scaledDecay (params.vcaDecay, patchInputs.vcaDecay));

        const float oscEgValue = oscEnv.process();
        const float vcfEgValue = vcfEnv.process();
        const float vcaEgValue = vcaEnv.process();

        // --- oscillator pitch -------------------------------------------------
        const float seqPitch = sequencer.getPitch (sequencer.getCurrentStep())
                               * params.seqPitchMod;
        const float seqSemis = seqPitch * 12.0f;
        const bool seqToOsc1 = params.seqPitchTarget == 2;
        const bool seqToOsc2 = params.seqPitchTarget >= 1;

        const float midiSemis = (float) (midiNote - 60);

        const float semis1 = (seqToOsc1 ? seqSemis : 0.0f)
                             + params.osc1Env * oscEgValue * 24.0f
                             + patchInputs.osc1Cv * 12.0f
                             + midiSemis;

        const float semis2 = (seqToOsc2 ? seqSemis : 0.0f)
                             + params.osc2Env * oscEgValue * 24.0f
                             + patchInputs.osc2Cv * 12.0f
                             + midiSemis;

        const float base1 = freqFromNorm (params.osc1FreqNorm);
        const float base2 = freqFromNorm (params.osc2FreqNorm);

        osc1.setFrequency (juce::jlimit (0.05f, (float) sampleRate * 0.45f,
                                         base1 * Math::semitonesToRatio (semis1)));
        osc2.setFrequency (juce::jlimit (0.05f, (float) sampleRate * 0.45f,
                                         base2 * Math::semitonesToRatio (semis2)));

        // --- oscillators ------------------------------------------------------
        const float fmTotal = juce::jlimit (0.0f, 1.0f, params.fmAmount + patchInputs.fmAmount);
        const double fmCycles = (double) (lastOsc2 * fmTotal * 0.45f
                                          * (float) (osc1.getFrequency() / sampleRate));

        const float s1 = osc1.process (fmCycles);

        if (params.hardSync && osc1.didWrap())
            osc2.resetPhase();

        const float s2 = osc2.process();

        if (osc2.didWrap())
            lastOsc2 = s2;

        // --- noise + mixer ----------------------------------------------------
        const float noiseSample = noise.process();
        const float noiseLevel = juce::jlimit (0.0f, 1.0f,
                                               params.noiseLevel + patchInputs.noiseLevel);

        float mixer = s1 * params.osc1Level
                    + s2 * params.osc2Level
                    + noiseSample * noiseLevel * 0.55f
                    + patchInputs.extAudio * noiseLevel;

        mixer = Math::softClip (mixer);

        // --- ladder filter ----------------------------------------------------
        const float envOctaves = params.vcfEnv * vcfEgValue * 6.0f;
        const float modOctaves = patchInputs.vcfMod * 6.0f;
        const float noiseOctaves = noiseSample * params.noiseVcfMod * 2.5f;

        const float cutoff = juce::jlimit (20.0f, (float) sampleRate * 0.45f,
                                           params.cutoffHz
                                           * std::pow (2.0f, envOctaves + modOctaves + noiseOctaves));

        filter.setCutoff (cutoff);
        const float filtered = filter.process (mixer);

        // --- VCA --------------------------------------------------------------
        const float velocityGain = 0.25f + 0.75f * juce::jlimit (0.0f, 1.0f, currentVelocity);
        float vcaGain = vcaEgValue * velocityGain * (1.0f + patchInputs.vcaCv * 0.5f);
        vcaGain = juce::jlimit (0.0f, 2.5f, vcaGain);

        const float vca = filtered * vcaGain;
        const float output = Math::softClip (vca * params.volumeGain);

        left = output;
        right = output;

        scopeBuffer[scopeWrite.load (std::memory_order_relaxed)] = output;
        scopeWrite.store ((scopeWrite.load (std::memory_order_relaxed) + 1) % scopeSize,
                          std::memory_order_relaxed);

        // --- outputs available at the jacks -----------------------------------
        patchOutputs.osc1 = s1;
        patchOutputs.osc2 = s2;
        patchOutputs.oscEg = oscEgValue;
        patchOutputs.trigger = triggerPulse;
        patchOutputs.velocity = currentVelocity;
        patchOutputs.pitch = seqPitch;
        patchOutputs.vcfEg = vcfEgValue;
        patchOutputs.vca = output;
        patchOutputs.vcaEg = vcaEgValue;

        // --- telemetry (relaxed atomics, UI only) -----------------------------
        telemetryStep.store (sequencer.getCurrentStep(), std::memory_order_relaxed);
        telemetryPlaying.store (sequencer.isPlaying(), std::memory_order_relaxed);
        telemetryOsc1.store (s1, std::memory_order_relaxed);
        telemetryOsc2.store (s2, std::memory_order_relaxed);
        telemetryVca.store (vca, std::memory_order_relaxed);
        telemetryCutoff.store (cutoff, std::memory_order_relaxed);
        telemetryOscEnv.store (oscEgValue, std::memory_order_relaxed);
        telemetryVcfEnv.store (vcfEgValue, std::memory_order_relaxed);
        telemetryVcaEnv.store (vcaEgValue, std::memory_order_relaxed);
        telemetryTrigger.store (triggerPulse, std::memory_order_relaxed);
        telemetryVelocity.store (currentVelocity, std::memory_order_relaxed);
        telemetryPitch.store (seqPitch, std::memory_order_relaxed);
        telemetryOsc1Out.store (s1, std::memory_order_relaxed);
        telemetryOsc2Out.store (s2, std::memory_order_relaxed);
        telemetryVcaOut.store (output, std::memory_order_relaxed);
        telemetryPitchOut.store (seqPitch, std::memory_order_relaxed);
    }

    //==============================================================================
    EngineTelemetry SynthEngine::getTelemetry() const
    {
        EngineTelemetry t;
        t.step = telemetryStep.load (std::memory_order_relaxed);
        t.playing = telemetryPlaying.load (std::memory_order_relaxed);
        t.osc1 = telemetryOsc1.load (std::memory_order_relaxed);
        t.osc2 = telemetryOsc2.load (std::memory_order_relaxed);
        t.vca = telemetryVca.load (std::memory_order_relaxed);
        t.cutoff = telemetryCutoff.load (std::memory_order_relaxed);
        t.oscEnv = telemetryOscEnv.load (std::memory_order_relaxed);
        t.vcfEnv = telemetryVcfEnv.load (std::memory_order_relaxed);
        t.vcaEnv = telemetryVcaEnv.load (std::memory_order_relaxed);
        t.trigger = telemetryTrigger.load (std::memory_order_relaxed);
        t.velocity = telemetryVelocity.load (std::memory_order_relaxed);
        t.pitch = telemetryPitch.load (std::memory_order_relaxed);
        return t;
    }

    void SynthEngine::copyScope (float* destination, int count) const
    {
        if (destination == nullptr || count <= 0)
            return;

        const int clamped = juce::jmin (count, scopeSize);
        int write = scopeWrite.load (std::memory_order_relaxed);
        int read = (write - clamped + scopeSize) % scopeSize;

        for (int i = 0; i < clamped; ++i)
        {
            destination[i] = scopeBuffer[read];
            read = (read + 1) % scopeSize;
        }
    }

    float SynthEngine::getJackOutput (Jack jack) const noexcept
    {
        switch (jack)
        {
            case Jack::Osc1:        return telemetryOsc1Out.load (std::memory_order_relaxed);
            case Jack::Osc2:        return telemetryOsc2Out.load (std::memory_order_relaxed);
            case Jack::Vca:         return telemetryVcaOut.load (std::memory_order_relaxed);
            case Jack::OscEg:       return telemetryOscEnv.load (std::memory_order_relaxed);
            case Jack::VcfEg:       return telemetryVcfEnv.load (std::memory_order_relaxed);
            case Jack::VcaEg:       return telemetryVcaEnv.load (std::memory_order_relaxed);
            case Jack::TriggerOut:  return telemetryTrigger.load (std::memory_order_relaxed);
            case Jack::VelocityOut: return telemetryVelocity.load (std::memory_order_relaxed);
            case Jack::Pitch:       return telemetryPitchOut.load (std::memory_order_relaxed);
            default:                return 0.0f;
        }
    }
}
