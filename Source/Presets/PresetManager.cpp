#include "PresetManager.h"

namespace ReplicantEdgy
{
    namespace
    {
        const std::array<float, StepSequencer::numSteps> kDefaultPitch =
            { 0.00f, 0.00f, 0.25f, 0.00f, -0.15f, 0.00f, 0.35f, 0.00f };

        const std::array<float, StepSequencer::numSteps> kDefaultVelocity =
            { 0.90f, 0.55f, 0.75f, 0.50f, 0.85f, 0.55f, 0.70f, 0.50f };

        std::array<float, StepSequencer::numSteps> pitches (std::initializer_list<float> list)
        {
            std::array<float, StepSequencer::numSteps> result = kDefaultPitch;
            int i = 0;

            for (float value : list)
                if (i < StepSequencer::numSteps)
                    result[(size_t) i++] = value;

            return result;
        }

        std::array<float, StepSequencer::numSteps> velocities (std::initializer_list<float> list)
        {
            std::array<float, StepSequencer::numSteps> result = kDefaultVelocity;
            int i = 0;

            for (float value : list)
                if (i < StepSequencer::numSteps)
                    result[(size_t) i++] = value;

            return result;
        }

        Cable cable (Jack source, Jack destination, int colour = 0)
        {
            Cable c;
            c.source = source;
            c.destination = destination;
            c.colour = colour;
            return c;
        }

        juce::String sanitiseFileName (const juce::String& name)
        {
            auto safe = name.trim().replaceCharacter (' ', '_');
            safe = safe.retainCharacters ("abcdefghijklmnopqrstuvwxyz"
                                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                          "0123456789_-");

            return safe.isEmpty() ? juce::String ("Preset") : safe;
        }
    }

    //==============================================================================
    PresetManager::PresetManager() = default;

    void PresetManager::initialise()
    {
        buildFactoryPresets();
        reloadUserPresets();
    }

    juce::File PresetManager::getUserPresetDirectory() const
    {
        if (overrideDirectory != juce::File())
            return overrideDirectory;

        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("ReplicantEdgy")
                   .getChildFile ("Presets");
    }

    void PresetManager::setUserPresetDirectory (const juce::File& directory)
    {
        overrideDirectory = directory;
        reloadUserPresets();
    }

    void PresetManager::ensureUserDirectory() const
    {
        getUserPresetDirectory().createDirectory();
    }

    //==============================================================================
    const Preset& PresetManager::getPreset (int index) const
    {
        const int factoryCount = (int) factory.size();

        if (juce::isPositiveAndBelow (index, factoryCount))
            return factory[(size_t) index];

        const int userIndex = juce::jlimit (0, juce::jmax (0, (int) user.size() - 1),
                                            index - factoryCount);

        if (! user.empty())
            return user[(size_t) userIndex];

        return factory.front();
    }

    bool PresetManager::isUserPreset (int index) const noexcept
    {
        return index >= (int) factory.size() && index < getNumPresets();
    }

    juce::String PresetManager::getName (int index) const
    {
        return getPreset (index).name;
    }

    juce::String PresetManager::getCategory (int index) const
    {
        return getPreset (index).category;
    }

    juce::StringArray PresetManager::getCategories() const
    {
        juce::StringArray categories;

        for (const auto& preset : factory)
            categories.addIfNotAlreadyThere (preset.category);

        if (! user.empty())
            categories.addIfNotAlreadyThere ("User");

        return categories;
    }

    juce::StringArray PresetManager::getNames() const
    {
        juce::StringArray names;

        for (const auto& preset : factory)
            names.add (preset.name);

        for (const auto& preset : user)
            names.add (preset.name);

        return names;
    }

    //==============================================================================
    int PresetManager::saveUserPreset (const Preset& preset)
    {
        ensureUserDirectory();

        auto copy = preset;
        copy.name = copy.name.trim().isEmpty() ? juce::String ("Untitled") : copy.name.trim();
        copy.category = "User";

        auto file = getUserPresetDirectory().getChildFile (sanitiseFileName (copy.name) + ".edgy");
        int suffix = 2;

        while (file.existsAsFile())
        {
            // Overwrite only if the name matches exactly (STORE behaviour).
            const auto existing = juce::XmlDocument::parse (file);

            if (existing != nullptr)
            {
                const auto tree = juce::ValueTree::fromXml (*existing);

                if (tree.getProperty ("name").toString() == copy.name)
                    break;
            }

            file = getUserPresetDirectory()
                       .getChildFile (sanitiseFileName (copy.name) + "_" + juce::String (suffix++) + ".edgy");
        }

        if (auto xml = copy.toValueTree().createXml())
        {
            if (! xml->writeTo (file))
                return -1;
        }
        else
        {
            return -1;
        }

        reloadUserPresets();

        for (int i = 0; i < (int) user.size(); ++i)
            if (user[(size_t) i].name == copy.name)
                return (int) factory.size() + i;

        return getNumPresets() - 1;
    }

    bool PresetManager::deleteUserPreset (int index)
    {
        const int userIndex = index - (int) factory.size();

        if (! juce::isPositiveAndBelow (userIndex, (int) user.size())
             || ! juce::isPositiveAndBelow (userIndex, (int) userFiles.size()))
            return false;

        const bool removed = userFiles[(size_t) userIndex].deleteFile();
        reloadUserPresets();
        return removed;
    }

    bool PresetManager::renameUserPreset (int index, const juce::String& newName)
    {
        const int userIndex = index - (int) factory.size();

        if (! juce::isPositiveAndBelow (userIndex, (int) user.size()))
            return false;

        auto preset = user[(size_t) userIndex];
        preset.name = newName;
        return saveUserPreset (preset) >= 0;
    }

    //==============================================================================
    bool PresetManager::exportPreset (int index, const juce::File& file) const
    {
        const auto xml = getPreset (index).toValueTree().createXml();

        if (xml == nullptr)
            return false;

        file.getParentDirectory().createDirectory();
        return xml->writeTo (file);
    }

    bool PresetManager::importPreset (const juce::File& file, Preset& imported) const
    {
        const auto xml = juce::XmlDocument::parse (file);

        if (xml == nullptr || ! xml->hasTagName ("PRESET"))
            return false;

        imported = Preset::fromValueTree (juce::ValueTree::fromXml (*xml));
        imported.category = "User";
        return true;
    }

    //==============================================================================
    void PresetManager::reloadUserPresets()
    {
        user.clear();
        userFiles.clear();

        const auto directory = getUserPresetDirectory();

        if (! directory.isDirectory())
            return;

        for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "*.edgy"))
        {
            const auto file = entry.getFile();
            const auto xml = juce::XmlDocument::parse (file);

            if (xml == nullptr || ! xml->hasTagName ("PRESET"))
                continue;

            auto preset = Preset::fromValueTree (juce::ValueTree::fromXml (*xml));
            preset.category = "User";
            user.push_back (preset);
            userFiles.push_back (file);
        }

        // Stable alphabetical order, so the browser does not jump around.
        std::vector<size_t> order (user.size());

        for (size_t i = 0; i < order.size(); ++i)
            order[i] = i;

        std::sort (order.begin(), order.end(), [this] (size_t a, size_t b)
        {
            return user[a].name.compareIgnoreCase (user[b].name) < 0;
        });

        std::vector<Preset> sortedUser;
        std::vector<juce::File> sortedFiles;

        for (size_t i : order)
        {
            sortedUser.push_back (user[i]);
            sortedFiles.push_back (userFiles[i]);
        }

        user = std::move (sortedUser);
        userFiles = std::move (sortedFiles);
    }

    //==============================================================================
    void PresetManager::buildFactoryPresets()
    {
        factory.clear();

        auto make = [this] (const juce::String& name,
                            const juce::String& category,
                            std::initializer_list<std::pair<const char*, float>> overrides,
                            std::vector<Cable> cables,
                            std::array<float, StepSequencer::numSteps> stepPitch,
                            std::array<float, StepSequencer::numSteps> stepVelocity)
        {
            Preset preset = Preset::makeInit();
            preset.name = name;
            preset.category = category;

            for (const auto& entry : overrides)
                preset.values[entry.first] = entry.second;

            preset.cables = std::move (cables);
            preset.pitch = stepPitch;
            preset.velocity = stepVelocity;

            for (int i = 0; i < StepSequencer::numSteps; ++i)
            {
                preset.values[ParamID::stepPitch (i)] = stepPitch[(size_t) i];
                preset.values[ParamID::stepVelocity (i)] = stepVelocity[(size_t) i];
            }

            factory.push_back (std::move (preset));
        };

        // --- PERC -------------------------------------------------------------
        make ("Neon Pulse", "Perc",
              { { ParamID::osc1Freq, 0.34f }, { ParamID::osc1Env, 0.55f },
                { ParamID::osc2Level, 0.35f }, { ParamID::osc2Env, -0.20f },
                { ParamID::egDecay, 0.18f }, { ParamID::cutoff, 520.0f },
                { ParamID::resonance, 0.55f }, { ParamID::vcfEnv, 0.62f },
                { ParamID::vcfDecay, 0.22f }, { ParamID::vcaDecay, 0.22f },
                { ParamID::seqPitchMod, 0.62f }, { ParamID::tempo, 128.0f } },
              {}, pitches ({ 0.0f, 0.0f, 0.35f, 0.1f, -0.2f, 0.0f, 0.25f, 0.0f }),
              velocities ({ 0.95f, 0.45f, 0.7f, 0.4f, 0.85f, 0.45f, 0.65f, 0.4f }));

        make ("Chrome Rain", "Perc",
              { { ParamID::osc1Shape, 1.0f }, { ParamID::osc1Freq, 0.42f },
                { ParamID::osc1Env, 0.35f }, { ParamID::osc2Level, 0.55f },
                { ParamID::noiseLevel, 0.25f }, { ParamID::noiseVcfMod, 0.35f },
                { ParamID::cutoff, 780.0f }, { ParamID::resonance, 0.68f },
                { ParamID::egDecay, 0.14f }, { ParamID::vcfDecay, 0.16f },
                { ParamID::vcaDecay, 0.18f }, { ParamID::tempo, 134.0f } },
              {}, pitches ({ 0.0f, 0.4f, 0.0f, 0.4f, 0.2f, 0.0f, 0.4f, 0.2f }),
              velocities ({ 0.9f, 0.6f, 0.85f, 0.55f, 0.8f, 0.6f, 0.75f, 0.55f }));

        make ("Razor Wire", "Perc",
              { { ParamID::osc1Freq, 0.52f }, { ParamID::osc1Env, 0.8f },
                { ParamID::osc2Level, 0.5f }, { ParamID::hardSync, 1.0f },
                { ParamID::osc2Freq, 0.63f }, { ParamID::cutoff, 1400.0f },
                { ParamID::resonance, 0.82f }, { ParamID::vcfEnv, 0.5f },
                { ParamID::egDecay, 0.11f }, { ParamID::vcfDecay, 0.12f },
                { ParamID::vcaDecay, 0.14f }, { ParamID::tempo, 140.0f } },
              {}, pitches ({ 0.0f, 0.0f, 0.5f, 0.0f, 0.3f, 0.0f, 0.6f, 0.0f }),
              velocities ({ 0.95f, 0.5f, 0.85f, 0.45f, 0.9f, 0.5f, 0.8f, 0.45f }));

        make ("Voltage Drop", "Perc",
              { { ParamID::osc1Freq, 0.30f }, { ParamID::osc1Env, -0.75f },
                { ParamID::osc2Level, 0.25f }, { ParamID::egDecay, 0.26f },
                { ParamID::cutoff, 430.0f }, { ParamID::resonance, 0.45f },
                { ParamID::vcfEnv, 0.4f }, { ParamID::vcfDecay, 0.3f },
                { ParamID::vcaDecay, 0.3f }, { ParamID::tempo, 124.0f } },
              {}, pitches ({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }),
              velocities ({ 0.95f, 0.6f, 0.85f, 0.55f, 0.9f, 0.6f, 0.8f, 0.55f }));

        make ("Toxic Bloom", "Perc",
              { { ParamID::osc1Freq, 0.47f }, { ParamID::osc1Env, 0.3f },
                { ParamID::osc2Freq, 0.36f }, { ParamID::osc2Env, 0.2f },
                { ParamID::cutoff, 620.0f }, { ParamID::resonance, 0.6f },
                { ParamID::vcfEnv, 0.85f }, { ParamID::vcfDecay, 0.5f },
                { ParamID::egDecay, 0.3f }, { ParamID::vcaDecay, 0.4f },
                { ParamID::noiseLevel, 0.12f }, { ParamID::tempo, 118.0f } },
              {}, pitches ({ 0.0f, 0.15f, 0.3f, 0.15f, 0.0f, -0.15f, 0.2f, 0.0f }),
              velocities ({ 0.9f, 0.55f, 0.75f, 0.5f, 0.85f, 0.55f, 0.7f, 0.5f }));

        // --- BASS -------------------------------------------------------------
        make ("Acid Protocol", "Bass",
              { { ParamID::osc1Shape, 1.0f }, { ParamID::osc1Freq, 0.28f },
                { ParamID::osc1Env, 0.25f }, { ParamID::osc2Level, 0.55f },
                { ParamID::osc2Freq, 0.33f }, { ParamID::cutoff, 420.0f },
                { ParamID::resonance, 0.78f }, { ParamID::vcfEnv, 0.7f },
                { ParamID::vcfDecay, 0.28f }, { ParamID::egDecay, 0.22f },
                { ParamID::vcaDecay, 0.34f }, { ParamID::seqPitchMod, 0.8f },
                { ParamID::tempo, 132.0f } },
              {}, pitches ({ 0.0f, 0.0f, 0.4f, 0.0f, -0.25f, 0.0f, 0.5f, 0.0f }),
              velocities ({ 0.95f, 0.5f, 0.8f, 0.45f, 0.9f, 0.5f, 0.85f, 0.45f }));

        make ("Sub Level", "Bass",
              { { ParamID::osc1Freq, 0.18f }, { ParamID::osc1Env, 0.0f },
                { ParamID::osc2Level, 0.5f }, { ParamID::osc2Freq, 0.21f },
                { ParamID::cutoff, 240.0f }, { ParamID::resonance, 0.25f },
                { ParamID::vcfEnv, 0.25f }, { ParamID::vcfDecay, 0.6f },
                { ParamID::egDecay, 0.5f }, { ParamID::vcaDecay, 0.7f },
                { ParamID::volume, -3.0f }, { ParamID::tempo, 112.0f } },
              {}, pitches ({ 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.25f, 0.0f }),
              velocities ({ 0.95f, 0.7f, 0.85f, 0.7f, 0.9f, 0.7f, 0.85f, 0.7f }));

        make ("Chrome Sub", "Bass",
              { { ParamID::osc1Freq, 0.22f }, { ParamID::osc2Freq, 0.24f },
                { ParamID::osc2Level, 0.8f }, { ParamID::hardSync, 1.0f },
                { ParamID::cutoff, 320.0f }, { ParamID::resonance, 0.4f },
                { ParamID::vcfEnv, 0.35f }, { ParamID::vcfDecay, 0.4f },
                { ParamID::egDecay, 0.35f }, { ParamID::vcaDecay, 0.5f },
                { ParamID::tempo, 120.0f } },
              { cable (Jack::Osc2, Jack::Osc1Cv, 2) },
              pitches ({ 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f }),
              velocities ({ 0.9f, 0.6f, 0.8f, 0.6f, 0.9f, 0.6f, 0.8f, 0.6f }));

        make ("Dark Current", "Bass",
              { { ParamID::osc1Freq, 0.25f }, { ParamID::osc1Env, -0.3f },
                { ParamID::osc2Freq, 0.27f }, { ParamID::osc2Level, 0.4f },
                { ParamID::cutoff, 180.0f }, { ParamID::resonance, 0.55f },
                { ParamID::vcfEnv, 0.3f }, { ParamID::vcfDecay, 0.7f },
                { ParamID::egDecay, 0.6f }, { ParamID::vcaDecay, 0.9f },
                { ParamID::scale, 2.0f }, { ParamID::tempo, 96.0f } },
              {}, pitches ({ 0.0f, 0.0f, 0.15f, 0.0f, 0.0f, -0.1f, 0.15f, 0.0f }),
              velocities ({ 0.95f, 0.65f, 0.8f, 0.65f, 0.9f, 0.65f, 0.8f, 0.65f }));

        // --- LEAD -------------------------------------------------------------
        make ("Neon Lead", "Lead",
              { { ParamID::osc1Freq, 0.62f }, { ParamID::osc1Env, 0.2f },
                { ParamID::osc2Freq, 0.66f }, { ParamID::osc2Level, 0.7f },
                { ParamID::cutoff, 2400.0f }, { ParamID::resonance, 0.5f },
                { ParamID::vcfEnv, 0.4f }, { ParamID::egDecay, 0.35f },
                { ParamID::vcfDecay, 0.5f }, { ParamID::vcaDecay, 0.6f },
                { ParamID::tempo, 126.0f } },
              {}, pitches ({ 0.0f, 0.25f, 0.5f, 0.25f, 0.0f, -0.25f, 0.0f, 0.25f }),
              velocities ({ 0.9f, 0.7f, 0.85f, 0.65f, 0.9f, 0.7f, 0.85f, 0.65f }));

        make ("Ghost Wire", "Lead",
              { { ParamID::osc1Freq, 0.58f }, { ParamID::osc1Env, 0.4f },
                { ParamID::osc2Freq, 0.72f }, { ParamID::fmAmount, 0.55f },
                { ParamID::cutoff, 1800.0f }, { ParamID::resonance, 0.6f },
                { ParamID::vcfEnv, 0.5f }, { ParamID::egDecay, 0.25f },
                { ParamID::vcfDecay, 0.35f }, { ParamID::vcaDecay, 0.5f },
                { ParamID::tempo, 122.0f } },
              { cable (Jack::Osc2, Jack::FmAmount, 4) },
              pitches ({ 0.0f, 0.3f, 0.0f, 0.5f, 0.2f, 0.0f, 0.4f, 0.1f }),
              velocities ({ 0.9f, 0.6f, 0.8f, 0.55f, 0.85f, 0.6f, 0.75f, 0.55f }));

        make ("Laser Grid", "Lead",
              { { ParamID::osc1Shape, 1.0f }, { ParamID::osc1Freq, 0.66f },
                { ParamID::osc2Freq, 0.70f }, { ParamID::hardSync, 1.0f },
                { ParamID::cutoff, 3200.0f }, { ParamID::resonance, 0.72f },
                { ParamID::vcfEnv, 0.65f }, { ParamID::egDecay, 0.16f },
                { ParamID::vcfDecay, 0.2f }, { ParamID::vcaDecay, 0.24f },
                { ParamID::scale, 0.0f }, { ParamID::tempo, 138.0f } },
              {}, pitches ({ 0.0f, 0.5f, 0.0f, 0.35f, 0.0f, 0.6f, 0.0f, 0.35f }),
              velocities ({ 0.95f, 0.6f, 0.9f, 0.55f, 0.95f, 0.6f, 0.9f, 0.55f }));

        make ("Chrome Solo", "Lead",
              { { ParamID::osc1Freq, 0.55f }, { ParamID::osc1Env, 0.6f },
                { ParamID::osc2Freq, 0.60f }, { ParamID::osc2Level, 0.45f },
                { ParamID::cutoff, 1500.0f }, { ParamID::resonance, 0.35f },
                { ParamID::vcfEnv, 0.55f }, { ParamID::egDecay, 0.4f },
                { ParamID::vcfDecay, 0.6f }, { ParamID::vcaDecay, 0.8f } },
              {}, pitches ({ 0.0f, 0.2f, 0.4f, 0.6f, 0.4f, 0.2f, 0.0f, -0.2f }),
              velocities ({ 0.9f, 0.75f, 0.85f, 0.8f, 0.85f, 0.75f, 0.8f, 0.7f }));

        // --- NOISE ------------------------------------------------------------
        make ("Static Rain", "Noise",
              { { ParamID::noiseLevel, 0.9f }, { ParamID::osc1Level, 0.15f },
                { ParamID::osc2Level, 0.1f }, { ParamID::cutoff, 1200.0f },
                { ParamID::resonance, 0.65f }, { ParamID::noiseVcfMod, 0.55f },
                { ParamID::vcfEnv, 0.2f }, { ParamID::vcfDecay, 0.5f },
                { ParamID::egDecay, 0.3f }, { ParamID::vcaDecay, 0.4f },
                { ParamID::tempo, 116.0f } },
              {}, pitches ({ 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f }),
              velocities ({ 0.9f, 0.45f, 0.75f, 0.4f, 0.85f, 0.45f, 0.7f, 0.4f }));

        make ("Pink Corridor", "Noise",
              { { ParamID::noiseColour, 1.0f }, { ParamID::noiseLevel, 0.8f },
                { ParamID::osc1Level, 0.0f }, { ParamID::osc2Level, 0.0f },
                { ParamID::cutoff, 700.0f }, { ParamID::resonance, 0.8f },
                { ParamID::noiseVcfMod, 0.8f }, { ParamID::vcfEnv, 0.0f },
                { ParamID::vcfDecay, 0.9f }, { ParamID::vcaDecay, 0.6f },
                { ParamID::scale, 5.0f }, { ParamID::tempo, 104.0f } },
              {}, pitches ({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }),
              velocities ({ 0.85f, 0.5f, 0.7f, 0.45f, 0.8f, 0.5f, 0.7f, 0.45f }));

        make ("White Noise Hit", "Noise",
              { { ParamID::noiseColour, 1.0f }, { ParamID::noiseLevel, 1.0f },
                { ParamID::osc1Level, 0.0f }, { ParamID::osc2Level, 0.0f },
                { ParamID::cutoff, 4200.0f }, { ParamID::resonance, 0.3f },
                { ParamID::vcfEnv, 0.0f }, { ParamID::vcfDecay, 0.1f },
                { ParamID::egDecay, 0.1f }, { ParamID::vcaDecay, 0.12f },
                { ParamID::scale, 0.0f }, { ParamID::tempo, 140.0f } },
              {}, pitches ({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }),
              velocities ({ 1.0f, 0.5f, 0.9f, 0.4f, 0.95f, 0.5f, 0.85f, 0.4f }));

        // --- DRONE ------------------------------------------------------------
        make ("Reactor Hum", "Drone",
              { { ParamID::osc1Freq, 0.20f }, { ParamID::osc1Env, 0.15f },
                { ParamID::osc2Freq, 0.22f }, { ParamID::osc2Level, 0.6f },
                { ParamID::cutoff, 300.0f }, { ParamID::resonance, 0.55f },
                { ParamID::vcfEnv, 0.4f }, { ParamID::vcfDecay, 2.4f },
                { ParamID::egDecay, 2.0f }, { ParamID::vcaDecay, 3.0f },
                { ParamID::scale, 2.0f }, { ParamID::tempo, 70.0f } },
              {}, pitches ({ 0.0f, 0.0f, 0.1f, 0.0f, 0.0f, 0.0f, 0.1f, 0.0f }),
              velocities ({ 0.9f, 0.7f, 0.8f, 0.7f, 0.9f, 0.7f, 0.8f, 0.7f }));

        make ("Obsidian Drone", "Drone",
              { { ParamID::osc1Freq, 0.16f }, { ParamID::osc1Env, -0.4f },
                { ParamID::osc2Freq, 0.17f }, { ParamID::osc2Env, 0.3f },
                { ParamID::cutoff, 220.0f }, { ParamID::resonance, 0.75f },
                { ParamID::vcfEnv, 0.5f }, { ParamID::vcfDecay, 3.2f },
                { ParamID::egDecay, 2.8f }, { ParamID::vcaDecay, 4.0f },
                { ParamID::noiseLevel, 0.1f }, { ParamID::tempo, 64.0f } },
              {}, pitches ({ 0.0f, 0.0f, -0.1f, 0.0f, 0.1f, 0.0f, 0.0f, 0.0f }),
              velocities ({ 0.8f, 0.7f, 0.75f, 0.7f, 0.8f, 0.7f, 0.75f, 0.7f }));

        make ("Voltage Field", "Drone",
              { { ParamID::osc1Freq, 0.24f }, { ParamID::osc2Freq, 0.26f },
                { ParamID::osc2Level, 0.7f }, { ParamID::fmAmount, 0.3f },
                { ParamID::cutoff, 380.0f }, { ParamID::resonance, 0.6f },
                { ParamID::vcfEnv, 0.3f }, { ParamID::vcfDecay, 2.0f },
                { ParamID::egDecay, 1.6f }, { ParamID::vcaDecay, 2.4f },
                { ParamID::tempo, 76.0f } },
              { cable (Jack::Osc2, Jack::FmAmount, 5) },
              pitches ({ 0.0f, 0.0f, 0.15f, 0.0f, -0.15f, 0.0f, 0.15f, 0.0f }),
              velocities ({ 0.85f, 0.7f, 0.8f, 0.7f, 0.85f, 0.7f, 0.8f, 0.7f }));

        // --- FX ---------------------------------------------------------------
        make ("Cross Patch", "FX",
              { { ParamID::osc1Freq, 0.45f }, { ParamID::osc2Freq, 0.62f },
                { ParamID::fmAmount, 0.35f }, { ParamID::cutoff, 1200.0f },
                { ParamID::resonance, 0.7f }, { ParamID::tempo, 128.0f } },
              { cable (Jack::Osc2, Jack::FmAmount, 0), cable (Jack::Osc1, Jack::ExtAudio, 1) },
              pitches ({ 0.0f, 0.3f, 0.0f, 0.5f, 0.0f, 0.3f, 0.0f, 0.5f }),
              velocities ({ 0.9f, 0.6f, 0.8f, 0.55f, 0.9f, 0.6f, 0.8f, 0.55f }));

        make ("Velocity Filter", "FX",
              { { ParamID::osc1Freq, 0.4f }, { ParamID::cutoff, 500.0f },
                { ParamID::resonance, 0.75f }, { ParamID::vcfEnv, 0.0f },
                { ParamID::vcfDecay, 0.3f }, { ParamID::egDecay, 0.25f },
                { ParamID::vcaDecay, 0.3f }, { ParamID::tempo, 130.0f } },
              { cable (Jack::VelocityOut, Jack::VcfMod, 3) },
              pitches ({ 0.0f, 0.0f, 0.3f, 0.0f, -0.2f, 0.0f, 0.4f, 0.0f }),
              velocities ({ 0.35f, 0.95f, 0.5f, 0.85f, 0.4f, 0.9f, 0.55f, 0.8f }));

        make ("Envelope Sweep", "FX",
              { { ParamID::osc1Freq, 0.5f }, { ParamID::osc1Env, 0.0f },
                { ParamID::osc2Level, 0.4f }, { ParamID::cutoff, 900.0f },
                { ParamID::resonance, 0.5f }, { ParamID::vcfEnv, 0.5f },
                { ParamID::vcfDecay, 0.8f }, { ParamID::egDecay, 0.8f },
                { ParamID::vcaDecay, 0.6f }, { ParamID::tempo, 120.0f } },
              { cable (Jack::VcfEg, Jack::Osc1Cv, 6) },
              pitches ({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }),
              velocities ({ 0.9f, 0.6f, 0.8f, 0.55f, 0.9f, 0.6f, 0.8f, 0.55f }));

        make ("Feedback Grid", "FX",
              { { ParamID::osc1Freq, 0.38f }, { ParamID::cutoff, 600.0f },
                { ParamID::resonance, 0.85f }, { ParamID::vcfEnv, 0.3f },
                { ParamID::vcfDecay, 0.4f }, { ParamID::egDecay, 0.3f },
                { ParamID::vcaDecay, 0.45f }, { ParamID::volume, -8.0f },
                { ParamID::tempo, 126.0f } },
              { cable (Jack::Vca, Jack::ExtAudio, 7) },
              pitches ({ 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f }),
              velocities ({ 0.85f, 0.6f, 0.75f, 0.55f, 0.85f, 0.6f, 0.75f, 0.55f }));

        make ("Clock Divide", "FX",
              { { ParamID::osc1Freq, 0.4f }, { ParamID::cutoff, 800.0f },
                { ParamID::resonance, 0.6f }, { ParamID::vcfEnv, 0.4f },
                { ParamID::scale, 3.0f }, { ParamID::tempo, 124.0f } },
              { cable (Jack::TriggerOut, Jack::AdvClock, 2) },
              pitches ({ 0.0f, 0.4f, 0.0f, 0.0f, 0.4f, 0.0f, 0.4f, 0.0f }),
              velocities ({ 0.9f, 0.55f, 0.8f, 0.5f, 0.9f, 0.55f, 0.8f, 0.5f }));

        make ("Patch Lab", "FX",
              { { ParamID::osc1Freq, 0.40f }, { ParamID::osc2Freq, 0.58f },
                { ParamID::fmAmount, 0.25f }, { ParamID::cutoff, 900.0f },
                { ParamID::resonance, 0.62f }, { ParamID::vcfEnv, 0.35f },
                { ParamID::vcfDecay, 0.5f }, { ParamID::egDecay, 0.3f },
                { ParamID::vcaDecay, 0.5f }, { ParamID::noiseLevel, 0.15f },
                { ParamID::tempo, 128.0f } },
              { cable (Jack::Osc2, Jack::FmAmount, 0),
                cable (Jack::VcfEg, Jack::Osc1Cv, 1),
                cable (Jack::VelocityOut, Jack::VcfMod, 2),
                cable (Jack::Osc1, Jack::ExtAudio, 3),
                cable (Jack::VcaEg, Jack::VcaCv, 4) },
              pitches ({ 0.0f, 0.2f, 0.0f, 0.4f, 0.0f, -0.2f, 0.0f, 0.3f }),
              velocities ({ 0.9f, 0.55f, 0.8f, 0.5f, 0.9f, 0.55f, 0.8f, 0.5f }));

        make ("Rate Chaos", "FX",
              { { ParamID::osc1Freq, 0.33f }, { ParamID::cutoff, 640.0f },
                { ParamID::resonance, 0.7f }, { ParamID::vcfEnv, 0.3f },
                { ParamID::vcfDecay, 0.3f }, { ParamID::egDecay, 0.25f },
                { ParamID::vcaDecay, 0.3f }, { ParamID::tempo, 120.0f } },
              { cable (Jack::TriggerOut, Jack::TempoIn, 3) },
              pitches ({ 0.0f, 0.0f, 0.25f, 0.0f, 0.0f, 0.25f, 0.0f, 0.0f }),
              velocities ({ 0.9f, 0.6f, 0.8f, 0.55f, 0.9f, 0.6f, 0.8f, 0.55f }));
    }
}
