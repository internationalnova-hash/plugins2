#include "BeatWindow.h"

using namespace NovaStudioUI;

BeatWindow::BeatWindow()
{
    addAndMakeVisible(leftBrowser);
    addAndMakeVisible(topPlaylist);
    addAndMakeVisible(bottomPiano);
    addAndMakeVisible(rightInspector);

    auto f = juce::File("/workspaces/plugins2/Nova Studio/Source/ui/Assets/beat-window.png");
    if (f.existsAsFile())
        beatMockup = juce::ImageFileFormat::loadFrom(f);
}

BeatWindow::~BeatWindow() = default;

void BeatWindow::paint(juce::Graphics& g)
{
    g.fillAll(Theme::background());
    g.setColour(Theme::panelBackground());
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(6.0f), 12.0f);

    if (!beatMockup.isNull())
    {
        auto area = getLocalBounds().toFloat().reduced(12.0f);
        g.drawImageWithin(beatMockup, (int)area.getX(), (int)area.getY(), (int)area.getWidth(), (int)area.getHeight(), juce::RectanglePlacement::stretchToFit);
    }
}

void BeatWindow::resized() {}
