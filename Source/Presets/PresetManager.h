#pragma once

#include "Preset.h"

#include <vector>

namespace ReplicantEdgy
{
    //==============================================================================
    /** Factory bank + user presets on disk.

        Factory patches live in code and are always available. User patches are
        written as `.edgy` XML files under the application data directory, so
        saving and recalling your own sounds needs no project file. The same
        format is used for IMPORT / EXPORT, which makes patches shareable.
    */
    class PresetManager
    {
    public:
        PresetManager();

        void initialise();
        void reloadUserPresets();

        int getNumPresets() const noexcept { return (int) (factory.size() + user.size()); }
        int getNumFactory() const noexcept { return (int) factory.size(); }
        int getNumUser() const noexcept { return (int) user.size(); }

        const Preset& getPreset (int index) const;
        bool isUserPreset (int index) const noexcept;
        juce::String getName (int index) const;
        juce::String getCategory (int index) const;

        juce::StringArray getCategories() const;
        juce::StringArray getNames() const;

        //==========================================================================
        /** Writes a preset to the user bank. Returns its index, or -1. */
        int saveUserPreset (const Preset& preset);

        bool deleteUserPreset (int index);
        bool renameUserPreset (int index, const juce::String& newName);

        bool exportPreset (int index, const juce::File& file) const;
        bool importPreset (const juce::File& file, Preset& imported) const;

        juce::File getUserPresetDirectory() const;

        /** Redirects the user bank (tests, or a portable install). */
        void setUserPresetDirectory (const juce::File& directory);

    private:
        void buildFactoryPresets();
        void ensureUserDirectory() const;

        std::vector<Preset> factory;
        std::vector<Preset> user;
        std::vector<juce::File> userFiles;
        juce::File overrideDirectory;
    };
}
