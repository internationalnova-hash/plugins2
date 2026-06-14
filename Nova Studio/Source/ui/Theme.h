#pragma once

#include <JuceHeader.h>

namespace NovaStudioUI
{
    struct Theme
    {
        static juce::Colour background();
        static juce::Colour panelBackground();
        static juce::Colour accent();
        static juce::Colour glowAccent();
        static juce::Colour panelEdge();
        static juce::Font  headingFont(float size = 16.0f);
    };
}
