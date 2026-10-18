#pragma once

#include "CommonEdgy.h"

namespace ReplicantEdgy
{
    //==============================================================================
    /** Decay-only envelope, exactly like the three EGs on the Edge: a trigger
        jumps the level to the velocity and it falls away from there.

        The VCA has a FAST / SLOW switch on the panel. FAST uses a steep
        exponential for the snappy percussion decay; SLOW keeps more energy in
        the tail for long, breathing notes.
    */
    class DecayEnvelope
    {
    public:
        void prepare (double newSampleRate) noexcept
        {
            sampleRate = juce::jmax (1000.0, newSampleRate);
            reset();
        }

        void reset() noexcept
        {
            level = 0.0f;
            active = false;
        }

        void setDecay (float seconds) noexcept
        {
            decay = juce::jlimit (0.010f, 6.0f, seconds);
        }

        float getDecay() const noexcept { return decay; }

        void setFastMode (bool shouldBeFast) noexcept { fast = shouldBeFast; }

        /** Fires the envelope. `amount` is the velocity, 0..1. */
        void trigger (float amount = 1.0f) noexcept
        {
            level = juce::jlimit (0.0f, 1.5f, amount);
            active = level > 0.0001f;
        }

        float process() noexcept
        {
            if (! active)
                return 0.0f;

            // Time to -60 dB, shaped by the VCA mode.
            const float timeScale = fast ? 0.38f : 1.0f;
            const float coeff = std::exp (-6.9f / (decay * timeScale * (float) sampleRate));
            level *= coeff;

            if (level < 1.0e-4f)
            {
                level = 0.0f;
                active = false;
            }

            return level;
        }

        float getValue() const noexcept { return level; }
        bool isActive() const noexcept { return active; }

    private:
        double sampleRate = 44100.0;
        float decay = 0.4f;
        float level = 0.0f;
        bool active = false;
        bool fast = false;
    };
}
