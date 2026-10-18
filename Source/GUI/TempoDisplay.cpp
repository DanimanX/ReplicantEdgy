#include "TempoDisplay.h"
#include "EdgyTheme.h"

namespace ReplicantEdgy
{
    namespace
    {
        // Seven-segment bit masks, bit 0 = A (top) ... bit 6 = G (middle).
        constexpr juce::uint8 kSegments[10] =
        {
            0x3f, // 0
            0x06, // 1
            0x5b, // 2
            0x4f, // 3
            0x66, // 4
            0x6d, // 5
            0x7d, // 6
            0x07, // 7
            0x7f, // 8
            0x6f  // 9
        };
    }

    //==============================================================================
    TempoDisplay::TempoDisplay (juce::RangedAudioParameter& tempoParameter)
        : parameter (tempoParameter),
          attachment (tempoParameter,
                      [this] (float plainValue)
                      {
                          value = plainValue;

                          if (! editing)
                              repaint();
                      })
    {
        setOpaque (false);
        setMouseCursor (juce::MouseCursor::IBeamCursor);
        setWantsKeyboardFocus (false);
        setTitle ("Tempo display");

        editor.setVisible (false);
        editor.setJustification (juce::Justification::centred);
        editor.setFont (uiFont (17.0f, juce::Font::bold));
        editor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff12070a));
        editor.setColour (juce::TextEditor::textColourId, juce::Colour (0xffff5a5a));
        editor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xffff2f3a));
        editor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xffff8a8a));
        editor.setColour (juce::TextEditor::highlightColourId, juce::Colour (0xffff2f3a).withAlpha (0.4f));
        editor.setInputRestrictions (5, "0123456789.");
        editor.onReturnKey = [this] { commitEdit(); };
        editor.onEscapeKey = [this] { cancelEdit(); };
        editor.onFocusLost = [this] { if (editing) commitEdit(); };
        addChildComponent (editor);

        attachment.sendInitialUpdate();
    }

    TempoDisplay::~TempoDisplay()
    {
        editor.onFocusLost = nullptr;
    }

    //==============================================================================
    void TempoDisplay::resized()
    {
        editor.setBounds (getLocalBounds().reduced (8, 12));
    }

    void TempoDisplay::refresh()
    {
        attachment.sendInitialUpdate();
        repaint();
    }

    void TempoDisplay::setValue (float bpm)
    {
        const auto& range = parameter.getNormalisableRange();
        const float clamped = juce::jlimit (range.start, range.end, bpm);

        // A typed tempo is a discrete user action, so it travels as a complete
        // gesture: hosts then record it in automation like a knob move.
        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (parameter.convertTo0to1 (clamped));
        parameter.endChangeGesture();

        value = clamped;
        repaint();
    }

    //==============================================================================
    void TempoDisplay::mouseDown (const juce::MouseEvent& event)
    {
        if (event.mods.isPopupMenu())
        {
            // Right-click resets to the panel's default tempo.
            setValue (120.0f);
            return;
        }

        beginEdit();
    }

    void TempoDisplay::mouseWheelMove (const juce::MouseEvent&,
                                       const juce::MouseWheelDetails& wheel)
    {
        // Trackpad momentum sends neutral and horizontal events; those must not
        // decrement the tempo, and "natural" scrolling must still feel natural.
        if (std::abs (wheel.deltaY) < 1.0e-5f)
            return;

        float step = wheel.deltaY > 0.0f ? 0.5f : -0.5f;

        if (wheel.isReversed)
            step = -step;

        setValue (value + step);
    }

    void TempoDisplay::beginEdit()
    {
        if (editing)
            return;

        editing = true;

        auto text = juce::String (value, value == std::floor (value) ? 0 : 1);
        editor.setText (text, juce::dontSendNotification);
        editor.setVisible (true);
        editor.toFront (false);
        editor.grabKeyboardFocus();
        editor.selectAll();
        repaint();
    }

    void TempoDisplay::commitEdit()
    {
        if (! editing)
            return;

        editing = false;
        editor.setVisible (false);

        const auto text = editor.getText().trim();

        if (text.isNotEmpty())
        {
            const auto parsed = text.getFloatValue();

            if (parsed > 0.0f)
                setValue (parsed);
        }

        repaint();
    }

    void TempoDisplay::cancelEdit()
    {
        editing = false;
        editor.setVisible (false);
        repaint();
    }

    //==============================================================================
    void TempoDisplay::paint (juce::Graphics& g)
    {
        const auto& t = ThemeManager::getInstance().getCurrentTheme();
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);

        const juce::Colour red = juce::Colour (0xffff2f3a);
        const juce::Colour redHot = juce::Colour (0xffff8a8a);
        const juce::Colour redOff = juce::Colour (0xff2a0508);

        // Recessed black chamber with a soft red rim.
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 4.0f);

        juce::ColourGradient chamber (juce::Colour (0xff12070a), bounds.getX(), bounds.getY(),
                                      juce::Colour (0xff050203), bounds.getX(),
                                      bounds.getBottom(), false);
        g.setGradientFill (chamber);
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setColour (red.withAlpha (0.35f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.2f);

        auto inner = bounds.reduced (5.0f, 4.0f);

        // The grid behind the digits: a small oscilloscope-style graticule.
        g.setColour (red.withAlpha (0.10f));

        for (int i = 1; i < 6; ++i)
            g.drawVerticalLine ((int) (inner.getX() + inner.getWidth() * i / 6.0f),
                                inner.getY(), inner.getBottom());

        for (int i = 1; i < 3; ++i)
            g.drawHorizontalLine ((int) (inner.getY() + inner.getHeight() * i / 3.0f),
                                  inner.getX(), inner.getRight());

        // Seven-segment value.
        const bool whole = std::abs (value - std::round (value)) < 0.01f;
        const auto text = whole ? juce::String (juce::roundToInt (value))
                                : juce::String (value, 1);

        const int numGlyphs = (int) text.length();
        const float glyphGap = 2.5f;
        const float cellWidth = juce::jmin (20.0f,
                                            (inner.getWidth() - glyphGap * (numGlyphs - 1))
                                                / juce::jmax (1, numGlyphs));
        const float cellHeight = inner.getHeight() - 12.0f;
        const float totalWidth = numGlyphs * cellWidth + (numGlyphs - 1) * glyphGap;
        float x = inner.getCentreX() - totalWidth * 0.5f;

        for (int i = 0; i < numGlyphs; ++i)
        {
            const auto glyph = text[i];
            const juce::Rectangle<float> cell (x, inner.getY() + 1.0f, cellWidth, cellHeight);

            if (glyph == '.')
            {
                g.setColour (red);
                g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f)
                                   .withCentre ({ cell.getX() + cell.getWidth() * 0.5f,
                                                  cell.getBottom() }));
            }
            else
            {
                drawDigit (g, cell, glyph - '0', red, redOff);
            }

            x += cellWidth + glyphGap;
        }

        // BPM caption.
        g.setColour (red.withAlpha (0.85f));
        g.setFont (uiFont (7.0f, juce::Font::bold));
        g.drawText ("BPM", inner.getX(), inner.getBottom() - 11.0f, inner.getWidth(), 10.0f,
                    juce::Justification::centredRight);

        juce::ignoreUnused (redHot, t);
    }

    void TempoDisplay::drawDigit (juce::Graphics& g, juce::Rectangle<float> cell, int digit,
                                  juce::Colour lit, juce::Colour unlit)
    {
        if (digit < 0 || digit > 9)
            return;

        const auto mask = kSegments[digit];
        const float w = cell.getWidth();
        const float h = cell.getHeight();
        const float thickness = juce::jmax (2.0f, juce::jmin (w, h) * 0.19f);
        const float radius = thickness * 0.42f;

        const float left = cell.getX();
        const float right = cell.getRight();
        const float top = cell.getY();
        const float bottom = cell.getBottom();
        const float middle = cell.getCentreY();

        // Horizontal bars sit between the vertical ones; vertical bars sit
        // between the horizontal ones, which is what keeps the glyphs legible
        // instead of melting into an 8.
        const float hInset = thickness * 0.55f;
        const float vInset = thickness * 0.60f;
        const float half = thickness * 0.5f;

        struct Segment
        {
            juce::Rectangle<float> rect;
            int bit;
        };

        const Segment segments[] =
        {
            { { left + hInset, top,               w - hInset * 2.0f, thickness }, 0 }, // A
            { { right - thickness, top + vInset,  thickness, h * 0.5f - vInset }, 1 }, // B
            { { right - thickness, middle + half * 0.2f, thickness, h * 0.5f - vInset }, 2 }, // C
            { { left + hInset, bottom - thickness, w - hInset * 2.0f, thickness }, 3 }, // D
            { { left, middle + half * 0.2f,        thickness, h * 0.5f - vInset }, 4 }, // E
            { { left, top + vInset,                thickness, h * 0.5f - vInset }, 5 }, // F
            { { left + hInset, middle - half,      w - hInset * 2.0f, thickness }, 6 }  // G
        };

        for (const auto& segment : segments)
        {
            const bool active = (mask & (1u << segment.bit)) != 0;

            if (active)
            {
                // Soft halo first, then the bar itself.
                g.setColour (lit.withAlpha (0.16f));
                g.fillRoundedRectangle (segment.rect.expanded (1.6f), radius * 1.6f);
                g.setColour (lit);
                g.fillRoundedRectangle (segment.rect, radius);
            }
            else
            {
                g.setColour (unlit);
                g.fillRoundedRectangle (segment.rect, radius);
            }
        }
    }

}
