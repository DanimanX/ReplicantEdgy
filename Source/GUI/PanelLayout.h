#pragma once

#include "DSP/PatchBay.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace ReplicantEdgy::Layout
{
    //==============================================================================
    /** Logical geometry shared by painting, hit testing and the cable layer.
        The panel is laid out once on this 1280 x 760 grid; the window wrapper
        scales the whole grid, so painting and hit testing can never drift. */
    inline constexpr int panelWidth  = 1280;
    inline constexpr int panelHeight = 760;

    inline constexpr int drawerHeight = 340;

    inline constexpr int headerHeight = 70;

    // Patch bay grid.
    inline constexpr float jackStartX = 104.0f;
    inline constexpr float jackStepX  = 88.0f;
    inline constexpr float jackRadius = 17.0f;
    inline constexpr float jackTopY   = 120.0f;
    inline constexpr float jackBottomY = 180.0f;
    inline constexpr float jackLabelTopY = 84.0f;
    inline constexpr float jackLabelBottomY = 144.0f;

    // Section boxes.
    inline juce::Rectangle<float> osc1Section()   { return { 16.0f, 206.0f, 370.0f, 174.0f }; }
    inline juce::Rectangle<float> egSection()     { return { 392.0f, 206.0f, 84.0f, 174.0f }; }
    inline juce::Rectangle<float> noiseSection()  { return { 482.0f, 206.0f, 124.0f, 174.0f }; }
    inline juce::Rectangle<float> vcaSection()    { return { 612.0f, 206.0f, 288.0f, 174.0f }; }
    inline juce::Rectangle<float> scopeSection()  { return { 906.0f, 206.0f, 358.0f, 348.0f }; }
    inline juce::Rectangle<float> osc2Section()   { return { 16.0f, 388.0f, 454.0f, 172.0f }; }
    inline juce::Rectangle<float> filterSection() { return { 476.0f, 388.0f, 424.0f, 172.0f }; }
    inline juce::Rectangle<float> seqSection()    { return { 16.0f, 568.0f, 1248.0f, 182.0f }; }

    // Header widgets.
    inline juce::Rectangle<float> presetPrevButton()  { return { 470.0f, 20.0f, 34.0f, 30.0f }; }
    inline juce::Rectangle<float> presetNameBox()     { return { 508.0f, 20.0f, 180.0f, 30.0f }; }
    inline juce::Rectangle<float> presetNextButton()  { return { 692.0f, 20.0f, 34.0f, 30.0f }; }
    inline juce::Rectangle<float> saveButton()        { return { 730.0f, 20.0f, 72.0f, 30.0f }; }
    inline juce::Rectangle<float> presetsButton()     { return { 806.0f, 20.0f, 88.0f, 30.0f }; }
    inline juce::Rectangle<float> randomButton()      { return { 898.0f, 20.0f, 88.0f, 30.0f }; }
    inline juce::Rectangle<float> themeBox()          { return { 990.0f, 20.0f, 140.0f, 30.0f }; }

    /** Panel zoom, 50..300 %. Lives in the header so it scales with the panel. */
    inline juce::Rectangle<float> zoomSlider()        { return { 1138.0f, 20.0f, 130.0f, 30.0f }; }

    // Sequential controls.
    inline juce::Rectangle<float> tempoKnob()     { return { 42.0f, 608.0f, 72.0f, 72.0f }; }
    inline juce::Rectangle<float> clockSwitch()   { return { 138.0f, 618.0f, 34.0f, 62.0f }; }
    inline juce::Rectangle<float> scaleKnob()     { return { 190.0f, 612.0f, 64.0f, 64.0f }; }
    inline juce::Rectangle<float> tempoDisplay()  { return { 26.0f, 688.0f, 110.0f, 54.0f }; }
    inline juce::Rectangle<float> triggerButton() { return { 288.0f, 620.0f, 64.0f, 46.0f }; }
    inline juce::Rectangle<float> playButton()    { return { 360.0f, 620.0f, 64.0f, 46.0f }; }
    inline juce::Rectangle<float> advanceButton() { return { 432.0f, 620.0f, 64.0f, 46.0f }; }

    inline constexpr float stepStartX = 596.0f;
    inline constexpr float stepStepX  = 84.0f;
    inline constexpr float stepPitchY = 620.0f;
    inline constexpr float stepVelocityY = 700.0f;
    inline constexpr float stepLedY   = 608.0f;
    inline constexpr int   stepKnobSize = 48;

    inline juce::Rectangle<float> stepPitchKnob (int step)
    {
        return { stepStartX + step * stepStepX, stepPitchY, (float) stepKnobSize, (float) stepKnobSize };
    }

    inline juce::Rectangle<float> stepVelocityKnob (int step)
    {
        return { stepStartX + step * stepStepX, stepVelocityY, (float) stepKnobSize, (float) stepKnobSize };
    }

    //==============================================================================
    /** Grid position of every patch point, matching the printed panel. */
    inline juce::Point<float> jackPosition (Jack jack)
    {
        static const Jack topRow[12] =
        {
            Jack::Osc1Cv, Jack::Osc2Cv, Jack::OscDecay, Jack::AdvClock,
            Jack::TriggerIn, Jack::VelocityIn, Jack::NoiseLevel,
            Jack::Osc1, Jack::Osc2, Jack::OscEg, Jack::TriggerOut, Jack::VelocityOut
        };

        static const Jack bottomRow[12] =
        {
            Jack::FmAmount, Jack::ExtAudio, Jack::TempoIn, Jack::PlayStop,
            Jack::VcfMod, Jack::VcfDecay, Jack::VcaCv, Jack::VcaDecay,
            Jack::Pitch, Jack::VcfEg, Jack::Vca, Jack::VcaEg
        };

        for (int i = 0; i < 12; ++i)
        {
            if (topRow[i] == jack)
                return { jackStartX + i * jackStepX, jackTopY };

            if (bottomRow[i] == jack)
                return { jackStartX + i * jackStepX, jackBottomY };
        }

        return { jackStartX, jackTopY };
    }

    inline juce::Point<float> jackLabelPosition (Jack jack)
    {
        const auto position = jackPosition (jack);
        const bool isTop = position.y < (jackTopY + jackBottomY) * 0.5f;
        return { position.x, isTop ? jackLabelTopY : jackLabelBottomY };
    }

    inline juce::Point<float> phonesJackPosition() { return { 1180.0f, jackBottomY }; }

    /** Nearest patch point to a screen point, or NumJacks when none is close. */
    inline Jack jackAt (juce::Point<float> point, float maxDistance = 24.0f)
    {
        Jack best = Jack::NumJacks;
        float bestDistance = maxDistance;

        for (int i = 0; i < numJacks; ++i)
        {
            const auto jack = (Jack) i;
            const float distance = point.getDistanceFrom (jackPosition (jack));

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = jack;
            }
        }

        return best;
    }
}
