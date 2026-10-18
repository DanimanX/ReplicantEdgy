#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "GUI/EdgyTheme.h"
#include "GUI/PanelLayout.h"

//==============================================================================
/** Slide-down preset archive.

    Factory bank + user patches, category filters, load / save / delete and
    `.edgy` import/export, plus a RANDOM "mutation" button that builds a fresh
    cyberpunk patch from the current one.
*/
class PresetDrawer : public juce::Component,
                     private juce::ListBoxModel
{
public:
    explicit PresetDrawer (ReplicantEdgyAudioProcessor&);
    ~PresetDrawer() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Rebuilds the filtered list and repaints. */
    void refresh();

    /** Called whenever a preset was loaded/saved so the editor can repaint. */
    std::function<void()> onPresetChanged;
    std::function<void()> onCloseRequested;

    void setRevealOffset (float pixels);

private:
    static const ReplicantEdgy::Theme& theme() noexcept
    {
        return ReplicantEdgy::ThemeManager::getInstance().getCurrentTheme();
    }

    // ListBoxModel -----------------------------------------------------------
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;

    void rebuildFilter();
    int selectedPresetIndex() const;
    void loadSelected();
    void saveCurrent();
    void deleteSelected();
    void importPreset();
    void exportSelected();
    void randomisePatch();
    void showStatus (const juce::String&);

    ReplicantEdgyAudioProcessor& processor;

    juce::ListBox listBox { "presets", this };
    std::vector<int> filtered;
    juce::String activeCategory { "All" };

    std::array<juce::TextButton, 8> categoryButtons;
    juce::TextButton loadButton { "LOAD" };
    juce::TextButton saveButton { "SAVE AS" };
    juce::TextButton deleteButton { "DELETE" };
    juce::TextButton importButton { "IMPORT" };
    juce::TextButton exportButton { "EXPORT" };
    juce::TextButton randomButton { "RANDOM" };
    juce::TextButton closeButton { "CLOSE" };
    juce::TextEditor nameEditor;

    juce::String status;
    double statusUntil = 0.0;
    float revealOffset = 0.0f;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetDrawer)
};
