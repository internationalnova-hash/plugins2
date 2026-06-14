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

    // Toggle bars
    leftToggleBar.isLeft = true;
    rightToggleBar.isLeft = false;
    addAndMakeVisible(leftToggleBar);
    addAndMakeVisible(rightToggleBar);

    leftToggleBar.onClick = [this]() { setLeftPanelCollapsed(!leftCollapsed); };
    rightToggleBar.onClick = [this]() { setRightPanelCollapsed(!rightCollapsed); };

    transportState.addChangeListener(this);
    arrangementModel.addChangeListener(this);

    trackPanel = std::make_unique<TrackPanel>(arrangement.getSession());
    leftPanel.addAndMakeVisible(*trackPanel);

    productionPanel = std::make_unique<ProductionPanel>(arrangementModel);
    rightPanel.addAndMakeVisible(*productionPanel);

    // Insert/send callbacks are wired after setEngine() is called (see setEngine)

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
        arrangementView->onPunchRangeChanged = [this](int64_t pIn, int64_t pOut) {
            if (onPunchRangeChanged) onPunchRangeChanged(pIn, pOut);
        };
    }
}

void EditWindow::setEngine(NovaStudio::StudioAudioEngine& e)
{
    enginePtr = &e;

    if (!productionPanel) return;

    // onInsertClicked: open editor if plugin loaded, else open browser
    productionPanel->onInsertClicked = [this](int slot)
    {
        if (!enginePtr) return;
        int idx = arrangementModel.getSelectedTrackIndex();
        if (idx < 0) return;
        if (enginePtr->getTrackPlugin(idx, slot) != nullptr)
        {
            if (onOpenPluginEditor) onOpenPluginEditor(idx, slot);
        }
    };

    productionPanel->onInsertChangePlugin = [this](int slot)
    {
        if (!enginePtr) return;
        int idx = arrangementModel.getSelectedTrackIndex();
        if (idx < 0) return;
        enginePtr->removePluginFromTrack(idx, slot);
        arrangementModel.sendChangeMessage(); // both views refresh via changeListenerCallback
    };

    productionPanel->onInsertRemovePlugin = [this](int slot)
    {
        if (!enginePtr) return;
        int idx = arrangementModel.getSelectedTrackIndex();
        if (idx < 0) return;
        enginePtr->removePluginFromTrack(idx, slot);
        arrangementModel.sendChangeMessage();
    };

    productionPanel->onEQChanged = [this](int band, float freq, float gainDb, float q)
    {
        if (!enginePtr) return;
        int idx = arrangementModel.getSelectedTrackIndex();
        if (idx < 0) return;
        enginePtr->setTrackEQBand(idx, band, true, freq, gainDb, q);
    };

    productionPanel->onSendLevelChanged = [this](int send, float db)
    {
        if (!enginePtr) return;
        int idx = arrangementModel.getSelectedTrackIndex();
        if (idx >= 0) enginePtr->setTrackSendLevel(idx, send, db);
    };

    productionPanel->onSendBusChanged = [this](int send, int busIndex)
    {
        if (!enginePtr) return;
        int idx = arrangementModel.getSelectedTrackIndex();
        if (idx < 0) return;
        enginePtr->setTrackSendBus(idx, send, busIndex);
        if (busIndex >= 0)
        {
            const int n = enginePtr->getSession().getNumTracks();
            bool found = false;
            for (int t = 0; t < n; ++t)
            {
                const auto& tr = enginePtr->getSession().getTrack(t);
                if (tr.type == NovaStudio::TrackType::Aux && tr.auxInputBusIndex == busIndex)
                { found = true; break; }
            }
            if (!found)
                enginePtr->addAuxTrack("Aux " + NovaStudio::StudioAudioEngine::sendBusName(busIndex), busIndex);
        }
        arrangementModel.sendChangeMessage();
    };

    productionPanel->onSendPreFaderChanged = [this](int send, bool pre)
    {
        if (!enginePtr) return;
        int idx = arrangementModel.getSelectedTrackIndex();
        if (idx >= 0) enginePtr->setTrackSendPreFader(idx, send, pre);
    };

    productionPanel->getMeterLevel = [this](int ch) -> float
    {
        if (!enginePtr) return 0.0f;
        const int idx = arrangementModel.getSelectedTrackIndex();
        if (idx < 0) return 0.0f;
        return enginePtr->getTrackPeakLevel(idx, ch);
    };

    productionPanel->onVolumeChanged = [this](float db)
    {
        if (!enginePtr) return;
        const int idx = arrangementModel.getSelectedTrackIndex();
        if (idx >= 0) enginePtr->setTrackVolume(idx, db);
    };
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

void EditWindow::zoomVertical(int direction)
{
    if (arrangementView)
        arrangementView->adjustVZoom(direction);
}

void EditWindow::setEditMode(EditModeToolbar::EditMode m)
{
    if (editModeToolbar) editModeToolbar->setEditMode(m);
    if (arrangementView)  arrangementView->setEditMode(m);
}

void EditWindow::setCursorTool(EditModeToolbar::CursorTool t)
{
    if (editModeToolbar) editModeToolbar->setCursorTool(t);
    if (arrangementView)  arrangementView->setCursorTool(t);
}

void EditWindow::setLeftPanelCollapsed(bool collapsed)
{
    leftCollapsed = collapsed;
    leftToggleBar.collapsed = collapsed;
    leftPanel.setVisible(!collapsed);
    leftToggleBar.repaint();
    resized();
}

void EditWindow::setRightPanelCollapsed(bool collapsed)
{
    rightCollapsed = collapsed;
    rightToggleBar.collapsed = collapsed;
    rightPanel.setVisible(!collapsed);
    rightToggleBar.repaint();
    resized();
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
        trackPanel->onAddTrackClicked = [this]() { if (onAddTrackClicked) onAddTrackClicked(); };
    }
}

void EditWindow::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(10, 11, 16));

    // Left track panel background
    if (!leftCollapsed)
    {
        g.setColour(juce::Colour::fromRGB(14, 16, 22));
        g.fillRect(leftPanel.getBounds());
        g.setColour(juce::Colour::fromRGB(35, 38, 52));
        g.fillRect(leftPanel.getRight(), 0, 1, getHeight());
    }

    // Right inspector background
    if (!rightCollapsed)
    {
        g.setColour(juce::Colour::fromRGB(16, 18, 26));
        g.fillRect(rightPanel.getBounds());
        g.setColour(juce::Colour::fromRGB(35, 38, 52));
        g.fillRect(rightPanel.getX() - 1, 0, 1, getHeight());
    }
}

void EditWindow::resized()
{
    auto r = getLocalBounds();

    // Left toggle bar is always visible at the left edge
    leftToggleBar.setBounds(r.removeFromLeft(kToggleBarW));

    // Left panel (only if not collapsed)
    if (!leftCollapsed)
    {
        auto left = r.removeFromLeft(220);
        leftPanel.setBounds(left);
    }
    else
    {
        leftPanel.setBounds({});
    }

    // Right toggle bar at the right edge
    rightToggleBar.setBounds(r.removeFromRight(kToggleBarW));

    // Right panel (only if not collapsed)
    if (!rightCollapsed)
    {
        auto right = r.removeFromRight(240);
        rightPanel.setBounds(right);
    }
    else
    {
        rightPanel.setBounds({});
    }

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
        {
            productionPanel->updateFromTrack(arrangementModel.getSession().getTrack(idx));

            // Sync insert slot names from the single engine plugin chain
            if (enginePtr)
            {
                const int kMaxSlots = 10;
                for (int s = 0; s < kMaxSlots; ++s)
                {
                    auto* plugin = enginePtr->getTrackPlugin(idx, s);
                    productionPanel->setInsertSlotName(s, plugin ? plugin->getName() : juce::String());
                }
            }
        }
        productionPanel->repaint();
    }
    if (arrangementView)
        arrangementView->repaint();
    repaint();
}
