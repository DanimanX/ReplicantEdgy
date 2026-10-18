#include "Preset.h"

namespace ReplicantEdgy
{
    void Preset::fillDefaults (std::map<juce::String, float>& destination)
    {
        destination.clear();

        destination[ParamID::osc1Freq]  = Defaults::osc1Freq;
        destination[ParamID::osc1Shape] = (float) Defaults::osc1Shape;
        destination[ParamID::osc1Env]   = Defaults::osc1Env;
        destination[ParamID::osc1Level] = Defaults::osc1Level;

        destination[ParamID::osc2Freq]  = Defaults::osc2Freq;
        destination[ParamID::osc2Shape] = (float) Defaults::osc2Shape;
        destination[ParamID::osc2Env]   = Defaults::osc2Env;
        destination[ParamID::osc2Level] = Defaults::osc2Level;
        destination[ParamID::hardSync]  = (float) Defaults::hardSync;
        destination[ParamID::fmAmount]  = Defaults::fmAmount;

        destination[ParamID::seqPitchMod]    = Defaults::seqPitchMod;
        destination[ParamID::seqPitchTarget] = (float) Defaults::seqPitchTarget;
        destination[ParamID::egDecay]        = Defaults::egDecay;

        destination[ParamID::noiseColour] = (float) Defaults::noiseColour;
        destination[ParamID::noiseLevel]  = Defaults::noiseLevel;

        destination[ParamID::vcfMode]     = (float) Defaults::vcfMode;
        destination[ParamID::cutoff]      = Defaults::cutoff;
        destination[ParamID::resonance]   = Defaults::resonance;
        destination[ParamID::vcfDecay]    = Defaults::vcfDecay;
        destination[ParamID::vcfEnv]      = Defaults::vcfEnv;
        destination[ParamID::noiseVcfMod] = Defaults::noiseVcfMod;

        destination[ParamID::vcaMode]   = (float) Defaults::vcaMode;
        destination[ParamID::vcaDecay]  = Defaults::vcaDecay;
        destination[ParamID::volume]    = Defaults::volume;

        destination[ParamID::tempo]       = Defaults::tempo;
        destination[ParamID::clockSource] = (float) Defaults::clockSource;
        destination[ParamID::scale]       = (float) Defaults::scale;

        for (int i = 0; i < StepSequencer::numSteps; ++i)
        {
            destination[ParamID::stepPitch (i)] = 0.0f;
            destination[ParamID::stepVelocity (i)] = 0.65f;
        }
    }

    Preset Preset::makeInit()
    {
        Preset preset;
        preset.name = "Init";
        preset.category = "Init";
        fillDefaults (preset.values);

        // A simple rising line so the Init patch makes sound out of the box.
        const float pitches[StepSequencer::numSteps] =
            { 0.0f, 0.0f, 0.25f, 0.0f, -0.15f, 0.0f, 0.35f, 0.0f };
        const float velocities[StepSequencer::numSteps] =
            { 0.90f, 0.55f, 0.75f, 0.50f, 0.85f, 0.55f, 0.70f, 0.50f };

        for (int i = 0; i < StepSequencer::numSteps; ++i)
        {
            preset.pitch[(size_t) i] = pitches[i];
            preset.velocity[(size_t) i] = velocities[i];
            preset.values[ParamID::stepPitch (i)] = pitches[i];
            preset.values[ParamID::stepVelocity (i)] = velocities[i];
        }

        return preset;
    }

    float Preset::get (const juce::String& id, float fallback) const
    {
        const auto it = values.find (id);
        return it != values.end() ? it->second : fallback;
    }

    //==============================================================================
    juce::ValueTree Preset::toValueTree() const
    {
        juce::ValueTree tree ("PRESET");
        tree.setProperty ("name", name, nullptr);
        tree.setProperty ("category", category, nullptr);

        juce::ValueTree parameters ("PARAMETERS");

        for (const auto& entry : values)
            parameters.setProperty (juce::Identifier (entry.first), (double) entry.second, nullptr);

        tree.appendChild (parameters, nullptr);

        juce::ValueTree steps ("SEQUENCE");

        for (int i = 0; i < StepSequencer::numSteps; ++i)
        {
            juce::ValueTree step ("STEP");
            step.setProperty ("index", i, nullptr);
            step.setProperty ("pitch", (double) pitch[(size_t) i], nullptr);
            step.setProperty ("velocity", (double) velocity[(size_t) i], nullptr);
            steps.appendChild (step, nullptr);
        }

        tree.appendChild (steps, nullptr);

        juce::ValueTree patch ("PATCH");

        for (const auto& cable : cables)
        {
            juce::ValueTree node ("CABLE");
            node.setProperty ("source", (int) cable.source, nullptr);
            node.setProperty ("destination", (int) cable.destination, nullptr);
            node.setProperty ("colour", cable.colour, nullptr);
            patch.appendChild (node, nullptr);
        }

        tree.appendChild (patch, nullptr);
        return tree;
    }

    Preset Preset::fromValueTree (const juce::ValueTree& tree)
    {
        Preset preset;
        preset.name = tree.getProperty ("name", "Preset").toString();
        preset.category = tree.getProperty ("category", "User").toString();
        fillDefaults (preset.values);

        const auto parameters = tree.getChildWithName ("PARAMETERS");

        if (parameters.isValid())
        {
            for (int i = 0; i < parameters.getNumProperties(); ++i)
            {
                const auto property = parameters.getPropertyName (i);
                preset.values[property.toString()] = (float) (double) parameters.getProperty (property);
            }
        }

        const auto steps = tree.getChildWithName ("SEQUENCE");

        if (steps.isValid())
        {
            for (int i = 0; i < steps.getNumChildren(); ++i)
            {
                const auto step = steps.getChild (i);
                const int index = (int) step.getProperty ("index", i);

                if (! juce::isPositiveAndBelow (index, StepSequencer::numSteps))
                    continue;

                preset.pitch[(size_t) index] = (float) (double) step.getProperty ("pitch", 0.0);
                preset.velocity[(size_t) index] = (float) (double) step.getProperty ("velocity", 0.65);
            }
        }

        // Older files may not carry per-step parameters; keep them in sync.
        for (int i = 0; i < StepSequencer::numSteps; ++i)
        {
            preset.values[ParamID::stepPitch (i)] = preset.pitch[(size_t) i];
            preset.values[ParamID::stepVelocity (i)] = preset.velocity[(size_t) i];
        }

        const auto patch = tree.getChildWithName ("PATCH");

        if (patch.isValid())
        {
            for (int i = 0; i < patch.getNumChildren(); ++i)
            {
                const auto node = patch.getChild (i);

                if (! node.hasType ("CABLE"))
                    continue;

                Cable cable;
                cable.source = (Jack) (int) node.getProperty ("source", 0);
                cable.destination = (Jack) (int) node.getProperty ("destination", 0);
                cable.colour = (int) node.getProperty ("colour", 0);

                if (jackIsOutput (cable.source) && jackIsInput (cable.destination))
                    preset.cables.push_back (cable);
            }
        }

        return preset;
    }
}
