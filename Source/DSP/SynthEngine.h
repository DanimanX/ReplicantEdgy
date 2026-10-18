#pragma once

#include "CommonEdgy.h"
#include "LadderFilter.h"
#include "EnvelopeEdgy.h"
#include "NoiseEdgy.h"
#include "OscillatorEdgy.h"
#include "PatchBay.h"
#include "StepSequencer.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

namespace ReplicantEdgy
{
    //==============================================================================
    /** Plain snapshot of every panel control, produced once per block by the
        processor. Kept free of pointers and strings so the audio thread never
        touches the parameter tree. */
    struct EngineParams
    {
        float osc1FreqNorm = 0.45f;
        int   osc1Shape    = 0;      // 0 triangle, 1 square
        float osc1Env      = 0.0f;   // -1..1
        float osc1Level    = 0.80f;

        float osc2FreqNorm = 0.50f;
        int   osc2Shape    = 0;      // 0 saw, 1 square
        float osc2Env      = 0.0f;
        float osc2Level    = 0.65f;
        bool  hardSync     = false;
        float fmAmount     = 0.0f;

        float seqPitchMod  = 0.60f;
        int   seqPitchTarget = 2;    // 0 off, 1 osc2, 2 both

        float egDecay      = 0.30f;  // seconds

        int   noiseColour  = 0;      // 0 pink, 1 white
        float noiseLevel   = 0.0f;

        bool  vcfHighpass  = false;
        float cutoffHz     = 700.0f;
        float resonance    = 0.25f;
        float vcfDecay     = 0.40f;
        float vcfEnv       = 0.35f;  // -1..1
        float noiseVcfMod  = 0.0f;

        bool  vcaFast      = true;
        float vcaDecay     = 0.35f;
        float volumeGain   = 0.70f;

        float tempo        = 120.0f;
        int   clockSource  = 0;
        int   scale        = 1;
    };

    //==============================================================================
    struct EngineTelemetry
    {
        int   step = 0;
        bool  playing = false;
        float osc1 = 0.0f;
        float osc2 = 0.0f;
        float vca = 0.0f;
        float cutoff = 0.0f;
        float oscEnv = 0.0f;
        float vcfEnv = 0.0f;
        float vcaEnv = 0.0f;
        float trigger = 0.0f;
        float velocity = 0.0f;
        float pitch = 0.0f;
    };

    //==============================================================================
    /** The complete EDGE voice: two oscillators, noise, ladder filter, three
        decay envelopes, a patchable modulation matrix and the 8-step sequencer. */
    class SynthEngine
    {
    public:
        SynthEngine();

        void prepare (double sampleRate);
        void reset();

        /** Copies the current panel state. Audio thread only. */
        void setParams (const EngineParams& newParams) noexcept { params = newParams; }
        const EngineParams& getParams() const noexcept { return params; }

        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

        StepSequencer& getSequencer() noexcept { return sequencer; }
        const StepSequencer& getSequencer() const noexcept { return sequencer; }

        PatchBay& getPatchBay() noexcept { return patchBay; }
        const PatchBay& getPatchBay() const noexcept { return patchBay; }

        /** UI telemetry, written once per block. */
        EngineTelemetry getTelemetry() const;

        /** Last output jack value, for the cable activity LEDs. */
        float getJackOutput (Jack jack) const noexcept;

        /** Copies the most recent `count` output samples for the UI scope. */
        static constexpr int scopeSize = 512;
        void copyScope (float* destination, int count) const;

        void setHostState (bool hostIsPlaying, double hostBpm) noexcept;
        void setHostTransportAvailable (bool available) noexcept;

        void noteOn (int midiNote, float velocity) noexcept;
        void allNotesOff() noexcept;

    private:
        void processSample (float& left, float& right);

        double sampleRate = 44100.0;
        EngineParams params;

        OscillatorEdgy osc1, osc2;
        NoiseEdgy noise;
        LadderFilter filter;
        DecayEnvelope oscEnv, vcfEnv, vcaEnv;
        StepSequencer sequencer;
        PatchBay patchBay;

        PatchBay::Snapshot cableSnapshot;
        PatchInputs patchInputs;
        PatchOutputs patchOutputs;

        float triggerPulse = 0.0f;
        float triggerDecay = 0.995f;
        bool lastTriggerInput = false;

        float currentVelocity = 0.8f;
        float lastOsc2 = 0.0f;
        int midiNote = 60;
        bool midiHeld = false;

        std::atomic<int>   telemetryStep { 0 };
        std::atomic<bool>  telemetryPlaying { false };
        std::atomic<float> telemetryOsc1 { 0.0f };
        std::atomic<float> telemetryOsc2 { 0.0f };
        std::atomic<float> telemetryVca { 0.0f };
        std::atomic<float> telemetryCutoff { 0.0f };
        std::atomic<float> telemetryOscEnv { 0.0f };
        std::atomic<float> telemetryVcfEnv { 0.0f };
        std::atomic<float> telemetryVcaEnv { 0.0f };
        std::atomic<float> telemetryTrigger { 0.0f };
        std::atomic<float> telemetryVelocity { 0.0f };
        std::atomic<float> telemetryPitch { 0.0f };
        std::atomic<float> telemetryOsc1Out { 0.0f };
        std::atomic<float> telemetryOsc2Out { 0.0f };
        std::atomic<float> telemetryVcaOut { 0.0f };
        std::atomic<float> telemetryPitchOut { 0.0f };

        float scopeBuffer[scopeSize] {};
        std::atomic<int> scopeWrite { 0 };
    };
}
