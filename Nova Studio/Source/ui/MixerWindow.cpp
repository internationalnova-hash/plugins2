#include "MixerWindow.h"

using namespace NovaStudioUI;

MixerWindow::MixerWindow()
{
    addAndMakeVisible(leftStrip);
    addAndMakeVisible(centerStrips);
    addAndMakeVisible(rightMaster);

    auto f = juce::File("/workspaces/plugins2/Nova Studio/Source/ui/Assets/mixer-window.png");
    if (f.existsAsFile())
        mixerMockup = juce::ImageFileFormat::loadFrom(f);
}

MixerWindow::~MixerWindow() = default;

void MixerWindow::paint(juce::Graphics& g)
{
    g.fillAll(Theme::background());
    g.setColour(Theme::panelBackground());
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(6.0f), 12.0f);

    if (!mixerMockup.isNull())
    {
        auto area = getLocalBounds().toFloat().reduced(12.0f);
        g.drawImageWithin(mixerMockup, (int)area.getX(), (int)area.getY(), (int)area.getWidth(), (int)area.getHeight(), juce::RectanglePlacement::stretchToFit);
    }
}

void MixerWindow::resized() {}
