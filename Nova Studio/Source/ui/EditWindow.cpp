#include "EditWindow.h"

using namespace NovaStudioUI;

EditWindow::EditWindow(NovaStudio::TransportState& transport,
                       NovaStudio::TimelineModel& timeline,
                       NovaStudio::ArrangementModel& arrangement)
    : transportState(transport), timelineModel(timeline), arrangementModel(arrangement)
{
    addAndMakeVisible(leftPanel);
    addAndMakeVisible(centerPanel);
    addAndMakeVisible(rightPanel);

    transportState.addChangeListener(this);
    arrangementModel.addChangeListener(this);

    trackPanel = std::make_unique<TrackPanel>(arrangement.getSession());
    leftPanel.addAndMakeVisible(*trackPanel);

    inspectorPanel = std::make_unique<InspectorPanel>(arrangementModel);
    rightPanel.addAndMakeVisible(*inspectorPanel);

    editModeToolbar = std::make_unique<EditModeToolbar>();
    centerPanel.addAndMakeVisible(*editModeToolbar);

    arrangementView = std::make_unique<ArrangementView>(transportState, timelineModel, arrangementModel);
    centerPanel.addAndMakeVisible(*arrangementView);

    // Wire toolbar callbacks to arrangement view
    editModeToolbar->onEditModeChanged = [this](EditModeToolbar::EditMode m) {
        if (arrangementView) arrangementView->setEditMode(m);
    };
    editModeToolbar->onSnapResolutionChanged = [this](double beats) {
        if (arrangementView) arrangementView->setSnapResolution(beats);
    };
    editModeToolbar->onSnapEnabled = [this](bool on) {
        if (arrangementView) arrangementView->setSnapEnabled(on);
    };
    editModeToolbar->onZoomChanged = [this](int dir) {
        if (arrangementView) arrangementView->adjustZoom(dir);
    };
}

EditWindow::~EditWindow()
{
    transportState.removeChangeListener(this);
    arrangementModel.removeChangeListener(this);
}

void EditWindow::setTrackCallbacks(std::function<void(int,bool)> onArm,
                                   std::function<void(int,bool)> onMute,
                                   std::function<void(int,bool)> onSolo)
{
    if (trackPanel)
    {
        trackPanel->onTrackArm  = std::move(onArm);
        trackPanel->onTrackMute = std::move(onMute);
        trackPanel->onTrackSolo = std::move(onSolo);
    }
}

void EditWindow::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(10, 11, 16));

    // Left track panel background
    g.setColour(juce::Colour::fromRGB(14, 16, 22));
    g.fillRect(leftPanel.getBounds());

    // Right inspector background
    g.setColour(juce::Colour::fromRGB(16, 18, 26));
    g.fillRect(rightPanel.getBounds());

    // Subtle dividers
    g.setColour(juce::Colour::fromRGB(35, 38, 52));
    g.fillRect(leftPanel.getRight(), 0, 1, getHeight());
    g.fillRect(rightPanel.getX() - 1, 0, 1, getHeight());
}

void EditWindow::resized()
{
    auto r = getLocalBounds();

    // Left: track panel — fixed width, full height
    auto left = r.removeFromLeft(220);
    leftPanel.setBounds(left);

    // Right: inspector — fixed width, full height
    auto right = r.removeFromRight(240);
    rightPanel.setBounds(right);

    // Center: edit mode toolbar at top, arrangement view fills rest
    centerPanel.setBounds(r);

    if (trackPanel)
        trackPanel->setBounds(leftPanel.getLocalBounds());
    if (inspectorPanel)
        inspectorPanel->setBounds(rightPanel.getLocalBounds().reduced(8));

    auto centerArea = centerPanel.getLocalBounds();
    const int toolbarH = 28;
    if (editModeToolbar)
        editModeToolbar->setBounds(centerArea.removeFromTop(toolbarH));
    if (arrangementView)
        arrangementView->setBounds(centerArea);
}

void EditWindow::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused(source);
    if (trackPanel)
        trackPanel->refresh();
    if (inspectorPanel)
        inspectorPanel->repaint();
    if (arrangementView)
        arrangementView->repaint();
    repaint();
}
