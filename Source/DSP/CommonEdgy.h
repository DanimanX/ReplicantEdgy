#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>

namespace ReplicantEdgy
{
    //==============================================================================
    /** Small shared DSP helpers. Everything here is allocation free and safe to
        call from the audio thread. */
    namespace Math
    {
        inline constexpr float pi = 3.14159265358979323846f;
        inline constexpr float twoPi = 6.28318530717958647692f;

        inline float clamp (float v, float lo, float hi) noexcept
        {
            return v < lo ? lo : (v > hi ? hi : v);
        }

        inline float clamp01 (float v) noexcept { return clamp (v, 0.0f, 1.0f); }

        /** Bipolar -1..1 curve used by the ENVELOPE amount knobs. */
        inline float bipolar (float v) noexcept { return clamp (v, -1.0f, 1.0f); }

        /** Equal-tempered semitones to a frequency multiplier. */
        inline float semitonesToRatio (float semitones) noexcept
        {
            return std::pow (2.0f, semitones / 12.0f);
        }

        inline float hzToMidi (float hz) noexcept
        {
            return 69.0f + 12.0f * std::log2 (juce::jmax (1.0e-6f, hz) / 440.0f);
        }

        inline float midiToHz (float note) noexcept
        {
            return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f);
        }

        /** 0..1 knob to a logarithmic frequency range. */
        inline float logMap (float norm, float lo, float hi) noexcept
        {
            return lo * std::pow (hi / lo, clamp01 (norm));
        }

        /** Soft clipper used on audio buses so a wild patch can never explode. */
        inline float softClip (float v) noexcept
        {
            return std::tanh (juce::jlimit (-6.0f, 6.0f, v));
        }

        /** Maps -1..1 to 0..1 (used for unipolar sequencer pitch offsets). */
        inline float unipolar (float v) noexcept { return 0.5f * (v + 1.0f); }
    }

    //==============================================================================
    /** Parameter identifiers, kept in one place so the processor, the presets and
        the tests cannot drift apart. */
    namespace ParamID
    {
        inline constexpr const char* osc1Freq     = "osc1_freq";
        inline constexpr const char* osc1Shape    = "osc1_shape";
        inline constexpr const char* osc1Env      = "osc1_env";
        inline constexpr const char* osc1Level    = "osc1_level";

        inline constexpr const char* osc2Freq     = "osc2_freq";
        inline constexpr const char* osc2Shape    = "osc2_shape";
        inline constexpr const char* osc2Env      = "osc2_env";
        inline constexpr const char* osc2Level    = "osc2_level";
        inline constexpr const char* hardSync     = "hard_sync";
        inline constexpr const char* fmAmount     = "fm_amt";

        inline constexpr const char* seqPitchMod  = "seq_pitch_mod";
        inline constexpr const char* seqPitchTarget = "seq_pitch_target";

        inline constexpr const char* egDecay      = "eg_decay";

        inline constexpr const char* noiseColour  = "noise_colour";
        inline constexpr const char* noiseLevel   = "noise_level";

        inline constexpr const char* vcfMode      = "vcf_mode";
        inline constexpr const char* cutoff       = "cutoff";
        inline constexpr const char* resonance    = "resonance";
        inline constexpr const char* vcfDecay     = "vcf_decay";
        inline constexpr const char* vcfEnv       = "vcf_env";
        inline constexpr const char* noiseVcfMod  = "noise_vcf_mod";

        inline constexpr const char* vcaMode      = "vca_mode";
        inline constexpr const char* vcaDecay     = "vca_decay";
        inline constexpr const char* volume       = "volume";

        inline constexpr const char* tempo        = "tempo";
        inline constexpr const char* clockSource  = "clock_source";
        inline constexpr const char* scale        = "scale";

        inline constexpr const char* uiTheme      = "ui_theme";

        inline juce::String stepPitch (int step) { return "step" + juce::String (step + 1) + "_pitch"; }
        inline juce::String stepVelocity (int step) { return "step" + juce::String (step + 1) + "_vel"; }
    }

    //==============================================================================
    /** Sequencer scale divisions. The Edge offers straight and triplet values. */
    enum class ClockScale : int
    {
        Div32 = 0, Div16, Div8, Div4,
        Div4T, Div8T, Div16T, Div32T,
        NumScales
    };

    inline float scaleToBeats (ClockScale scale) noexcept
    {
        switch (scale)
        {
            case ClockScale::Div32:  return 0.125f;
            case ClockScale::Div16:  return 0.25f;
            case ClockScale::Div8:   return 0.5f;
            case ClockScale::Div4:   return 1.0f;
            case ClockScale::Div4T:  return 2.0f / 3.0f;
            case ClockScale::Div8T:  return 1.0f / 3.0f;
            case ClockScale::Div16T: return 1.0f / 6.0f;
            case ClockScale::Div32T: return 1.0f / 12.0f;
            default:                 return 0.25f;
        }
    }

    inline juce::StringArray getScaleNames()
    {
        return { "1/32", "1/16", "1/8", "1/4", "1/4T", "1/8T", "1/16T", "1/32T" };
    }

    //==============================================================================
    /** The three clock sources printed on the panel. EXT follows the
        ADV/CLOCK patch point, MIDI follows the host transport. */
    enum class ClockSource : int
    {
        Internal = 0,
        Midi     = 1,
        External = 2
    };

    inline juce::StringArray getClockSourceNames()
    {
        return { "INT", "MIDI", "EXT" };
    }
}
