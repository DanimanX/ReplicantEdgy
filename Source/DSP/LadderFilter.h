#pragma once

#include "CommonEdgy.h"

namespace ReplicantEdgy
{
    //==============================================================================
    /** Four-pole transistor-ladder low-pass / high-pass filter.

        Each stage saturates with tanh, and the feedback path is taken from the
        fourth pole with the classic half-sample compensation. High-pass mode
        subtracts the ladder output from the input, which flips the response
        while keeping the same resonant character.
    */
    class LadderFilter
    {
    public:
        void prepare (double newSampleRate) noexcept
        {
            sampleRate = juce::jmax (1000.0, newSampleRate);
            reset();
        }

        void reset() noexcept
        {
            z1 = z2 = z3 = z4 = 0.0f;
            ic1 = ic2 = 0.0f;
            smoothedCutoff = cutoff;
        }

        void setCutoff (float hz) noexcept
        {
            cutoff = juce::jlimit (20.0f, (float) sampleRate * 0.45f, hz);
        }

        void setResonance (float amount01) noexcept
        {
            resonance = juce::jlimit (0.0f, 1.0f, amount01);
        }

        void setHighpass (bool shouldBeHighpass) noexcept { highpass = shouldBeHighpass; }

        float process (float input) noexcept
        {
            // Short smoothing keeps knob moves and CV sweeps free of zipper noise
            // without softening the percussive envelope.
            smoothedCutoff += 0.004f * (cutoff - smoothedCutoff);

            if (highpass)
            {
                // The ladder's own high-pass tap is a subtractive one and loses
                // its low-end rejection once the resonance is up. A TPT
                // state-variable high-pass keeps the 12 dB/oct slope and the
                // resonant peak the panel promises.
                const float g = std::tan (Math::pi * smoothedCutoff / (float) sampleRate);
                const float q = 0.5f + resonance * 9.0f;
                const float k = 1.0f / q;
                const float a1 = 1.0f / (1.0f + g * (g + k));
                const float a2 = g * a1;
                const float a3 = g * a2;

                const float v3 = input - ic2;
                const float v1 = a1 * ic1 + a2 * v3;
                const float v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = 2.0f * v1 - ic1;
                ic2 = 2.0f * v2 - ic2;

                ic1 = juce::jlimit (-8.0f, 8.0f, ic1);
                ic2 = juce::jlimit (-8.0f, 8.0f, ic2);

                return juce::jlimit (-4.0f, 4.0f, input - k * v1 - v2);
            }

            const float g = 1.0f - std::exp (-Math::twoPi * smoothedCutoff / (float) sampleRate);
            const float k = resonance * 4.2f;

            float x = input - k * (z4 - 0.5f * input);
            x = Math::softClip (x * 1.15f);

            z1 += g * (Math::softClip (x)   - Math::softClip (z1));
            z2 += g * (Math::softClip (z1)  - Math::softClip (z2));
            z3 += g * (Math::softClip (z2)  - Math::softClip (z3));
            z4 += g * (Math::softClip (z3)  - Math::softClip (z4));

            // Resonance steals low-end; put a little of it back.
            return z4 * (1.0f + resonance * 0.35f);
        }

    private:
        double sampleRate = 44100.0;
        float cutoff = 800.0f;
        float smoothedCutoff = 800.0f;
        float resonance = 0.2f;
        float z1 = 0.0f, z2 = 0.0f, z3 = 0.0f, z4 = 0.0f;
        float ic1 = 0.0f, ic2 = 0.0f;
        bool highpass = false;
    };
}
