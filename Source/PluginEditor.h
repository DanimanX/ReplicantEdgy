#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

#include "PluginProcessor.h"
#include "GUI/EdgyLookAndFeel.h"
#include "GUI/EdgyTheme.h"
#include "GUI/PanelLayout.h"
#include "GUI/PresetDrawer.h"
#include "GUI/TempoDisplay.h"

//==============================================================================
/** The hardware panel itself, drawn at a fixed 1280 x 760 logical grid.

    Everything the user interacts with lives here: knobs, switches, patch bay,
    sequencer and the preset drawer. The window wrapper below scales this whole
    component with a single AffineTransform, so painting, fonts, hit testing and
    child components all zoom together instead of being repositioned by hand.
*/
class ReplicantEdgyPanel final : public juce::Component,
                                 private juce::Timer,
                                 private juce::ChangeListener
{
public:
    explicit ReplicantEdgyPanel (ReplicantEdgyAudioProcessor&);
    ~ReplicantEdgyPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void setPresetDrawerOpen (bool shouldBeOpen, bool animate = true);
    bool isPresetDrawerOpen() const noexcept { return drawerOpen; }

    /** Used by generate_snapshot to patch a cable without a mouse. */
    bool connectJacks (ReplicantEdgy::Jack source, ReplicantEdgy::Jack destination);
    void refreshFromProcessor();

    /** Logical height: the 760 px panel, plus the drawer while it is out. */
    int getLogicalHeight() const noexcept;

    /** Updates the zoom readout without echoing back to the window. */
    void setZoomPercent (float percent);
    float getZoomPercent() const noexcept;

    /** Fired when the user moves the zoom control (percent, 50..300). */
    std::function<void (float)> onZoomChanged;

    /** Fired when the drawer changes the content's logical height. */
    std::function<void()> onLogicalHeightChanged;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    static const ReplicantEdgy::Theme& theme() noexcept
    {
        return ReplicantEdgy::ThemeManager::getInstance().getCurrentTheme();
    }

    //==========================================================================
    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& parameterId, const juce::String& tooltip,
                    bool bipolar = false, bool stepKnob = false);
    void setupStepKnob (Knob&, const juce::String& parameterId, const juce::String& tooltip,
                        bool bipolar);

    void applyBounds();
    void styleZoomSlider();
    void updateTempoDisplaySync();
    void nudgeZoom (float deltaPercent);

    // Painting ---------------------------------------------------------------
    void drawChassis (juce::Graphics&);
    void drawHeader (juce::Graphics&);
    void drawPatchBay (juce::Graphics&);
    void drawJackLabels (juce::Graphics&);
    void drawOscillator1 (juce::Graphics&);
    void drawOscillator2 (juce::Graphics&);
    void drawEnvelopeSection (juce::Graphics&);
    void drawNoiseSection (juce::Graphics&);
    void drawFilter (juce::Graphics&);
    void drawVca (juce::Graphics&);
    void drawSequencer (juce::Graphics&);
    void drawScope (juce::Graphics&);
    void drawCables (juce::Graphics&);
    void drawCable (juce::Graphics&, juce::Point<float> from, juce::Point<float> to,
                    juce::Colour colour, float thickness, float glow, bool preview);
    void drawSectionFrame (juce::Graphics&, juce::Rectangle<float>, const juce::String& title);
    void drawSwitch (juce::Graphics&, juce::Rectangle<float>, const juce::String& top,
                     const juce::String& bottom, bool topActive, bool isVertical = true);
    void drawThreeWaySwitch (juce::Graphics&, juce::Rectangle<float>, const juce::String& a,
                             const juce::String& b, const juce::String& c, int state);
    void drawStepLeds (juce::Graphics&);
    void drawLabel (juce::Graphics&, float centreX, float y, const juce::String& text,
                    float size = 8.0f, juce::Colour colour = juce::Colours::white);

    // Interaction ------------------------------------------------------------
    void handleSwitchClick (juce::Point<float>);
    void handleHeaderClick (juce::Point<float>);
    void toggleBoolParameter (const juce::String& id);
    void cycleChoiceParameter (const juce::String& id, int numStates);
    bool hitCable (juce::Point<float>, int& cableIndex) const;
    void removeCablesAt (ReplicantEdgy::Jack jack);

    //==========================================================================
    ReplicantEdgyAudioProcessor& processor;
    ReplicantEdgy::EdgyLookAndFeel lookAndFeel;

    Knob osc1Freq, osc1Env, osc1SeqMod, osc1Level;
    Knob osc2Freq, osc2Env, osc2Level, fmAmount;
    Knob egDecay, noiseLevel, vcaDecay, volume;
    Knob cutoff, resonance, vcfDecay, vcfEnv, noiseVcfMod;
    Knob tempoKnob, scaleKnob;
    std::array<Knob, ReplicantEdgy::StepSequencer::numSteps> stepPitch;
    std::array<Knob, ReplicantEdgy::StepSequencer::numSteps> stepVelocity;

    juce::ComboBox themeBox;
    std::unique_ptr<juce::ParameterAttachment> themeAttachment;

    juce::TextButton prevButton { "<" };
    juce::TextButton nextButton { ">" };
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton presetsButton { "PRESETS" };
    juce::TextButton randomButton { "RANDOM" };

    juce::Slider zoomSlider;
    bool updatingZoom = false;

    std::unique_ptr<ReplicantEdgy::TempoDisplay> tempoDisplay;

    // Cable drag state.
    ReplicantEdgy::Jack dragSource = ReplicantEdgy::Jack::NumJacks;
    ReplicantEdgy::Jack hoverJack = ReplicantEdgy::Jack::NumJacks;
    juce::Point<float> dragPoint;
    bool draggingCable = false;
    float cablePhase = 0.0f;

    // Drawer.
    std::unique_ptr<PresetDrawer> presetDrawer;
    bool drawerOpen = false;
    bool drawerAnimating = false;
    float drawerProgress = 0.0f;
    float drawerAnimFrom = 0.0f;
    double drawerAnimStartMs = 0.0;
    void applyDrawerProgress();

    int lastStep = -1;
    bool lastPlaying = false;
    bool lastHostSync = false;
    float lastHostBpm = 0.0f;
    juce::String lastPresetName;
    float scopeData[ReplicantEdgy::SynthEngine::scopeSize] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReplicantEdgyPanel)
};

//==============================================================================
/** Host-facing window. Owns the scaled panel and a resize corner/constrainer.

    Window size is the zoom: 50% renders the 1280 x 760 panel into 640 x 380,
    300% into 3840 x 2280. Dragging the corner or moving the header zoom slider
    both go through setZoom(), so the two controls can never disagree.
*/
class ReplicantEdgyAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReplicantEdgyAudioProcessorEditor (ReplicantEdgyAudioProcessor&);
    ~ReplicantEdgyAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void setPresetDrawerOpen (bool shouldBeOpen, bool animate = true);
    bool isPresetDrawerOpen() const noexcept { return panel.isPresetDrawerOpen(); }

    bool connectJacks (ReplicantEdgy::Jack source, ReplicantEdgy::Jack destination);
    void refreshFromProcessor();

    /** 0.5 = 50 %, 3.0 = 300 %. Public so the test suite can drive it. */
    void setZoom (float newZoom);
    float getZoom() const noexcept { return zoom; }

    const ReplicantEdgyPanel& getPanel() const noexcept { return panel; }

private:
    static constexpr float minZoom = 0.5f;
    static constexpr float maxZoom = 3.0f;

    void syncWindowToContent();
    void updateConstrainer();

    ReplicantEdgyAudioProcessor& processor;
    ReplicantEdgyPanel panel;
    juce::ComponentBoundsConstrainer constrainer;
    float zoom = 1.0f;
    bool syncingWindow = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReplicantEdgyAudioProcessorEditor)
};
