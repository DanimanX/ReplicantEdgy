#pragma once

#include "CommonEdgy.h"

#include <atomic>

namespace ReplicantEdgy
{
    //==============================================================================
    /** Eight-step analog-style sequencer with independent pitch and velocity per
        step, exactly like the EDGE / DFAM sequencer row.

        The clock can run from the internal tempo knob, from the host transport
        (MIDI) or from pulses patched into ADV/CLOCK (EXT). The TRIGGER button
        fires the envelopes without touching the step position; ADVANCE and any
        clock pulse move the sequence on by one step.
    */
    class StepSequencer
    {
    public:
        static constexpr int numSteps = 8;

        struct Event
        {
            bool advanced = false;    // the step index moved
            bool triggered = false;   // fire the envelopes
        };

        void prepare (double newSampleRate) noexcept;
        void reset() noexcept;

        void setTempo (float bpm) noexcept;
        void setScale (ClockScale newScale) noexcept { scale.store ((int) newScale); }
        void setClockSource (ClockSource newSource) noexcept
        {
            clockSource.store ((int) newSource);
        }
        ClockSource getClockSource() const noexcept
        {
            return (ClockSource) clockSource.load (std::memory_order_relaxed);
        }

        void setHostState (bool hostIsPlaying, double hostBpm) noexcept;
        void setPlaying (bool shouldPlay) noexcept { playing.store (shouldPlay); }
        bool isPlaying() const noexcept { return playing.load (std::memory_order_relaxed); }
        void togglePlaying() noexcept { playing.store (! playing.load (std::memory_order_relaxed)); }

        /** Manual TRIGGER button: fire the envelopes, keep the step position. */
        void triggerNow() noexcept { pendingTrigger.store (true); }

        /** Manual ADVANCE button: move one step and fire. */
        void advanceNow() noexcept { pendingAdvance.store (true); }

        void setStep (int index, float pitch, float velocity) noexcept;
        float getPitch (int index) const noexcept;
        float getVelocity (int index) const noexcept;

        int getCurrentStep() const noexcept
        {
            return currentStep.load (std::memory_order_relaxed);
        }

        /** 0..1 progress through the current step, for the UI. */
        float getPhase() const noexcept;

        /** One sample of clock. `tempoCv` is a bipolar CV offset (patch TEMPO),
            `playGate` a gate from the PLAY/STOP patch point and `advClock` the
            signal patched into ADV/CLOCK. */
        Event process (float tempoCv, float playGate, float advClock) noexcept;

    private:
        void advance() noexcept;

        double sampleRate = 44100.0;
        double samplesPerStep = 12000.0;
        double samplesIntoStep = 0.0;

        std::atomic<float> tempoBpm { 120.0f };
        double hostBpm = 0.0;
        bool hostPlaying = false;
        std::atomic<int> scale { (int) ClockScale::Div16 };
        std::atomic<int> clockSource { (int) ClockSource::Internal };

        std::atomic<bool> playing { false };
        bool needsInitialTrigger = true;
        bool transportWasRunning = false;
        bool playGateHigh = false;
        bool lastAdvClock = false;

        std::atomic<bool> pendingTrigger { false };
        std::atomic<bool> pendingAdvance { false };

        std::atomic<int> currentStep { 0 };

        // Written by the editor/preset loader, read by the audio thread. Plain
        // atomics keep the step knobs safe to drag while the sequence plays.
        std::atomic<float> pitch[numSteps] {};
        std::atomic<float> velocity[numSteps] {};
    };
}
