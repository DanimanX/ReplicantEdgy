#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ReplicantEdgy
{
    //==============================================================================
    /** Cyberpunk palette for one skin.

        The panel is still drawn like hardware - anodised chassis, PCB underlay,
        silkscreen, real jack sockets - but every skin is pushed through a neon
        cyberpunk filter: magenta/cyan edge light, scanlines, glow and
        high-contrast silkscreen.
    */
    struct Theme
    {
        juce::String name;
        juce::String description;

        juce::Colour chassisTop;
        juce::Colour chassisBottom;
        juce::Colour panelFace;
        juce::Colour panelFaceDark;
        juce::Colour panelEdge;
        juce::Colour silkscreen;
        juce::Colour silkscreenDim;

        juce::Colour accent;        // primary neon (magenta)
        juce::Colour accentAlt;     // secondary neon (cyan)
        juce::Colour warning;       // yellow / amber
        juce::Colour led;
        juce::Colour ledOff;

        juce::Colour jackRing;
        juce::Colour jackBody;
        juce::Colour jackHole;

        juce::Colour knobFace;
        juce::Colour knobFaceDark;
        juce::Colour knobEdge;
        juce::Colour knobPointer;

        juce::Colour buttonFace;
        juce::Colour buttonLit;
        juce::Colour buttonEdge;

        juce::Colour pcb;
        juce::Colour pcbTrace;
        juce::Colour scopeBackground;

        juce::Colour blackKey;
        juce::Colour whiteKey;

        /** Eight cable colours, used round-robin as the user patches. */
        std::array<juce::Colour, 8> cableColours;

        juce::Colour cableColour (int index) const noexcept
        {
            return cableColours[(size_t) (index & 7)];
        }
    };

    //==============================================================================
    class ThemeManager : public juce::ChangeBroadcaster
    {
    public:
        static ThemeManager& getInstance();

        static int getNumThemes();
        static juce::String getThemeName (int index);
        static juce::StringArray getThemeNames();

        const Theme& getCurrentTheme() const noexcept { return themes[(size_t) currentIndex]; }
        int getCurrentIndex() const noexcept { return currentIndex; }

        void setCurrentIndex (int index);

        static float indexToNormalized (int index);
        static int normalizedToIndex (float normalized);

    private:
        ThemeManager();

        std::vector<Theme> themes;
        int currentIndex = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThemeManager)
    };

    //==============================================================================
    /** Global UI type scale, same idea as Replicant303's uiFont helper. */
    inline constexpr float uiFontScale = 1.0f;

    inline juce::FontOptions uiFont (float baseSize, int style = juce::Font::plain)
    {
        return juce::FontOptions (baseSize * uiFontScale, style);
    }
}
