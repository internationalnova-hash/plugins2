#include "MainComponent.h"

MainComponent::MainComponent()
    : transportState(engine.getTransportState()),
      timelineModel(engine.getSession(), transportState),
      arrangementModel(engine.getSession(), timelineModel),
      trackPanel(engine.getSession()),
      arrangementView(transportState, timelineModel, arrangementModel),
      alignPanel(arrangementModel)
{
    setSize(1440, 900);

    addAndMakeVisible(menuBar);
    addAndMakeVisible(workspaceToolbar);
    addAndMakeVisible(browserPanel);
    addAndMakeVisible(browserToggleBar);
    browserToggleBar.onClick = [this]() {
        browserCollapsed = !browserCollapsed;
        browserToggleBar.collapsed = browserCollapsed;
        browserPanel.setVisible(!browserCollapsed);
        browserToggleBar.repaint();
        resized();
    };
    // Legacy components kept for API compat but hidden — EditWindow owns the real instances.
    // They MUST be invisible so they don't intercept mouse events over other panels.
    addAndMakeVisible(trackPanel);
    trackPanel.setVisible(false);
    addAndMakeVisible(arrangementView);
    arrangementView.setVisible(false);
    addAndMakeVisible(alignPanel);
    alignPanel.setVisible(false);
    editWindow = std::make_unique<NovaStudioUI::EditWindow>(transportState, timelineModel, arrangementModel);
    addAndMakeVisible(*editWindow);
    editWindow->setTrackCallbacks(
        [this](int idx, bool armed) {
            engine.setTrackArm(idx, armed);
            if (editWindow) editWindow->repaint();
        },
        [this](int idx, bool muted) {
            engine.setTrackMute(idx, muted);
            if (editWindow) editWindow->repaint();
        },
        [this](int idx, bool solo) {
            engine.setTrackSolo(idx, solo);
            if (editWindow) editWindow->repaint();
        });

    // "+" button in track panel header
    editWindow->onAddTrackClicked = [this]() { menuItemSelected(301, 0); };

    editWindow->onCreateAudioTrack = [this](const juce::String& name, bool /*stereo*/) -> int {
        engine.addTrack(name, NovaStudio::TrackType::Audio);
        refreshTrackList();
        return engine.getSession().getNumTracks() - 1;
    };

    editWindow->setLevelCallback([this](int track, int ch) -> float {
        return engine.getTrackPeakLevel(track, ch);
    });
    editWindow->setEngine(engine);
    editWindow->onOpenPluginEditor = [this](int trackIndex, int slot) {
        if (mixerWindow) mixerWindow->openPluginEditor(trackIndex, slot);
    };

    mixerWindow = std::make_unique<NovaStudioUI::MixerWindow>(engine);
    mixerWindow->setArrangementModel(arrangementModel);
    addAndMakeVisible(*mixerWindow);
    mixerWindow->setVisible(false);
    beatWindow = std::make_unique<NovaStudioUI::BeatWindow>(transportState);
    addAndMakeVisible(*beatWindow);
    beatWindow->setVisible(false);

    beatWindow->setOnSampleAssigned([this](int row, const juce::String& path) {
        engine.setStepSeqSample(row, path);
    });
    beatWindow->setOnStepChanged([this](int row, int step, bool active) {
        engine.setStepSeqStep(row, step, active);
    });

    // Pop-out buttons
    auto configPopBtn = [](juce::TextButton& btn) {
        btn.setTooltip("Pop out as floating window");
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(30, 34, 48));
        btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.7f));
    };
    configPopBtn(popMixerBtn);
    configPopBtn(popBeatBtn);
    configPopBtn(popBrowserBtn);
    addAndMakeVisible(popMixerBtn);
    addAndMakeVisible(popBeatBtn);
    addAndMakeVisible(popBrowserBtn);

    popMixerBtn.onClick   = [this]() { popOutMixer(); };
    popBeatBtn.onClick    = [this]() { popOutBeat(); };
    popBrowserBtn.onClick = [this]() { popOutBrowser(); };
    addAndMakeVisible(mixerPanel);
    mixerPanel.setVisible(false);
    addAndMakeVisible(bottomDock);
    addAndMakeVisible(bottomDockToggleBar);
    bottomDockToggleBar.onClick = [this]() {
        bottomDockCollapsed = !bottomDockCollapsed;
        bottomDockToggleBar.collapsed = bottomDockCollapsed;
        bottomDock.setVisible(!bottomDockCollapsed);
        bottomDockToggleBar.repaint();
        resized();
    };
    addAndMakeVisible(statusLabel);

    
    
    

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
    statusLabel.setText("Nova Studio Lite prototype ready", juce::dontSendNotification);

    workspaceToolbar.onReturnToZero = [this] {
        engine.stop();
        transportState.setPositionSamples(0, true);
        workspaceToolbar.setPlayState(false, false);
        updateStatusMessage("Returned to zero.");
    };

    workspaceToolbar.onPlay = [this] {
        engine.play();
        workspaceToolbar.setPlayState(true, engine.isRecording());
        updateStatusMessage("Playback engaged.");
    };

    workspaceToolbar.onStop = [this] {
        bool wasRecording = engine.isRecording();
        engine.stop();
        workspaceToolbar.setPlayState(false, false);
        if (wasRecording)
        {
            auto f = engine.getLastRecordingFile();
            updateStatusMessage("Recorded: " + (f.existsAsFile() ? f.getFullPathName() : "unknown path"));
            refreshTrackList();
            arrangementModel.sendChangeMessage();
        }
        else
        {
            updateStatusMessage("Playback stopped.");
        }
    };

    workspaceToolbar.onRecord = [this] {
        engine.toggleRecord();
        workspaceToolbar.setPlayState(engine.getTransportState().isPlaying(), engine.isRecording());
        updateStatusMessage(engine.isRecording() ? "Recording active." : "Record stopped.");
    };

    workspaceToolbar.onArm = [this] {
        const bool armed = !engine.getTransportState().isRecordArmed();
        engine.getTransportState().setRecordArmed(armed);
        // Arm/disarm the first audio track in the session so createRecordingClipIfNeeded finds it
        for (int i = 0; i < engine.getTrackCount(); ++i)
        {
            if (engine.getSession().getTrack(i).type == NovaStudio::TrackType::Audio)
            {
                engine.setTrackArm(i, armed);
                break;
            }
        }
        workspaceToolbar.setArmState(armed);
        updateStatusMessage(armed ? "Record armed." : "Record disarmed.");
    };

    workspaceToolbar.onMonitor = [this] {
        const bool monitor = !engine.getTransportState().isInputMonitoring();
        engine.getTransportState().setInputMonitoring(monitor);
        workspaceToolbar.setMonitorState(monitor);
        updateStatusMessage(monitor ? "Input monitoring enabled." : "Input monitoring disabled.");
    };

    workspaceToolbar.onLoop = [this] {
        const bool looping = !engine.getTransportState().isLooping();
        engine.getTransportState().setLooping(looping);
        workspaceToolbar.setLoopState(looping);
        updateStatusMessage(looping ? "Loop enabled." : "Loop disabled.");
    };

    workspaceToolbar.onPunchIn = [this] {
        const int64_t pos = engine.getTransportState().getPositionSamples();
        engine.getTransportState().setPunchRange(pos,
            engine.getTransportState().getPunchOutSample() > pos
                ? engine.getTransportState().getPunchOutSample() : pos + (int64_t)(engine.getCurrentSampleRate() * 4));
        updateStatusMessage("Punch in set at " + engine.getTransportState().getTimecodeString(pos));
    };

    workspaceToolbar.onPunchOut = [this] {
        const int64_t pos = engine.getTransportState().getPositionSamples();
        engine.getTransportState().setPunchRange(engine.getTransportState().getPunchInSample(), pos);
        updateStatusMessage("Punch out set.");
    };

    workspaceToolbar.onPunchToggle = [this] {
        const bool enabled = !engine.getTransportState().isPunchEnabled();
        engine.getTransportState().setPunchEnabled(enabled);
        workspaceToolbar.setPunchState(enabled);
        updateStatusMessage(enabled ? "Punch recording enabled." : "Punch recording disabled.");
    };

    workspaceToolbar.setPlayState(false, false);
    workspaceToolbar.setArmState(engine.getTransportState().isRecordArmed());
    workspaceToolbar.setMonitorState(engine.getTransportState().isInputMonitoring());
    workspaceToolbar.setLoopState(engine.getTransportState().isLooping());

    if (!engine.initialize())
        updateStatusMessage("Audio initialization failed. Check System Preferences -> Sound.");
    else if (engine.getActiveInputChannelCount() == 0)
        updateStatusMessage("No input channels — grant microphone access in System Preferences -> Security & Privacy -> Microphone, then relaunch.");

    workspaceToolbar.setTempo(static_cast<int>(engine.getSession().getTempo()));
    workspaceToolbar.setTimecode(transportState.getTimecodeString(transportState.getPositionSamples()));
    workspaceToolbar.onTempoChanged = [this](int bpm) {
        engine.getSession().setTempo(static_cast<double>(bpm));
        transportState.setTempo(static_cast<double>(bpm));
        updateStatusMessage("Tempo: " + juce::String(bpm) + " BPM");
    };

    // initial playback indicator and toggle hookup
    workspaceToolbar.setPlaybackState(engine.getSession().isPreviewPlaybackEnabled(), arrangementModel.hasAlignmentPreview());
    workspaceToolbar.onTogglePreview = [this](bool wantsPreview) {
        engine.getSession().setPreviewPlaybackEnabled(wantsPreview);
        arrangementModel.sendChangeMessage();
        updateStatusMessage(wantsPreview ? "Preview audio enabled." : "Playing original audio.");
    };

    // listen for arrangement changes to update playback indicator
    arrangementModel.addChangeListener(this);

    engine.addTrack("Audio 1", NovaStudio::TrackType::Audio);
    engine.addTrack("MIDI 1",  NovaStudio::TrackType::Midi);
    engine.addTrack("Aux 1",   NovaStudio::TrackType::Aux);
    engine.addTrack("Aux 2",   NovaStudio::TrackType::Aux);
    refreshTrackList();

    workspaceToolbar.onModeSelected = [this](int mode) { setWorkspaceMode(mode); };
    workspaceToolbar.onEditModeSelected = [this](NovaStudio::ArrangementModel::EditMode mode)
    {
        arrangementModel.setEditMode(mode);
        updateStatusMessage("Edit mode set to " + juce::String(static_cast<int>(mode)) + ".");
    };
    workspaceToolbar.onNovaAlign = [this]()
    {
        setWorkspaceMode(0);
        alignPanel.setVisible(true);
        resized(); // re-layout so editWindow shrinks to make room
        updateStatusMessage("Nova Align panel opened.");
    };

    workspaceToolbar.onSave = [this]()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Save Session",
            NovaStudio::StudioAudioEngine::getDefaultSessionsFolder(),
            "*.novastudio");
        chooser->launchAsync(
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File{}) return;
                auto target = f.withFileExtension(".novastudio");
                target.getParentDirectory().createDirectory();
                if (engine.saveSession(target))
                    updateStatusMessage("Session saved: " + target.getFileName());
                else
                    updateStatusMessage("Save failed.");
            });
    };

    workspaceToolbar.onLoad = [this]()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Open Session",
            NovaStudio::StudioAudioEngine::getDefaultSessionsFolder(),
            "*.novastudio");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (!f.existsAsFile()) return;
                if (engine.loadSession(f))
                {
                    if (mixerWindow) mixerWindow->refresh();
                    refreshTrackList();
                    updateStatusMessage("Session loaded: " + f.getFileName());
                }
                else
                {
                    updateStatusMessage("Load failed.");
                }
            });
    };

    workspaceToolbar.onAudioSettings = [this]()
    {
        if (audioSettingsWindow == nullptr)
            audioSettingsWindow = std::make_unique<NovaStudioUI::AudioSettingsWindow>(engine.getDeviceManager());
        else
        {
            audioSettingsWindow->setVisible(true);
            audioSettingsWindow->toFront(true);
        }
    };

    alignPanel.onStatusMessage = [this](const juce::String& message)
    {
        updateStatusMessage(message);
    };

    // load saved workspace mode
    juce::File cfg = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("NovaStudio").getChildFile("workspace.json");
    if (cfg.existsAsFile())
    {
        auto content = cfg.loadFileAsString();
        auto parsed = juce::JSON::parse(content);
        if (parsed.isObject())
        {
            auto obj = parsed.getDynamicObject();
            if (obj->hasProperty("mode"))
                setWorkspaceMode((int)obj->getProperty("mode"));
            else
                setWorkspaceMode(0);
        }
        else
        {
            setWorkspaceMode(0);
        }
    }
    else
    {
        setWorkspaceMode(0);
    }

    // Register for keyboard shortcuts (undo/redo)
    addKeyListener(this);
    setWantsKeyboardFocus(true);
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &arrangementModel)
    {
        workspaceToolbar.setPlaybackState(engine.getSession().isPreviewPlaybackEnabled(), arrangementModel.hasAlignmentPreview());
    }
}

MainComponent::~MainComponent()
{
    // Close floating windows before destroying content
    if (floatingMixer)  { floatingMixer->setContentNonOwned(nullptr, false);  floatingMixer.reset(); }
    if (floatingBeat)   { floatingBeat->setContentNonOwned(nullptr, false);   floatingBeat.reset(); }
    if (floatingBrowser){ floatingBrowser->setContentNonOwned(nullptr, false); floatingBrowser.reset(); }
}

void MainComponent::popOutMixer()
{
    if (floatingMixer) { floatingMixer->toFront(true); return; }
    if (mixerWindow)
    {
        mixerWindow->setVisible(true);
        floatingMixer = std::make_unique<FloatingPanelWindow>(
            "Nova Studio — Mixer", mixerWindow.get(),
            [this](FloatingPanelWindow* w) {
                juce::ignoreUnused(w);
                redockMixer();
            });
        // Position to the right of or below the main window by default
        auto mainBounds = getTopLevelComponent()->getBounds();
        floatingMixer->setBounds(mainBounds.getX(), mainBounds.getBottom() + 4,
                                 juce::jmax(900, mainBounds.getWidth()), 900);
        floatingMixer->setResizable(true, false);
    }
}

void MainComponent::redockMixer()
{
    floatingMixer.reset();
    if (mixerWindow)
    {
        addAndMakeVisible(*mixerWindow);
        mixerWindow->setVisible(false); // hidden until mode 1 selected
    }
    resized();
}

void MainComponent::popOutBeat()
{
    if (floatingBeat) { floatingBeat->toFront(true); return; }
    if (beatWindow)
    {
        beatWindow->setVisible(true);
        floatingBeat = std::make_unique<FloatingPanelWindow>(
            "Nova Studio — Beat Production", beatWindow.get(),
            [this](FloatingPanelWindow* w) {
                juce::ignoreUnused(w);
                redockBeat();
            });
        auto mainBounds = getTopLevelComponent()->getBounds();
        floatingBeat->setBounds(mainBounds.getX(), mainBounds.getBottom() + 4,
                                juce::jmax(800, mainBounds.getWidth()), 700);
        floatingBeat->setResizable(true, false);
    }
}

void MainComponent::redockBeat()
{
    floatingBeat.reset();
    if (beatWindow)
    {
        addAndMakeVisible(*beatWindow);
        beatWindow->setVisible(false);
    }
    resized();
}

void MainComponent::popOutBrowser()
{
    if (floatingBrowser) { floatingBrowser->toFront(true); return; }
    browserPanel.setVisible(true);
    browserCollapsed = true;
    browserToggleBar.collapsed = true;
    browserToggleBar.repaint();
    floatingBrowser = std::make_unique<FloatingPanelWindow>(
        "Browser", &browserPanel,
        [this](FloatingPanelWindow* w) {
            juce::ignoreUnused(w);
            redockBrowser();
        });
    resized();
}

void MainComponent::redockBrowser()
{
    floatingBrowser.reset();
    addAndMakeVisible(browserPanel);
    browserCollapsed = false;
    browserToggleBar.collapsed = false;
    browserToggleBar.repaint();
    browserPanel.setVisible(true);
    resized();
}

void MainComponent::paint(juce::Graphics& g)
{
    juce::Colour top = juce::Colour::fromRGB(10, 12, 18);
    juce::Colour bottom = juce::Colour::fromRGB(16, 20, 30);
    g.setGradientFill({ top, 0.0f, 0.0f, bottom, 0.0f, (float)getHeight(), false });
    g.fillAll();
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    // Native menu bar (macOS draws it in the system bar; on other platforms show inline)
    menuBar.setBounds(area.removeFromTop(kMenuH));

    // Unified top header
    workspaceToolbar.setBounds(area.removeFromTop(56));

    // Left browser sidebar + toggle bar
    browserToggleBar.setBounds(area.removeFromLeft(kToggleW));
    if (!browserCollapsed)
    {
        auto leftSidebar = area.removeFromLeft(190);
        browserPanel.setBounds(leftSidebar);
    }
    else
    {
        browserPanel.setBounds({});
    }

    // Bottom dock toggle bar (always visible — 14px horizontal strip)
    bottomDockToggleBar.setBounds(area.removeFromBottom(14));
    // Bottom dock: only if not collapsed
    if (!bottomDockCollapsed)
    {
        const int dockH = juce::jmax(110, getHeight() / 4);
        auto bottomArea = area.removeFromBottom(dockH);
        bottomDock.setBounds(bottomArea);
    }
    else
    {
        bottomDock.setBounds({});
    }

    // Status label (overlay, bottom right)
    statusLabel.setBounds(getWidth() - 500, getHeight() - 26, 480, 22);

    // Nova Align: reserve right 360px when visible so it doesn't overlap editWindow
    if (alignPanel.isVisible())
        alignPanel.setBounds(area.removeFromRight(360));
    else
        alignPanel.setBounds({});

    // Edit mode: editWindow takes remaining center area
    if (editWindow)
        editWindow->setBounds(area);

    // Mixer mode: full screen minus header
    if (mixerWindow)
        mixerWindow->setBounds(getLocalBounds().withTrimmedTop(56));

    // Beat mode: full screen minus header
    if (beatWindow)
        beatWindow->setBounds(getLocalBounds().withTrimmedTop(56));

    // Legacy panels (hidden in new layout but kept for compat)
    trackPanel.setBounds(0, 0, 0, 0);
    arrangementView.setBounds(0, 0, 0, 0);
    mixerPanel.setBounds(0, 0, 0, 0);
}

void MainComponent::updateStatusMessage(const juce::String& message)
{
    statusMessage = message;
    statusLabel.setText(statusMessage, juce::dontSendNotification);
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* /*originatingComponent*/)
{
    const bool isCmd = key.getModifiers().isCommandDown();
    const bool isCtrl = key.getModifiers().isCtrlDown();
    const bool isShift = key.getModifiers().isShiftDown();

    // Undo: Cmd/Ctrl + Z
    if ((isCmd || isCtrl) && key.getTextCharacter() == 'z' && !isShift)
    {
        arrangementModel.undo();
        updateStatusMessage("Undo");
        return true;
    }

    // Redo: Shift + Cmd/Ctrl + Z
    if ((isCmd || isCtrl) && isShift && (key.getTextCharacter() == 'z' || key.getTextCharacter() == 'Z'))
    {
        arrangementModel.redo();
        updateStatusMessage("Redo");
        return true;
    }

    // Toggle Preview/Original: Cmd/Ctrl + Shift + P (only when preview exists)
    if ((isCmd || isCtrl) && isShift && (key.getTextCharacter() == 'p' || key.getTextCharacter() == 'P'))
    {
        if (arrangementModel.hasAlignmentPreview())
        {
            bool current = engine.getSession().isPreviewPlaybackEnabled();
            engine.getSession().setPreviewPlaybackEnabled(!current);
            arrangementModel.sendChangeMessage();
            updateStatusMessage(!current ? "Preview audio enabled." : "Playing original audio.");
            return true;
        }
        return false;
    }

    // ── Window / mode switching ──────────────────────────────────────────────
    // Cmd+E  or  Cmd+=  → Edit window  (Pro Tools: Cmd+=)
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 'e' || key.getTextCharacter() == 'E'
                               || key.getKeyCode() == '='))
    {
        setWorkspaceMode(0);
        updateStatusMessage("Edit window");
        return true;
    }
    // Cmd+M  or  Cmd+-  → Mixer  (Pro Tools: Cmd+-)
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 'm' || key.getTextCharacter() == 'M'
                               || key.getKeyCode() == '-'))
    {
        setWorkspaceMode(1);
        updateStatusMessage("Mixer");
        return true;
    }
    // Cmd+B → Beat production screen
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 'b' || key.getTextCharacter() == 'B'))
    {
        setWorkspaceMode(3);  // Beat/browse mode
        updateStatusMessage("Beat Production");
        return true;
    }
    // Cmd+W → Browse panel
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 'w' || key.getTextCharacter() == 'W'))
    {
        setWorkspaceMode(2);
        updateStatusMessage("Browser");
        return true;
    }

    // ── Transport shortcuts ──────────────────────────────────────────────────
    // Space → Play / Stop toggle
    if (!isCmd && !isCtrl && key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (workspaceToolbar.onPlay)  workspaceToolbar.onPlay();
        return true;
    }
    // Return/Enter → Return to zero
    if (!isCmd && !isCtrl && key.getKeyCode() == juce::KeyPress::returnKey)
    {
        if (workspaceToolbar.onReturnToZero) workspaceToolbar.onReturnToZero();
        return true;
    }

    // ── Clip editing shortcuts ───────────────────────────────────────────────
    const double sr = engine.getTransportState().getSampleRate();

    // Cmd+F → Apply default fade (0.08s in + 0.08s out) to selected clip
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 'f' || key.getTextCharacter() == 'F'))
    {
        if (arrangementModel.hasSelection())
        {
            const int64_t fadeSamples = (int64_t)(0.08 * sr);
            arrangementModel.setSelectedClipFadeIn(fadeSamples);
            arrangementModel.setSelectedClipFadeOut(fadeSamples);
            updateStatusMessage("Fade applied — adjust in Inspector");
            if (editWindow) editWindow->repaint();
        }
        return true;
    }

    // Cmd+D → Duplicate selected clip
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 'd' || key.getTextCharacter() == 'D'))
    {
        arrangementModel.duplicateSelectedClip();
        refreshTrackList();
        updateStatusMessage("Clip duplicated");
        return true;
    }

    // Delete / Backspace → Delete selected clip
    if (!isCmd && !isCtrl && (key.getKeyCode() == juce::KeyPress::deleteKey
                               || key.getKeyCode() == juce::KeyPress::backspaceKey))
    {
        if (arrangementModel.hasSelection())
        {
            arrangementModel.deleteSelectedClip();
            refreshTrackList();
            updateStatusMessage("Clip deleted");
            return true;
        }
    }

    // Cmd+T → Split clip at playhead  (Pro Tools: Cmd+E / B in commands focus)
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 't' || key.getTextCharacter() == 'T'))
    {
        arrangementModel.splitSelectedClip(engine.getTransportState().getPositionSamples());
        refreshTrackList();
        updateStatusMessage("Clip split at playhead");
        return true;
    }

    // ── Save session ─────────────────────────────────────────────────────────
    if ((isCmd || isCtrl) && !isShift && (key.getTextCharacter() == 's' || key.getTextCharacter() == 'S'))
    {
        if (workspaceToolbar.onSave) workspaceToolbar.onSave();
        return true;
    }

    // ── Panel collapse shortcuts ─────────────────────────────────────────────
    // Cmd+\ → Maximize timeline (collapse left + right panels in edit mode)
    if ((isCmd || isCtrl) && key.getKeyCode() == '\\')
    {
        if (editWindow && editWindow->isVisible())
        {
            bool anyCollapsed = editWindow->isLeftPanelCollapsed() || editWindow->isRightPanelCollapsed();
            editWindow->setLeftPanelCollapsed(!anyCollapsed);
            editWindow->setRightPanelCollapsed(!anyCollapsed);
            browserCollapsed = !anyCollapsed;
            browserToggleBar.collapsed = browserCollapsed;
            browserPanel.setVisible(!browserCollapsed);
            browserToggleBar.repaint();
            resized();
            updateStatusMessage(anyCollapsed ? "Panels restored." : "Timeline maximized.");
        }
        return true;
    }

    // ── Zoom shortcuts ───────────────────────────────────────────────────────
    // Cmd+]  or  Cmd++  → Zoom in
    if ((isCmd || isCtrl) && (key.getKeyCode() == ']' || key.getKeyCode() == '='))
    {
        if (editWindow) editWindow->zoomHorizontal(1);
        return true;
    }
    // Cmd+[  or  Cmd+-  → Zoom out  (Cmd+- also opens Mixer; zoom wins when Edit is active)
    if ((isCmd || isCtrl) && (key.getKeyCode() == '['))
    {
        if (editWindow) editWindow->zoomHorizontal(-1);
        return true;
    }

    // ── Single-key commands (Pro Tools Commands Focus style) ─────────────────
    // Only fires when no modifier held and no text input is focused
    if (!isCmd && !isCtrl && !isShift)
    {
        const int k = key.getKeyCode();
        // D → Fade in to playhead position (sets fade in on selected clip)
        if (k == 'd' || k == 'D')
        {
            if (arrangementModel.hasSelection())
            {
                const int64_t pos    = engine.getTransportState().getPositionSamples();
                const auto*   clip   = arrangementModel.getSelectedClip();
                if (clip)
                {
                    const int64_t rel = juce::jmax<int64_t>(0, pos - clip->startSample);
                    arrangementModel.setSelectedClipFadeIn(rel);
                    if (editWindow) editWindow->repaint();
                    updateStatusMessage("Fade in set");
                }
            }
            return true;
        }
        // G → Fade out from playhead position
        if (k == 'g' || k == 'G')
        {
            if (arrangementModel.hasSelection())
            {
                const int64_t pos  = engine.getTransportState().getPositionSamples();
                const auto*   clip = arrangementModel.getSelectedClip();
                if (clip)
                {
                    const int64_t rel = juce::jmax<int64_t>(0, (clip->startSample + clip->lengthSamples) - pos);
                    arrangementModel.setSelectedClipFadeOut(rel);
                    if (editWindow) editWindow->repaint();
                    updateStatusMessage("Fade out set");
                }
            }
            return true;
        }
        // B → Split clip at playhead
        if (k == 'b' || k == 'B')
        {
            arrangementModel.splitSelectedClip(engine.getTransportState().getPositionSamples());
            refreshTrackList();
            updateStatusMessage("Clip split");
            return true;
        }
        // Z → Undo (single key, like Pro Tools commands focus)
        if (k == 'z' || k == 'Z')
        {
            arrangementModel.undo();
            updateStatusMessage("Undo");
            return true;
        }
    }

    return false;
}

void MainComponent::refreshTrackList()
{
    if (editWindow)
        editWindow->repaint();
    if (mixerWindow && mixerWindow->isVisible())
        mixerWindow->refresh();
}

void MainComponent::setWorkspaceMode(int mode)
{
    switch (mode)
    {
    case 0: // Edit — show edit window, hide/close floating mixer & beat
        if (editWindow) editWindow->setVisible(true);
        // Don't close floating mixer/beat — user may want them visible on other monitor
        // Just bring edit window to front
        if (editWindow) editWindow->getTopLevelComponent()->toFront(true);
        alignPanel.setVisible(false);
        bottomDock.setVisible(true);
        resized();
        break;

    case 1: // Mixer — open as floating window (draggable to any monitor)
        if (editWindow) editWindow->setVisible(true); // keep edit visible
        if (!floatingMixer)
        {
            if (mixerWindow) { mixerWindow->refresh(); }
            popOutMixer();
        }
        else
        {
            floatingMixer->setVisible(true);
            floatingMixer->toFront(true);
            if (mixerWindow) mixerWindow->refresh();
        }
        bottomDock.setVisible(true);
        resized();
        break;

    case 2: // Beat — open as floating window
        if (editWindow) editWindow->setVisible(true); // keep edit visible
        if (!floatingBeat)
            popOutBeat();
        else
        {
            floatingBeat->setVisible(true);
            floatingBeat->toFront(true);
        }
        bottomDock.setVisible(true);
        resized();
        break;

    default:
        if (editWindow) editWindow->setVisible(true);
        alignPanel.setVisible(false);
        bottomDock.setVisible(true);
        break;
    }

    // persist
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("mode", mode);
    juce::var root(obj.get());
    juce::File cfgDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("NovaStudio");
    cfgDir.createDirectory();
    cfgDir.getChildFile("workspace.json").replaceWithText(juce::JSON::toString(root));
    resized();
}

// ── Menu bar ─────────────────────────────────────────────────────────────────

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Track", "Clip", "Audio", "Plugins", "View", "AI", "Window", "Help" };
}

juce::PopupMenu MainComponent::getMenuForIndex(int menuIndex, const juce::String&)
{
    juce::PopupMenu menu;
    switch (menuIndex)
    {
    case 0: // File
        menu.addItem(101, "New Session");
        menu.addItem(102, "Open Session...");
        menu.addSeparator();
        menu.addItem(103, "Save");
        menu.addItem(104, "Save As...");
        menu.addSeparator();
        menu.addItem(105, "Import Audio...");
        menu.addItem(106, "Export Mix...",        false);
        menu.addItem(107, "Export Stems...",      false);
        menu.addSeparator();
        menu.addItem(108, "Session Settings...",  false);
        menu.addSeparator();
        menu.addItem(109, "Quit");
        break;

    case 1: // Edit
        menu.addItem(201, "Undo");
        menu.addItem(202, "Redo");
        menu.addSeparator();
        menu.addItem(203, "Cut",       false);
        menu.addItem(204, "Copy",      false);
        menu.addItem(205, "Paste",     false);
        menu.addItem(206, "Duplicate");
        menu.addItem(207, "Delete");
        menu.addSeparator();
        menu.addItem(208, "Split Clip at Playhead");
        menu.addItem(209, "Select All", false);
        break;

    case 2: // Track
        menu.addItem(301, "New Audio Track");
        menu.addItem(302, "New MIDI Track");
        menu.addItem(303, "New Instrument Track");
        menu.addItem(304, "New Aux Track");
        menu.addSeparator();
        menu.addItem(307, "Duplicate Track",       false);
        menu.addItem(308, "Remove Last Track",     engine.getTrackCount() > 0);
        break;

    case 3: // Clip
        menu.addItem(401, "Consolidate",      false);
        menu.addItem(402, "Normalize",        false);
        menu.addItem(403, "Reverse",          false);
        menu.addSeparator();
        menu.addItem(404, "Fade In",          false);
        menu.addItem(405, "Fade Out",         false);
        menu.addSeparator();
        menu.addItem(406, "Bounce Clip",      false);
        menu.addItem(407, "Nova Align Selected");
        break;

    case 4: // Audio
        menu.addItem(501, "Audio Settings...");
        menu.addItem(502, "Input Routing...",    false);
        menu.addItem(503, "Output Routing...",   false);
        menu.addSeparator();
        menu.addItem(504, "Buffer Size",         false);
        menu.addItem(505, "Sample Rate",         false);
        break;

    case 5: // Plugins
        menu.addItem(601, "Plugin Manager...",   false);
        menu.addItem(602, "Scan Plugins",        false);
        menu.addItem(603, "Rescan Plugins",      false);
        menu.addItem(604, "Plugin Paths...",     false);
        break;

    case 6: // View
        menu.addItem(701, "Edit Window");
        menu.addItem(702, "Mixer");
        menu.addItem(703, "Piano Roll",          false);
        menu.addItem(704, "Step Sequencer",      false);
        menu.addItem(705, "Browser");
        menu.addSeparator();
        menu.addItem(706, "Inspector");
        menu.addItem(707, "Nova Align");
        break;

    case 7: // AI
        menu.addItem(801, "Nova Assistant",           false);
        menu.addSeparator();
        menu.addItem(802, "Align Background Vocals",  false);
        menu.addItem(803, "Organize Session",         false);
        menu.addItem(804, "Create Vocal Buses",       false);
        menu.addItem(805, "Stem Out Session",         false);
        menu.addItem(806, "Export For Pro Tools",     false);
        menu.addItem(807, "Find Clipping Tracks",     false);
        break;

    case 8: // Window
        menu.addItem(901, "Reset Layout");
        menu.addItem(902, "Full Screen",       false);
        menu.addSeparator();
        menu.addItem(903, "Floating Mixer",    false);
        menu.addItem(904, "Floating Browser",  false);
        break;

    case 9: // Help
        menu.addItem(1001, "Nova Studio Manual",   false);
        menu.addItem(1002, "Keyboard Shortcuts",   false);
        menu.addSeparator();
        menu.addItem(1003, "About Nova Studio...", false);
        break;

    default: break;
    }
    return menu;
}

void MainComponent::menuItemSelected(int id, int)
{
    switch (id)
    {
    // ── File ────────────────────────────────────────────────────────────────
    case 101: // New Session
        updateStatusMessage("New Session: not yet implemented.");
        break;
    case 102: if (workspaceToolbar.onLoad) workspaceToolbar.onLoad(); break;
    case 103: // Save
    case 104: if (workspaceToolbar.onSave) workspaceToolbar.onSave(); break;
    case 105: // Import Audio
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Import Audio", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.wav;*.aif;*.aiff;*.mp3;*.flac");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (!f.existsAsFile()) return;
                juce::AudioFormatManager fmt;
                fmt.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(f));
                NovaStudio::Clip clip;
                clip.file          = f;
                clip.startSample   = 0;
                clip.lengthSamples = reader ? reader->lengthInSamples : (int64_t)(44100 * 5);
                clip.isMidi        = false;
                if (engine.getSession().getNumTracks() > 0)
                    engine.getSession().getTrack(0).clips.add(clip);
                refreshTrackList();
                arrangementModel.sendChangeMessage();
                updateStatusMessage("Imported: " + f.getFileName());
                browserPanel.refresh();
            });
        break;
    }
    case 109: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;

    // ── Edit ────────────────────────────────────────────────────────────────
    case 201: arrangementModel.undo();  updateStatusMessage("Undo"); break;
    case 202: arrangementModel.redo();  updateStatusMessage("Redo"); break;
    case 206: arrangementModel.duplicateSelectedClip(); refreshTrackList(); break;
    case 207: arrangementModel.deleteSelectedClip();    refreshTrackList(); break;
    case 208: arrangementModel.splitSelectedClip(transportState.getPositionSamples()); refreshTrackList(); break;

    // ── Track ───────────────────────────────────────────────────────────────
    case 301:
    case 302:
    case 303:
    case 304:
    {
        NovaStudio::TrackType type;
        juce::String prefix;
        switch (id)
        {
            case 301: type = NovaStudio::TrackType::Audio;  prefix = "Audio ";  break;
            case 302: type = NovaStudio::TrackType::Midi;   prefix = "MIDI ";   break;
            case 303: type = NovaStudio::TrackType::Midi;   prefix = "Inst ";   break;
            case 304: type = NovaStudio::TrackType::Aux;    prefix = "Aux ";    break;
            default:  type = NovaStudio::TrackType::Audio;  prefix = "Audio ";  break;
        }
        // Ask track count + mono/stereo
        auto* dlg = new juce::AlertWindow("Add " + prefix.trimEnd() + " Track",
                                          "Configure new track:",
                                          juce::MessageBoxIconType::NoIcon);
        dlg->addTextEditor("count", "1", "Number of tracks:");
        // Mono/stereo only relevant for Audio and Aux
        if (type == NovaStudio::TrackType::Audio || type == NovaStudio::TrackType::Aux)
            dlg->addComboBox("channels", { "Stereo", "Mono" }, "Channels:");
        dlg->addButton("Add",    1, juce::KeyPress(juce::KeyPress::returnKey));
        dlg->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        dlg->setColour(juce::AlertWindow::backgroundColourId, juce::Colour::fromRGB(18, 20, 28));
        dlg->setColour(juce::AlertWindow::textColourId, juce::Colours::white);
        dlg->enterModalState(true,
            juce::ModalCallbackFunction::create([this, dlg, type, prefix](int result) mutable
            {
                if (result == 1)
                {
                    int count = juce::jlimit(1, 64, dlg->getTextEditorContents("count").getIntValue());
                    bool isStereo = true;
                    if (auto* cb = dlg->getComboBoxComponent("channels"))
                        isStereo = (cb->getSelectedItemIndex() == 0); // 0=Stereo, 1=Mono
                    for (int k = 0; k < count; ++k)
                    {
                        const int n = engine.getTrackCount() + 1;
                        engine.addTrack(prefix + juce::String(n), type);
                        auto& t = engine.getSession().getTrack(engine.getSession().getNumTracks() - 1);
                        t.isStereo  = isStereo;
                        t.outputBus = "Main Out";
                    }
                    refreshTrackList();
                    arrangementModel.sendChangeMessage();
                    updateStatusMessage("Added " + juce::String(count) + " " + prefix.trimEnd() + " track(s)");
                }
                delete dlg;
            }), false);
        break;
    }
    case 308:
        updateStatusMessage("Remove Track: not yet implemented.");
        break;

    // ── Clip ────────────────────────────────────────────────────────────────
    case 407:
        setWorkspaceMode(0);
        alignPanel.setVisible(true);
        alignPanel.toFront(true);
        break;

    // ── Audio ───────────────────────────────────────────────────────────────
    case 501: if (workspaceToolbar.onAudioSettings) workspaceToolbar.onAudioSettings(); break;

    // ── View ────────────────────────────────────────────────────────────────
    case 701: setWorkspaceMode(0); break;
    case 702: setWorkspaceMode(1); break;
    case 705: setWorkspaceMode(0); browserPanel.refresh(); break;
    case 706: setWorkspaceMode(0); break;
    case 707:
        setWorkspaceMode(0);
        alignPanel.setVisible(true);
        alignPanel.toFront(true);
        break;

    // ── Window ──────────────────────────────────────────────────────────────
    case 901: setWorkspaceMode(0); resized(); break;

    default:
        updateStatusMessage("Coming soon.");
        break;
    }
}
