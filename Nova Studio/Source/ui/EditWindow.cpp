#include "EditWindow.h"

using namespace NovaStudioUI;

EditWindow::EditWindow(NovaStudio::TransportState& transport,
                       NovaStudio::TimelineModel& timeline,
                       NovaStudio::ArrangementModel& arrangement)
    : transportState(transport), timelineModel(timeline), arrangementModel(arrangement)
{
    addAndMakeVisible(leftPanel);
    addAndMakeVisible(centerPanel);
    addAndMakeVisible(bottomPanel);
    addAndMakeVisible(rightPanel);
    addAndMakeVisible(topPanel);

    transportState.addChangeListener(this);
    arrangementModel.addChangeListener(this);

    transportBar = std::make_unique<TransportBar>();
    topPanel.addAndMakeVisible(*transportBar);

    trackPanel = std::make_unique<TrackPanel>(arrangement.getSession());
    leftPanel.addAndMakeVisible(*trackPanel);

    inspectorPanel = std::make_unique<InspectorPanel>(arrangementModel);
    rightPanel.addAndMakeVisible(*inspectorPanel);

    arrangementView = std::make_unique<ArrangementView>(transportState, timelineModel, arrangementModel);
    centerPanel.addAndMakeVisible(*arrangementView);

    // load edit window mockup image if present (kept for reference/backdrop)
    auto f = juce::File("/workspaces/plugins2/Nova Studio/Source/ui/Assets/edit-window.png");
    if (f.existsAsFile())
        editMockup = juce::ImageFileFormat::loadFrom(f);
}

EditWindow::~EditWindow()
{
    transportState.removeChangeListener(this);
    arrangementModel.removeChangeListener(this);
}

void EditWindow::paint(juce::Graphics& g)
{
    g.fillAll(Theme::background());
    
    // Paint panel backgrounds with theme colors
    auto topBounds = topPanel.getBounds().toFloat();
    g.setColour(Theme::panelBackground());
    g.fillRoundedRectangle(topBounds, 12.0f);
    g.setColour(Theme::glowAccent().withAlpha(0.1f));
    g.drawRoundedRectangle(topBounds, 12.0f, 2.0f);

    auto leftBounds = leftPanel.getBounds().toFloat();
    g.setColour(Theme::panelBackground().withAlpha(0.95f));
    g.fillRoundedRectangle(leftBounds, 12.0f);
    g.setColour(Theme::glowAccent().withAlpha(0.08f));
    g.drawRoundedRectangle(leftBounds, 12.0f, 1.5f);

    auto centerBounds = centerPanel.getBounds().toFloat();
    g.setColour(juce::Colour::fromRGB(16, 18, 26));
    g.fillRoundedRectangle(centerBounds, 12.0f);
    g.setColour(Theme::accent().withAlpha(0.12f));
    g.drawRoundedRectangle(centerBounds, 12.0f, 2.0f);

    auto rightBounds = rightPanel.getBounds().toFloat();
    g.setColour(Theme::panelBackground());
    g.fillRoundedRectangle(rightBounds, 12.0f);
    g.setColour(Theme::glowAccent().withAlpha(0.08f));
    g.drawRoundedRectangle(rightBounds, 12.0f, 1.5f);

    auto bottomBounds = bottomPanel.getBounds().toFloat();
    g.setColour(juce::Colour::fromRGB(18, 20, 28));
    g.fillRoundedRectangle(bottomBounds, 12.0f);
    g.setColour(Theme::accent().withAlpha(0.1f));
    g.drawRoundedRectangle(bottomBounds, 12.0f, 1.5f);
}

void EditWindow::resized()
{
    auto r = getLocalBounds().reduced(12);
    auto top = r.removeFromTop(72);
    topPanel.setBounds(top);
    topPanel.setOpaque(true);

    auto bottom = r.removeFromBottom(160);
    bottomPanel.setBounds(bottom);
    bottomPanel.setOpaque(true);

    auto right = r.removeFromRight(320);
    rightPanel.setBounds(right);
    rightPanel.setOpaque(true);

    auto left = r.removeFromLeft(280);
    leftPanel.setBounds(left);
    leftPanel.setOpaque(true);

    centerPanel.setBounds(r);

    if (transportBar)
        transportBar->setBounds(topPanel.getLocalBounds().reduced(8));
    if (trackPanel)
        trackPanel->setBounds(leftPanel.getLocalBounds().reduced(8));
    if (inspectorPanel)
        inspectorPanel->setBounds(rightPanel.getLocalBounds().reduced(8));
    if (arrangementView)
        arrangementView->setBounds(centerPanel.getLocalBounds().reduced(8));
}

void EditWindow::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused(source);
    if (trackPanel)
        trackPanel->repaint();
    if (inspectorPanel)
        inspectorPanel->repaint();
    repaint();
}
