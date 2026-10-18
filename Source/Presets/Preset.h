#pragma once

#include "DSP/PatchBay.h"
#include "DSP/StepSequencer.h"

#include <juce_data_structures/juce_data_structures.h>

#include <array>
#include <map>

namespace ReplicantEdgy
{
    //==============================================================================
    /** Plain defaults shared by the parameter layout, the factory presets and
        the tests. Values are the same units the UI shows (Hz, seconds, dB). */
    namespace Defaults
    {
        inline constexpr float osc1Freq      = 0.42f;   // 0..1 log knob
        inline constexpr int   osc1Shape     = 0;       // triangle
        inline constexpr float osc1Env       = 0.10f;
        inline constexpr float osc1Level     = 0.85f;

        inline constexpr float osc2Freq      = 0.50f;
        inline constexpr int   osc2Shape     = 0;       // saw
        inline constexpr float osc2Env       = 0.00f;
        inline constexpr float osc2Level     = 0.70f;
        inline constexpr int   hardSync      = 0;
        inline constexpr float fmAmount      = 0.00f;

        inline constexpr float seqPitchMod   = 0.55f;
        inline constexpr int   seqPitchTarget = 2;      // both oscillators

        inline constexpr float egDecay       = 0.28f;

        inline constexpr int   noiseColour   = 0;       // pink
        inline constexpr float noiseLevel    = 0.00f;

        inline constexpr int   vcfMode       = 0;       // LP
        inline constexpr float cutoff        = 900.0f;  // Hz
        inline constexpr float resonance     = 0.30f;
        inline constexpr float vcfDecay      = 0.45f;
        inline constexpr float vcfEnv        = 0.35f;
        inline constexpr float noiseVcfMod   = 0.00f;

        inline constexpr int   vcaMode       = 0;       // fast
        inline constexpr float vcaDecay      = 0.32f;
        inline constexpr float volume        = -4.0f;   // dB

        inline constexpr float tempo         = 120.0f;
        // Factory patches ship host-synced: opening the editor in a DAW must
        // follow the DAW tempo without asking the user to flip the clock switch.
        inline constexpr int   clockSource   = 1;       // MIDI (host transport)
        inline constexpr int   scale         = 1;       // 1/16

        inline constexpr int   uiTheme       = 0;
    }

    //==============================================================================
    /** A complete patch: every panel control, the eight sequencer steps and the
        patch cables. This is what the preset browser stores and what the plugin
        state carries. */
    struct Preset
    {
        juce::String name = "Init";
        juce::String category = "User";
        std::map<juce::String, float> values;
        std::array<float, StepSequencer::numSteps> pitch {};
        std::array<float, StepSequencer::numSteps> velocity {};
        std::vector<Cable> cables;

        /** Fills `values` with the panel defaults. */
        static void fillDefaults (std::map<juce::String, float>& destination);

        /** A preset that matches the plugin's power-on state. */
        static Preset makeInit();

        juce::ValueTree toValueTree() const;
        static Preset fromValueTree (const juce::ValueTree& tree);

        float get (const juce::String& id, float fallback = 0.0f) const;
        void set (const juce::String& id, float value) { values[id] = value; }
    };
}
