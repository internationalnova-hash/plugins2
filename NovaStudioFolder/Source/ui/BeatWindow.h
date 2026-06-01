#pragma once

#include <JuceHeader.h>
#include "Theme.h"

namespace NovaStudioUI
{
    class BeatWindow : public juce::Component
    {
    public:
        BeatWindow();
        ~BeatWindow() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        juce::Component leftBrowser, topPlaylist, bottomPiano, rightInspector;
        juce::Image beatMockup;
    };
}
