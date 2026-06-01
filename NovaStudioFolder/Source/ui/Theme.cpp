#include "Theme.h"

using namespace NovaStudioUI;

juce::Colour Theme::background()
{
    return juce::Colour::fromRGBA(12, 14, 20, 255);
}

juce::Colour Theme::panelBackground()
{
    return juce::Colour::fromRGBA(20, 22, 30, 255);
}

juce::Colour Theme::accent()
{
    return juce::Colour::fromRGB(150, 80, 255);
}

juce::Colour Theme::glowAccent()
{
    return juce::Colour::fromRGBA(110, 60, 220, 160);
}

juce::Colour Theme::panelEdge()
{
    return juce::Colour::fromRGBA(255, 255, 255, 12);
}

juce::Font Theme::headingFont(float size)
{
    return juce::Font(size, juce::Font::bold);
}
