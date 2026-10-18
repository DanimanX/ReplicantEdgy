//==============================================================================
// Writes the factory presets as `.edgy` files.
//
//   generate_presets [output-directory]
//
// The files are produced by the same PresetManager the plugin uses, so the
// checked-in examples cannot drift from the factory bank.
//==============================================================================

#include "Presets/PresetManager.h"

#include <iostream>

int main (int argc, char* argv[])
{
    const juce::File output = (argc > 1)
        ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
        : juce::File::getCurrentWorkingDirectory().getChildFile ("presets");

    juce::ScopedJuceInitialiser_GUI gui;

    ReplicantEdgy::PresetManager manager;
    manager.setUserPresetDirectory (juce::File());   // read-only factory dump
    manager.initialise();

    // Drop stale examples so a renamed preset cannot leave an orphan behind.
    for (const auto& entry : juce::RangedDirectoryIterator (output, true, "*.edgy"))
        entry.getFile().deleteFile();

    int written = 0;

    for (int i = 0; i < manager.getNumFactory(); ++i)
    {
        const auto preset = manager.getPreset (i);
        auto category = preset.category;
        category = category.retainCharacters ("abcdefghijklmnopqrstuvwxyz"
                                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ");

        auto name = preset.name.replaceCharacter (' ', '_');
        name = name.retainCharacters ("abcdefghijklmnopqrstuvwxyz"
                                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                      "0123456789_-");

        if (name.isEmpty())
            name = "Preset";

        auto directory = output.getChildFile (category);
        directory.createDirectory();

        const auto file = directory.getChildFile (
            juce::String (i + 1).paddedLeft ('0', 2) + "_" + name + ".edgy");

        const auto xml = preset.toValueTree().createXml();

        if (xml == nullptr || ! xml->writeTo (file))
        {
            std::cerr << "Cannot write " << file.getFullPathName().toRawUTF8() << std::endl;
            return 1;
        }

        ++written;
    }

    std::cout << "Wrote " << written << " factory preset(s) to "
              << output.getFullPathName().toRawUTF8() << std::endl;
    return 0;
}
