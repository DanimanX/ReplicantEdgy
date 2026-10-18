#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace ReplicantEdgy
{
    //==============================================================================
    /** Red LED tempo readout, styled after an 80s drum-machine display.

        It shows the live BPM on a grid behind seven-segment digits. Clicking
        opens an inline editor so a tempo can be typed in directly, the mouse
        wheel nudges it by half a BPM, and the value always round-trips through
        the real `tempo` parameter.
    */
    class TempoDisplay : public juce::Component
    {
    public:
        explicit TempoDisplay (juce::RangedAudioParameter& tempoParameter);
        ~TempoDisplay() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void mouseDown (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&,
                             const juce::MouseWheelDetails&) override;

        /** Pulls the parameter value and repaints. */
        void refresh();

        /** Current value shown by the display (BPM). */
        float getValue() const noexcept { return value; }

        /** Applies a tempo as if it had been typed. Used by tests. */
        void setValue (float bpm);

    private:
        void beginEdit();
        void commitEdit();
        void cancelEdit();

        static void drawDigit (juce::Graphics&, juce::Rectangle<float>, int digit,
                               juce::Colour lit, juce::Colour unlit);

        juce::RangedAudioParameter& parameter;
        juce::ParameterAttachment attachment;
        float value = 120.0f;

        juce::TextEditor editor;
        bool editing = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempoDisplay)
    };
}
