#include "EdgyLookAndFeel.h"

namespace ReplicantEdgy
{
    namespace
    {
        juce::Colour withAlpha (juce::Colour colour, float alpha)
        {
            return colour.withAlpha (juce::jlimit (0.0f, 1.0f, alpha));
        }
    }

    //==============================================================================
    EdgyLookAndFeel::EdgyLookAndFeel()
    {
        applyTheme();
    }

    void EdgyLookAndFeel::applyTheme()
    {
        const auto& t = theme();

        setColour (juce::ResizableWindow::backgroundColourId, t.chassisBottom);
        setColour (juce::Label::textColourId, t.silkscreen);
        setColour (juce::Label::textWhenEditingColourId, t.silkscreen);

        setColour (juce::ComboBox::backgroundColourId, t.panelFaceDark);
        setColour (juce::ComboBox::textColourId, t.silkscreen);
        setColour (juce::ComboBox::outlineColourId, withAlpha (t.accent, 0.8f));
        setColour (juce::ComboBox::arrowColourId, t.accent);
        setColour (juce::ComboBox::focusedOutlineColourId, t.accentAlt);

        setColour (juce::PopupMenu::backgroundColourId, t.panelFaceDark);
        setColour (juce::PopupMenu::textColourId, t.silkscreen);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, withAlpha (t.accent, 0.28f));
        setColour (juce::PopupMenu::highlightedTextColourId, t.silkscreen);

        setColour (juce::TextButton::buttonColourId, t.buttonFace);
        setColour (juce::TextButton::buttonOnColourId, withAlpha (t.buttonLit, 0.35f));
        setColour (juce::TextButton::textColourOffId, t.silkscreen);
        setColour (juce::TextButton::textColourOnId, t.silkscreen);

        setColour (juce::ToggleButton::textColourId, t.silkscreen);
        setColour (juce::ToggleButton::tickColourId, t.accent);

        setColour (juce::TextEditor::backgroundColourId, t.panelFaceDark);
        setColour (juce::TextEditor::textColourId, t.silkscreen);
        setColour (juce::TextEditor::outlineColourId, withAlpha (t.accent, 0.6f));
        setColour (juce::TextEditor::focusedOutlineColourId, t.accentAlt);
        setColour (juce::TextEditor::highlightColourId, withAlpha (t.accent, 0.35f));

        setColour (juce::TooltipWindow::backgroundColourId, t.panelFaceDark);
        setColour (juce::TooltipWindow::textColourId, t.silkscreen);
        setColour (juce::TooltipWindow::outlineColourId, withAlpha (t.accent, 0.6f));

        setColour (juce::ListBox::backgroundColourId, t.panelFaceDark);
        setColour (juce::ListBox::outlineColourId, withAlpha (t.accent, 0.4f));
        setColour (juce::ListBox::textColourId, t.silkscreen);
    }

    //==============================================================================
    void EdgyLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float minSliderPos, float maxSliderPos,
                                            juce::Slider::SliderStyle style, juce::Slider& slider)
    {
        if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos,
                                              maxSliderPos, style, slider);
            return;
        }

        const auto& t = theme();
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
        const float centreY = bounds.getCentreY();
        const float trackHeight = juce::jmax (5.0f, bounds.getHeight() * 0.18f);

        auto track = juce::Rectangle<float> (bounds.getX(), centreY - trackHeight * 0.5f,
                                             bounds.getWidth(), trackHeight);

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (track.translated (0.0f, 1.5f), trackHeight * 0.5f);

        g.setColour (t.panelFaceDark);
        g.fillRoundedRectangle (track, trackHeight * 0.5f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (track.reduced (1.0f), trackHeight * 0.5f);
        g.setColour (t.accent.withAlpha (0.38f));
        g.drawRoundedRectangle (track, trackHeight * 0.5f, 1.0f);

        auto filled = track.withRight (juce::jlimit (track.getX(), track.getRight(), sliderPos));
        g.setColour (t.accentAlt.withAlpha (0.85f));
        g.fillRoundedRectangle (filled, trackHeight * 0.5f);

        const float radius = juce::jmax (4.0f, juce::jmin (9.0f, bounds.getHeight() * 0.36f));
        const juce::Point<float> thumb (sliderPos, centreY);

        g.setColour (t.accent.withAlpha (0.22f));
        g.fillEllipse (juce::Rectangle<float> (radius * 3.2f, radius * 3.2f).withCentre (thumb));
        g.setColour (t.knobFace.brighter (0.10f));
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (thumb));
        g.setColour (t.accent);
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (thumb), 1.3f);
        g.fillEllipse (juce::Rectangle<float> (radius * 0.7f, radius * 0.7f).withCentre (thumb));
    }

    //==============================================================================
    void EdgyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider& slider)
    {
        const auto& t = theme();
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
        const auto centre = bounds.getCentre();
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f;

        if (radius <= 2.0f)
            return;

        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const bool isStep = slider.getProperties()["stepKnob"];
        const bool isBipolar = slider.getProperties()["bipolar"];

        // Recessed socket.
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (bounds.translated (0.0f, 2.0f).expanded (1.0f));

        // Tick ring.
        const int ticks = isStep ? 9 : 11;

        for (int i = 0; i < ticks; ++i)
        {
            const float norm = (float) i / (float) (ticks - 1);
            const float tickAngle = rotaryStartAngle + norm * (rotaryEndAngle - rotaryStartAngle);
            const float inner = radius + 2.0f;
            const float outer = radius + ((i == 0 || i == ticks - 1 || i == ticks / 2) ? 7.0f : 4.0f);

            g.setColour (withAlpha (t.silkscreen, (i == 0 || i == ticks - 1) ? 0.75f : 0.32f));
            g.drawLine (centre.x + inner * std::cos (tickAngle - juce::MathConstants<float>::halfPi),
                        centre.y + inner * std::sin (tickAngle - juce::MathConstants<float>::halfPi),
                        centre.x + outer * std::cos (tickAngle - juce::MathConstants<float>::halfPi),
                        centre.y + outer * std::sin (tickAngle - juce::MathConstants<float>::halfPi),
                        (i == 0 || i == ticks - 1) ? 1.4f : 1.0f);
        }

        // Neon value arc.
        juce::Path arc;
        const float arcRadius = radius + 5.5f;
        const float from = isBipolar
                               ? rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle)
                               : rotaryStartAngle;
        const float sweep = isBipolar ? (angle - from) : (angle - rotaryStartAngle);

        arc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                           from, angle, true);

        g.setColour (withAlpha (t.accent, 0.22f));
        g.strokePath (arc, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        g.setColour (withAlpha (t.accent, 0.95f));
        g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        juce::ignoreUnused (sweep);

        // Knob body: spun metal cap with a dark skirt.
        juce::ColourGradient body (t.knobFace.brighter (0.30f), centre.x - radius * 0.5f,
                                   centre.y - radius * 0.6f,
                                   t.knobFaceDark, centre.x + radius * 0.4f,
                                   centre.y + radius * 0.9f, true);
        g.setGradientFill (body);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));

        // Rim.
        g.setColour (withAlpha (t.knobEdge, 0.85f));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.2f);

        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillEllipse (juce::Rectangle<float> (radius * 1.5f, radius * 1.5f)
                           .withCentre (centre.translated (-radius * 0.10f, -radius * 0.14f)));

        // Pointer.
        const float pointerLength = radius * 0.78f;
        const float pointerAngle = angle - juce::MathConstants<float>::halfPi;
        const auto tip = centre + juce::Point<float> (std::cos (pointerAngle), std::sin (pointerAngle))
                                    * pointerLength;
        const auto root = centre + juce::Point<float> (std::cos (pointerAngle), std::sin (pointerAngle))
                                     * (radius * 0.18f);

        g.setColour (withAlpha (t.accent, 0.35f));
        g.drawLine ({ root, tip }, 5.0f);
        g.setColour (t.knobPointer);
        g.drawLine ({ root, tip }, 2.2f);

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (tip));
    }

    //==============================================================================
    void EdgyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                            bool highlighted, bool down)
    {
        const auto& t = theme();
        auto bounds = button.getLocalBounds().toFloat();

        g.setColour (t.buttonFace);
        g.fillRoundedRectangle (bounds, 3.0f);

        g.setColour (withAlpha (t.buttonEdge, 0.9f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);

        const bool on = button.getToggleState();
        auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre (bounds.getCentre());

        if (on)
        {
            g.setColour (withAlpha (t.accent, 0.35f));
            g.fillEllipse (led.expanded (5.0f));
        }

        g.setColour (on ? t.accent : t.ledOff);
        g.fillEllipse (led);

        if (highlighted || down)
        {
            g.setColour (juce::Colours::white.withAlpha (down ? 0.12f : 0.06f));
            g.fillRoundedRectangle (bounds, 3.0f);
        }
    }

    //==============================================================================
    void EdgyLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box)
    {
        const auto& t = theme();
        auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);

        g.setColour (t.panelFaceDark);
        g.fillRoundedRectangle (bounds, 3.0f);

        g.setColour (withAlpha (t.accent, box.isMouseOver() ? 0.95f : 0.55f));
        g.drawRoundedRectangle (bounds, 3.0f, 1.2f);

        juce::Path arrow;
        const float cx = bounds.getRight() - 14.0f;
        const float cy = bounds.getCentreY();
        arrow.addTriangle (cx - 5.0f, cy - 2.5f, cx + 5.0f, cy - 2.5f, cx, cy + 3.5f);
        g.setColour (t.accent);
        g.fillPath (arrow);
    }

    void EdgyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool highlighted, bool down)
    {
        const auto& t = theme();
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const bool cyber = (bool) button.getProperties()["cyberRandom"];

        if (cyber)
        {
            // The mutation button gets its own neon treatment: a magenta/cyan
            // gradient, a double rim and a soft glow, so it never reads as a
            // plain utility button.
            const float pulse = 0.55f + 0.25f * std::sin ((float) juce::Time::getMillisecondCounterHiRes()
                                                          * 0.0022f);

            g.setColour (t.accent.withAlpha (0.12f + 0.10f * pulse));
            g.fillRoundedRectangle (bounds.expanded (4.0f), 5.0f);

            juce::ColourGradient gradient (t.accent.withAlpha (0.55f), bounds.getX(),
                                           bounds.getY(),
                                           t.accentAlt.withAlpha (0.55f), bounds.getRight(),
                                           bounds.getBottom(), false);
            g.setGradientFill (gradient);
            g.fillRoundedRectangle (bounds, 3.5f);

            g.setColour (t.accent.withAlpha (0.95f));
            g.drawRoundedRectangle (bounds, 3.5f, 1.4f);
            g.setColour (t.accentAlt.withAlpha (0.55f));
            g.drawRoundedRectangle (bounds.reduced (2.0f), 2.5f, 0.9f);

            if (down || highlighted)
            {
                g.setColour (juce::Colours::white.withAlpha (down ? 0.14f : 0.07f));
                g.fillRoundedRectangle (bounds, 3.5f);
            }

            return;
        }

        auto face = backgroundColour;

        if (button.getToggleState())
            face = withAlpha (t.buttonLit, 0.32f);

        if (down)
            face = face.brighter (0.18f);
        else if (highlighted)
            face = face.brighter (0.08f);

        g.setColour (face);
        g.fillRoundedRectangle (bounds, 3.0f);

        g.setColour (withAlpha (button.getToggleState() ? t.accent : t.buttonEdge, 0.9f));
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

        if (button.getToggleState())
        {
            g.setColour (withAlpha (t.accent, 0.30f));
            g.drawRoundedRectangle (bounds.expanded (2.0f), 4.0f, 2.0f);
        }
    }

    void EdgyLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                          bool, bool)
    {
        const auto& t = theme();
        const bool cyber = (bool) button.getProperties()["cyberRandom"];

        if (cyber)
        {
            auto bounds = button.getLocalBounds().toFloat();

            // Lightning bolt, then the label.
            const float cx = bounds.getX() + 16.0f;
            const float cy = bounds.getCentreY();

            juce::Path bolt;
            bolt.startNewSubPath (cx + 3.0f, cy - 9.0f);
            bolt.lineTo (cx - 3.0f, cy + 1.0f);
            bolt.lineTo (cx + 0.5f, cy + 1.0f);
            bolt.lineTo (cx - 2.0f, cy + 9.0f);
            bolt.lineTo (cx + 5.0f, cy - 2.0f);
            bolt.lineTo (cx + 1.0f, cy - 2.0f);
            bolt.closeSubPath();

            g.setColour (juce::Colours::white.withAlpha (0.92f));
            g.fillPath (bolt);

            g.setColour (juce::Colours::black.withAlpha (0.88f));
            g.setFont (uiFont (10.5f, juce::Font::bold));
            g.drawFittedText (button.getButtonText(),
                              button.getLocalBounds().withTrimmedLeft (28).reduced (2, 0),
                              juce::Justification::centred, 1, 0.9f);
            return;
        }

        g.setColour (withAlpha (t.silkscreen, button.isEnabled() ? 0.95f : 0.35f));
        g.setFont (uiFont (11.0f, juce::Font::bold));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (4, 0),
                          juce::Justification::centred, 1, 0.9f);
    }

    //==============================================================================
    void EdgyLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
    {
        const auto& t = theme();
        g.fillAll (t.panelFaceDark);
        g.setColour (withAlpha (t.accent, 0.6f));
        g.drawRect (0, 0, width, height, 1);
    }

    void EdgyLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                             bool isSeparator, bool isActive, bool isHighlighted,
                                             bool isTicked, bool, const juce::String& text,
                                             const juce::String&, const juce::Drawable*,
                                             const juce::Colour*)
    {
        const auto& t = theme();

        if (isSeparator)
        {
            g.setColour (withAlpha (t.silkscreen, 0.20f));
            g.drawHorizontalLine (area.getCentreY(), (float) area.getX() + 6.0f,
                                  (float) area.getRight() - 6.0f);
            return;
        }

        if (isHighlighted)
        {
            g.setColour (withAlpha (t.accent, 0.22f));
            g.fillRect (area);
        }

        g.setColour (withAlpha (t.silkscreen, isActive ? 0.95f : 0.4f));
        g.setFont (uiFont (12.0f, isTicked ? juce::Font::bold : juce::Font::plain));
        g.drawFittedText (text, area.reduced (10, 0), juce::Justification::centredLeft, 1, 0.9f);

        if (isTicked)
        {
            g.setColour (t.accent);
            g.fillEllipse ((float) area.getRight() - 14.0f, (float) area.getCentreY() - 3.0f, 6.0f, 6.0f);
        }
    }

    juce::Font EdgyLookAndFeel::getComboBoxFont (juce::ComboBox&)
    {
        return uiFont (12.0f, juce::Font::bold);
    }

    juce::Font EdgyLookAndFeel::getPopupMenuFont()
    {
        return uiFont (12.0f, juce::Font::plain);
    }

    juce::Font EdgyLookAndFeel::getLabelFont (juce::Label&)
    {
        return uiFont (11.0f, juce::Font::bold);
    }

    //==============================================================================
    void EdgyLookAndFeel::drawJack (juce::Graphics& g, juce::Point<float> centre, float radius,
                                    bool isInput, bool connected, float activity)
    {
        const auto& t = theme();
        const auto outer = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

        // Shadow.
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (outer.translated (0.0f, 2.0f));

        // Socket body.
        g.setColour (t.jackBody);
        g.fillEllipse (outer);

        // Ring: input jacks are white-ringed, outputs black-ringed on the Edge.
        g.setColour (withAlpha (isInput ? t.jackRing : t.knobEdge, 0.9f));
        g.drawEllipse (outer.reduced (0.8f), 1.6f);

        // Inner hole.
        g.setColour (t.jackHole);
        g.fillEllipse (outer.reduced (radius * 0.46f));

        g.setColour (withAlpha (t.silkscreen, 0.20f));
        g.fillEllipse (outer.reduced (radius * 0.62f).translated (-0.8f, -0.8f));

        // Connection glow.
        if (connected)
        {
            g.setColour (withAlpha (t.accentAlt, 0.28f + 0.25f * activity));
            g.fillEllipse (outer.expanded (radius * 0.45f));
            g.setColour (withAlpha (t.accentAlt, 0.85f));
            g.drawEllipse (outer.expanded (radius * 0.30f), 1.4f);
        }
    }

    void EdgyLookAndFeel::drawPanelscrew (juce::Graphics& g, juce::Point<float> centre, float radius)
    {
        const auto& t = theme();
        const auto rect = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (rect.translated (0.0f, 1.0f));

        juce::ColourGradient gradient (t.knobEdge.brighter (0.25f), centre.x - radius,
                                       centre.y - radius, t.knobFaceDark, centre.x + radius,
                                       centre.y + radius, true);
        g.setGradientFill (gradient);
        g.fillEllipse (rect);

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawLine (centre.x - radius * 0.55f, centre.y, centre.x + radius * 0.55f, centre.y, 1.4f);
        g.drawLine (centre.x, centre.y - radius * 0.55f, centre.x, centre.y + radius * 0.55f, 1.4f);
    }
}
