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
        currentStep.store (0);
        playing.store (false);
        hostBpm.store (0.0, std::memory_order_relaxed);
        hostBpmValid.store (false, std::memory_order_relaxed);
        hostPlaying.store (false, std::memory_order_relaxed);
        hostAvailable.store (false, std::memory_order_relaxed);
        hostSeen.store (false, std::memory_order_relaxed);
        needsInitialTrigger = true;
        transportWasRunning = false;
        playGateHigh = false;
        lastAdvClock = false;
        pendingTrigger.store (false);
        pendingAdvance.store (false);
        samplesPerStep = sampleRate * 60.0 / juce::jmax (20.0f, tempoBpm.load())
                         * scaleToBeats ((ClockScale) scale.load());
    }

    void StepSequencer::setTempo (float bpm) noexcept
    {
        tempoBpm.store (juce::jlimit (20.0f, 300.0f, bpm));
    }

    void StepSequencer::setHostState (bool hostIsPlaying, double hostBpmValue) noexcept
    {
        hostPlaying.store (hostIsPlaying, std::memory_order_relaxed);

        // The standalone wrapper (and some hosts while stopped) return a
        // position without a BPM. Only latch real timing information; a missing
        // value must never erase the tempo the host already gave us.
        if (hostBpmValue > 1.0 && std::isfinite (hostBpmValue))
        {
            hostBpm.store (hostBpmValue, std::memory_order_relaxed);
            hostBpmValid.store (true, std::memory_order_relaxed);
            hostAvailable.store (true, std::memory_order_relaxed);
            hostSeen.store (true, std::memory_order_relaxed);
        }

        if (hostIsPlaying)
            hostAvailable.store (true, std::memory_order_relaxed);
    }

    void StepSequencer::setHostTransportAvailable (bool available) noexcept
    {
        hostAvailable.store (available, std::memory_order_relaxed);

        if (available)
            hostSeen.store (true, std::memory_order_relaxed);
        else
            hostPlaying.store (false, std::memory_order_relaxed);
    }

    bool StepSequencer::isPlaying() const noexcept
    {
        if (! followsHost())
            return playing.load (std::memory_order_relaxed);

        // MIDI mode is driven by the host. Only a plugin that has never seen a
        // host transport (the standalone app) falls back to its local button.
        return hostPlaying.load (std::memory_order_relaxed)
               || (! hostSeen.load (std::memory_order_relaxed)
                   && playing.load (std::memory_order_relaxed));
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
        currentStep.store ((currentStep.load() + 1) % numSteps);
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
            playing.store (gateHigh);
        }

        if (pendingTrigger.exchange (false))
            event.triggered = true;

        if (pendingAdvance.exchange (false))
        {
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

        const auto source = (ClockSource) clockSource.load();
        const bool followsHost = source == ClockSource::Midi;
        const bool hostRuns = hostPlaying.load (std::memory_order_relaxed);
        const bool hostEverSeen = hostSeen.load (std::memory_order_relaxed);
        const bool isPlayingNow = playing.load (std::memory_order_relaxed);

        // In MIDI mode the host transport is the master: pressing play in the
        // DAW starts the sequence, and its absence is not overruled by a stale
        // local PLAY state. Only a plugin that has never had a host transport
        // (the standalone wrapper) falls back to the local button.
        const bool runs = followsHost ? (hostRuns || (! hostEverSeen && isPlayingNow))
                                      : isPlayingNow;
        const auto currentScale = (ClockScale) scale.load();

        // Restart from step 0 on every rising edge of the transport.
        if (runs && ! transportWasRunning)
        {
            samplesIntoStep = 0.0;
            needsInitialTrigger = true;
        }

        transportWasRunning = runs;

        if (! runs || source == ClockSource::External)
            return event;

        double bpm = followsHost && hostBpmValid.load (std::memory_order_relaxed)
                         ? hostBpm.load (std::memory_order_relaxed)
                         : (double) tempoBpm.load();

        // TEMPO patch point: +/- one octave of speed at full CV.
        bpm *= std::pow (2.0, (double) juce::jlimit (-1.0f, 1.0f, tempoCv));

        samplesPerStep = sampleRate * 60.0 / juce::jmax (10.0, bpm)
                         * (double) scaleToBeats (currentScale);

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
            currentStep.store ((currentStep.load() + 1) % numSteps);
            event.advanced = true;
            event.triggered = true;
        }

        samplesIntoStep += 1.0;

        return event;
    }
}
