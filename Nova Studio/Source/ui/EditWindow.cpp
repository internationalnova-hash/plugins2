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

    productionPanel = std::make_unique<ProductionPanel>(arrangementModel);
    rightPanel.addAndMakeVisible(*productionPanel);

    // Wire ProductionPanel callbacks (no-ops for now — engine wiring is separate)
    productionPanel->onInsertClicked = [](int slot) {
        DBG("Insert slot clicked: " + juce::String(slot));
    };
    productionPanel->onInsertChangePlugin = [](int slot) {
        DBG("Change plugin on slot: " + juce::String(slot));
    };
    productionPanel->onInsertRemovePlugin = [](int slot) {
        DBG("Remove plugin on slot: " + juce::String(slot));
    };
    productionPanel->onEQChanged = [](int band, float freq, float gainDb, float q) {
        DBG("EQ band " + juce::String(band) + " freq=" + juce::String(freq)
            + " gain=" + juce::String(gainDb) + " q=" + juce::String(q));
    };
    productionPanel->onSendLevelChanged = [](int send, float level) {
        DBG("Send " + juce::String(send) + " level=" + juce::String(level));
    };

    editModeToolbar = std::make_unique<EditModeToolbar>();
    centerPanel.addAndMakeVisible(*editModeToolbar);

    arrangementView = std::make_unique<ArrangementView>(transportState, timelineModel, arrangementModel);
    centerPanel.addAndMakeVisible(*arrangementView);

    // Wire toolbar callbacks to arrangement view
    editModeToolbar->onEditModeChanged = [this](EditModeToolbar::EditMode m) {
        if (arrangementView) arrangementView->setEditMode(m);
    };
    editModeToolbar->onCursorToolChanged = [this](EditModeToolbar::CursorTool t) {
        if (arrangementView) arrangementView->setCursorTool(t);
    };
    editModeToolbar->onSnapResolutionChanged = [this](double beats) {
        if (arrangementView) arrangementView->setSnapResolution(beats);
    };
    editModeToolbar->onNudgeResolutionChanged = [this](double beats) {
        if (arrangementView) arrangementView->setNudgeResolution(beats);
    };
    editModeToolbar->onSnapEnabled = [this](bool on) {
        if (arrangementView) arrangementView->setSnapEnabled(on);
    };
    editModeToolbar->onHZoomChanged = [this](int dir) {
        if (arrangementView) arrangementView->adjustHZoom(dir);
    };
    editModeToolbar->onVZoomChanged = [this](int dir) {
        if (arrangementView) arrangementView->adjustVZoom(dir);
    };

    // Sync vertical scroll between track panel and arrangement view
    if (trackPanel && arrangementView)
    {
        trackPanel->onScrollChanged = [this](int y) {
            if (arrangementView) arrangementView->setTrackScrollY(y);
        };
        arrangementView->onScrollChanged = [this](int y) {
            if (trackPanel) trackPanel->setScrollY(y);
        };
    }

    // Forward track-creation request to MainComponent via our own callback
    if (arrangementView)
    {
        arrangementView->onCreateAudioTrack = [this](const juce::String& name, bool stereo) -> int {
            if (onCreateAudioTrack) return onCreateAudioTrack(name, stereo);
            return -1;
        };
    }
}

void EditWindow::setLevelCallback(std::function<float(int,int)> fn)
{
    if (trackPanel)
        trackPanel->getTrackLevel = std::move(fn);
}

void EditWindow::zoomHorizontal(int direction)
{
    if (arrangementView)
        arrangementView->adjustHZoom(direction);
}

EditWindow::~EditWindow()
{
    transportState.removeChangeListener(this);
    arrangementModel.removeChangeListener(this);
}

void EditWindow::setTrackCallbacks(std::function<void(int,bool)> onArm,
                                   std::function<void(int,bool)> onMute,
                                   std::function<void(int,bool)> onSolo,
                                   std::function<void(int, const juce::String&)> onRenamed)
{
    if (trackPanel)
    {
        trackPanel->onTrackArm  = std::move(onArm);
        trackPanel->onTrackMute = std::move(onMute);
        trackPanel->onTrackSolo = std::move(onSolo);
        trackPanel->onTrackRenamed = onRenamed
            ? std::move(onRenamed)
            : [this](int, const juce::String&) { arrangementModel.sendChangeMessage(); };
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
    if (productionPanel)
        productionPanel->setBounds(rightPanel.getLocalBounds());

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
    if (productionPanel)
    {
        auto idx = arrangementModel.getSelectedTrackIndex();
        if (idx >= 0 && idx < arrangementModel.getSession().getNumTracks())
            productionPanel->updateFromTrack(arrangementModel.getSession().getTrack(idx));
        productionPanel->repaint();
    }
    if (arrangementView)
        arrangementView->repaint();
    repaint();
}
