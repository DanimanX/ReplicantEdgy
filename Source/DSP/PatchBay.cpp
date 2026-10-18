#include "PatchBay.h"

namespace ReplicantEdgy
{
    namespace
    {
        const JackInfo jackTable[numJacks] =
        {
            // inputs ---------------------------------------------------------
            { "OSC1 CV",     true,  JackSignal::Cv    },
            { "OSC2 CV",     true,  JackSignal::Cv    },
            { "OSC DECAY",   true,  JackSignal::Cv    },
            { "ADV/CLOCK",   true,  JackSignal::Gate  },
            { "TRIGGER",     true,  JackSignal::Gate  },
            { "VELOCITY",    true,  JackSignal::Cv    },
            { "NOISE LEVEL", true,  JackSignal::Cv    },
            { "1-2 FM AMT",  true,  JackSignal::Cv    },
            { "EXT AUDIO",   true,  JackSignal::Audio },
            { "TEMPO",       true,  JackSignal::Cv    },
            { "PLAY/STOP",   true,  JackSignal::Gate  },
            { "VCF MOD",     true,  JackSignal::Cv    },
            { "VCF DECAY",   true,  JackSignal::Cv    },
            { "VCA CV",      true,  JackSignal::Cv    },
            { "VCA DECAY",   true,  JackSignal::Cv    },

            // outputs --------------------------------------------------------
            { "OSC1",        false, JackSignal::Audio },
            { "OSC2",        false, JackSignal::Audio },
            { "OSC EG",      false, JackSignal::Cv    },
            { "TRIGGER",     false, JackSignal::Gate  },
            { "VELOCITY",    false, JackSignal::Cv    },
            { "PITCH",       false, JackSignal::Cv    },
            { "VCF EG",      false, JackSignal::Cv    },
            { "VCA",         false, JackSignal::Audio },
            { "VCA EG",      false, JackSignal::Cv    }
        };
    }

    const JackInfo& getJackInfo (Jack jack) noexcept
    {
        return jackTable[juce::jlimit (0, numJacks - 1, (int) jack)];
    }

    //==============================================================================
    bool PatchBay::connect (Jack source, Jack destination, int colour)
    {
        if (! jackIsOutput (source) || ! jackIsInput (destination))
            return false;

        const juce::SpinLock::ScopedLockType scoped (lock);

        if (source == Jack::Vca && destination == Jack::ExtAudio)
        {
            // Allowed, but only as a one-sample feedback path; the engine clamps
            // it. Kept explicit here so the rule is documented rather than
            // discovered by a screaming audience.
        }

        if ((int) cables.size() >= maxCables)
            return false;

        for (const auto& cable : cables)
            if (cable.source == source && cable.destination == destination)
                return false;

        Cable cable;
        cable.source = source;
        cable.destination = destination;
        cable.colour = colour >= 0 ? (colour % 8)
                                   : nextColour (cables.empty() ? 0 : cables.back().colour);
        cables.push_back (cable);
        return true;
    }

    int PatchBay::disconnect (Jack jack)
    {
        const juce::SpinLock::ScopedLockType scoped (lock);
        const auto before = cables.size();

        cables.erase (std::remove_if (cables.begin(), cables.end(),
                                      [jack] (const Cable& cable)
                                      {
                                          return cable.source == jack || cable.destination == jack;
                                      }),
                      cables.end());

        return (int) (before - cables.size());
    }

    void PatchBay::removeAt (int index)
    {
        const juce::SpinLock::ScopedLockType scoped (lock);

        if (juce::isPositiveAndBelow (index, (int) cables.size()))
            cables.erase (cables.begin() + index);
    }

    bool PatchBay::cableInto (Jack jack, Cable& result) const noexcept
    {
        const juce::SpinLock::ScopedLockType scoped (lock);

        for (const auto& cable : cables)
        {
            if (cable.destination == jack)
            {
                result = cable;
                return true;
            }
        }

        return false;
    }

    void PatchBay::setCables (std::vector<Cable> newCables)
    {
        {
            const juce::SpinLock::ScopedLockType scoped (lock);
            cables.clear();
        }

        for (const auto& cable : newCables)
            connect (cable.source, cable.destination, cable.colour);
    }

    //==============================================================================
    PatchBay::Snapshot PatchBay::snapshot() const
    {
        const juce::SpinLock::ScopedLockType scoped (lock);

        Snapshot result;
        result.count = juce::jmin ((int) cables.size(), maxCables);

        for (int i = 0; i < result.count; ++i)
            result.cables[(size_t) i] = cables[(size_t) i];

        return result;
    }

    PatchInputs PatchBay::resolve (const PatchOutputs& outputs) const noexcept
    {
        return resolve (snapshot(), outputs);
    }

    PatchInputs PatchBay::resolve (const Snapshot& snapshot,
                                   const PatchOutputs& outputs) noexcept
    {
        PatchInputs inputs;

        auto read = [&outputs] (Jack jack) noexcept -> float
        {
            switch (jack)
            {
                case Jack::Osc1:        return outputs.osc1;
                case Jack::Osc2:        return outputs.osc2;
                case Jack::OscEg:       return outputs.oscEg;
                case Jack::TriggerOut:  return outputs.trigger;
                case Jack::VelocityOut: return outputs.velocity;
                case Jack::Pitch:       return outputs.pitch;
                case Jack::VcfEg:       return outputs.vcfEg;
                case Jack::Vca:         return outputs.vca;
                case Jack::VcaEg:       return outputs.vcaEg;
                default:                return 0.0f;
            }
        };

        for (int i = 0; i < snapshot.count; ++i)
        {
            const auto& cable = snapshot.cables[(size_t) i];
            const float value = read (cable.source);

            switch (cable.destination)
            {
                case Jack::Osc1Cv:    inputs.osc1Cv    += value; break;
                case Jack::Osc2Cv:    inputs.osc2Cv    += value; break;
                case Jack::OscDecay:  inputs.oscDecay  += value; break;
                case Jack::AdvClock:  inputs.advClock  += value; break;
                case Jack::TriggerIn: inputs.trigger   += value; break;
                case Jack::VelocityIn: inputs.velocity += value; break;
                case Jack::NoiseLevel: inputs.noiseLevel += value; break;
                case Jack::FmAmount:  inputs.fmAmount  += value; break;
                case Jack::ExtAudio:  inputs.extAudio  += value; break;
                case Jack::TempoIn:   inputs.tempo     += value; break;
                case Jack::PlayStop:  inputs.playStop  += value; break;
                case Jack::VcfMod:    inputs.vcfMod    += value; break;
                case Jack::VcfDecay:  inputs.vcfDecay  += value; break;
                case Jack::VcaCv:     inputs.vcaCv     += value; break;
                case Jack::VcaDecay:  inputs.vcaDecay  += value; break;
                default: break;
            }
        }

        // A cable can sum several outputs; clamp so a wild patch stays musical
        // instead of slamming the oscillators into the rails.
        inputs.osc1Cv   = juce::jlimit (-2.0f, 2.0f, inputs.osc1Cv);
        inputs.osc2Cv   = juce::jlimit (-2.0f, 2.0f, inputs.osc2Cv);
        inputs.oscDecay = juce::jlimit (-1.0f, 1.0f, inputs.oscDecay);
        inputs.velocity = juce::jlimit (-1.0f, 1.0f, inputs.velocity);
        inputs.noiseLevel = juce::jlimit (-1.0f, 1.0f, inputs.noiseLevel);
        inputs.fmAmount = juce::jlimit (-1.0f, 1.0f, inputs.fmAmount);
        inputs.extAudio = juce::jlimit (-4.0f, 4.0f, inputs.extAudio);
        inputs.tempo    = juce::jlimit (-1.0f, 1.0f, inputs.tempo);
        inputs.vcfMod   = juce::jlimit (-2.0f, 2.0f, inputs.vcfMod);
        inputs.vcfDecay = juce::jlimit (-1.0f, 1.0f, inputs.vcfDecay);
        inputs.vcaCv    = juce::jlimit (-2.0f, 2.0f, inputs.vcaCv);
        inputs.vcaDecay = juce::jlimit (-1.0f, 1.0f, inputs.vcaDecay);
        inputs.advClock = juce::jlimit (-1.0f, 1.0f, inputs.advClock);
        inputs.trigger  = juce::jlimit (-1.0f, 1.0f, inputs.trigger);
        inputs.playStop = juce::jlimit (-1.0f, 1.0f, inputs.playStop);

        return inputs;
    }

    //==============================================================================
    juce::ValueTree PatchBay::toValueTree() const
    {
        const juce::SpinLock::ScopedLockType scoped (lock);
        juce::ValueTree tree ("PATCH");

        for (const auto& cable : cables)
        {
            juce::ValueTree node ("CABLE");
            node.setProperty ("source", (int) cable.source, nullptr);
            node.setProperty ("destination", (int) cable.destination, nullptr);
            node.setProperty ("colour", cable.colour, nullptr);
            tree.appendChild (node, nullptr);
        }

        return tree;
    }

    void PatchBay::restoreFromValueTree (const juce::ValueTree& tree)
    {
        clear();

        if (! tree.isValid() || ! tree.hasType ("PATCH"))
            return;

        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            const auto child = tree.getChild (i);

            if (! child.hasType ("CABLE"))
                continue;

            const int source = (int) child.getProperty ("source", -1);
            const int destination = (int) child.getProperty ("destination", -1);

            if (! juce::isPositiveAndBelow (source, numJacks)
                 || ! juce::isPositiveAndBelow (destination, numJacks))
                continue;

            connect ((Jack) source, (Jack) destination,
                     (int) child.getProperty ("colour", 0));
        }
    }
}
