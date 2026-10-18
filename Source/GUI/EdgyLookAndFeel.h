#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EdgyTheme.h"

namespace ReplicantEdgy
{
    //==============================================================================
    /** Theme-driven cyberpunk Look & Feel: neon rotary knobs, glowing arcs,
        dark chrome and hardware-style typography. */
    class EdgyLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        EdgyLookAndFeel();
        ~EdgyLookAndFeel() override = default;

        /** Re-reads the active theme. Message thread only. */
        void applyTheme();

        static const Theme& theme() noexcept { return ThemeManager::getInstance().getCurrentTheme(); }

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override;

        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;

        void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;

        void drawButtonBackground (juce::Graphics&, juce::Button&,
                                   const juce::Colour& backgroundColour,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown) override;

        void drawButtonText (juce::Graphics&, juce::TextButton&,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override;

        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

        void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                                bool isSeparator, bool isActive, bool isHighlighted,
                                bool isTicked, bool hasSubMenu, const juce::String& text,
                                const juce::String& shortcutKeyText,
                                const juce::Drawable* icon,
                                const juce::Colour* textColour) override;

        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getPopupMenuFont() override;
        juce::Font getLabelFont (juce::Label&) override;

        /** Shared procedural hardware helpers used by the editor too. */
        static void drawJack (juce::Graphics&, juce::Point<float> centre, float radius,
                              bool isInput, bool connected, float activity);
        static void drawPanelscrew (juce::Graphics&, juce::Point<float> centre, float radius);

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EdgyLookAndFeel)
    };
}
