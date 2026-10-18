#include "PluginEditor.h"

using namespace ReplicantEdgy;

namespace
{
    // Switch hit zones, matched to the drawn hardware.
    const juce::Rectangle<float> osc1ShapeSwitch   { 124.0f, 252.0f, 26.0f, 46.0f };
    const juce::Rectangle<float> seqTargetSwitch   { 305.0f, 250.0f, 34.0f, 46.0f };
    const juce::Rectangle<float> osc2ShapeSwitch   { 124.0f, 434.0f, 26.0f, 46.0f };
    const juce::Rectangle<float> hardSyncSwitch    { 256.0f, 446.0f, 50.0f, 24.0f };
    const juce::Rectangle<float> noiseColourSwitch { 508.0f, 452.0f, 26.0f, 46.0f };
    const juce::Rectangle<float> vcfModeSwitch     { 516.0f, 444.0f, 46.0f, 24.0f };
    const juce::Rectangle<float> vcaModeSwitch     { 640.0f, 262.0f, 26.0f, 46.0f };
    const juce::Rectangle<float> clockSwitchZone   { 136.0f, 618.0f, 38.0f, 62.0f };

    const juce::Rectangle<float> triggerZone { 288.0f, 620.0f, 64.0f, 46.0f };
    const juce::Rectangle<float> playZone    { 360.0f, 620.0f, 64.0f, 46.0f };
    const juce::Rectangle<float> advanceZone { 432.0f, 620.0f, 64.0f, 46.0f };

    juce::String boolText (bool value, const juce::String& on, const juce::String& off)
    {
        return value ? on : off;
    }
}

//==============================================================================
ReplicantEdgyPanel::ReplicantEdgyPanel (ReplicantEdgyAudioProcessor& p)
    : processor (p)
{
    // ---- skin -------------------------------------------------------------
    themeBox.addItemList (ThemeManager::getThemeNames(), 1);
    addAndMakeVisible (themeBox);

    if (auto* themeParameter = processor.apvts.getParameter (ParamID::uiTheme))
    {
        themeAttachment = std::make_unique<juce::ParameterAttachment> (
            *themeParameter,
            [this] (float plainIndex)
            {
                // ParameterAttachment reports the *denormalised* value, which
                // for a choice parameter is the item index.
                const int index = juce::jlimit (0, ThemeManager::getNumThemes() - 1,
                                                juce::roundToInt (plainIndex));
                themeBox.setSelectedId (index + 1, juce::dontSendNotification);
                ThemeManager::getInstance().setCurrentIndex (index);
            });

        themeAttachment->sendInitialUpdate();
    }

    themeBox.onChange = [this]()
    {
        if (themeAttachment != nullptr)
            themeAttachment->setValueAsCompleteGesture ((float) (themeBox.getSelectedId() - 1));
    };

    ThemeManager::getInstance().addChangeListener (this);
    lookAndFeel.applyTheme();
    setLookAndFeel (&lookAndFeel);

    // ---- knobs ------------------------------------------------------------
    setupKnob (osc1Freq, ParamID::osc1Freq, "VCO 1 frequency");
    setupKnob (osc1Env, ParamID::osc1Env, "VCO 1 envelope amount", true);
    setupKnob (osc1SeqMod, ParamID::seqPitchMod, "Sequencer pitch modulation depth");
    setupKnob (osc1Level, ParamID::osc1Level, "VCO 1 level");

    setupKnob (osc2Freq, ParamID::osc2Freq, "VCO 2 frequency");
    setupKnob (osc2Env, ParamID::osc2Env, "VCO 2 envelope amount", true);
    setupKnob (osc2Level, ParamID::osc2Level, "VCO 2 level");
    setupKnob (fmAmount, ParamID::fmAmount, "VCO 2 -> VCO 1 FM amount");

    setupKnob (egDecay, ParamID::egDecay, "VCO envelope decay");
    setupKnob (noiseLevel, ParamID::noiseLevel, "Noise / external audio level");
    setupKnob (vcaDecay, ParamID::vcaDecay, "VCA envelope decay");
    setupKnob (volume, ParamID::volume, "Master volume");

    setupKnob (cutoff, ParamID::cutoff, "VCF cutoff");
    setupKnob (resonance, ParamID::resonance, "VCF resonance");
    setupKnob (vcfDecay, ParamID::vcfDecay, "VCF envelope decay");
    setupKnob (vcfEnv, ParamID::vcfEnv, "VCF envelope amount", true);
    setupKnob (noiseVcfMod, ParamID::noiseVcfMod, "Noise modulation of the VCF");

    setupKnob (tempoKnob, ParamID::tempo, "Sequencer tempo");
    setupKnob (scaleKnob, ParamID::scale, "Clock scale", false, true);

    for (int i = 0; i < StepSequencer::numSteps; ++i)
    {
        setupStepKnob (stepPitch[(size_t) i], ParamID::stepPitch (i),
                       "Step " + juce::String (i + 1) + " pitch", true);
        setupStepKnob (stepVelocity[(size_t) i], ParamID::stepVelocity (i),
                       "Step " + juce::String (i + 1) + " velocity", false);
    }

    // ---- header buttons ---------------------------------------------------
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (presetsButton);
    addAndMakeVisible (randomButton);

    prevButton.setTooltip ("Previous preset");
    nextButton.setTooltip ("Next preset");
    saveButton.setTooltip ("Open the preset archive to save this patch");
    presetsButton.setTooltip ("Open the preset archive");
    randomButton.setTooltip ("Generate a random cyberpunk patch");
    randomButton.getProperties().set ("cyberRandom", true);

    prevButton.onClick = [this] { processor.previousPreset(); refreshFromProcessor(); };
    nextButton.onClick = [this] { processor.nextPreset(); refreshFromProcessor(); };
    saveButton.onClick = [this] { setPresetDrawerOpen (true); presetDrawer->refresh(); };
    presetsButton.onClick = [this] { setPresetDrawerOpen (! drawerOpen); };
    randomButton.onClick = [this]
    {
        processor.randomisePatch();
        refreshFromProcessor();
        repaint();
    };

    // ---- zoom -------------------------------------------------------------
    zoomSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    zoomSlider.setRange (50.0, 300.0, 1.0);
    zoomSlider.setValue (100.0, juce::dontSendNotification);
    zoomSlider.setNumDecimalPlacesToDisplay (0);
    zoomSlider.setTextValueSuffix ("%");
    zoomSlider.setTooltip ("Panel zoom (50-300 %)");
    addAndMakeVisible (zoomSlider);
    styleZoomSlider();

    zoomSlider.onValueChange = [this]
    {
        if (! updatingZoom && onZoomChanged != nullptr)
            onZoomChanged ((float) zoomSlider.getValue());
    };

    // Red LED tempo readout under the TEMPO knob.
    if (auto* tempoParameter = processor.apvts.getParameter (ParamID::tempo))
    {
        tempoDisplay = std::make_unique<TempoDisplay> (*tempoParameter);
        addAndMakeVisible (*tempoDisplay);

        // Editing the tempo by hand means leaving host sync: otherwise the
        // typed value would be silently overridden by the DAW. Keep the
        // transport running across the switch, so a tempo edit mid-playback
        // does not silence the sequence.
        tempoDisplay->onManualEdit = [this]
        {
            const bool wasRunning = processor.getEngine().getSequencer().isPlaying();

            processor.setParameterValueAsGesture (ParamID::clockSource, 0.0f);
            processor.getEngine().getSequencer().setPlaying (wasRunning);
            repaint();
        };
    }

    // ---- drawer -----------------------------------------------------------
    presetDrawer = std::make_unique<PresetDrawer> (processor);
    presetDrawer->onCloseRequested = [this] { setPresetDrawerOpen (false); };
    presetDrawer->onPresetChanged = [this] { refreshFromProcessor(); };
    addChildComponent (*presetDrawer);

    setWantsKeyboardFocus (true);
    setSize (Layout::panelWidth, Layout::panelHeight);

    startTimerHz (30);
}

ReplicantEdgyPanel::~ReplicantEdgyPanel()
{
    ThemeManager::getInstance().removeChangeListener (this);
    stopTimer();
    presetDrawer.reset();
    setLookAndFeel (nullptr);
}

//==============================================================================
void ReplicantEdgyPanel::setupKnob (Knob& knob, const juce::String& parameterId,
                                                   const juce::String& tooltip, bool bipolar,
                                                   bool stepKnob)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.slider.setTooltip (tooltip);
    knob.slider.getProperties().set ("bipolar", bipolar);
    knob.slider.getProperties().set ("stepKnob", stepKnob);
    addAndMakeVisible (knob.slider);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, parameterId, knob.slider);
}

void ReplicantEdgyPanel::setupStepKnob (Knob& knob, const juce::String& parameterId,
                                                       const juce::String& tooltip, bool bipolar)
{
    setupKnob (knob, parameterId, tooltip, bipolar, true);
}

//==============================================================================
void ReplicantEdgyPanel::applyBounds()
{
    osc1Freq.slider.setBounds (36, 244, 60, 60);
    osc1Env.slider.setBounds (178, 244, 60, 60);
    osc1SeqMod.slider.setBounds (250, 250, 52, 52);
    osc1Level.slider.setBounds (348, 244, 52, 52);

    osc2Freq.slider.setBounds (36, 426, 60, 60);
    osc2Env.slider.setBounds (178, 426, 60, 60);
    osc2Level.slider.setBounds (322, 430, 56, 56);
    fmAmount.slider.setBounds (410, 430, 56, 56);

    egDecay.slider.setBounds (418, 244, 56, 56);
    noiseLevel.slider.setBounds (540, 244, 56, 56);
    vcaDecay.slider.setBounds (690, 244, 56, 56);
    volume.slider.setBounds (780, 244, 56, 56);

    cutoff.slider.setBounds (568, 426, 56, 56);
    resonance.slider.setBounds (636, 426, 56, 56);
    vcfDecay.slider.setBounds (704, 426, 56, 56);
    vcfEnv.slider.setBounds (772, 426, 56, 56);
    noiseVcfMod.slider.setBounds (840, 426, 52, 52);

    tempoKnob.slider.setBounds (42, 608, 72, 72);
    scaleKnob.slider.setBounds (190, 612, 64, 64);

    if (tempoDisplay != nullptr)
        tempoDisplay->setBounds (Layout::tempoDisplay().toNearestInt());

    for (int i = 0; i < StepSequencer::numSteps; ++i)
    {
        stepPitch[(size_t) i].slider.setBounds (
            (int) Layout::stepPitchKnob (i).getX(), (int) Layout::stepPitchKnob (i).getY(),
            Layout::stepKnobSize, Layout::stepKnobSize);
        stepVelocity[(size_t) i].slider.setBounds (
            (int) Layout::stepVelocityKnob (i).getX(), (int) Layout::stepVelocityKnob (i).getY(),
            Layout::stepKnobSize, Layout::stepKnobSize);
    }

    prevButton.setBounds (Layout::presetPrevButton().toNearestInt());
    nextButton.setBounds (Layout::presetNextButton().toNearestInt());
    saveButton.setBounds (Layout::saveButton().toNearestInt());
    presetsButton.setBounds (Layout::presetsButton().toNearestInt());
    randomButton.setBounds (Layout::randomButton().toNearestInt());
    themeBox.setBounds (Layout::themeBox().toNearestInt());
    zoomSlider.setBounds (Layout::zoomSlider().toNearestInt());
}

void ReplicantEdgyPanel::styleZoomSlider()
{
    zoomSlider.setColour (juce::Slider::backgroundColourId, theme().panelFaceDark);
    zoomSlider.setColour (juce::Slider::trackColourId, theme().accentAlt);
    zoomSlider.setColour (juce::Slider::thumbColourId, theme().accent);
    zoomSlider.setColour (juce::Slider::textBoxBackgroundColourId, theme().panelFaceDark);
    zoomSlider.setColour (juce::Slider::textBoxTextColourId, theme().accent);
    zoomSlider.setColour (juce::Slider::textBoxOutlineColourId, theme().accent.withAlpha (0.45f));
}

void ReplicantEdgyPanel::resized()
{
    applyBounds();

    if (presetDrawer != nullptr)
    {
        presetDrawer->setBounds (0, Layout::panelHeight + (int) ((1.0f - drawerProgress)
                                                                * (float) Layout::drawerHeight),
                                 Layout::panelWidth, Layout::drawerHeight);
    }
}

//==============================================================================
void ReplicantEdgyPanel::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source != &ThemeManager::getInstance())
        return;

    lookAndFeel.applyTheme();
    styleZoomSlider();

    if (presetDrawer != nullptr)
        presetDrawer->repaint();

    repaint();
}

void ReplicantEdgyPanel::timerCallback()
{
    if (drawerAnimating)
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const float t = juce::jlimit (0.0f, 1.0f, (float) ((now - drawerAnimStartMs) / 220.0));
        const float eased = 1.0f - std::pow (1.0f - t, 3.0f);
        const float target = drawerOpen ? 1.0f : 0.0f;

        drawerProgress = drawerAnimFrom + (target - drawerAnimFrom) * eased;
        applyDrawerProgress();

        if (t >= 1.0f)
        {
            drawerAnimating = false;
            drawerProgress = target;
            applyDrawerProgress();
            startTimerHz (30);
        }
    }

    auto& engine = processor.getEngine();
    const auto telemetry = engine.getTelemetry();

    engine.copyScope (scopeData, SynthEngine::scopeSize);

    cablePhase += 0.06f;

    const bool playing = telemetry.playing;
    const auto presetName = processor.getCurrentPresetName();

    if (telemetry.step != lastStep || playing != lastPlaying || presetName != lastPresetName)
    {
        lastStep = telemetry.step;
        lastPlaying = playing;
        lastPresetName = presetName;

        repaint (0, 70, Layout::panelWidth, 130);      // patch bay / cable glow
        repaint (0, Layout::panelHeight - 200, Layout::panelWidth, 200);
        repaint (460, 8, 360, 52);                     // preset name
    }
    else
    {
        repaint (0, 70, Layout::panelWidth, 130);
        repaint (906, 206, 358, 348);
        repaint (560, 568, 704, 48);
    }

    updateTempoDisplaySync();

    // The plugin state can carry a zoom restored by the host after the editor
    // was created. Follow it without fighting a slider the user is dragging.
    const float requestedZoom = processor.getUiZoom() * 100.0f;

    if (onZoomChanged != nullptr && ! zoomSlider.isMouseButtonDown()
        && std::abs (requestedZoom - (float) zoomSlider.getValue()) > 0.5f)
    {
        onZoomChanged (requestedZoom);
    }
}

void ReplicantEdgyPanel::updateTempoDisplaySync()
{
    if (tempoDisplay == nullptr)
        return;

    const auto& sequencer = processor.getEngine().getSequencer();
    const bool following = sequencer.followsHost() && sequencer.hasHostTempo();
    const float bpm = following ? (float) sequencer.getHostBpm() : 0.0f;

    tempoDisplay->setHostSync (following, bpm);

    if (following != lastHostSync || std::abs (bpm - lastHostBpm) > 0.001f)
    {
        lastHostSync = following;
        lastHostBpm = bpm;
        repaint (20, 580, 280, 60);   // clock switch / TEMPO caption area
    }
}

//==============================================================================
void ReplicantEdgyPanel::refreshFromProcessor()
{
    if (auto* themeParameter = processor.apvts.getParameter (ParamID::uiTheme))
        ThemeManager::getInstance().setCurrentIndex (
            juce::jlimit (0, ThemeManager::getNumThemes() - 1,
                          juce::roundToInt (themeParameter->getValue()
                                            * (float) (ThemeManager::getNumThemes() - 1))));

    if (presetDrawer != nullptr && drawerProgress > 0.0f)
        presetDrawer->refresh();

    if (tempoDisplay != nullptr)
        tempoDisplay->refresh();

    repaint();
}

//==============================================================================
// Painting
//==============================================================================
void ReplicantEdgyPanel::paint (juce::Graphics& g)
{
    drawChassis (g);
    drawHeader (g);
    drawPatchBay (g);
    drawOscillator1 (g);
    drawOscillator2 (g);
    drawEnvelopeSection (g);
    drawNoiseSection (g);
    drawFilter (g);
    drawVca (g);
    drawSequencer (g);
    drawScope (g);
    drawCables (g);
    drawJackLabels (g);
}

void ReplicantEdgyPanel::drawChassis (juce::Graphics& g)
{
    const auto& t = theme();
    const float w = (float) Layout::panelWidth;
    const float h = (float) Layout::panelHeight;

    juce::ColourGradient body (t.chassisTop, 0.0f, 0.0f, t.chassisBottom, 0.0f, h, false);
    g.setGradientFill (body);
    g.fillAll();

    // PCB traces under the panel.
    g.setColour (t.pcbTrace.withAlpha (0.10f));

    for (int i = 0; i < 12; ++i)
    {
        const float y = 84.0f + i * 56.0f;
        g.drawLine (0.0f, y, w * 0.12f, y, 1.0f);
        g.drawLine (w * 0.12f, y, w * 0.16f, y + 16.0f, 1.0f);
        g.drawLine (w * 0.16f, y + 16.0f, w, y + 16.0f, 1.0f);
    }

    // Scanlines.
    g.setColour (juce::Colours::black.withAlpha (0.10f));

    for (float y = 0.0f; y < h; y += 4.0f)
        g.drawHorizontalLine ((int) y, 0.0f, w);

    // Neon edge light along the top and bottom.
    g.setColour (t.accent.withAlpha (0.75f));
    g.drawHorizontalLine (0, 0.0f, w);
    g.setColour (t.accent.withAlpha (0.18f));
    g.drawHorizontalLine (1, 0.0f, w);
    g.drawHorizontalLine (2, 0.0f, w);

    g.setColour (t.accentAlt.withAlpha (0.25f));
    g.drawHorizontalLine ((int) h - 1, 0.0f, w);

    // Corner screws.
    const juce::Point<float> screws[] =
    {
        { 10.0f, 10.0f }, { w - 10.0f, 10.0f }, { 10.0f, h - 10.0f }, { w - 10.0f, h - 10.0f }
    };

    for (auto point : screws)
        EdgyLookAndFeel::drawPanelscrew (g, point, 5.0f);
}

void ReplicantEdgyPanel::drawHeader (juce::Graphics& g)
{
    const auto& t = theme();

    g.setColour (t.panelFaceDark.withAlpha (0.85f));
    g.fillRect (0.0f, 0.0f, (float) Layout::panelWidth, (float) Layout::headerHeight);
    g.setColour (t.accent.withAlpha (0.35f));
    g.drawHorizontalLine (Layout::headerHeight - 1, 0.0f, (float) Layout::panelWidth);

    // Logo.
    g.setFont (uiFont (30.0f, juce::Font::bold));
    g.setColour (t.silkscreen);
    g.drawText ("REPLICANT", 20, 8, 210, 32, juce::Justification::centredLeft);

    g.setColour (t.accent.withAlpha (0.30f));
    g.setFont (uiFont (30.0f, juce::Font::bold));
    g.drawText ("EDGE", 208, 8, 150, 32, juce::Justification::centredLeft);
    g.setColour (t.accent);
    g.drawText ("EDGE", 206, 8, 150, 32, juce::Justification::centredLeft);

    g.setColour (t.accentAlt.withAlpha (0.75f));
    g.setFont (uiFont (8.0f, juce::Font::bold));
    g.drawText ("SEMI-MODULAR PERCUSSION SYNTHESIZER  //  NEON PROTOCOL",
                20, 42, 360, 14, juce::Justification::centredLeft);

    // Preset name window.
    auto nameBox = Layout::presetNameBox();
    g.setColour (t.scopeBackground);
    g.fillRoundedRectangle (nameBox, 3.0f);
    g.setColour (t.accent.withAlpha (0.45f));
    g.drawRoundedRectangle (nameBox, 3.0f, 1.0f);

    g.setColour (t.accentAlt);
    g.setFont (uiFont (8.0f, juce::Font::bold));
    g.drawText ("PATCH", nameBox.getX() + 8.0f, nameBox.getY() + 2.0f, 60.0f, 10.0f,
                juce::Justification::centredLeft);

    g.setColour (t.silkscreen);
    g.setFont (uiFont (12.0f, juce::Font::bold));
    g.drawFittedText (processor.getCurrentPresetName(),
                      nameBox.reduced (8.0f, 4.0f).withTrimmedTop (8.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.85f);
}

void ReplicantEdgyPanel::drawPatchBay (juce::Graphics& g)
{
    const auto& t = theme();
    auto& patch = processor.getEngine().getPatchBay();
    const auto telemetry = processor.getEngine().getTelemetry();

    g.setColour (t.silkscreenDim);
    g.setFont (uiFont (8.0f, juce::Font::bold));
    g.drawText ("PATCH BAY  //  24 POINTS  //  DRAG OUT > IN  //  RIGHT-CLICK TO UNPATCH",
                16, 60, 620, 14, juce::Justification::centredLeft);

    g.setColour (t.accentAlt.withAlpha (0.55f));
    g.drawText ("WHITE = INPUT    CYAN = OUTPUT", 900, 60, 360, 14,
                juce::Justification::centredRight);

    // Recessed patch bay strip.
    g.setColour (juce::Colours::black.withAlpha (0.32f));
    g.fillRoundedRectangle (16.0f, 74.0f, 1210.0f, 128.0f, 5.0f);
    g.setColour (t.accent.withAlpha (0.10f));
    g.drawRoundedRectangle (16.0f, 74.0f, 1210.0f, 128.0f, 5.0f, 1.0f);

    for (int i = 0; i < numJacks; ++i)
    {
        const auto jack = (Jack) i;
        const auto position = Layout::jackPosition (jack);
        const auto labelPosition = Layout::jackLabelPosition (jack);
        const bool isInput = jackIsInput (jack);

        // Which cable feeds this input (if any)?
        bool connected = false;
        Cable cable;

        if (isInput)
            connected = patch.cableInto (jack, cable);
        else
        {
            for (const auto& candidate : patch.getCables())
                if (candidate.source == jack)
                {
                    connected = true;
                    break;
                }
        }

        const float activity = isInput ? 0.0f
                                       : juce::jlimit (0.0f, 1.0f,
                                                       std::abs (processor.getEngine()
                                                                     .getJackOutput (jack)));

        EdgyLookAndFeel::drawJack (g, position, Layout::jackRadius, isInput, connected,
                                   isInput ? 0.4f + 0.2f * std::sin (cablePhase)
                                           : activity);

        juce::ignoreUnused (labelPosition);

        // Small direction pips.
        g.setColour (isInput ? t.silkscreen.withAlpha (0.35f) : t.accentAlt.withAlpha (0.45f));
        const float pipY = position.y + Layout::jackRadius + 4.0f;
        g.fillEllipse (position.x - 1.5f, pipY - 1.5f, 3.0f, 3.0f);
    }

    // PHONES jack.
    const auto phones = Layout::phonesJackPosition();
    EdgyLookAndFeel::drawJack (g, phones, Layout::jackRadius, false, false, 0.0f);

    g.setColour (t.silkscreen.withAlpha (0.75f));
    g.setFont (uiFont (7.6f, juce::Font::bold));
    g.drawText ("PHONES", phones.x - 30.0f, phones.y - 32.0f, 60.0f, 12.0f,
                juce::Justification::centred);

    g.setColour (t.silkscreenDim);
    g.setFont (uiFont (6.5f, juce::Font::plain));
    g.drawText ("IN / OUT", phones.x - 30.0f, phones.y + 20.0f, 60.0f, 10.0f,
                juce::Justification::centred);

    juce::ignoreUnused (telemetry);
}

void ReplicantEdgyPanel::drawJackLabels (juce::Graphics& g)
{
    const auto& t = theme();

    for (int i = 0; i < numJacks; ++i)
    {
        const auto jack = (Jack) i;
        const auto labelPosition = Layout::jackLabelPosition (jack);
        const auto label = juce::String (jackLabel (jack));
        const bool isInput = jackIsInput (jack);

        g.setColour (isInput ? t.silkscreen.withAlpha (0.92f)
                             : t.accentAlt.withAlpha (0.92f));
        g.setFont (uiFont (7.6f, juce::Font::bold));

        const float labelWidth = juce::jmax (46.0f, (float) label.length() * 5.0f);
        g.drawFittedText (label,
                          juce::Rectangle<float> (labelPosition.x - labelWidth * 0.5f,
                                                  labelPosition.y - 7.0f,
                                                  labelWidth, 14.0f).toNearestInt(),
                          juce::Justification::centred, 1, 0.8f);
    }
}

//==============================================================================
void ReplicantEdgyPanel::drawSectionFrame (juce::Graphics& g,
                                                          juce::Rectangle<float> bounds,
                                                          const juce::String& title)
{
    const auto& t = theme();

    // Drop shadow, so the module reads as a raised metal can on the chassis.
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f), 5.0f);

    juce::ColourGradient gradient (t.panelFace.brighter (0.10f), bounds.getX(), bounds.getY(),
                                   t.panelFaceDark.darker (0.25f), bounds.getX(),
                                   bounds.getBottom(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, 4.0f);

    // Inner top highlight and neon rim.
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine ((int) bounds.getY() + 1, bounds.getX() + 6.0f,
                          bounds.getRight() - 6.0f);

    g.setColour (t.accent.withAlpha (0.35f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    // Corner brackets.
    const float tick = 10.0f;
    g.setColour (t.accent.withAlpha (0.75f));

    for (auto corner : { bounds.getTopLeft(), bounds.getTopRight(),
                         bounds.getBottomLeft(), bounds.getBottomRight() })
    {
        const float dx = corner.x < bounds.getCentreX() ? tick : -tick;
        const float dy = corner.y < bounds.getCentreY() ? tick : -tick;
        g.drawLine (corner.x, corner.y, corner.x + dx, corner.y, 1.6f);
        g.drawLine (corner.x, corner.y, corner.x, corner.y + dy, 1.6f);
    }

    g.setColour (t.accent);
    g.setFont (uiFont (8.5f, juce::Font::bold));
    g.drawText (title, bounds.getX() + 12.0f, bounds.getY() + 5.0f,
                bounds.getWidth() - 24.0f, 14.0f, juce::Justification::centredLeft);

    g.setColour (t.accent.withAlpha (0.20f));
    g.drawHorizontalLine ((int) (bounds.getY() + 22.0f), bounds.getX() + 8.0f,
                          bounds.getRight() - 8.0f);
}

void ReplicantEdgyPanel::drawSwitch (juce::Graphics& g, juce::Rectangle<float> bounds,
                                                    const juce::String& top,
                                                    const juce::String& bottom, bool topActive,
                                                    bool isVertical)
{
    const auto& t = theme();

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (bounds, 3.0f);

    g.setColour (t.buttonEdge);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    auto lever = isVertical
                     ? juce::Rectangle<float> (bounds.getWidth() - 8.0f, bounds.getHeight() * 0.38f)
                           .withCentre ({ bounds.getCentreX(),
                                          topActive ? bounds.getY() + bounds.getHeight() * 0.28f
                                                    : bounds.getBottom() - bounds.getHeight() * 0.28f })
                     : juce::Rectangle<float> (bounds.getWidth() * 0.38f, bounds.getHeight() - 8.0f)
                           .withCentre ({ topActive ? bounds.getX() + bounds.getWidth() * 0.28f
                                                    : bounds.getRight() - bounds.getWidth() * 0.28f,
                                          bounds.getCentreY() });

    g.setColour (t.knobFace.brighter (0.15f));
    g.fillRoundedRectangle (lever, 2.5f);
    g.setColour (t.accent.withAlpha (0.85f));
    g.drawRoundedRectangle (lever, 2.5f, 1.0f);

    g.setColour (t.accent);
    g.fillEllipse (lever.getCentreX() - 2.0f, lever.getCentreY() - 2.0f, 4.0f, 4.0f);

    g.setColour (t.silkscreen.withAlpha (0.65f));
    g.setFont (uiFont (6.6f, juce::Font::bold));

    if (isVertical)
    {
        g.drawText (top, bounds.getX() - 8.0f, bounds.getY() - 12.0f, bounds.getWidth() + 16.0f,
                    10.0f, juce::Justification::centred);
        g.drawText (bottom, bounds.getX() - 8.0f, bounds.getBottom() + 2.0f,
                    bounds.getWidth() + 16.0f, 10.0f, juce::Justification::centred);
    }
    else
    {
        g.drawText (top, bounds.getX() - 34.0f, bounds.getCentreY() - 5.0f, 30.0f, 10.0f,
                    juce::Justification::centredRight);
        g.drawText (bottom, bounds.getRight() + 4.0f, bounds.getCentreY() - 5.0f, 34.0f, 10.0f,
                    juce::Justification::centredLeft);
    }
}

void ReplicantEdgyPanel::drawThreeWaySwitch (juce::Graphics& g,
                                                            juce::Rectangle<float> bounds,
                                                            const juce::String& a,
                                                            const juce::String& b,
                                                            const juce::String& c,
                                                            int state)
{
    const auto& t = theme();

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (t.buttonEdge);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    // Labels live inside the switch, one per notch, so they can never collide
    // with the neighbouring knob.
    const juce::String labels[3] = { a, b, c };
    const float notch = bounds.getHeight() / 3.0f;

    for (int i = 0; i < 3; ++i)
    {
        const float y = bounds.getY() + i * notch;
        const bool active = (i == juce::jlimit (0, 2, state));

        g.setColour (active ? t.accent : t.silkscreen.withAlpha (0.45f));
        g.setFont (uiFont (5.4f, juce::Font::bold));
        g.drawText (labels[i],
                    juce::Rectangle<float> (bounds.getX() + 2.0f, y + 1.0f,
                                            bounds.getWidth() - 12.0f, notch - 2.0f)
                        .toNearestInt(),
                    juce::Justification::centredLeft);
    }

    auto lever = juce::Rectangle<float> (8.0f, notch * 0.72f);
    lever.setCentre (bounds.getRight() - 7.0f,
                     bounds.getY() + notch * (0.5f + (float) juce::jlimit (0, 2, state)));

    g.setColour (t.knobFace.brighter (0.18f));
    g.fillRoundedRectangle (lever, 2.5f);
    g.setColour (t.accent);
    g.drawRoundedRectangle (lever, 2.5f, 1.0f);
    g.fillEllipse (lever.getCentreX() - 2.0f, lever.getCentreY() - 2.0f, 4.0f, 4.0f);
}

void ReplicantEdgyPanel::drawLabel (juce::Graphics& g, float centreX, float y,
                                                   const juce::String& text, float size,
                                                   juce::Colour colour)
{
    g.setColour (colour);
    g.setFont (uiFont (size, juce::Font::bold));
    g.drawText (text, centreX - 48.0f, y, 96.0f, 12.0f, juce::Justification::centred);
}

//==============================================================================
void ReplicantEdgyPanel::drawOscillator1 (juce::Graphics& g)
{
    const auto& t = theme();
    const auto bounds = Layout::osc1Section();
    drawSectionFrame (g, bounds, "OSCILLATOR (VCO 1)");

    drawLabel (g, 66.0f, 308.0f, "FREQUENCY");
    drawLabel (g, 208.0f, 308.0f, "ENVELOPE");
    drawLabel (g, 276.0f, 306.0f, "SEQ PITCH MOD", 7.5f);
    drawLabel (g, 374.0f, 300.0f, "LEVEL");

    g.setColour (t.silkscreen.withAlpha (0.6f));
    g.setFont (uiFont (7.0f, juce::Font::bold));
    g.drawText ("SHAPE", 108.0f, 234.0f, 58.0f, 10.0f, juce::Justification::centred);

    const bool square = processor.apvts.getRawParameterValue (ParamID::osc1Shape)->load() > 0.5f;
    drawSwitch (g, osc1ShapeSwitch, "SQR", "TRI", square);

    const int target = juce::roundToInt (
        processor.apvts.getRawParameterValue (ParamID::seqPitchTarget)->load());
    drawThreeWaySwitch (g, seqTargetSwitch, "OFF", "2", "12", target);
}

void ReplicantEdgyPanel::drawOscillator2 (juce::Graphics& g)
{
    const auto& t = theme();
    const auto bounds = Layout::osc2Section();
    drawSectionFrame (g, bounds, "OSCILLATOR (VCO 2)");

    drawLabel (g, 66.0f, 492.0f, "FREQUENCY");
    drawLabel (g, 208.0f, 492.0f, "ENVELOPE");
    drawLabel (g, 350.0f, 492.0f, "LEVEL");
    drawLabel (g, 438.0f, 492.0f, "1-2 FM AMT", 7.2f);

    g.setColour (t.silkscreen.withAlpha (0.6f));
    g.setFont (uiFont (7.0f, juce::Font::bold));
    g.drawText ("SHAPE", 108.0f, 416.0f, 58.0f, 10.0f, juce::Justification::centred);

    const bool square = processor.apvts.getRawParameterValue (ParamID::osc2Shape)->load() > 0.5f;
    drawSwitch (g, osc2ShapeSwitch, "SQR", "SAW", square);

    const bool sync = processor.apvts.getRawParameterValue (ParamID::hardSync)->load() > 0.5f;
    drawSwitch (g, hardSyncSwitch, "ON", "OFF", sync, false);

    g.setColour (t.silkscreen.withAlpha (0.6f));
    g.setFont (uiFont (7.0f, juce::Font::bold));
    g.drawText ("HARD SYNC", 238.0f, 430.0f, 86.0f, 10.0f, juce::Justification::centred);
}

void ReplicantEdgyPanel::drawEnvelopeSection (juce::Graphics& g)
{
    drawSectionFrame (g, Layout::egSection(), "EG (VCO)");
    drawLabel (g, 446.0f, 308.0f, "DECAY", 7.2f);
}

void ReplicantEdgyPanel::drawNoiseSection (juce::Graphics& g)
{
    const auto& t = theme();
    drawSectionFrame (g, Layout::noiseSection(), "NOISE");

    g.setColour (t.silkscreen.withAlpha (0.6f));
    g.setFont (uiFont (7.0f, juce::Font::bold));
    g.drawText ("COLOUR", 484.0f, 432.0f, 74.0f, 10.0f, juce::Justification::centred);

    const bool white = processor.apvts.getRawParameterValue (ParamID::noiseColour)->load() > 0.5f;
    drawSwitch (g, noiseColourSwitch, "WHT", "PNK", white);

    drawLabel (g, 568.0f, 308.0f, "NOISE", 7.2f);
}

void ReplicantEdgyPanel::drawFilter (juce::Graphics& g)
{
    const auto& t = theme();
    drawSectionFrame (g, Layout::filterSection(), "FILTER (VCF)");

    const bool highpass = processor.apvts.getRawParameterValue (ParamID::vcfMode)->load() > 0.5f;
    drawSwitch (g, vcfModeSwitch, "HP", "LP", highpass, false);

    drawLabel (g, 596.0f, 492.0f, "CUTOFF", 7.2f);
    drawLabel (g, 664.0f, 492.0f, "RES", 7.2f);
    drawLabel (g, 732.0f, 492.0f, "DECAY", 7.2f);
    drawLabel (g, 800.0f, 492.0f, "ENV", 7.2f);
    drawLabel (g, 866.0f, 492.0f, "N/VCF", 6.8f);

    g.setColour (t.silkscreenDim);
    g.setFont (uiFont (6.4f, juce::Font::plain));
    g.drawText ("20Hz", 556.0f, 414.0f, 40.0f, 10.0f, juce::Justification::centredRight);
    g.drawText ("20kHz", 620.0f, 414.0f, 40.0f, 10.0f, juce::Justification::centredLeft);
}

void ReplicantEdgyPanel::drawVca (juce::Graphics& g)
{
    const auto& t = theme();
    drawSectionFrame (g, Layout::vcaSection(), "OUTPUT (VCA)");

    const bool fast = processor.apvts.getRawParameterValue (ParamID::vcaMode)->load() < 0.5f;
    drawSwitch (g, vcaModeSwitch, "FAST", "SLOW", fast);

    drawLabel (g, 718.0f, 308.0f, "DECAY");
    drawLabel (g, 808.0f, 308.0f, "VOLUME");

    const auto telemetry = processor.getEngine().getTelemetry();
    const float level = juce::jlimit (0.0f, 1.0f, telemetry.vcaEnv * 1.4f);

    const juce::Point<float> led (866.0f, 276.0f);
    const auto rect = juce::Rectangle<float> (12.0f, 12.0f).withCentre (led);

    if (level > 0.02f)
    {
        g.setColour (t.led.withAlpha (0.20f + 0.35f * level));
        g.fillEllipse (rect.expanded (7.0f));
    }

    g.setColour (t.ledOff);
    g.fillEllipse (rect);
    g.setColour (t.led.withAlpha (0.25f + 0.75f * level));
    g.fillEllipse (rect.reduced (2.0f));

    g.setColour (t.silkscreenDim);
    g.setFont (uiFont (6.4f, juce::Font::bold));
    g.drawText ("VCA", 848.0f, 288.0f, 36.0f, 10.0f, juce::Justification::centred);
}

//==============================================================================
void ReplicantEdgyPanel::drawStepLeds (juce::Graphics& g)
{
    const auto& t = theme();
    const auto telemetry = processor.getEngine().getTelemetry();

    for (int i = 0; i < StepSequencer::numSteps; ++i)
    {
        const float x = Layout::stepStartX + i * Layout::stepStepX + Layout::stepKnobSize * 0.5f;
        const float y = Layout::stepLedY + 4.0f;
        const bool active = telemetry.playing && telemetry.step == i;

        const auto rect = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x, y });

        if (active)
        {
            g.setColour (t.led.withAlpha (0.35f));
            g.fillEllipse (rect.expanded (5.0f));
            g.setColour (t.led);
            g.fillEllipse (rect);
        }
        else
        {
            g.setColour (t.ledOff);
            g.fillEllipse (rect);
        }

        g.setColour (t.silkscreen.withAlpha (0.55f));
        g.setFont (uiFont (7.5f, juce::Font::bold));
        g.drawText (juce::String (i + 1),
                    juce::Rectangle<float> (x - 20.0f, y - 22.0f, 40.0f, 12.0f).toNearestInt(),
                    juce::Justification::centred);
    }
}

void ReplicantEdgyPanel::drawSequencer (juce::Graphics& g)
{
    const auto& t = theme();
    drawSectionFrame (g, Layout::seqSection(), "SEQUENCER");

    g.setColour (t.silkscreen.withAlpha (0.6f));
    g.setFont (uiFont (7.2f, juce::Font::bold));
    g.drawText ("TEMPO", 48.0f, 590.0f, 60.0f, 12.0f, juce::Justification::centred);
    g.drawText ("CLOCK", 126.0f, 590.0f, 60.0f, 12.0f, juce::Justification::centred);
    g.drawText ("SCALE", 196.0f, 590.0f, 60.0f, 12.0f, juce::Justification::centred);

    const int clockSource = juce::roundToInt (
        processor.apvts.getRawParameterValue (ParamID::clockSource)->load());

    // Notches run top to bottom as EXT / MIDI / INT, which is the order
    // 2 - clockSource indexes. Getting this backwards would draw the lever on
    // the wrong label, and MIDI is now the factory default.
    drawThreeWaySwitch (g, clockSwitchZone, "EXT", "MIDI", "INT", 2 - clockSource);

    g.setColour (t.silkscreenDim);
    g.setFont (uiFont (6.5f, juce::Font::plain));
    g.drawText (getScaleNames()[juce::jlimit (0, 7, juce::roundToInt (
                    processor.apvts.getRawParameterValue (ParamID::scale)->load()))],
                196.0f, 688.0f, 68.0f, 10.0f, juce::Justification::centred);

    auto& sequencer = processor.getEngine().getSequencer();

    auto drawTransport = [&] (juce::Rectangle<float> rect, const juce::String& label, bool lit)
    {
        g.setColour (juce::Colours::black.withAlpha (0.50f));
        g.fillRoundedRectangle (rect.translated (0.0f, 2.0f), 4.0f);

        juce::ColourGradient gradient (lit ? t.buttonLit : t.buttonFace.brighter (0.05f),
                                       rect.getX(), rect.getY(),
                                       lit ? t.buttonLit.darker (0.35f) : t.buttonFace.darker (0.45f),
                                       rect.getX(), rect.getBottom(), false);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (rect, 4.0f);

        g.setColour (lit ? t.accent : t.buttonEdge);
        g.drawRoundedRectangle (rect, 4.0f, 1.2f);

        g.setColour (lit ? juce::Colours::black.withAlpha (0.85f) : t.silkscreen);
        g.setFont (uiFont (7.6f, juce::Font::bold));
        g.drawFittedText (label, rect.toNearestInt().reduced (3, 2),
                          juce::Justification::centred, 2, 0.8f);
    };

    drawTransport (triggerZone, "TRIGGER", false);
    drawTransport (playZone, sequencer.isPlaying() ? "STOP" : "PLAY", sequencer.isPlaying());
    drawTransport (advanceZone, "ADVANCE", false);

    g.setColour (t.silkscreen.withAlpha (0.55f));
    g.setFont (uiFont (8.0f, juce::Font::bold));
    g.drawText ("PITCH", 508.0f, 632.0f, 64.0f, 14.0f, juce::Justification::centredRight);
    g.drawText ("VEL", 508.0f, 712.0f, 64.0f, 14.0f, juce::Justification::centredRight);

    drawStepLeds (g);
}

//==============================================================================
void ReplicantEdgyPanel::drawScope (juce::Graphics& g)
{
    const auto& t = theme();
    const auto bounds = Layout::scopeSection();
    drawSectionFrame (g, bounds, "SIGNAL ANALYSER");

    auto scopeArea = juce::Rectangle<float> (bounds.getX() + 12.0f, bounds.getY() + 34.0f,
                                             bounds.getWidth() - 24.0f, 118.0f);

    g.setColour (t.scopeBackground);
    g.fillRoundedRectangle (scopeArea, 3.0f);

    g.setColour (t.accentAlt.withAlpha (0.10f));

    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (scopeArea.getY() + scopeArea.getHeight() * i / 4.0f),
                              scopeArea.getX(), scopeArea.getRight());

    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine ((int) (scopeArea.getX() + scopeArea.getWidth() * i / 8.0f),
                            scopeArea.getY(), scopeArea.getBottom());

    juce::Path waveform;
    const int count = SynthEngine::scopeSize;

    for (int i = 0; i < count; ++i)
    {
        const float x = scopeArea.getX()
                        + scopeArea.getWidth() * (float) i / (float) (count - 1);
        const float value = juce::jlimit (-1.0f, 1.0f, scopeData[i] * 1.6f);
        const float y = scopeArea.getCentreY() - value * scopeArea.getHeight() * 0.42f;

        if (i == 0)
            waveform.startNewSubPath (x, y);
        else
            waveform.lineTo (x, y);
    }

    g.setColour (t.accent.withAlpha (0.22f));
    g.strokePath (waveform, juce::PathStrokeType (5.0f));
    g.setColour (t.accentAlt);
    g.strokePath (waveform, juce::PathStrokeType (1.4f));

    // Meters.
    const auto telemetry = processor.getEngine().getTelemetry();
    auto meterRow = [&] (float y, const juce::String& label, float value, juce::Colour colour)
    {
        g.setColour (t.silkscreenDim);
        g.setFont (uiFont (7.5f, juce::Font::bold));
        g.drawText (label, bounds.getX() + 12.0f, y - 6.0f, 52.0f, 12.0f,
                    juce::Justification::centredLeft);

        auto track = juce::Rectangle<float> (bounds.getX() + 68.0f, y - 4.0f,
                                             bounds.getWidth() - 92.0f, 8.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (track, 3.0f);

        const float norm = juce::jlimit (0.0f, 1.0f, std::abs (value));
        g.setColour (colour.withAlpha (0.9f));
        g.fillRoundedRectangle (track.withWidth (juce::jmax (3.0f, track.getWidth() * norm)), 3.0f);
    };

    const float meterY = bounds.getY() + 172.0f;
    meterRow (meterY, "VCO 1", telemetry.osc1, t.accent);
    meterRow (meterY + 24.0f, "VCO 2", telemetry.osc2, t.accentAlt);
    meterRow (meterY + 48.0f, "VCA", telemetry.vca, t.warning);
    meterRow (meterY + 72.0f, "VCF EG", telemetry.vcfEnv, t.accentAlt);

    g.setColour (t.silkscreen.withAlpha (0.75f));
    g.setFont (uiFont (8.0f, juce::Font::bold));
    g.drawText ("CUTOFF  " + juce::String (telemetry.cutoff, 0) + " Hz",
                bounds.getX() + 12.0f, bounds.getY() + 286.0f, 200.0f, 12.0f,
                juce::Justification::centredLeft);

    g.drawText ("STEP  " + juce::String (telemetry.step + 1) + " / 8",
                bounds.getX() + 12.0f, bounds.getY() + 302.0f, 200.0f, 12.0f,
                juce::Justification::centredLeft);

    g.setColour (t.accentAlt.withAlpha (0.65f));
    g.drawText (juce::String (processor.getEngine().getPatchBay().getNumCables()) + " PATCH CABLES",
                bounds.getX() + 12.0f, bounds.getY() + 318.0f, 220.0f, 12.0f,
                juce::Justification::centredLeft);

    // EDGE emblem.
    const juce::Point<float> emblem (bounds.getRight() - 66.0f, bounds.getBottom() - 30.0f);

    g.setColour (t.accent.withAlpha (0.12f));
    g.fillEllipse (juce::Rectangle<float> (54.0f, 54.0f).withCentre (emblem));
    g.setColour (t.accent.withAlpha (0.75f));
    g.drawEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (emblem), 1.4f);

    juce::Path zigzag;
    zigzag.startNewSubPath (emblem.x - 12.0f, emblem.y + 4.0f);
    zigzag.lineTo (emblem.x - 5.0f, emblem.y - 6.0f);
    zigzag.lineTo (emblem.x + 1.0f, emblem.y + 5.0f);
    zigzag.lineTo (emblem.x + 8.0f, emblem.y - 6.0f);
    zigzag.lineTo (emblem.x + 13.0f, emblem.y + 3.0f);
    g.strokePath (zigzag, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
}

//==============================================================================
void ReplicantEdgyPanel::drawCables (juce::Graphics& g)
{
    auto& patch = processor.getEngine().getPatchBay();
    const auto& cables = patch.getCables();

    for (int i = 0; i < (int) cables.size(); ++i)
    {
        const auto& cable = cables[(size_t) i];
        const auto from = Layout::jackPosition (cable.source);
        const auto to = Layout::jackPosition (cable.destination);
        const auto colour = theme().cableColour (cable.colour);

        const float activity = juce::jlimit (0.0f, 1.0f,
                                             std::abs (processor.getEngine()
                                                           .getJackOutput (cable.source)));

        drawCable (g, from, to, colour, 3.0f, 0.25f + activity * 0.5f, false);
    }

    if (draggingCable && dragSource != Jack::NumJacks)
    {
        const auto from = Layout::jackPosition (dragSource);
        const auto colour = theme().cableColour (patch.getNumCables());

        drawCable (g, from, dragPoint, colour, 3.0f, 0.7f, true);
    }
}

void ReplicantEdgyPanel::drawCable (juce::Graphics& g, juce::Point<float> from,
                                                   juce::Point<float> to, juce::Colour colour,
                                                   float thickness, float glow, bool preview)
{
    // Cables hang, but they must not dive into the panel sections: both
    // control points share a clamped rail just under the patch bay.
    const float distance = from.getDistanceFrom (to);
    const float sag = juce::jmin (30.0f, 10.0f + distance * 0.05f);
    const float railY = juce::jmin (198.0f, juce::jmax (from.y, to.y) + sag);

    juce::Path path;
    path.startNewSubPath (from);
    path.cubicTo (from.x, railY, to.x, railY, to.x, to.y);

    if (preview)
    {
        const float dashes[] = { 6.0f, 5.0f };
        juce::Path dashed;
        juce::PathStrokeType (2.0f).createDashedStroke (dashed, path, dashes, 2);
        g.setColour (colour.withAlpha (0.75f));
        g.fillPath (dashed);
    }
    else
    {
        g.setColour (colour.withAlpha (glow * 0.45f));
        g.strokePath (path, juce::PathStrokeType (thickness + 6.0f));

        g.setColour (colour.withAlpha (0.92f));
        g.strokePath (path, juce::PathStrokeType (thickness));

        g.setColour (juce::Colours::white.withAlpha (0.30f));
        g.strokePath (path, juce::PathStrokeType (thickness * 0.35f));

        // Plugs.
        for (auto point : { from, to })
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (point + juce::Point<float> (0.0f, 1.0f)));
            g.setColour (colour);
            g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (point));
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (point - juce::Point<float> (1.5f, 1.5f)));
        }
    }
}

//==============================================================================
// Interaction
//==============================================================================
void ReplicantEdgyPanel::handleSwitchClick (juce::Point<float> position)
{
    if (osc1ShapeSwitch.contains (position))
    {
        toggleBoolParameter (ParamID::osc1Shape);
        return;
    }

    if (osc2ShapeSwitch.contains (position))
    {
        toggleBoolParameter (ParamID::osc2Shape);
        return;
    }

    if (hardSyncSwitch.contains (position))
    {
        toggleBoolParameter (ParamID::hardSync);
        return;
    }

    if (noiseColourSwitch.contains (position))
    {
        toggleBoolParameter (ParamID::noiseColour);
        return;
    }

    if (vcfModeSwitch.contains (position))
    {
        toggleBoolParameter (ParamID::vcfMode);
        return;
    }

    if (vcaModeSwitch.contains (position))
    {
        toggleBoolParameter (ParamID::vcaMode);
        return;
    }

    if (seqTargetSwitch.contains (position))
    {
        cycleChoiceParameter (ParamID::seqPitchTarget, 3);
        return;
    }

    if (clockSwitchZone.contains (position))
    {
        cycleChoiceParameter (ParamID::clockSource, 3);
        return;
    }

    auto& sequencer = processor.getEngine().getSequencer();

    if (triggerZone.contains (position))
    {
        sequencer.triggerNow();
        return;
    }

    if (playZone.contains (position))
    {
        sequencer.togglePlaying();
        repaint();
        return;
    }

    if (advanceZone.contains (position))
    {
        sequencer.advanceNow();
        return;
    }
}

void ReplicantEdgyPanel::toggleBoolParameter (const juce::String& id)
{
    if (auto* parameter = processor.apvts.getParameter (id))
    {
        const float next = parameter->getValue() > 0.5f ? 0.0f : 1.0f;
        parameter->setValueNotifyingHost (next);
        repaint();
    }
}

void ReplicantEdgyPanel::cycleChoiceParameter (const juce::String& id, int numStates)
{
    if (auto* parameter = processor.apvts.getParameter (id))
    {
        const int current = juce::roundToInt (parameter->getValue() * (float) (numStates - 1));
        const int next = (current + 1) % numStates;
        parameter->setValueNotifyingHost ((float) next / (float) (numStates - 1));
        repaint();
    }
}

void ReplicantEdgyPanel::handleHeaderClick (juce::Point<float>)
{
    // Header buttons are real components; nothing to do here yet.
}

bool ReplicantEdgyPanel::hitCable (juce::Point<float> point, int& cableIndex) const
{
    const auto& cables = processor.getEngine().getPatchBay().getCables();
    float best = 8.0f;
    bool found = false;

    for (int i = 0; i < (int) cables.size(); ++i)
    {
        const auto from = Layout::jackPosition (cables[(size_t) i].source);
        const auto to = Layout::jackPosition (cables[(size_t) i].destination);
        const float distance = from.getDistanceFrom (to);
        const float sag = juce::jmin (30.0f, 10.0f + distance * 0.05f);
        const float railY = juce::jmin (198.0f, juce::jmax (from.y, to.y) + sag);

        juce::Point<float> previous = from;

        for (int s = 1; s <= 24; ++s)
        {
            const float t = (float) s / 24.0f;
            const float mt = 1.0f - t;

            const juce::Point<float> cubic {
                mt * mt * mt * from.x + 3.0f * mt * mt * t * from.x
                    + 3.0f * mt * t * t * to.x + t * t * t * to.x,
                mt * mt * mt * from.y + 3.0f * mt * mt * t * railY
                    + 3.0f * mt * t * t * railY + t * t * t * to.y
            };

            const auto line = juce::Line<float> (previous, cubic);
            const float distance = point.getDistanceFrom (line.findNearestPointTo (point));

            if (distance < best)
            {
                best = distance;
                cableIndex = i;
                found = true;
            }

            previous = cubic;
        }
    }

    return found;
}

void ReplicantEdgyPanel::removeCablesAt (Jack jack)
{
    processor.getEngine().getPatchBay().disconnect (jack);
    repaint();
}

void ReplicantEdgyPanel::mouseDown (const juce::MouseEvent& e)
{
    const auto position = e.position;

    if (e.mods.isPopupMenu())
    {
        const auto jack = Layout::jackAt (position);

        if (jack != Jack::NumJacks)
        {
            removeCablesAt (jack);
            return;
        }

        int cableIndex = -1;

        if (hitCable (position, cableIndex))
        {
            processor.getEngine().getPatchBay().removeAt (cableIndex);
            repaint();
        }

        return;
    }

    handleSwitchClick (position);

    const auto jack = Layout::jackAt (position);

    if (jack != Jack::NumJacks)
    {
        dragSource = jack;
        dragPoint = position;
        draggingCable = true;
        repaint();
    }
}

void ReplicantEdgyPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingCable)
        return;

    dragPoint = e.position;
    repaint (0, 70, Layout::panelWidth, 140);
}

void ReplicantEdgyPanel::mouseUp (const juce::MouseEvent& e)
{
    if (! draggingCable)
        return;

    const auto target = Layout::jackAt (e.position);
    const auto source = dragSource;

    draggingCable = false;
    dragSource = Jack::NumJacks;

    if (target != Jack::NumJacks && target != source)
    {
        if (jackIsOutput (source) && jackIsInput (target))
            processor.getEngine().getPatchBay().connect (source, target);
        else if (jackIsInput (source) && jackIsOutput (target))
            processor.getEngine().getPatchBay().connect (target, source);
    }

    repaint();
}

void ReplicantEdgyPanel::mouseMove (const juce::MouseEvent& e)
{
    const auto jack = Layout::jackAt (e.position);

    if (jack != hoverJack)
    {
        hoverJack = jack;
        setMouseCursor (jack != Jack::NumJacks ? juce::MouseCursor::PointingHandCursor
                                               : juce::MouseCursor::NormalCursor);
    }
}

void ReplicantEdgyPanel::mouseExit (const juce::MouseEvent&)
{
    hoverJack = Jack::NumJacks;
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

bool ReplicantEdgyPanel::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && drawerOpen)
    {
        setPresetDrawerOpen (false);
        return true;
    }

    if (key == juce::KeyPress::spaceKey)
    {
        processor.getEngine().getSequencer().togglePlaying();
        repaint();
        return true;
    }

    // Ctrl/Cmd +/-/0: zoom without hunting for the header slider.
    const auto modifiers = key.getModifiers();

    if (modifiers.isCommandDown() || modifiers.isCtrlDown())
    {
        const auto character = key.getTextCharacter();

        if (character == '+' || character == '=')
        {
            nudgeZoom (5.0f);
            return true;
        }

        if (character == '-' || character == '_')
        {
            nudgeZoom (-5.0f);
            return true;
        }

        if (character == '0')
        {
            nudgeZoom (100.0f - (float) zoomSlider.getValue());
            return true;
        }
    }

    return false;
}

void ReplicantEdgyPanel::nudgeZoom (float deltaPercent)
{
    const float next = juce::jlimit (50.0f, 300.0f,
                                     (float) zoomSlider.getValue() + deltaPercent);

    if (std::abs (next - (float) zoomSlider.getValue()) < 0.5f)
        return;

    setZoomPercent (next);

    if (onZoomChanged != nullptr)
        onZoomChanged (next);
}

void ReplicantEdgyPanel::setZoomPercent (float percent)
{
    const juce::ScopedValueSetter<bool> guard (updatingZoom, true);
    zoomSlider.setValue (juce::jlimit (50.0f, 300.0f, percent), juce::dontSendNotification);
}

float ReplicantEdgyPanel::getZoomPercent() const noexcept
{
    return (float) zoomSlider.getValue();
}

int ReplicantEdgyPanel::getLogicalHeight() const noexcept
{
    const bool drawerOut = drawerOpen || drawerProgress > 0.001f;
    return Layout::panelHeight + (drawerOut ? Layout::drawerHeight : 0);
}

bool ReplicantEdgyPanel::connectJacks (Jack source, Jack destination)
{
    if ((jackIsOutput (source) && jackIsInput (destination))
        || (jackIsInput (source) && jackIsOutput (destination)))
    {
        const bool ok = jackIsOutput (source)
                            ? processor.getEngine().getPatchBay().connect (source, destination)
                            : processor.getEngine().getPatchBay().connect (destination, source);
        repaint();
        return ok;
    }

    return false;
}

//==============================================================================
void ReplicantEdgyPanel::setPresetDrawerOpen (bool shouldBeOpen, bool animate)
{
    drawerOpen = shouldBeOpen;

    if (! animate)
    {
        drawerAnimating = false;
        drawerProgress = shouldBeOpen ? 1.0f : 0.0f;
        applyDrawerProgress();

        if (onLogicalHeightChanged != nullptr)
            onLogicalHeightChanged();

        repaint();
        return;
    }

    drawerAnimFrom = drawerProgress;
    drawerAnimStartMs = juce::Time::getMillisecondCounterHiRes();
    drawerAnimating = true;

    if (presetDrawer != nullptr)
        presetDrawer->setVisible (true);

    // Opening grows the window immediately so the drawer has somewhere to
    // slide into. Closing keeps the window tall until the animation ends.
    if (onLogicalHeightChanged != nullptr)
        onLogicalHeightChanged();

    startTimerHz (60);
    repaint();
}

void ReplicantEdgyPanel::applyDrawerProgress()
{
    const bool fullyClosed = drawerProgress <= 0.001f && ! drawerOpen;

    if (presetDrawer != nullptr)
    {
        presetDrawer->setBounds (0,
                                 Layout::panelHeight
                                     + (int) ((1.0f - drawerProgress) * (float) Layout::drawerHeight),
                                 Layout::panelWidth, Layout::drawerHeight);
        presetDrawer->setVisible (! fullyClosed);
    }

    if (fullyClosed && onLogicalHeightChanged != nullptr)
        onLogicalHeightChanged();
}

//==============================================================================
// Window wrapper: the whole panel is one transform away from any zoom.
//==============================================================================
ReplicantEdgyAudioProcessorEditor::ReplicantEdgyAudioProcessorEditor (ReplicantEdgyAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), panel (p)
{
    addAndMakeVisible (panel);

    zoom = juce::jlimit (minZoom, maxZoom, processor.getUiZoom());
    panel.setZoomPercent (zoom * 100.0f);

    panel.onZoomChanged = [this] (float percent) { setZoom (percent * 0.01f); };
    panel.onLogicalHeightChanged = [this]
    {
        updateConstrainer();
        syncWindowToContent();
    };

    updateConstrainer();
    setConstrainer (&constrainer);
    setResizable (true, true);

    syncWindowToContent();
}

ReplicantEdgyAudioProcessorEditor::~ReplicantEdgyAudioProcessorEditor()
{
    // The constrainer is a member and dies before the base class, so detach it.
    setResizable (false, false);
    setConstrainer (nullptr);
}

void ReplicantEdgyAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Letterbox colour for hosts that force a size that is not exactly the
    // panel's aspect ratio.
    const auto& t = ReplicantEdgy::ThemeManager::getInstance().getCurrentTheme();
    g.fillAll (t.chassisBottom.darker (0.45f));
}

void ReplicantEdgyAudioProcessorEditor::resized()
{
    const int logicalHeight = panel.getLogicalHeight();
    panel.setSize (Layout::panelWidth, logicalHeight);

    const float scaleX = (float) getWidth() / (float) Layout::panelWidth;
    const float scaleY = (float) getHeight() / (float) logicalHeight;
    const float scale = juce::jlimit (minZoom, maxZoom, juce::jmin (scaleX, scaleY));

    const float offsetX = ((float) getWidth() - (float) Layout::panelWidth * scale) * 0.5f;
    const float offsetY = ((float) getHeight() - (float) logicalHeight * scale) * 0.5f;

    panel.setTransform (juce::AffineTransform::scale (scale).translated (offsetX, offsetY));

    if (! syncingWindow)
    {
        // A host or corner-resize changed the window: adopt the resulting zoom
        // so the header readout and the window can never disagree.
        zoom = scale;
        panel.setZoomPercent (zoom * 100.0f);
        processor.setUiZoom (zoom);
    }
}

void ReplicantEdgyAudioProcessorEditor::setZoom (float newZoom)
{
    zoom = juce::jlimit (minZoom, maxZoom, newZoom);
    panel.setZoomPercent (zoom * 100.0f);
    processor.setUiZoom (zoom);
    syncWindowToContent();
}

void ReplicantEdgyAudioProcessorEditor::syncWindowToContent()
{
    const juce::ScopedValueSetter<bool> guard (syncingWindow, true);

    const int width = juce::roundToInt ((float) Layout::panelWidth * zoom);
    const int height = juce::roundToInt ((float) panel.getLogicalHeight() * zoom);

    if (width != getWidth() || height != getHeight())
        setSize (width, height);
}

void ReplicantEdgyAudioProcessorEditor::updateConstrainer()
{
    const int logicalHeight = panel.getLogicalHeight();

    constrainer.setSizeLimits (juce::roundToInt ((float) Layout::panelWidth * minZoom),
                               juce::roundToInt ((float) logicalHeight * minZoom),
                               juce::roundToInt ((float) Layout::panelWidth * maxZoom),
                               juce::roundToInt ((float) logicalHeight * maxZoom));
    constrainer.setFixedAspectRatio ((double) Layout::panelWidth / (double) logicalHeight);
}

void ReplicantEdgyAudioProcessorEditor::setPresetDrawerOpen (bool shouldBeOpen, bool animate)
{
    panel.setPresetDrawerOpen (shouldBeOpen, animate);
}

bool ReplicantEdgyAudioProcessorEditor::connectJacks (ReplicantEdgy::Jack source,
                                                      ReplicantEdgy::Jack destination)
{
    return panel.connectJacks (source, destination);
}

void ReplicantEdgyAudioProcessorEditor::refreshFromProcessor()
{
    panel.refreshFromProcessor();
}
