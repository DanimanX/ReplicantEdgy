#pragma once

#include "CommonEdgy.h"

namespace ReplicantEdgy
{
    //==============================================================================
    /** White / pink noise generator.

        Pink is the Paul Kellet economy filter: three one-pole sections that
        approximate -3 dB/octave. It is the colour the Edge labels PINK and the
        one the hardware uses for its filter modulation trick.
    */
    class NoiseEdgy
    {
    public:
        enum class Colour : int { Pink = 0, White = 1 };

        void prepare (double newSampleRate) noexcept
        {
            sampleRate = juce::jmax (1000.0, newSampleRate);
            reset();
        }

        void reset() noexcept
        {
            b0 = b1 = b2 = 0.0f;
            pink = 0.0f;
            randomState = 0x9e3779b9u;
        }

        void setColour (Colour newColour) noexcept { colour = newColour; }
        Colour getColour() const noexcept { return colour; }

        float process() noexcept
        {
            const float white = nextWhite();

            if (colour == Colour::White)
                return white;

            b0 = 0.99765f * b0 + white * 0.0990460f;
            b1 = 0.96300f * b1 + white * 0.2965164f;
            b2 = 0.57000f * b2 + white * 1.0526913f;
            pink = b0 + b1 + b2 + white * 0.1848f;

            // Normalise back to roughly +/-1.
            return juce::jlimit (-1.0f, 1.0f, pink * 0.32f);
        }

    private:
        float nextWhite() noexcept
        {
            // xorshift32: deterministic, fast, no allocation.
            randomState ^= randomState << 13;
            randomState ^= randomState >> 17;
            randomState ^= randomState << 5;
            return (float) ((juce::int32) randomState) * (1.0f / 2147483648.0f);
        }

        double sampleRate = 44100.0;
        Colour colour = Colour::Pink;
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, pink = 0.0f;
        juce::uint32 randomState = 0x9e3779b9u;
    };
}
