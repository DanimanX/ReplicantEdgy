#pragma once

#include "CommonEdgy.h"

namespace ReplicantEdgy
{
    //==============================================================================
    /** Band-limited oscillator with the three shapes the Edge exposes:
        triangle (VCO 1), square (both) and saw (VCO 2).

        Saw and square use PolyBLEP to tame the aliasing; the triangle is
        derived from the square through a one-pole integrator, which gives the
        soft, slightly rounded tone of the hardware without the shrill edges of
        a naive triangle.
    */
    class OscillatorEdgy
    {
    public:
        enum class Shape : int
        {
            Triangle = 0,
            Square,
            Saw
        };

        void prepare (double newSampleRate) noexcept
        {
            sampleRate = juce::jmax (1000.0, newSampleRate);
            reset();
        }

        void reset() noexcept
        {
            phase = 0.0;
            wrapped = false;
            triangleState = 0.0f;
        }

        void setShape (Shape newShape) noexcept { shape = newShape; }
        Shape getShape() const noexcept { return shape; }

        void setFrequency (float hz) noexcept
        {
            frequency = juce::jlimit (0.01f, (float) sampleRate * 0.45f, hz);
            increment = (double) frequency / sampleRate;
        }

        float getFrequency() const noexcept { return frequency; }

        /** One sample. `fmCycles` adds phase modulation (cycles per sample),
            which is how the 1-2 FM patch point and the oscillator envelope
            bend the pitch. */
        float process (double fmCycles = 0.0) noexcept
        {
            wrapped = false;

            const double inc = juce::jlimit (-0.45, 0.45, increment + fmCycles);
            phase += inc;

            if (phase >= 1.0)
            {
                phase -= 1.0;
                wrapped = true;
            }
            else if (phase < 0.0)
            {
                phase += 1.0;
                wrapped = true;
            }

            const double t = phase;
            const double dt = juce::jlimit (1.0e-7, 0.45, std::abs (inc));

            switch (shape)
            {
                case Shape::Saw:
                    return (float) (2.0 * t - 1.0 - polyBlep (t, dt));

                case Shape::Square:
                {
                    float value = t < 0.5 ? 1.0f : -1.0f;
                    value += (float) polyBlep (t, dt);
                    value -= (float) polyBlep (std::fmod (t + 0.5, 1.0), dt);
                    return value;
                }

                case Shape::Triangle:
                default:
                {
                    float square = t < 0.5 ? 1.0f : -1.0f;
                    square += (float) polyBlep (t, dt);
                    square -= (float) polyBlep (std::fmod (t + 0.5, 1.0), dt);

                    // One-pole integrator: the classic analogue triangle shaper.
                    const float coeff = (float) juce::jlimit (0.02, 0.6,
                                                              220.0 / sampleRate * (frequency / 220.0f + 1.0f));
                    triangleState += coeff * (square - triangleState);
                    return triangleState * 2.2f;
                }
            }
        }

        /** True when the last process() call wrapped the phase (hard sync). */
        bool didWrap() const noexcept { return wrapped; }

        /** Hard-sync target: jumps back to the start of the cycle. */
        void resetPhase (double newPhase = 0.0) noexcept
        {
            phase = newPhase;
            wrapped = true;
        }

    private:
        static double polyBlep (double t, double dt) noexcept
        {
            if (t < dt)
            {
                const double x = t / dt;
                return x + x - x * x - 1.0;
            }

            if (t > 1.0 - dt)
            {
                const double x = (t - 1.0) / dt;
                return x * x + x + x + 1.0;
            }

            return 0.0;
        }

        double sampleRate = 44100.0;
        double phase = 0.0;
        double increment = 0.0;
        double frequency = 440.0;
        float triangleState = 0.0f;
        bool wrapped = false;
        Shape shape = Shape::Saw;
    };
}
