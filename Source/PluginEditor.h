#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "GUI/EdgyLookAndFeel.h"
#include "GUI/EdgyTheme.h"
#include "GUI/PanelLayout.h"
#include "GUI/PresetDrawer.h"

class ReplicantEdgyAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer,
                                          private juce::ChangeListener
{
public:
    explicit ReplicantEdgyAudioProcessorEditor (ReplicantEdgyAudioProcessor&);
    ~ReplicantEdgyAudioProcessorEditor() override;

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
    juce::String lastPresetName;
    float scopeData[ReplicantEdgy::SynthEngine::scopeSize] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReplicantEdgyAudioProcessorEditor)
};
