#pragma once

#include "CommonEdgy.h"

#include <array>
#include <vector>

#include <juce_data_structures/juce_data_structures.h>

namespace ReplicantEdgy
{
    //==============================================================================
    /** The 24 patch points printed across the top of the Edge.

        White labels are inputs, black labels are outputs. Keeping the direction
        in the model (instead of the UI) is what makes a cable drag fail
        gracefully: output -> input only, never input -> input.
    */
    enum class Jack : int
    {
        // --- inputs (white silkscreen) ---------------------------------------
        Osc1Cv = 0,
        Osc2Cv,
        OscDecay,
        AdvClock,
        TriggerIn,
        VelocityIn,
        NoiseLevel,
        FmAmount,
        ExtAudio,
        TempoIn,
        PlayStop,
        VcfMod,
        VcfDecay,
        VcaCv,
        VcaDecay,

        // --- outputs (black silkscreen) --------------------------------------
        Osc1,
        Osc2,
        OscEg,
        TriggerOut,
        VelocityOut,
        Pitch,
        VcfEg,
        Vca,
        VcaEg,

        NumJacks
    };

    inline constexpr int numJacks = (int) Jack::NumJacks;
    inline constexpr int maxCables = 64;

    enum class JackSignal : int { Audio, Cv, Gate };

    struct JackInfo
    {
        const char* label;
        bool isInput;
        JackSignal signal;
    };

    const JackInfo& getJackInfo (Jack jack) noexcept;
    inline bool jackIsInput (Jack jack) noexcept { return getJackInfo (jack).isInput; }
    inline bool jackIsOutput (Jack jack) noexcept { return ! getJackInfo (jack).isInput; }
    inline const char* jackLabel (Jack jack) noexcept { return getJackInfo (jack).label; }

    //==============================================================================
    struct Cable
    {
        Jack source = Jack::Osc1;
        Jack destination = Jack::Osc1Cv;
        int colour = 0;

        bool operator== (const Cable& other) const noexcept
        {
            return source == other.source && destination == other.destination;
        }
    };

    /** Every value patched into an input, already summed. */
    struct PatchInputs
    {
        float osc1Cv = 0.0f;
        float osc2Cv = 0.0f;
        float oscDecay = 0.0f;
        float advClock = 0.0f;
        float trigger = 0.0f;
        float velocity = 0.0f;
        float noiseLevel = 0.0f;
        float fmAmount = 0.0f;
        float extAudio = 0.0f;
        float tempo = 0.0f;
        float playStop = 0.0f;
        float vcfMod = 0.0f;
        float vcfDecay = 0.0f;
        float vcaCv = 0.0f;
        float vcaDecay = 0.0f;
    };

    /** Every value available at an output jack. */
    struct PatchOutputs
    {
        float osc1 = 0.0f;
        float osc2 = 0.0f;
        float oscEg = 0.0f;
        float trigger = 0.0f;
        float velocity = 0.0f;
        float pitch = 0.0f;
        float vcfEg = 0.0f;
        float vca = 0.0f;
        float vcaEg = 0.0f;
    };

    //==============================================================================
    /** Cable storage + routing matrix.

        Message thread: add/remove/serialise.
        Audio thread: resolve() is lock free (it only reads a plain array), and
        the engine always feeds it the *previous* sample's outputs, so a feedback
        patch is a one-sample delay instead of an algebraic loop.
    */
    class PatchBay
    {
    public:
        PatchBay() = default;

        void clear() noexcept { cables.clear(); }

        /** Connects an output to an input. Returns false for an invalid pair,
            a duplicate or a full board. */
        bool connect (Jack source, Jack destination, int colour = -1);

        /** Removes every cable touching `jack`. Returns how many were removed. */
        int disconnect (Jack jack);

        /** Removes one cable by index. */
        void removeAt (int index);

        /** Copies the first cable feeding `jack`, for the UI. */
        bool cableInto (Jack jack, Cable& result) const noexcept;

        int getNumCables() const noexcept { return (int) cables.size(); }
        const Cable& getCable (int index) const noexcept { return cables[(size_t) index]; }
        const std::vector<Cable>& getCables() const noexcept { return cables; }

        void setCables (std::vector<Cable> newCables);

        /** Lock-free per-block snapshot: the audio thread copies the cables
            once, then resolves per sample without ever taking a lock. */
        struct Snapshot
        {
            std::array<Cable, maxCables> cables {};
            int count = 0;
        };

        Snapshot snapshot() const;

        static PatchInputs resolve (const Snapshot& snapshot,
                                    const PatchOutputs& outputs) noexcept;

        PatchInputs resolve (const PatchOutputs& outputs) const noexcept;

        juce::ValueTree toValueTree() const;
        void restoreFromValueTree (const juce::ValueTree& tree);

        static int nextColour (int previous) noexcept { return (previous + 1) % 8; }

    private:
        std::vector<Cable> cables;
        mutable juce::SpinLock lock;
    };
}
