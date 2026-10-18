#include "StepSequencer.h"

namespace ReplicantEdgy
{
    void StepSequencer::prepare (double newSampleRate) noexcept
    {
        sampleRate = juce::jmax (1000.0, newSampleRate);
        reset();
    }

    void StepSequencer::reset() noexcept
    {
        for (int i = 0; i < numSteps; ++i)
        {
            pitch[i].store (0.0f, std::memory_order_relaxed);
            velocity[i].store (0.65f, std::memory_order_relaxed);
        }

        samplesIntoStep = 0.0;
        currentStep = 0;
        playing = false;
        needsInitialTrigger = true;
        playGateHigh = false;
        lastAdvClock = false;
        pendingTrigger = false;
        pendingAdvance = false;
        samplesPerStep = sampleRate * 60.0 / juce::jmax (20.0f, tempoBpm)
                         * scaleToBeats (scale);
    }

    void StepSequencer::setTempo (float bpm) noexcept
    {
        tempoBpm = juce::jlimit (20.0f, 300.0f, bpm);
    }

    void StepSequencer::setHostState (bool hostIsPlaying, double hostBpmValue) noexcept
    {
        hostPlaying = hostIsPlaying;
        hostBpm = hostBpmValue;
    }

    void StepSequencer::setPlaying (bool shouldPlay) noexcept
    {
        if (shouldPlay && ! playing)
        {
            samplesIntoStep = 0.0;
            needsInitialTrigger = true;
        }

        playing = shouldPlay;
    }

    void StepSequencer::setStep (int index, float pitchValue, float velocityValue) noexcept
    {
        if (! juce::isPositiveAndBelow (index, numSteps))
            return;

        pitch[index].store (juce::jlimit (-1.0f, 1.0f, pitchValue), std::memory_order_relaxed);
        velocity[index].store (juce::jlimit (0.0f, 1.0f, velocityValue), std::memory_order_relaxed);
    }

    float StepSequencer::getPitch (int index) const noexcept
    {
        return pitch[juce::jlimit (0, numSteps - 1, index)].load (std::memory_order_relaxed);
    }

    float StepSequencer::getVelocity (int index) const noexcept
    {
        return velocity[juce::jlimit (0, numSteps - 1, index)].load (std::memory_order_relaxed);
    }

    float StepSequencer::getPhase() const noexcept
    {
        if (samplesPerStep <= 0.0)
            return 0.0f;

        return (float) juce::jlimit (0.0, 1.0, samplesIntoStep / samplesPerStep);
    }

    void StepSequencer::advance() noexcept
    {
        currentStep = (currentStep + 1) % numSteps;
        samplesIntoStep = 0.0;
    }

    StepSequencer::Event StepSequencer::process (float tempoCv, float playGate, float advClock) noexcept
    {
        Event event;

        // A gate on PLAY/STOP latches the transport, like holding the button.
        const bool gateHigh = playGate > 0.5f;
        if (gateHigh != playGateHigh)
        {
            playGateHigh = gateHigh;

            if (gateHigh && ! playing)
            {
                samplesIntoStep = 0.0;
                needsInitialTrigger = true;
            }

            playing = gateHigh;
        }

        if (pendingTrigger)
        {
            pendingTrigger = false;
            event.triggered = true;
        }

        if (pendingAdvance)
        {
            pendingAdvance = false;
            advance();
            event.advanced = true;
            event.triggered = true;
        }

        // External clock: rising edge on ADV/CLOCK always steps.
        const bool advHigh = advClock > 0.5f;
        if (advHigh && ! lastAdvClock)
        {
            advance();
            event.advanced = true;
            event.triggered = true;
        }
        lastAdvClock = advHigh;

        const bool followsHost = clockSource == ClockSource::Midi;
        const bool runs = followsHost ? (playing && hostPlaying) : playing;

        if (! runs || clockSource == ClockSource::External)
            return event;

        double bpm = followsHost && hostBpm > 1.0 ? hostBpm : (double) tempoBpm;

        // TEMPO patch point: +/- one octave of speed at full CV.
        bpm *= std::pow (2.0, (double) juce::jlimit (-1.0f, 1.0f, tempoCv));

        samplesPerStep = sampleRate * 60.0 / juce::jmax (10.0, bpm)
                         * (double) scaleToBeats (scale);

        // The transport fires step 0 as soon as it starts, then every step
        // boundary lands exactly on the computed sample (6000 at 120 BPM 1/16).
        if (needsInitialTrigger)
        {
            needsInitialTrigger = false;
            event.triggered = true;
        }

        if (samplesIntoStep >= samplesPerStep)
        {
            samplesIntoStep -= samplesPerStep;
            currentStep = (currentStep + 1) % numSteps;
            event.advanced = true;
            event.triggered = true;
        }

        samplesIntoStep += 1.0;

        return event;
    }
}
