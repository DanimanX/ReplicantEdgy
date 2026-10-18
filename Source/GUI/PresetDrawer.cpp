#include "PresetDrawer.h"

namespace
{
    const char* const kCategories[] = { "All", "Perc", "Bass", "Lead", "Noise", "Drone", "FX", "User" };
}

//==============================================================================
PresetDrawer::PresetDrawer (ReplicantEdgyAudioProcessor& p)
    : processor (p)
{
    setOpaque (true);

    for (int i = 0; i < (int) categoryButtons.size(); ++i)
    {
        auto& button = categoryButtons[(size_t) i];
        button.setButtonText (kCategories[i]);
        button.onClick = [this, i]()
        {
            activeCategory = kCategories[i];
            rebuildFilter();
        };
        addAndMakeVisible (button);
    }

    addAndMakeVisible (listBox);
    listBox.setRowHeight (30);
    listBox.setColour (juce::ListBox::backgroundColourId, theme().panelFaceDark);
    listBox.setOutlineThickness (1);
    listBox.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId,
                                              theme().accent.withAlpha (0.55f));
    listBox.getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId,
                                              theme().panelFaceDark);

    addAndMakeVisible (nameEditor);
    nameEditor.setText (processor.getCurrentPresetName(), juce::dontSendNotification);
    nameEditor.setJustification (juce::Justification::centredLeft);
    nameEditor.setTextToShowWhenEmpty ("Preset name", theme().silkscreenDim);

    auto addButton = [this] (juce::TextButton& button, std::function<void()> action)
    {
        button.onClick = std::move (action);
        addAndMakeVisible (button);
    };

    addButton (loadButton, [this] { loadSelected(); });
    addButton (saveButton, [this] { saveCurrent(); });
    addButton (deleteButton, [this] { deleteSelected(); });
    addButton (importButton, [this] { importPreset(); });
    addButton (exportButton, [this] { exportSelected(); });
    addButton (randomButton, [this] { randomisePatch(); });
    addButton (closeButton, [this] { if (onCloseRequested) onCloseRequested(); });

    refresh();
}

//==============================================================================
void PresetDrawer::resized()
{
    auto area = getLocalBounds().reduced (14, 10);

    area.removeFromTop (30);                 // title + close
    auto filters = area.removeFromTop (32);
    area.removeFromTop (8);

    for (int i = 0; i < (int) categoryButtons.size(); ++i)
        categoryButtons[(size_t) i].setBounds (filters.removeFromLeft (82).reduced (0, 1));

    auto right = area.removeFromRight (330);
    listBox.setBounds (area.reduced (0, 2));

    right.removeFromLeft (16);
    nameEditor.setBounds (right.removeFromTop (30));
    right.removeFromTop (8);

    auto row = right.removeFromTop (32);
    saveButton.setBounds (row.removeFromLeft (150).reduced (0, 1));
    deleteButton.setBounds (row.removeFromLeft (150).reduced (0, 1));

    right.removeFromTop (8);
    row = right.removeFromTop (32);
    importButton.setBounds (row.removeFromLeft (150).reduced (0, 1));
    exportButton.setBounds (row.removeFromLeft (150).reduced (0, 1));

    right.removeFromTop (8);
    row = right.removeFromTop (32);
    loadButton.setBounds (row.removeFromLeft (150).reduced (0, 1));
    randomButton.setBounds (row.removeFromLeft (150).reduced (0, 1));

    closeButton.setBounds (getWidth() - 100, 10, 86, 22);
}

//==============================================================================
void PresetDrawer::refresh()
{
    rebuildFilter();
    nameEditor.setText (processor.getCurrentPresetName(), juce::dontSendNotification);
    repaint();
}

void PresetDrawer::rebuildFilter()
{
    filtered.clear();

    auto& manager = processor.getPresetManager();

    for (int i = 0; i < manager.getNumPresets(); ++i)
    {
        const auto category = manager.getCategory (i);

        if (activeCategory == "All"
            || activeCategory == category
            || (activeCategory == "User" && manager.isUserPreset (i)))
            filtered.push_back (i);
    }

    listBox.updateContent();
    listBox.deselectAllRows();

    for (int row = 0; row < (int) filtered.size(); ++row)
    {
        if (filtered[(size_t) row] == processor.getCurrentPresetIndex())
        {
            listBox.selectRow (row, true);
            listBox.scrollToEnsureRowIsOnscreen (row);
            break;
        }
    }

    repaint();
}

int PresetDrawer::selectedPresetIndex() const
{
    const int row = listBox.getSelectedRow();

    if (juce::isPositiveAndBelow (row, (int) filtered.size()))
        return filtered[(size_t) row];

    return -1;
}

//==============================================================================
int PresetDrawer::getNumRows()
{
    return (int) filtered.size();
}

void PresetDrawer::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    const auto& t = theme();

    if (! juce::isPositiveAndBelow (row, (int) filtered.size()))
        return;

    const int index = filtered[(size_t) row];
    const bool isUser = processor.getPresetManager().isUserPreset (index);

    auto bounds = juce::Rectangle<int> (0, 0, width, height);

    g.setColour (selected ? t.accent.withAlpha (0.20f)
                          : (row % 2 ? t.panelFace.withAlpha (0.35f)
                                     : juce::Colours::transparentBlack));
    g.fillRect (bounds);

    if (selected)
    {
        g.setColour (t.accent.withAlpha (0.85f));
        g.fillRect (bounds.removeFromLeft (3));
    }

    g.setColour (selected ? t.silkscreen : t.silkscreen.withAlpha (0.82f));
    g.setFont (ReplicantEdgy::uiFont (12.0f, selected ? juce::Font::bold : juce::Font::plain));
    g.drawFittedText (processor.getPresetManager().getName (index),
                      bounds.reduced (12, 0), juce::Justification::centredLeft, 1, 0.9f);

    g.setColour (isUser ? t.warning.withAlpha (0.85f) : t.accentAlt.withAlpha (0.65f));
    g.setFont (ReplicantEdgy::uiFont (9.0f, juce::Font::bold));
    g.drawFittedText (isUser ? "USER" : processor.getPresetManager().getCategory (index).toUpperCase(),
                      bounds.removeFromRight (80), juce::Justification::centredRight, 1, 0.9f);

    g.setColour (t.silkscreen.withAlpha (0.08f));
    g.drawHorizontalLine (height - 1, 0.0f, (float) width);
}

void PresetDrawer::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow (row, (int) filtered.size()))
    {
        processor.applyPreset (filtered[(size_t) row]);
        showStatus ("LOADED " + processor.getCurrentPresetName());
    }
}

void PresetDrawer::selectedRowsChanged (int)
{
    const int index = selectedPresetIndex();

    if (index >= 0)
        nameEditor.setText (processor.getPresetManager().getName (index), juce::dontSendNotification);
}

//==============================================================================
void PresetDrawer::loadSelected()
{
    const int index = selectedPresetIndex();

    if (index < 0)
    {
        showStatus ("SELECT A PRESET FIRST");
        return;
    }

    processor.applyPreset (index);
    showStatus ("LOADED " + processor.getCurrentPresetName());

    if (onPresetChanged)
        onPresetChanged();
}

void PresetDrawer::saveCurrent()
{
    auto name = nameEditor.getText().trim();

    if (name.isEmpty())
        name = processor.getCurrentPresetName() + " Edit";

    const int index = processor.saveUserPreset (name);

    if (index >= 0)
    {
        showStatus ("SAVED " + name);
        refresh();
        listBox.selectRow (-1);

        for (int row = 0; row < (int) filtered.size(); ++row)
            if (filtered[(size_t) row] == index)
                listBox.selectRow (row, true);
    }
    else
    {
        showStatus ("SAVE FAILED");
    }

    if (onPresetChanged)
        onPresetChanged();
}

void PresetDrawer::deleteSelected()
{
    const int index = selectedPresetIndex();

    if (index < 0 || ! processor.getPresetManager().isUserPreset (index))
    {
        showStatus ("ONLY USER PRESETS CAN BE DELETED");
        return;
    }

    const auto name = processor.getPresetManager().getName (index);
    processor.deleteUserPreset (index);
    refresh();
    showStatus ("DELETED " + name);

    if (onPresetChanged)
        onPresetChanged();
}

void PresetDrawer::importPreset()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Import a ReplicantEdgy preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.edgy");

    auto safeThis = juce::Component::SafePointer<PresetDrawer> (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser)
                              {
                                  if (safeThis == nullptr)
                                      return;

                                  const auto file = chooser.getResult();

                                  if (file.getFullPathName().isEmpty())
                                      return;

                                  const bool ok = safeThis->processor.importPreset (file);

                                  safeThis->showStatus (ok ? "IMPORTED " + file.getFileName()
                                                           : "IMPORT FAILED");
                                  safeThis->refresh();

                                  if (ok && safeThis->onPresetChanged)
                                      safeThis->onPresetChanged();
                              });
}

void PresetDrawer::exportSelected()
{
    const int index = selectedPresetIndex();

    if (index < 0)
    {
        showStatus ("SELECT A PRESET FIRST");
        return;
    }

    auto safeName = processor.getPresetManager().getName (index);
    safeName = safeName.replaceCharacter (' ', '_');
    safeName = safeName.retainCharacters ("abcdefghijklmnopqrstuvwxyz"
                                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                          "0123456789_-");

    if (safeName.isEmpty())
        safeName = "ReplicantEdgy_Preset";

    fileChooser = std::make_unique<juce::FileChooser> (
        "Export preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile (safeName + ".edgy"),
        "*.edgy");

    auto safeThis = juce::Component::SafePointer<PresetDrawer> (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis, index] (const juce::FileChooser& chooser)
                              {
                                  if (safeThis == nullptr)
                                      return;

                                  auto file = chooser.getResult();

                                  if (file.getFullPathName().isEmpty())
                                      return;

                                  if (file.getFileExtension().isEmpty())
                                      file = file.getSiblingFile (file.getFileNameWithoutExtension()
                                                                  + ".edgy");

                                  const bool ok = safeThis->processor.exportPreset (index, file);
                                  safeThis->showStatus (ok ? "EXPORTED " + file.getFileName()
                                                           : "EXPORT FAILED");
                              });
}

//==============================================================================
void PresetDrawer::randomisePatch()
{
    processor.randomisePatch();
    showStatus ("MUTATED - SAVE TO KEEP");

    if (onPresetChanged)
        onPresetChanged();
}

//==============================================================================
void PresetDrawer::paint (juce::Graphics& g)
{
    const auto& t = theme();
    const float w = (float) getWidth();
    const float h = (float) getHeight();

    juce::ColourGradient background (t.panelFace.brighter (0.05f), 0.0f, 0.0f,
                                     t.chassisBottom, 0.0f, h, false);
    g.setGradientFill (background);
    g.fillAll();

    // Circuit traces.
    g.setColour (t.pcbTrace.withAlpha (0.12f));

    for (int i = 0; i < 7; ++i)
    {
        const float y = 26.0f + i * 44.0f;
        g.drawLine (0.0f, y, w * 0.20f, y, 1.0f);
        g.drawLine (w * 0.20f, y, w * 0.24f, y + 14.0f, 1.0f);
        g.drawLine (w * 0.24f, y + 14.0f, w, y + 14.0f, 1.0f);
    }

    g.setColour (t.accent.withAlpha (0.85f));
    g.drawHorizontalLine (0, 0.0f, w);

    g.setColour (t.silkscreen);
    g.setFont (ReplicantEdgy::uiFont (16.0f, juce::Font::bold));
    g.drawText ("PRESET ARCHIVE", 16, 8, 300, 24, juce::Justification::centredLeft);

    g.setColour (t.silkscreenDim);
    g.setFont (ReplicantEdgy::uiFont (9.0f, juce::Font::plain));
    g.drawText ("FACTORY BANK + YOUR OWN PATCHES  //  DOUBLE-CLICK TO LOAD",
                320, 10, 600, 20, juce::Justification::centredLeft);

    if (status.isNotEmpty()
        && juce::Time::getMillisecondCounterHiRes() < statusUntil)
    {
        g.setColour (t.accentAlt);
        g.setFont (ReplicantEdgy::uiFont (10.0f, juce::Font::bold));
        g.drawText (status, w - 470, 10, 350, 20, juce::Justification::centredRight);
    }

    // Name caption above the editor.
    g.setColour (t.silkscreenDim);
    g.setFont (ReplicantEdgy::uiFont (8.5f, juce::Font::bold));
    g.drawText ("PATCH NAME", w - 330, 66, 100, 12, juce::Justification::centredLeft);

}

void PresetDrawer::showStatus (const juce::String& text)
{
    status = text;
    statusUntil = juce::Time::getMillisecondCounterHiRes() + 3000.0;
    repaint();
}

void PresetDrawer::setRevealOffset (float pixels)
{
    if (juce::approximatelyEqual (revealOffset, pixels))
        return;

    revealOffset = pixels;
    repaint();
}
