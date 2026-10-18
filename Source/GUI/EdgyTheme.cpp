#include "EdgyTheme.h"

namespace ReplicantEdgy
{
    //==============================================================================
    // Skin 1: Neon Magenta. The Edge's own magenta panel pushed into a
        // Blade-Runner back alley: near-black anodised chassis, magenta edge light
        // and cyan instrumentation.
    //==============================================================================
    static Theme makeNeonMagenta()
    {
        Theme t;
        t.name = "Neon Magenta";
        t.description = "Black anodised chassis, magenta edge light, cyan instrumentation";

        t.chassisTop    = juce::Colour (0xff1a1220);
        t.chassisBottom = juce::Colour (0xff0a060d);
        t.panelFace     = juce::Colour (0xff141018);
        t.panelFaceDark = juce::Colour (0xff0c0a10);
        t.panelEdge     = juce::Colour (0xff2b1f33);
        t.silkscreen    = juce::Colour (0xffe8dff2);
        t.silkscreenDim = juce::Colour (0xff9b8aa8);

        t.accent      = juce::Colour (0xffff2f8e);
        t.accentAlt   = juce::Colour (0xff2fe6ff);
        t.warning     = juce::Colour (0xffffd23f);
        t.led         = juce::Colour (0xffff3b7f);
        t.ledOff      = juce::Colour (0xff3a1226);

        t.jackRing    = juce::Colour (0xffd8c9e6);
        t.jackBody    = juce::Colour (0xff2a2430);
        t.jackHole    = juce::Colour (0xff050407);

        t.knobFace     = juce::Colour (0xff2b2633);
        t.knobFaceDark = juce::Colour (0xff141019);
        t.knobEdge     = juce::Colour (0xff6b5c7a);
        t.knobPointer  = juce::Colour (0xffff2f8e);

        t.buttonFace = juce::Colour (0xff231c2b);
        t.buttonLit  = juce::Colour (0xffff2f8e);
        t.buttonEdge = juce::Colour (0xff4a3556);

        t.pcb        = juce::Colour (0xff100b16);
        t.pcbTrace   = juce::Colour (0xff7a1b4d);
        t.scopeBackground = juce::Colour (0xff07050a);

        t.blackKey = juce::Colour (0xff100c14);
        t.whiteKey = juce::Colour (0xffd9d2e4);

        t.cableColours = {
            juce::Colour (0xffff2f8e), juce::Colour (0xff2fe6ff),
            juce::Colour (0xffffd23f), juce::Colour (0xff7cff6b),
            juce::Colour (0xffb06bff), juce::Colour (0xffff7a2f),
            juce::Colour (0xff2f7bff), juce::Colour (0xffff5ad1)
        };

        return t;
    }

    //==============================================================================
    // Skin 2: Toxic Lime. Acid green and hot magenta on a black chassis.
    //==============================================================================
    static Theme makeToxicLime()
    {
        Theme t;
        t.name = "Toxic Lime";
        t.description = "Black chassis, acid green silkscreen, magenta wire";

        t.chassisTop    = juce::Colour (0xff0f1a10);
        t.chassisBottom = juce::Colour (0xff040a05);
        t.panelFace     = juce::Colour (0xff0d140e);
        t.panelFaceDark = juce::Colour (0xff060b07);
        t.panelEdge     = juce::Colour (0xff1d2f20);
        t.silkscreen    = juce::Colour (0xffd9ffd0);
        t.silkscreenDim = juce::Colour (0xff7fa87a);

        t.accent    = juce::Colour (0xffb6ff2f);
        t.accentAlt = juce::Colour (0xffff2fb0);
        t.warning   = juce::Colour (0xffffc83f);
        t.led       = juce::Colour (0xffb6ff2f);
        t.ledOff    = juce::Colour (0xff1c3312);

        t.jackRing = juce::Colour (0xffcfe6c8);
        t.jackBody = juce::Colour (0xff1b2a1d);
        t.jackHole = juce::Colour (0xff040805);

        t.knobFace     = juce::Colour (0xff1e2b20);
        t.knobFaceDark = juce::Colour (0xff0b120c);
        t.knobEdge     = juce::Colour (0xff4d6b50);
        t.knobPointer  = juce::Colour (0xffb6ff2f);

        t.buttonFace = juce::Colour (0xff16221a);
        t.buttonLit  = juce::Colour (0xffb6ff2f);
        t.buttonEdge = juce::Colour (0xff345239);

        t.pcb        = juce::Colour (0xff08120a);
        t.pcbTrace   = juce::Colour (0xff2f7a1b);
        t.scopeBackground = juce::Colour (0xff040805);

        t.blackKey = juce::Colour (0xff0a120b);
        t.whiteKey = juce::Colour (0xffd2e6cc);

        t.cableColours = {
            juce::Colour (0xffb6ff2f), juce::Colour (0xffff2fb0),
            juce::Colour (0xffffc83f), juce::Colour (0xff2fe6ff),
            juce::Colour (0xffff6b2f), juce::Colour (0xff7c6bff),
            juce::Colour (0xff2fff9e), juce::Colour (0xffff5a5a)
        };

        return t;
    }

    //==============================================================================
    // Skin 3: Chrome Blue. Steel chassis with electric blue and hot pink.
    //==============================================================================
    static Theme makeChromeBlue()
    {
        Theme t;
        t.name = "Chrome Blue";
        t.description = "Brushed steel chassis, electric blue, hot pink";

        t.chassisTop    = juce::Colour (0xff161b24);
        t.chassisBottom = juce::Colour (0xff070a0f);
        t.panelFace     = juce::Colour (0xff121821);
        t.panelFaceDark = juce::Colour (0xff090d13);
        t.panelEdge     = juce::Colour (0xff26303d);
        t.silkscreen    = juce::Colour (0xffe3edf7);
        t.silkscreenDim = juce::Colour (0xff8a9bad);

        t.accent    = juce::Colour (0xff2f9bff);
        t.accentAlt = juce::Colour (0xffff2f8e);
        t.warning   = juce::Colour (0xffffd23f);
        t.led       = juce::Colour (0xff4fc3ff);
        t.ledOff    = juce::Colour (0xff122536);

        t.jackRing = juce::Colour (0xffd5e2ee);
        t.jackBody = juce::Colour (0xff222c38);
        t.jackHole = juce::Colour (0xff05080b);

        t.knobFace     = juce::Colour (0xff232d39);
        t.knobFaceDark = juce::Colour (0xff0d1218);
        t.knobEdge     = juce::Colour (0xff5d6f80);
        t.knobPointer  = juce::Colour (0xff4fc3ff);

        t.buttonFace = juce::Colour (0xff1b2430);
        t.buttonLit  = juce::Colour (0xff4fc3ff);
        t.buttonEdge = juce::Colour (0xff3a4a5c);

        t.pcb        = juce::Colour (0xff0a1018);
        t.pcbTrace   = juce::Colour (0xff1b4a7a);
        t.scopeBackground = juce::Colour (0xff05080b);

        t.blackKey = juce::Colour (0xff0b1016);
        t.whiteKey = juce::Colour (0xffdbe6f0);

        t.cableColours = {
            juce::Colour (0xff4fc3ff), juce::Colour (0xffff2f8e),
            juce::Colour (0xffffd23f), juce::Colour (0xff6bffb0),
            juce::Colour (0xffb06bff), juce::Colour (0xffff7a2f),
            juce::Colour (0xffe8f4ff), juce::Colour (0xffff5a5a)
        };

        return t;
    }

    //==============================================================================
    ThemeManager& ThemeManager::getInstance()
    {
        static ThemeManager instance;
        return instance;
    }

    ThemeManager::ThemeManager()
    {
        themes.push_back (makeNeonMagenta());
        themes.push_back (makeToxicLime());
        themes.push_back (makeChromeBlue());
    }

    int ThemeManager::getNumThemes()
    {
        return 3;
    }

    juce::String ThemeManager::getThemeName (int index)
    {
        const auto& themes = getInstance().themes;
        return themes[(size_t) juce::jlimit (0, (int) themes.size() - 1, index)].name;
    }

    juce::StringArray ThemeManager::getThemeNames()
    {
        juce::StringArray names;

        for (const auto& theme : getInstance().themes)
            names.add (theme.name);

        return names;
    }

    void ThemeManager::setCurrentIndex (int index)
    {
        const int clamped = juce::jlimit (0, (int) themes.size() - 1, index);

        if (clamped == currentIndex)
            return;

        currentIndex = clamped;
        sendChangeMessage();
    }

    float ThemeManager::indexToNormalized (int index)
    {
        const int numThemes = (int) getInstance().themes.size();

        if (numThemes <= 1)
            return 0.0f;

        return (float) juce::jlimit (0, numThemes - 1, index) / (float) (numThemes - 1);
    }

    int ThemeManager::normalizedToIndex (float normalized)
    {
        const int numThemes = (int) getInstance().themes.size();
        return juce::jlimit (0, numThemes - 1,
                             juce::roundToInt (normalized * (float) (numThemes - 1)));
    }
}
