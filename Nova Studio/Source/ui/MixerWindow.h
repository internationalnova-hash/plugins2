#pragma once

#include <JuceHeader.h>
#include "Theme.h"

namespace NovaStudioUI
{
    class MixerWindow : public juce::Component
    {
    public:
        MixerWindow();
        ~MixerWindow() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        juce::Component leftStrip, centerStrips, rightMaster;
        juce::Image mixerMockup;
    };
}
