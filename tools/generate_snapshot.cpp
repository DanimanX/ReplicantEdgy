//==============================================================================
// Renders the ReplicantEdgy editor off-screen to a PNG for the README and the
// promo video.
//
//   generate_snapshot [output.png] [themeIndex] [drawerOpen] [presetIndex]
//
// themeIndex switches the skin live through the ui_theme parameter (the same
// path the combo box uses), drawerOpen shows the preset archive, and
// presetIndex loads a factory patch so the panel shows real values and cables.
// tempo (BPM) overrides the tempo so the LED display can be reviewed.
//==============================================================================

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char* argv[])
{
    const juce::File output = (argc > 1)
        ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
        : juce::File::getCurrentWorkingDirectory().getChildFile ("replicantedgy_ui.png");

    juce::ScopedJuceInitialiser_GUI gui;
    ReplicantEdgyAudioProcessor processor;

    // A preset first: it fills the panel and may carry patch cables.
    if (argc > 4)
    {
        const int index = juce::String (argv[4]).getIntValue();
        processor.applyPreset (index);

        std::cout << "[INFO] Preset: " << processor.getCurrentPresetName().toRawUTF8()
                  << "  cables: " << processor.getEngine().getPatchBay().getNumCables()
                  << std::endl;
    }

    // Run half a second of the sequencer so the screenshot shows a live scope,
    // meters and step LEDs instead of a dead panel.
    processor.prepareToPlay (48000.0, 256);
    processor.getEngine().getSequencer().setPlaying (true);

    for (int block = 0; block < 90; ++block)
    {
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
    }

    if (argc > 5)
    {
        const float tempo = juce::String (argv[5]).getFloatValue();

        if (tempo > 0.0f)
            processor.setParameterValue (ReplicantEdgy::ParamID::tempo, tempo);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
        return 1;

    editor->setSize (ReplicantEdgy::Layout::panelWidth, ReplicantEdgy::Layout::panelHeight);
    editor->setVisible (true);

    if (argc > 2)
    {
        const int requested = juce::String (argv[2]).getIntValue();

        if (auto* themeParameter = processor.apvts.getParameter (ReplicantEdgy::ParamID::uiTheme))
        {
            const int current = ReplicantEdgy::ThemeManager::getInstance().getCurrentIndex();

            if (requested != current)
            {
                themeParameter->setValueNotifyingHost (
                    ReplicantEdgy::ThemeManager::indexToNormalized (requested));

                juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            }

            std::cout << "[INFO] Skin: " << ReplicantEdgy::ThemeManager::getThemeName (
                             ReplicantEdgy::ThemeManager::getInstance().getCurrentIndex())
                             .toRawUTF8()
                      << std::endl;
        }
    }

    if (argc > 3)
    {
        auto* concrete = dynamic_cast<ReplicantEdgyAudioProcessorEditor*> (editor.get());

        if (concrete == nullptr)
            return 3;

        const int mode = juce::String (argv[3]).getIntValue();
        concrete->setPresetDrawerOpen (mode != 0, false);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        std::cout << "[INFO] Drawer: " << (concrete->isPresetDrawerOpen() ? "open" : "closed")
                  << std::endl;
    }

    juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    juce::Graphics g (image);
    editor->paintEntireComponent (g, true);

    output.getParentDirectory().createDirectory();
    output.deleteFile();

    juce::FileOutputStream stream (output);

    if (! stream.openedOk())
    {
        std::cerr << "[ERROR] Cannot write " << output.getFullPathName().toRawUTF8() << std::endl;
        return 1;
    }

    juce::PNGImageFormat png;
    png.writeImageToStream (image, stream);

    std::cout << "[SUCCESS] Saved " << image.getWidth() << "x" << image.getHeight()
              << " to " << output.getFullPathName().toRawUTF8() << std::endl;
    return 0;
}
