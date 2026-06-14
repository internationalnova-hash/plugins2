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

    editWindow->onPunchRangeChanged = [this](int64_t pIn, int64_t pOut) {
        engine.getTransportState().setPunchRange(pIn, pOut);
        workspaceToolbar.setPunchState(engine.getTransportState().isPunchEnabled());
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
    // MixerWindow lives inside the bottom dock by default so both Edit and Mixer views
    // share the exact same component with real session-driven channel strips.
    // popOutMixer() reparents it to a FloatingPanelWindow when requested.
    beatWindow = std::make_unique<NovaStudioUI::BeatWindow>(transportState);
    addAndMakeVisible(*beatWindow);
    beatWindow->setVisible(false);
    // Show the same File/Edit/... menu bar as the edit window so the beat
    // window feels consistent whether docked in the workspace or floating.
    beatWindow->setMenuBarModel(this);

    beatWindow->setOnSampleAssigned([this](int row, const juce::String& path) {
        engine.setStepSeqSample(row, path);
    });
    beatWindow->setOnStepChanged([this](int row, int step, bool active) {
        engine.setStepSeqStep(row, step, active);
    });
    beatWindow->setOnVelocityChanged([this](int row, int step, float velocity) {
        engine.setStepSeqVelocity(row, step, velocity);
    });
    beatWindow->getCurrentStep = [this]() {
        return engine.getStepSeqCurrentStep();
    };
    beatWindow->setOnPreviewSample([this](const juce::String& path) {
        engine.previewSample(juce::File(path));
    });
    beatWindow->setOnPianoKeyEvent([this](int pitch, bool keyDown) {
        engine.previewPianoKey(pitch, keyDown);
    });
    beatWindow->setOnRowMixerTrackChanged([this](int row, int mixerTrack) {
        juce::ignoreUnused(row, mixerTrack);
    });
    beatWindow->setOnRowPitchChanged([this](int row, float semitones) {
        engine.setStepSeqRowPitch(row, semitones);
    });
    beatWindow->setOnRowEnvelopeChanged([this](int row, bool enabled, float attack, float hold, float decay, float sustain, float release) {
        engine.setStepSeqRowEnvelope(row, enabled, attack, hold, decay, sustain, release);
    });
    beatWindow->setOnRowFilterChanged([this](int row, bool enabled, int filterType, float cutoffHz, float resonance) {
        engine.setStepSeqRowFilter(row, enabled, filterType, cutoffHz, resonance);
    });
    beatWindow->setOnRowLFOChanged([this](int row, bool enabled, int target, float rateHz, float depth) {
        engine.setStepSeqRowLFO(row, enabled, target, rateHz, depth);
    });
    beatWindow->setOnRowSampleRegionChanged([this](int row, float startFrac, float endFrac, bool loop) {
        engine.setStepSeqRowSampleRegion(row, startFrac, endFrac, loop);
    });
    beatWindow->setOnSwingChanged([this](float swing) {
        engine.setStepSeqSwing(swing);
    });
    beatWindow->setOnGrooveChanged([this](int templateIndex) {
        engine.setStepSeqGroove(templateIndex);
    });
    beatWindow->setNumMixerInserts(engine.getSession().getNumTracks());

    // Compact in-window mixer — keeps producers in the beat view while mixing
    beatWindow->setOnMixerVolumeChanged([this](int track, float db) {
        engine.setTrackVolume(track, db);
        refreshTrackList();
    });
    beatWindow->setOnMixerPanChanged([this](int track, float pan) {
        engine.setTrackPan(track, pan);
        refreshTrackList();
    });
    beatWindow->setOnMixerMuteToggled([this](int track, bool muted) {
        engine.setTrackMute(track, muted);
        refreshTrackList();
    });

    // Beat window transport wiring
    beatWindow->onPlay = [this]() {
        // PAT mode: always loop from position 0
        if (beatWindow && beatWindow->isPatMode())
            transportState.setPositionSamples(0, true);
        engine.play();
        workspaceToolbar.setPlayState(true, engine.isRecording());
        if (beatWindow) beatWindow->setPlayState(true, engine.isRecording());
    };
    beatWindow->onStop = [this]() {
        bool wasRecording = engine.isRecording() || engine.getTransportState().isRecording();
        engine.stop();
        workspaceToolbar.setPlayState(false, false);
        if (beatWindow) beatWindow->setPlayState(false, false);
        if (wasRecording) { refreshTrackList(); arrangementModel.sendChangeMessage(); }
    };
    beatWindow->onRecord = [this]() {
        bool wasRecording = engine.isRecording() || engine.getTransportState().isRecording();
        engine.toggleRecord();
        workspaceToolbar.setPlayState(engine.getTransportState().isPlaying(), engine.isRecording());
        if (beatWindow) beatWindow->setPlayState(engine.getTransportState().isPlaying(), engine.isRecording());
        if (wasRecording && !engine.isRecording()) { refreshTrackList(); arrangementModel.sendChangeMessage(); }
    };
    beatWindow->onReturnToZero = [this]() {
        bool wasRecording = engine.isRecording() || engine.getTransportState().isRecording();
        engine.stop();
        transportState.setPositionSamples(0, true);
        workspaceToolbar.setPlayState(false, false);
        if (beatWindow) beatWindow->setPlayState(false, false);
        if (wasRecording) { refreshTrackList(); arrangementModel.sendChangeMessage(); }
    };
    beatWindow->onTempoChanged = [this](int bpm) {
        engine.getSession().setTempo(static_cast<double>(bpm));
        transportState.setTempo(static_cast<double>(bpm));
        workspaceToolbar.setTempo(bpm);
        updateStatusMessage("Tempo: " + juce::String(bpm) + " BPM");
    };

    beatWindow->onShowMixer = [this]() { popOutMixer(); };

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
    // Wire the engine so the dock can show real channels and live level meters
    bottomDock.setEngine(engine);
    addAndMakeVisible(bottomDockToggleBar);
    bottomDockToggleBar.isHorizontal = true;
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
        bool wasRecording = engine.isRecording() || engine.getTransportState().isRecording();
        engine.stop();
        transportState.setPositionSamples(0, true);
        workspaceToolbar.setPlayState(false, false);
        if (wasRecording)
        {
            refreshTrackList();
            arrangementModel.sendChangeMessage();
            updateStatusMessage("Recording stopped. Returned to zero.");
        }
        else
        {
            updateStatusMessage("Returned to zero.");
        }
    };

    workspaceToolbar.onPlay = [this] {
        engine.play();
        workspaceToolbar.setPlayState(true, engine.isRecording());
        updateStatusMessage("Playback engaged.");
    };

    workspaceToolbar.onStop = [this] {
        // Check both flags: recordingActive (file write in progress) and
        // transportState.isRecording() (user intended to record, even if file failed to open)
        bool wasRecording = engine.isRecording() || engine.getTransportState().isRecording();
        engine.stop();
        workspaceToolbar.setPlayState(false, false);
        if (wasRecording)
        {
            refreshTrackList();
            arrangementModel.sendChangeMessage();
            auto f = engine.getLastRecordingFile();
            updateStatusMessage("Recorded: " + (f.existsAsFile() ? f.getFullPathName() : "Stopped."));
        }
        else
        {
            updateStatusMessage("Playback stopped.");
        }
    };

    workspaceToolbar.onRecord = [this] {
        bool wasRecording = engine.isRecording() || engine.getTransportState().isRecording();
        engine.toggleRecord();
        workspaceToolbar.setPlayState(engine.getTransportState().isPlaying(), engine.isRecording());
        if (wasRecording && !engine.isRecording())
        {
            // Recording just stopped — add the clip to the timeline
            refreshTrackList();
            arrangementModel.sendChangeMessage();
            auto f = engine.getLastRecordingFile();
            updateStatusMessage("Recorded: " + (f.existsAsFile() ? f.getFullPathName() : "unknown path"));
        }
        else
        {
            updateStatusMessage(engine.isRecording() ? "Recording active." : "Record stopped.");
        }
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
        if (beatWindow) beatWindow->setBpm(static_cast<double>(bpm));
        updateStatusMessage("Tempo: " + juce::String(bpm) + " BPM");
    };
    // Sync initial BPM to beat window
    if (beatWindow) beatWindow->setBpm(engine.getSession().getTempo());

    // initial playback indicator and toggle hookup
    workspaceToolbar.setPlaybackState(engine.getSession().isPreviewPlaybackEnabled(), arrangementModel.hasAlignmentPreview());
    workspaceToolbar.onTogglePreview = [this](bool wantsPreview) {
        engine.getSession().setPreviewPlaybackEnabled(wantsPreview);
        arrangementModel.sendChangeMessage();
        updateStatusMessage(wantsPreview ? "Preview audio enabled." : "Playing original audio.");
    };

    // Master clip indicator — toolbar polls engine at ~12Hz, lights red on output > 0 dBFS
    workspaceToolbar.onPollMasterClip = [this]() {
        return engine.readAndClearMasterClip();
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
    workspaceToolbar.onHZoomChanged = [this](int dir) {
        if (editWindow) editWindow->zoomHorizontal(dir);
    };
    workspaceToolbar.onVZoomChanged = [this](int dir) {
        if (editWindow) editWindow->zoomVertical(dir);
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

    // Register for keyboard shortcuts so they fire regardless of which
    // child component currently holds keyboard focus.
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

void MainComponent::parentHierarchyChanged()
{
    // Once we have a top-level window, register as a key listener on it so
    // shortcuts fire regardless of which child component holds keyboard focus.
    if (auto* tlw = getTopLevelComponent())
        tlw->addKeyListener(this);
}

MainComponent::~MainComponent()
{
    if (auto* tlw = getTopLevelComponent())
        tlw->removeKeyListener(this);
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
        addAndMakeVisible(*mixerWindow);
        mixerWindow->setVisible(true);
        mixerWindow->refresh();
        floatingMixer = std::make_unique<FloatingPanelWindow>(
            "Nova Studio — Mixer", mixerWindow.get(),
            [this](FloatingPanelWindow* w) {
                juce::ignoreUnused(w);
                redockMixer();
            });
        const auto screen = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
        const int w = juce::jlimit(900, screen.getWidth(),  juce::jmax(900, getTopLevelComponent()->getWidth()));
        const int h = juce::jlimit(500, screen.getHeight(), 820);
        floatingMixer->setSize(w, h);
        floatingMixer->centreWithSize(w, h);
        floatingMixer->setResizable(true, false);
        floatingMixer->addKeyListener(this); // forward shortcuts to MainComponent
    }
}

void MainComponent::redockMixer()
{
    if (floatingMixer) floatingMixer->removeKeyListener(this);
    floatingMixer.reset();
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
        const auto screen = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
        const int w = juce::jlimit(800, screen.getWidth(),  juce::jmax(800, getTopLevelComponent()->getWidth()));
        const int h = juce::jlimit(500, screen.getHeight(), 700);
        floatingBeat->setSize(w, h);
        floatingBeat->centreWithSize(w, h);
        floatingBeat->setResizable(true, false);
        floatingBeat->addKeyListener(this);
    }
}

void MainComponent::redockBeat()
{
    if (floatingBeat) floatingBeat->removeKeyListener(this);
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
    floatingBrowser->addKeyListener(this);
    resized();
}

void MainComponent::redockBrowser()
{
    if (floatingBrowser) floatingBrowser->removeKeyListener(this);
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

    // Bottom dock: remove from bottom first, then toggle bar sits just above it
    if (!bottomDockCollapsed)
    {
        const int dockH = juce::jmax(110, getHeight() / 4);
        bottomDock.setBounds(area.removeFromBottom(dockH));
    }
    else
    {
        bottomDock.setBounds({});
    }
    // Toggle bar always sits between the arrangement area and the bottom dock
    bottomDockToggleBar.setBounds(area.removeFromBottom(14));

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

    // Mixer mode: full screen when floating (popped out).
    // When docked (in bottomDock) its bounds are managed by BottomDockPanel::resized().
    if (mixerWindow && floatingMixer == nullptr && mixerWindow->getParentComponent() == this)
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

void MainComponent::promptClipFadeLength(bool isFadeIn)
{
    auto* clip = arrangementModel.getSelectedClip();
    if (clip == nullptr || clip->isMidi)
    {
        updateStatusMessage(juce::String(isFadeIn ? "Fade In" : "Fade Out") + ": no audio clip selected");
        return;
    }

    const double sr = engine.getTransportState().getSampleRate();
    const int64_t currentSamples = isFadeIn ? clip->fadeInSamples : clip->fadeOutSamples;
    const int currentMs = sr > 0.0 ? (int) juce::roundToInt((double) currentSamples / sr * 1000.0) : 0;

    auto* dlg = new juce::AlertWindow(isFadeIn ? "Fade In" : "Fade Out",
                                      "Fade length in milliseconds:",
                                      juce::MessageBoxIconType::NoIcon);
    dlg->addTextEditor("ms", juce::String(currentMs), "Length (ms):");
    dlg->addButton("Apply",  1, juce::KeyPress(juce::KeyPress::returnKey));
    dlg->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dlg->setColour(juce::AlertWindow::backgroundColourId, juce::Colour::fromRGB(18, 20, 28));
    dlg->setColour(juce::AlertWindow::textColourId, juce::Colours::white);
    dlg->enterModalState(true,
        juce::ModalCallbackFunction::create([this, dlg, isFadeIn, sr](int result) mutable
        {
            if (result == 1 && sr > 0.0)
            {
                const int ms = juce::jmax(0, dlg->getTextEditorContents("ms").getIntValue());
                const int64_t samples = (int64_t) juce::roundToInt((double) ms / 1000.0 * sr);
                const bool ok = isFadeIn ? arrangementModel.setSelectedClipFadeIn(samples)
                                         : arrangementModel.setSelectedClipFadeOut(samples);
                updateStatusMessage(ok ? (juce::String(isFadeIn ? "Fade in" : "Fade out") + " set to " + juce::String(ms) + " ms")
                                       : juce::String(isFadeIn ? "Fade In" : "Fade Out") + ": could not apply");
                refreshTrackList();
            }
            delete dlg;
        }));
}

void MainComponent::promptBounceClipToAudio()
{
    auto* clip = arrangementModel.getSelectedClip();
    if (clip == nullptr || clip->isMidi)
    {
        updateStatusMessage("Bounce Clip: no audio clip selected");
        return;
    }

    if (clip->locked)
    {
        updateStatusMessage("Bounce Clip: clip is locked");
        return;
    }

    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,
        "Bounce Clip to Audio",
        "This will render the clip's gain and fade in/out into a new audio file, "
        "and reset its gain/fades to neutral. The clip can still be edited afterwards.",
        "Bounce", "Cancel", this,
        juce::ModalCallbackFunction::create([this](int result)
        {
            if (result != 1)
                return;
            if (arrangementModel.bounceSelectedClipToAudio())
                updateStatusMessage("Bounced clip to audio");
            else
                updateStatusMessage("Bounce Clip: nothing to bounce (no gain/fade applied)");
            refreshTrackList();
        }));
}

void MainComponent::promptExportMix()
{
    if (engine.getTrackCount() <= 0)
    {
        updateStatusMessage("Export Mix: session has no tracks");
        return;
    }

    auto chooser = std::make_shared<juce::FileChooser>(
        "Export Mix As...",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(engine.getSession().getName() + " Mix.wav"),
        "*.wav");
    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File())
                return;
            if (f.getFileExtension().isEmpty())
                f = f.withFileExtension("wav");

            updateStatusMessage("Exporting mix...");
            if (arrangementModel.exportMixToFile(f))
                updateStatusMessage("Exported mix to " + f.getFileName());
            else
                updateStatusMessage("Export Mix: nothing to render (no audio clips found)");
        });
}

void MainComponent::promptExportStems()
{
    if (engine.getTrackCount() <= 0)
    {
        updateStatusMessage("Export Stems: session has no tracks");
        return;
    }

    auto chooser = std::make_shared<juce::FileChooser>(
        "Choose a folder for stems",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto folder = fc.getResult();
            if (!folder.isDirectory())
                return;

            updateStatusMessage("Exporting stems...");
            int exported = 0;
            for (int t = 0; t < engine.getSession().getNumTracks(); ++t)
            {
                const auto& track = engine.getSession().getTrack(t);
                if (track.clips.isEmpty())
                    continue;

                auto safeName = track.name.isNotEmpty() ? track.name : ("Track " + juce::String(t + 1));
                safeName = safeName.retainCharacters(
                    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ -_").trim();
                if (safeName.isEmpty())
                    safeName = "Track " + juce::String(t + 1);

                auto destFile = folder.getChildFile(safeName + ".wav");
                if (arrangementModel.exportTrackToFile(t, destFile))
                    ++exported;
            }

            if (exported > 0)
                updateStatusMessage("Exported " + juce::String(exported) + " stem(s) to " + folder.getFileName());
            else
                updateStatusMessage("Export Stems: nothing to render (no audio clips found)");
        });
}

void MainComponent::showAboutDialog()
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
        "About Nova Studio",
        juce::String("Nova Studio\n"
                     "A digital audio workstation built on JUCE.\n\n")
            + juce::SystemStats::getJUCEVersion() + "\n"
            + "(c) Nova Studio");
}

void MainComponent::showKeyboardShortcutsDialog()
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
        "Keyboard Shortcuts",
        "Transport\n"
        "  Space — Play / Stop\n"
        "  Return — Return to start\n\n"
        "Editing\n"
        "  Ctrl/Cmd+Z — Undo\n"
        "  Ctrl/Cmd+Shift+Z — Redo\n"
        "  Ctrl/Cmd+X — Cut\n"
        "  Ctrl/Cmd+C — Copy\n"
        "  Ctrl/Cmd+V — Paste\n"
        "  Ctrl/Cmd+D — Duplicate\n"
        "  Ctrl/Cmd+A — Select All\n"
        "  Delete / Backspace — Delete selection\n\n"
        "View\n"
        "  Ctrl/Cmd+1 — Edit Window\n"
        "  Ctrl/Cmd+2 — Mixer\n");
}

void MainComponent::showManualDialog()
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
        "Nova Studio Manual",
        "Nova Studio is organized into a few core windows:\n\n"
        "  - Edit Window — arrange and edit audio/MIDI clips on the timeline\n"
        "  - Mixer — control track volume, pan, sends and EQ\n"
        "  - Piano Roll — compose and edit MIDI notes (click the on-screen keys to audition pitches)\n"
        "  - Browser — import audio files into your session\n\n"
        "Use the File menu to import audio and export your mix or stems, "
        "the Track menu to add or duplicate tracks, and the Clip menu for "
        "non-destructive editing tools such as Normalize, Reverse, Fades, "
        "Bounce to Audio and Consolidate.\n\n"
        "See Help > Keyboard Shortcuts for a full list of shortcuts.");
}

void MainComponent::promptSessionSettings()
{
    auto& session = engine.getSession();

    auto* dlg = new juce::AlertWindow("Session Settings",
                                      "Project-wide settings:",
                                      juce::MessageBoxIconType::NoIcon);
    dlg->addTextEditor("name",  session.getName(), "Project name:");
    dlg->addTextEditor("tempo", juce::String(session.getTempo(), 2), "Tempo (BPM):");
    dlg->addTextEditor("samplerate", juce::String((int) session.getSampleRate()), "Sample rate (Hz):");
    dlg->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    dlg->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dlg->setColour(juce::AlertWindow::backgroundColourId, juce::Colour::fromRGB(18, 20, 28));
    dlg->setColour(juce::AlertWindow::textColourId, juce::Colours::white);
    dlg->enterModalState(true,
        juce::ModalCallbackFunction::create([this, dlg](int result) mutable
        {
            if (result == 1)
            {
                auto& s = engine.getSession();
                const juce::String name = dlg->getTextEditorContents("name").trim();
                if (name.isNotEmpty())
                    s.setName(name);

                const double tempo = dlg->getTextEditorContents("tempo").getDoubleValue();
                if (tempo > 0.0)
                    s.setTempo(juce::jlimit(20.0, 999.0, tempo));

                const double sr = dlg->getTextEditorContents("samplerate").getDoubleValue();
                if (sr > 0.0)
                    s.setSampleRate(sr);

                arrangementModel.sendChangeMessage();
                updateStatusMessage("Session settings updated");
            }
            delete dlg;
        }), false);
}

void MainComponent::showPluginManager()
{
    if (pluginManagerWindow == nullptr)
    {
        if (pluginListProperties == nullptr)
        {
            juce::PropertiesFile::Options options;
            options.applicationName     = "NovaStudio";
            options.filenameSuffix      = "settings";
            options.osxLibrarySubFolder = "Application Support";
            options.folderName          = "NovaStudio";
            pluginListProperties = std::make_unique<juce::PropertiesFile>(options);
        }

        const auto deadMansPedal = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                       .getChildFile("NovaStudio")
                                       .getChildFile("PluginDeadMansPedal.txt");

        pluginManagerWindow = std::make_unique<NovaStudioUI::PluginManagerWindow>(
            engine.getPluginFormatManager(), engine.getKnownPluginList(), deadMansPedal, pluginListProperties.get());
    }
    else
    {
        pluginManagerWindow->setVisible(true);
        pluginManagerWindow->toFront(true);
    }
}

void MainComponent::scanForPlugins(bool clearExistingFirst)
{
    if (clearExistingFirst)
        engine.getKnownPluginList().clear();

    juce::FileSearchPath searchPath;
    for (auto* format : engine.getPluginFormatManager().getFormats())
        searchPath.addPath(format->getDefaultLocationsToSearch());

    int numFound = 0;
    for (auto* format : engine.getPluginFormatManager().getFormats())
    {
        juce::PluginDirectoryScanner scanner(engine.getKnownPluginList(), *format, searchPath, true,
                                             juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                                 .getChildFile("NovaStudio")
                                                 .getChildFile("PluginDeadMansPedal.txt"));
        juce::String pluginBeingScanned;
        while (scanner.scanNextFile(true, pluginBeingScanned))
            ++numFound;
    }

    updateStatusMessage(clearExistingFirst
        ? ("Rescanned plugins — found " + juce::String(numFound))
        : ("Scanned plugins — found " + juce::String(numFound)));
}

namespace
{
    bool looksLikeVocalTrackName(const juce::String& name)
    {
        static const char* keywords[] = { "vocal", "vox", "vx", "bgv", "choir", "singer", "harmony", "bg vox" };
        const auto lower = name.toLowerCase();
        for (auto* kw : keywords)
            if (lower.contains(kw))
                return true;
        return false;
    }
}

void MainComponent::findClippingTracks()
{
    auto hits = arrangementModel.findClippingClips();
    if (hits.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
            "Find Clipping Tracks",
            "No clipping detected — every audio clip peaks below 0 dBFS.");
        return;
    }

    juce::StringArray lines;
    for (auto& p : hits)
    {
        const auto& track = engine.getSession().getTrack(p.x);
        const auto& clip  = track.clips.getReference(p.y);
        lines.add(track.name + " — " + clip.file.getFileNameWithoutExtension());
    }

    arrangementModel.selectClip(hits.getFirst().x, hits.getFirst().y);
    refreshTrackList();

    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
        "Find Clipping Tracks",
        "Found " + juce::String(hits.size()) + " clipping clip(s):\n\n"
            + lines.joinIntoString("\n")
            + "\n\nThe first one has been selected — try Clip > Normalize, "
              "lower its gain, or add a Fade to tame the peak.");
}

void MainComponent::organizeSession()
{
    struct Category { const char* keyword; juce::Colour colour; const char* group; };
    static const Category categories[] = {
        { "kick",   juce::Colours::orangered, "Drums"   },
        { "snare",  juce::Colours::orangered, "Drums"   },
        { "drum",   juce::Colours::orangered, "Drums"   },
        { "perc",   juce::Colours::orange,    "Drums"   },
        { "bass",   juce::Colours::seagreen,  "Bass"    },
        { "vocal",  juce::Colours::royalblue, "Vocals"  },
        { "vox",    juce::Colours::royalblue, "Vocals"  },
        { "guitar", juce::Colours::goldenrod, "Guitars" },
        { "gtr",    juce::Colours::goldenrod, "Guitars" },
        { "key",    juce::Colours::orchid,    "Keys"    },
        { "synth",  juce::Colours::orchid,    "Keys"    },
        { "piano",  juce::Colours::orchid,    "Keys"    },
    };

    int changed = 0;
    for (int t = 0; t < engine.getSession().getNumTracks(); ++t)
    {
        auto& track = engine.getSession().getTrack(t);
        const auto lower = track.name.toLowerCase();
        for (auto& cat : categories)
        {
            if (lower.contains(cat.keyword))
            {
                track.colour    = cat.colour;
                track.groupName = cat.group;
                ++changed;
                break;
            }
        }
    }

    if (changed > 0)
    {
        refreshTrackList();
        arrangementModel.sendChangeMessage();
        updateStatusMessage("Organize Session: colour-coded and grouped " + juce::String(changed) + " track(s) by instrument type");
    }
    else
    {
        updateStatusMessage("Organize Session: no recognizable instrument names found to group");
    }
}

void MainComponent::createVocalBuses()
{
    juce::Array<int> vocalTracks;
    for (int t = 0; t < engine.getSession().getNumTracks(); ++t)
        if (looksLikeVocalTrackName(engine.getSession().getTrack(t).name))
            vocalTracks.add(t);

    if (vocalTracks.isEmpty())
    {
        updateStatusMessage("Create Vocal Buses: no tracks named like vocals were found");
        return;
    }

    engine.addTrack("Vocal Bus", NovaStudio::TrackType::Aux);
    auto& bus = engine.getSession().getTrack(engine.getSession().getNumTracks() - 1);
    bus.outputBus = "Main Out";
    bus.colour    = juce::Colours::mediumpurple;

    for (int t : vocalTracks)
        engine.getSession().getTrack(t).outputBus = bus.name;

    refreshTrackList();
    arrangementModel.sendChangeMessage();
    updateStatusMessage("Routed " + juce::String(vocalTracks.size()) + " vocal track(s) to a new Vocal Bus");
}

void MainComponent::alignBackgroundVocals()
{
    juce::Array<int> vocalTracks;
    for (int t = 0; t < engine.getSession().getNumTracks(); ++t)
        if (looksLikeVocalTrackName(engine.getSession().getTrack(t).name) && !engine.getSession().getTrack(t).clips.isEmpty())
            vocalTracks.add(t);

    if (vocalTracks.size() < 2)
    {
        updateStatusMessage("Align Background Vocals: need at least 2 vocal tracks with audio clips");
        return;
    }

    const int guideTrack = vocalTracks.getFirst();
    arrangementModel.clearGuideClip();
    if (!arrangementModel.setGuideClip(guideTrack, 0))
    {
        updateStatusMessage("Align Background Vocals: couldn't set a guide clip");
        return;
    }

    int numTargets = 0;
    for (int i = 1; i < vocalTracks.size(); ++i)
        if (arrangementModel.toggleAlignTargetClip(vocalTracks.getUnchecked(i), 0))
            ++numTargets;

    if (numTargets == 0)
    {
        arrangementModel.clearGuideClip();
        updateStatusMessage("Align Background Vocals: no target clips available to align");
        return;
    }

    NovaStudio::ArrangementModel::AlignSettings settings;
    const bool ok = arrangementModel.alignSelectedTargetsToGuide(settings, true);
    const juce::String guideName = engine.getSession().getTrack(guideTrack).name;
    arrangementModel.clearGuideClip();

    refreshTrackList();
    updateStatusMessage(ok
        ? ("Aligned " + juce::String(numTargets) + " background vocal clip(s) to \"" + guideName + "\"")
        : "Align Background Vocals: alignment failed");
}

void MainComponent::exportForProTools()
{
    if (engine.getTrackCount() <= 0)
    {
        updateStatusMessage("Export For Pro Tools: session has no tracks");
        return;
    }

    auto chooser = std::make_shared<juce::FileChooser>(
        "Choose a folder to export for Pro Tools",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto folder = fc.getResult();
            if (!folder.isDirectory())
                return;

            updateStatusMessage("Exporting for Pro Tools...");

            // Per-track stems, plus a plain-text session manifest describing
            // each clip's position — a simple, universally-readable interchange
            // format that can be used to rebuild the session in Pro Tools.
            int exported = 0;
            juce::StringArray manifest;
            manifest.add("Nova Studio session export — " + engine.getSession().getName());
            manifest.add("Tempo: " + juce::String(engine.getSession().getTempo(), 2) + " BPM");
            manifest.add("Sample rate: " + juce::String((int) engine.getSession().getSampleRate()) + " Hz");
            manifest.add("");

            for (int t = 0; t < engine.getSession().getNumTracks(); ++t)
            {
                const auto& track = engine.getSession().getTrack(t);
                if (track.clips.isEmpty())
                    continue;

                auto safeName = track.name.retainCharacters(
                    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ -_").trim();
                if (safeName.isEmpty())
                    safeName = "Track " + juce::String(t + 1);

                auto destFile = folder.getChildFile(safeName + ".wav");
                if (arrangementModel.exportTrackToFile(t, destFile))
                    ++exported;

                const double sr = engine.getSession().getSampleRate() > 0.0 ? engine.getSession().getSampleRate() : 44100.0;
                manifest.add("Track: " + track.name + "  (file: " + destFile.getFileName() + ")");
                for (auto& clip : track.clips)
                {
                    if (clip.isMidi)
                        continue;
                    manifest.add(juce::String::formatted("    clip \"%s\"  start %.3fs  length %.3fs",
                        clip.file.getFileNameWithoutExtension().toRawUTF8(),
                        (double) clip.startSample / sr,
                        (double) clip.lengthSamples / sr));
                }
            }

            folder.getChildFile("Session Info.txt").replaceWithText(manifest.joinIntoString("\n"));

            if (exported > 0)
                updateStatusMessage("Exported " + juce::String(exported) + " track(s) and a session manifest to " + folder.getFileName());
            else
                updateStatusMessage("Export For Pro Tools: nothing to render (no audio clips found)");
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// One-Click Mix — Ozuna / Rauw Alejandro Latin Urban style
//
// Bus hierarchy:
//   Lead vocals  → Lead Vocal Bus → Vocal Bus → Master Bus → Out
//   Doubles      → Lead Vocal Bus → Vocal Bus → Master Bus → Out
//   Harmonies/BGV→ Vocal Bus ──────────────── → Master Bus → Out
//   Music/beat   → Music Bus ──────────────── → Master Bus → Out
//   Sends:         Reverb Bus (Space By Nova) → Master Bus → Out
//                  Delay Bus  (Nova Delay)    → Master Bus → Out
// ─────────────────────────────────────────────────────────────────────────────
void MainComponent::runAiMix()
{
    auto& session = engine.getSession();
    const int numTracks = session.getNumTracks();

    if (numTracks == 0)
    {
        updateStatusMessage("One-Click Mix: session has no tracks");
        return;
    }

    // ── 1. Classify tracks ──────────────────────────────────────────────────
    enum class Role { LeadVocal, DoubleTrack, BackgroundVocal, Music, Unknown };

    auto classifyTrack = [](const juce::String& name) -> Role
    {
        const auto n = name.toLowerCase();
        if (n.contains("lead") || n.contains("main vox") || n.contains("main vocal"))
            return Role::LeadVocal;
        if (n.contains("double") || n.contains("dbl") || n.contains("ad lib") || n.contains("adlib"))
            return Role::DoubleTrack;
        if (n.contains("harmony") || n.contains("harm") || n.contains("bgv") || n.contains("bg vox")
            || n.contains("background") || n.contains("choir"))
            return Role::BackgroundVocal;
        if (n.contains("vocal") || n.contains("vox") || n.contains("vx") || n.contains("singer"))
            return Role::LeadVocal;
        if (n.contains("beat") || n.contains("music") || n.contains("inst") || n.contains("track")
            || n.contains("bass") || n.contains("drum") || n.contains("piano") || n.contains("synth")
            || n.contains("guitar") || n.contains("perc") || n.contains("keys") || n.contains("pad")
            || n.contains("arp") || n.contains("horn") || n.contains("brass") || n.contains("strings"))
            return Role::Music;
        return Role::Unknown;
    };

    bool hasAnyVocal = false;
    for (int t = 0; t < numTracks; ++t)
    {
        auto r = classifyTrack (session.getTrack(t).name);
        if (r == Role::LeadVocal || r == Role::DoubleTrack || r == Role::BackgroundVocal)
            hasAnyVocal = true;
    }
    bool useFallback = !hasAnyVocal;
    int firstAudioTrack = -1;
    for (int t = 0; t < numTracks; ++t)
        if (session.getTrack(t).type == NovaStudio::TrackType::Audio && firstAudioTrack < 0)
            firstAudioTrack = t;

    // ── 2. Find or create all 6 buses ───────────────────────────────────────
    // Bus indices; -1 = needs creation
    int leadVocalBusIdx = -1, vocalBusIdx  = -1, musicBusIdx   = -1;
    int reverbBusIdx    = -1, delayBusIdx  = -1, masterBusIdx  = -1;

    for (int t = 0; t < session.getNumTracks(); ++t)
    {
        if (session.getTrack(t).type != NovaStudio::TrackType::Aux) continue;
        const auto n = session.getTrack(t).name.toLowerCase();
        if      (n.contains("lead vocal bus"))  leadVocalBusIdx = t;
        else if (n.contains("vocal bus"))        vocalBusIdx    = t;
        else if (n.contains("music bus"))        musicBusIdx    = t;
        else if (n.contains("reverb"))           reverbBusIdx   = t;
        else if (n.contains("delay"))            delayBusIdx    = t;
        else if (n.contains("master bus"))       masterBusIdx   = t;
    }

    // Helper: create an Aux bus, return its index
    auto makeBus = [&](const juce::String& busName,
                       juce::Colour colour,
                       float volumeDb,
                       const juce::String& outputBus,
                       const juce::String& chain) -> int
    {
        engine.addTrack (busName, NovaStudio::TrackType::Aux);
        int idx = session.getNumTracks() - 1;
        auto& b = session.getTrack(idx);
        b.colour    = colour;
        b.volumeDb  = volumeDb;
        b.outputBus = outputBus;
        b.groupName = chain;
        return idx;
    };

    // Create Master Bus first — everything routes into it
    if (masterBusIdx < 0)
        masterBusIdx = makeBus ("Master Bus",
                                juce::Colour::fromRGB(200, 160, 60),
                                -18.0f, "Main Out",
                                "Nova Master → Nova Apex");

    const juce::String& masterBusName = session.getTrack(masterBusIdx).name;

    // Reverb and Delay return to Master Bus
    if (reverbBusIdx < 0)
        reverbBusIdx = makeBus ("Reverb Bus (Space By Nova)",
                                juce::Colour::fromRGB(80, 60, 140),
                                -6.0f, masterBusName,
                                "Space By Nova");

    if (delayBusIdx < 0)
        delayBusIdx = makeBus ("Delay Bus (Nova Delay)",
                               juce::Colour::fromRGB(60, 100, 140),
                               -9.0f, masterBusName,
                               "Nova Delay");

    // Music Bus → Master Bus  (Nova Console→Curve→Level→Clip)
    if (musicBusIdx < 0)
        musicBusIdx = makeBus ("Music Bus",
                               juce::Colour::fromRGB(50, 150, 80),
                               -3.0f, masterBusName,
                               "Nova Console → Nova Curve → Nova Level → Nova Clip");

    // Vocal Bus → Master Bus  (Nova Level glue + Nova Clip)
    if (vocalBusIdx < 0)
        vocalBusIdx = makeBus ("Vocal Bus",
                               juce::Colour::fromRGB(80, 120, 220),
                               -3.0f, masterBusName,
                               "Nova Level → Nova Clip");

    // Lead Vocal Bus → Vocal Bus  (Nova Aura + Nova Heat subtle saturation)
    const juce::String& vocalBusName = session.getTrack(vocalBusIdx).name;
    if (leadVocalBusIdx < 0)
        leadVocalBusIdx = makeBus ("Lead Vocal Bus",
                                   juce::Colour::fromRGB(60, 100, 200),
                                   -3.0f, vocalBusName,
                                   "Nova Aura → Nova Heat");

    const juce::String& leadVocalBusName = session.getTrack(leadVocalBusIdx).name;
    const juce::String& musicBusName     = session.getTrack(musicBusIdx).name;

    // ── 3. Apply settings to audio tracks ───────────────────────────────────
    juce::StringArray actions;
    int doubleCount  = 0;
    int harmonyCount = 0;

    for (int t = 0; t < numTracks; ++t)
    {
        auto& track = session.getTrack(t);
        if (track.type != NovaStudio::TrackType::Audio)
            continue;

        Role role = classifyTrack (track.name);
        if (useFallback)
            role = (t == firstAudioTrack) ? Role::LeadVocal : Role::Music;

        // Clear previous send assignments
        for (int s = 0; s < 6; ++s)
        {
            track.sendBusIndex[s] = -1;
            track.sendLevels[s]   = -100.0f;
            track.sendPreFader[s] = false;
        }

        switch (role)
        {
            case Role::LeadVocal:
            {
                track.volumeDb  = -12.0f;
                track.pan       = 0.0f;
                track.colour    = juce::Colour::fromRGB(60, 120, 220);
                track.outputBus = leadVocalBusName;
                // Reverb send (plate tail) + Delay send (slapback)
                track.sendBusIndex[0] = reverbBusIdx;  track.sendLevels[0] = -18.0f;
                track.sendBusIndex[1] = delayBusIdx;   track.sendLevels[1] = -22.0f;
                track.groupName = "Lead Vocals | Nova Clean→Silk→Pitch→Console→Tone→Curve→Level→Clip";
                actions.add ("Lead Vocal \"" + track.name + "\": -12 dB, center → Lead Vocal Bus");
                break;
            }

            case Role::DoubleTrack:
            {
                ++doubleCount;
                float panDir = (doubleCount % 2 == 1) ? 0.25f : -0.25f;
                track.volumeDb  = -16.0f;
                track.pan       = panDir;
                track.colour    = juce::Colour::fromRGB(80, 140, 220);
                track.outputBus = leadVocalBusName;
                track.sendBusIndex[0] = reverbBusIdx;  track.sendLevels[0] = -15.0f;
                track.sendBusIndex[1] = delayBusIdx;   track.sendLevels[1] = -24.0f;
                track.groupName = "Lead Vocals | Nova Clean→Silk→Pitch→Console→Level→Clip";
                actions.add ("Double/Ad-lib \"" + track.name + "\": -16 dB, "
                             + juce::String(panDir > 0 ? "R" : "L") + "25% → Lead Vocal Bus");
                break;
            }

            case Role::BackgroundVocal:
            {
                ++harmonyCount;
                float panDir = (harmonyCount % 2 == 1) ? 0.40f : -0.40f;
                track.volumeDb  = -18.0f;
                track.pan       = panDir;
                track.colour    = juce::Colour::fromRGB(100, 160, 240);
                track.outputBus = vocalBusName;          // bypass Lead Vocal Bus
                track.sendBusIndex[0] = reverbBusIdx;  track.sendLevels[0] = -12.0f;
                track.sendBusIndex[1] = delayBusIdx;   track.sendLevels[1] = -30.0f;
                track.groupName = "BGV/Harmony | Nova Clean→Silk→Pitch→Console→Curve→Level→Clip";
                actions.add ("Harmony/BGV \"" + track.name + "\": -18 dB, "
                             + juce::String(panDir > 0 ? "R" : "L") + "40% → Vocal Bus");
                break;
            }

            case Role::Music:
            case Role::Unknown:
            {
                track.volumeDb  = -18.0f;
                track.pan       = 0.0f;
                track.colour    = juce::Colour::fromRGB(60, 160, 90);
                track.outputBus = musicBusName;
                // Light glue reverb on music
                track.sendBusIndex[0] = reverbBusIdx;  track.sendLevels[0] = -28.0f;
                track.groupName = "Music | Nova Console → Nova Curve → Nova Level → Nova Clip";
                actions.add ("Music \"" + track.name + "\": -18 dB, center → Music Bus");
                break;
            }
        }
    }

    // ── 4. Refresh UI ───────────────────────────────────────────────────────
    refreshTrackList();
    arrangementModel.sendChangeMessage();

    // ── 5. Summary dialog ───────────────────────────────────────────────────
    juce::StringArray report;
    report.add ("One-Click Mix — Ozuna / Rauw Alejandro Latin Urban style");
    report.add ("");
    report.add ("Tracks configured:");
    for (auto& a : actions)
        report.add ("  • " + a);
    report.add ("");
    report.add ("Bus hierarchy created:");
    report.add ("  Lead Vocal Bus  → Vocal Bus → Master Bus → Out");
    report.add ("  Vocal Bus (BGV) → ──────────→ Master Bus → Out");
    report.add ("  Music Bus       → ──────────→ Master Bus → Out");
    report.add ("  Reverb Bus      → ──────────→ Master Bus → Out");
    report.add ("  Delay Bus       → ──────────→ Master Bus → Out");
    report.add ("");
    report.add ("Nova Suite plugin chains are shown in each track's group label.");
    report.add ("Load the plugins manually to complete the signal chain.");

    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        "One-Click Mix — Nova AI",
        report.joinIntoString ("\n"));

    updateStatusMessage ("One-Click Mix applied: " + juce::String(actions.size())
                         + " tracks configured across 6 buses");
}

void MainComponent::runNovaAssistant()
{
    auto& session = engine.getSession();
    const int numTracks = session.getNumTracks();

    int numAudio = 0, numMidi = 0, numAux = 0;
    int numVocalTracks = 0;
    for (int t = 0; t < numTracks; ++t)
    {
        const auto& track = session.getTrack(t);
        switch (track.type)
        {
            case NovaStudio::TrackType::Audio: ++numAudio; break;
            case NovaStudio::TrackType::Midi:  ++numMidi;  break;
            case NovaStudio::TrackType::Aux:   ++numAux;   break;
            default: break;
        }
        if (looksLikeVocalTrackName(track.name))
            ++numVocalTracks;
    }

    const auto clippingHits = arrangementModel.findClippingClips();

    juce::StringArray report;
    report.add("Session \"" + session.getName() + "\" — " + juce::String(numTracks) + " track(s) "
               "(" + juce::String(numAudio) + " audio, " + juce::String(numMidi) + " MIDI/instrument, " + juce::String(numAux) + " aux)");
    report.add("");

    if (clippingHits.isEmpty())
        report.add("- No clipping detected.");
    else
        report.add("- " + juce::String(clippingHits.size()) + " clip(s) are clipping — try AI > Find Clipping Tracks.");

    if (numVocalTracks >= 2)
        report.add("- " + juce::String(numVocalTracks) + " vocal-style tracks found — try AI > Align Background Vocals or Create Vocal Buses.");
    else if (numVocalTracks == 1)
        report.add("- 1 vocal-style track found.");

    if (numTracks >= 4)
        report.add("- Try AI > Organize Session to colour-code and group your tracks by instrument.");

    report.add("- Use File > Export Mix/Stems or AI > Export For Pro Tools when you're ready to deliver.");

    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Nova Assistant", report.joinIntoString("\n"));
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
    // Cmd+= → Open / bring Mixer to front
    if ((isCmd || isCtrl) && key.getKeyCode() == '=')
    {
        setWorkspaceMode(1);
        updateStatusMessage("Mix window");
        return true;
    }
    // Cmd+E → Back to Edit window
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 'e' || key.getTextCharacter() == 'E'))
    {
        setWorkspaceMode(0);
        updateStatusMessage("Edit window");
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
    if (key.getKeyCode() == juce::KeyPress::spaceKey && !isCmd && !isCtrl)
    {
        if (engine.getTransportState().isPlaying())
        {
            if (workspaceToolbar.onStop) workspaceToolbar.onStop();
        }
        else
        {
            if (workspaceToolbar.onPlay) workspaceToolbar.onPlay();
        }
        return true;
    }
    // Cmd+Space / F12 → Toggle Record (Pro Tools)
    if ((isCmd || isCtrl) && key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        bool wasRecording = engine.isRecording() || engine.getTransportState().isRecording();
        engine.toggleRecord();
        workspaceToolbar.setPlayState(engine.getTransportState().isPlaying(), engine.isRecording());
        if (wasRecording && !engine.isRecording())
        {
            refreshTrackList();
            arrangementModel.sendChangeMessage();
            updateStatusMessage("Recording stopped.");
        }
        return true;
    }
    // Return/Enter → Return to zero / go to start
    if (key.getKeyCode() == juce::KeyPress::returnKey && !isCmd && !isCtrl && !isShift)
    {
        if (workspaceToolbar.onReturnToZero) workspaceToolbar.onReturnToZero();
        return true;
    }
    // Cmd+Shift+L → Loop playback toggle (Pro Tools)
    if ((isCmd || isCtrl) && isShift && (key.getTextCharacter() == 'l' || key.getTextCharacter() == 'L'))
    {
        if (workspaceToolbar.onLoop) workspaceToolbar.onLoop();
        return true;
    }
    // F12 → Record
    if (key.getKeyCode() == juce::KeyPress::F12Key)
    {
        engine.toggleRecord();
        return true;
    }
    // Numpad 0 → Stop
    if (key.getKeyCode() == juce::KeyPress::numberPad0)
    {
        engine.stop();
        return true;
    }
    // Numpad Separator (Enter on numpad on some keyboards) → Play
    if (key.getKeyCode() == juce::KeyPress::numberPadSeparator)
    {
        if (workspaceToolbar.onPlay) workspaceToolbar.onPlay();
        return true;
    }
    // Numpad 1 → Rewind (go to start)
    if (key.getKeyCode() == juce::KeyPress::numberPad1)
    {
        engine.getTransportState().setPositionSamples(0, true);
        updateStatusMessage("Return to zero");
        return true;
    }
    // Numpad 3 → Record (Pro Tools alternate)
    if (key.getKeyCode() == juce::KeyPress::numberPad3)
    {
        engine.toggleRecord();
        return true;
    }
    // Numpad 4 → Toggle loop playback
    if (key.getKeyCode() == juce::KeyPress::numberPad4)
    {
        if (workspaceToolbar.onLoop) workspaceToolbar.onLoop();
        return true;
    }
    // Numpad 6 → Toggle QuickPunch
    if (key.getKeyCode() == juce::KeyPress::numberPad6)
    {
        if (workspaceToolbar.onPunchToggle) workspaceToolbar.onPunchToggle();
        return true;
    }

    // ── Edit mode F-keys (Pro Tools F1-F4) ──────────────────────────────────
    // F2 = Slip, F4 = Grid
    if (key.getKeyCode() == juce::KeyPress::F2Key && editWindow)
    {
        editWindow->setEditMode(NovaStudioUI::EditModeToolbar::EditMode::Slip);
        updateStatusMessage("Slip mode");
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::F3Key && editWindow)
    {
        editWindow->setEditMode(NovaStudioUI::EditModeToolbar::EditMode::Spot);
        updateStatusMessage("Spot mode");
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::F4Key && editWindow)
    {
        editWindow->setEditMode(NovaStudioUI::EditModeToolbar::EditMode::Grid);
        updateStatusMessage("Grid mode");
        return true;
    }

    // ── Edit tool F-keys (Pro Tools F6-F8) ──────────────────────────────────
    if (key.getKeyCode() == juce::KeyPress::F6Key && editWindow)
    {
        editWindow->setCursorTool(NovaStudioUI::EditModeToolbar::CursorTool::Trim);
        updateStatusMessage("Trim tool (F6)");
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::F7Key && editWindow)
    {
        editWindow->setCursorTool(NovaStudioUI::EditModeToolbar::CursorTool::Pointer);
        updateStatusMessage("Selector tool (F7)");
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::F8Key && editWindow)
    {
        editWindow->setCursorTool(NovaStudioUI::EditModeToolbar::CursorTool::Smart);
        updateStatusMessage("Smart tool (F8)");
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

    // Cmd+C → Copy selected clip
    if ((isCmd || isCtrl) && !isShift && (key.getTextCharacter() == 'c' || key.getTextCharacter() == 'C'))
    {
        arrangementModel.copySelectedClip();
        updateStatusMessage("Clip copied");
        return true;
    }
    // Cmd+V → Paste clip at playhead position on selected track
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 'v' || key.getTextCharacter() == 'V'))
    {
        const int64_t pos = engine.getTransportState().getPositionSamples();
        const int track   = arrangementModel.getSelectedTrackIndex();
        if (arrangementModel.pasteClipboardClip(track, pos))
        {
            refreshTrackList();
            updateStatusMessage("Clip pasted");
        }
        return true;
    }
    // Cmd+X → Cut selected clip
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 'x' || key.getTextCharacter() == 'X'))
    {
        if (arrangementModel.hasSelection())
        {
            arrangementModel.copySelectedClip();
            arrangementModel.deleteSelectedClip();
            refreshTrackList();
            updateStatusMessage("Clip cut");
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

    // Cmd+E → Split clip at selection/playhead (Pro Tools standard)
    if ((isCmd || isCtrl) && !isShift && (key.getTextCharacter() == 'e' || key.getTextCharacter() == 'E'))
    {
        arrangementModel.splitSelectedClip(engine.getTransportState().getPositionSamples());
        refreshTrackList();
        updateStatusMessage("Clip split");
        return true;
    }
    // Cmd+T → Split clip at playhead (Nova alias)
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 't' || key.getTextCharacter() == 'T'))
    {
        arrangementModel.splitSelectedClip(engine.getTransportState().getPositionSamples());
        refreshTrackList();
        updateStatusMessage("Clip split at playhead");
        return true;
    }
    // Cmd+H → Heal separation (join selected clip back with adjacent)
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 'h' || key.getTextCharacter() == 'H'))
    {
        // Heal is undo of a split — just undo
        arrangementModel.undo();
        updateStatusMessage("Heal separation");
        return true;
    }
    // Cmd+L → Lock / unlock selected clip
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 'l' || key.getTextCharacter() == 'L'))
    {
        if (arrangementModel.hasSelection())
        {
            const bool wasLocked = arrangementModel.getSelectedClip() && arrangementModel.getSelectedClip()->locked;
            arrangementModel.lockSelectedClip(!wasLocked);
            updateStatusMessage(wasLocked ? "Clip unlocked" : "Clip locked");
        }
        return true;
    }
    // Cmd+M → Mute / unmute selected clip
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 'm' || key.getTextCharacter() == 'M'))
    {
        if (arrangementModel.hasSelection())
        {
            arrangementModel.toggleSelectedClipMute();
            updateStatusMessage("Clip mute toggled");
        }
        return true;
    }
    // Cmd+A → Select all clips on selected track
    if ((isCmd || isCtrl) && (key.getTextCharacter() == 'a' || key.getTextCharacter() == 'A'))
    {
        const int ti = arrangementModel.getSelectedTrackIndex();
        if (ti >= 0)
        {
            const auto& track = arrangementModel.getSession().getTrack(ti);
            for (int ci = 0; ci < track.clips.size(); ++ci)
                arrangementModel.addClipToSelection(ti, ci);
            updateStatusMessage("All clips selected");
        }
        return true;
    }
    // Cmd+Shift+N → New audio track (Pro Tools)
    if ((isCmd || isCtrl) && isShift && (key.getTextCharacter() == 'n' || key.getTextCharacter() == 'N'))
    {
        engine.addTrack("Audio " + juce::String(engine.getSession().getNumTracks() + 1), NovaStudio::TrackType::Audio);
        refreshTrackList();
        updateStatusMessage("New audio track added");
        return true;
    }
    // Numpad + → Nudge clip forward
    if (key.getKeyCode() == juce::KeyPress::numberPadAdd)
    {
        if (arrangementModel.hasSelection())
        {
            const double nudgeSamples = engine.getCurrentSampleRate() * 0.01; // 10ms default nudge
            const auto targets = arrangementModel.getSelectedClips();
            arrangementModel.moveClipsBySamples(targets, (int64_t)nudgeSamples);
            updateStatusMessage("Nudge forward");
        }
        return true;
    }
    // Numpad - → Nudge clip backward
    if (key.getKeyCode() == juce::KeyPress::numberPadSubtract)
    {
        if (arrangementModel.hasSelection())
        {
            const double nudgeSamples = engine.getCurrentSampleRate() * 0.01;
            const auto targets = arrangementModel.getSelectedClips();
            arrangementModel.moveClipsBySamples(targets, -(int64_t)nudgeSamples);
            updateStatusMessage("Nudge backward");
        }
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
    // Cmd+] → Zoom in horizontally
    if ((isCmd || isCtrl) && key.getKeyCode() == ']')
    {
        if (editWindow) editWindow->zoomHorizontal(1);
        return true;
    }
    // Cmd+[ → Zoom out horizontally
    if ((isCmd || isCtrl) && key.getKeyCode() == '[')
    {
        if (editWindow) editWindow->zoomHorizontal(-1);
        return true;
    }
    // Opt+A (Mac) / Alt+A (Win) → Zoom to fit all (zoom out to see full session)
    if (key.getModifiers().isAltDown() && !isCmd && !isCtrl
        && (key.getTextCharacter() == 'a' || key.getTextCharacter() == 'A'))
    {
        // Zoom out repeatedly until full session is visible
        for (int i = 0; i < 10; ++i)
            if (editWindow) editWindow->zoomHorizontal(-1);
        updateStatusMessage("Zoom to fit session");
        return true;
    }
    // Opt+F / Alt+F → Zoom to fit selection
    if (key.getModifiers().isAltDown() && !isCmd && !isCtrl
        && (key.getTextCharacter() == 'f' || key.getTextCharacter() == 'F'))
    {
        for (int i = 0; i < 5; ++i)
            if (editWindow) editWindow->zoomHorizontal(1);
        updateStatusMessage("Zoom in");
        return true;
    }
    // Shift+Opt+3 (Mac) / Shift+Alt+3 (Win) → Consolidate selection
    if (isShift && key.getModifiers().isAltDown() && key.getKeyCode() == '3')
    {
        if (arrangementModel.hasSelection())
        {
            // Consolidate = duplicate clip then delete originals (simple version)
            arrangementModel.duplicateSelectedClip();
            updateStatusMessage("Consolidate: duplicate created");
        }
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
        // A → Trim clip start to playhead
        if (k == 'a' || k == 'A')
        {
            const auto* clip = arrangementModel.getSelectedClip();
            if (clip)
            {
                const int64_t pos = engine.getTransportState().getPositionSamples();
                if (pos > clip->startSample && pos < clip->startSample + clip->lengthSamples)
                {
                    NovaStudio::Clip c = *clip;
                    const int64_t delta = pos - c.startSample;
                    c.fileOffsetSamples += delta;
                    c.startSample  = pos;
                    c.lengthSamples -= delta;
                    arrangementModel.replaceSelectedClipWithUndo(c, "Trim Start to Playhead");
                    updateStatusMessage("Trim start");
                }
            }
            return true;
        }
        // S → Trim clip end to playhead
        if (k == 's' || k == 'S')
        {
            const auto* clip = arrangementModel.getSelectedClip();
            if (clip)
            {
                const int64_t pos = engine.getTransportState().getPositionSamples();
                if (pos > clip->startSample && pos < clip->startSample + clip->lengthSamples)
                {
                    NovaStudio::Clip c = *clip;
                    c.lengthSamples = pos - c.startSample;
                    arrangementModel.replaceSelectedClipWithUndo(c, "Trim End to Playhead");
                    updateStatusMessage("Trim end");
                }
            }
            return true;
        }
        // R → Zoom out horizontally
        if (k == 'r' || k == 'R')
        {
            if (editWindow) editWindow->zoomHorizontal(-1);
            return true;
        }
        // T → Zoom in horizontally
        if (k == 't' || k == 'T')
        {
            if (editWindow) editWindow->zoomHorizontal(1);
            return true;
        }
        // X → Cut (single key)
        if (k == 'x' || k == 'X')
        {
            if (arrangementModel.hasSelection())
            {
                arrangementModel.copySelectedClip();
                arrangementModel.deleteSelectedClip();
                refreshTrackList();
                updateStatusMessage("Cut");
            }
            return true;
        }
        // C → Copy (single key)
        if (k == 'c' || k == 'C')
        {
            arrangementModel.copySelectedClip();
            updateStatusMessage("Copied");
            return true;
        }
    }

    return false;
}

void MainComponent::refreshTrackList()
{
    if (editWindow)
        editWindow->repaint();
    if (mixerWindow)
        mixerWindow->refresh();  // always refresh — mixer must stay in sync regardless of visibility

    // Keep the beat window's compact mixer in sync with the session's tracks
    if (beatWindow)
    {
        static const juce::Colour kCompactMixerPalette[] = {
            juce::Colour::fromRGB(120, 80, 200),
            juce::Colour::fromRGB(80, 100, 200),
            juce::Colour::fromRGB(50, 160, 160),
            juce::Colour::fromRGB(60, 120, 200),
            juce::Colour::fromRGB(40, 180, 160),
            juce::Colour::fromRGB(60, 180, 80),
            juce::Colour::fromRGB(160, 190, 50),
            juce::Colour::fromRGB(190, 110, 40),
        };

        const int numTracks = engine.getSession().getNumTracks();
        beatWindow->setMixerTrackCount(numTracks);
        for (int i = 0; i < numTracks; ++i)
        {
            const auto& track = engine.getSession().getTrack(i);
            beatWindow->setMixerTrackInfo(i, track.name, kCompactMixerPalette[i % 8],
                                          track.volumeDb, track.pan, track.muted);
        }
    }
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
        menu.addItem(106, "Export Mix...",        engine.getTrackCount() > 0);
        menu.addItem(107, "Export Stems...",      engine.getTrackCount() > 0);
        menu.addSeparator();
        menu.addItem(108, "Session Settings...",  true);
        menu.addSeparator();
        menu.addItem(109, "Quit");
        break;

    case 1: // Edit
        menu.addItem(201, "Undo");
        menu.addItem(202, "Redo");
        menu.addSeparator();
        menu.addItem(203, "Cut",       arrangementModel.hasSelection());
        menu.addItem(204, "Copy",      arrangementModel.hasSelection());
        menu.addItem(205, "Paste",     arrangementModel.hasClipboardClip() && arrangementModel.getSelectedTrackIndex() >= 0);
        menu.addItem(206, "Duplicate");
        menu.addItem(207, "Delete");
        menu.addSeparator();
        menu.addItem(208, "Split Clip at Playhead");
        menu.addItem(209, "Select All", arrangementModel.getSelectedTrackIndex() >= 0);
        break;

    case 2: // Track
        menu.addItem(301, "New Audio Track");
        menu.addItem(302, "New MIDI Track");
        menu.addItem(303, "New Instrument Track");
        menu.addItem(304, "New Aux Track");
        menu.addSeparator();
        menu.addItem(307, "Duplicate Track",       arrangementModel.getSelectedTrackIndex() >= 0);
        menu.addItem(308, "Remove Last Track",     engine.getTrackCount() > 0);
        break;

    case 3: // Clip
        menu.addItem(401, "Consolidate",      arrangementModel.getSelectedClips().size() >= 2);
        menu.addItem(402, "Normalize",        arrangementModel.hasSelection());
        menu.addItem(403, "Reverse",          arrangementModel.hasSelection());
        menu.addSeparator();
        menu.addItem(404, "Fade In...",       arrangementModel.hasSelection());
        menu.addItem(405, "Fade Out...",      arrangementModel.hasSelection());
        menu.addSeparator();
        menu.addItem(406, "Bounce Clip to Audio", arrangementModel.hasSelection());
        menu.addItem(407, "Nova Align Selected");
        break;

    case 4: // Audio
        menu.addItem(501, "Audio Settings...");
        menu.addItem(502, "Input Routing...");
        menu.addItem(503, "Output Routing...");
        menu.addSeparator();
        menu.addItem(504, "Buffer Size");
        menu.addItem(505, "Sample Rate");
        break;

    case 5: // Plugins
        menu.addItem(601, "Plugin Manager...");
        menu.addItem(602, "Scan Plugins");
        menu.addItem(603, "Rescan Plugins");
        menu.addItem(604, "Plugin Paths...");
        break;

    case 6: // View
        menu.addItem(701, "Edit Window");
        menu.addItem(702, "Mixer");
        menu.addItem(703, "Piano Roll");
        menu.addItem(704, "Step Sequencer");
        menu.addItem(705, "Browser");
        menu.addSeparator();
        menu.addItem(706, "Inspector");
        menu.addItem(707, "Nova Align");
        break;

    case 7: // AI
        menu.addItem(801, "Nova Assistant");
        menu.addSeparator();
        menu.addItem(802, "Align Background Vocals",  engine.getTrackCount() > 0);
        menu.addItem(803, "Organize Session",         engine.getTrackCount() > 0);
        menu.addItem(804, "Create Vocal Buses",       engine.getTrackCount() > 0);
        menu.addItem(805, "Stem Out Session",         engine.getTrackCount() > 0);
        menu.addItem(806, "Export For Pro Tools",     engine.getTrackCount() > 0);
        menu.addItem(807, "Find Clipping Tracks",     engine.getTrackCount() > 0);
        menu.addSeparator();
        menu.addItem(808, "One-Click Mix (Ozuna/Rauw Style)", engine.getTrackCount() > 0);
        break;

    case 8: // Window
        menu.addItem(901, "Reset Layout");
        menu.addItem(902, "Full Screen",       true);
        menu.addSeparator();
        menu.addItem(903, "Floating Mixer");
        menu.addItem(904, "Floating Browser");
        break;

    case 9: // Help
        menu.addItem(1001, "Nova Studio Manual",   true);
        menu.addItem(1002, "Keyboard Shortcuts",   true);
        menu.addSeparator();
        menu.addItem(1003, "About Nova Studio...", true);
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
    {
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,
            "New Session",
            "Start a new session? Any unsaved changes will be lost.",
            "New Session", "Cancel", this,
            juce::ModalCallbackFunction::create([this](int result)
            {
                if (result != 1)
                    return;
                engine.newSession();
                arrangementModel.clearSelection();
                if (mixerWindow)  mixerWindow->refresh();
                if (beatWindow)   beatWindow->setMixerTrackCount(0);
                workspaceToolbar.setTempo(120);
                refreshTrackList();
                arrangementModel.sendChangeMessage();
                updateStatusMessage("New session created");
            }));
        break;
    }
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
    case 106: promptExportMix();   break;
    case 107: promptExportStems(); break;
    case 108: promptSessionSettings(); break;
    case 109: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;

    // ── Edit ────────────────────────────────────────────────────────────────
    case 201: arrangementModel.undo();  updateStatusMessage("Undo"); break;
    case 202: arrangementModel.redo();  updateStatusMessage("Redo"); break;
    case 203:
        if (arrangementModel.copySelectedClip() && arrangementModel.deleteSelectedClip())
            updateStatusMessage("Cut clip");
        refreshTrackList();
        break;
    case 204:
        if (arrangementModel.copySelectedClip())
            updateStatusMessage("Copied clip");
        break;
    case 205:
    {
        const int track = arrangementModel.getSelectedTrackIndex();
        const int64_t pos = engine.getTransportState().getPositionSamples();
        if (track >= 0 && arrangementModel.pasteClipboardClip(track, pos))
            updateStatusMessage("Pasted clip");
        refreshTrackList();
        break;
    }
    case 209:
    {
        const int ti = arrangementModel.getSelectedTrackIndex();
        if (ti >= 0)
        {
            const auto& track = arrangementModel.getSession().getTrack(ti);
            for (int ci = 0; ci < track.clips.size(); ++ci)
                arrangementModel.addClipToSelection(ti, ci);
            updateStatusMessage("All clips selected");
        }
        refreshTrackList();
        break;
    }
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
    case 307:
    {
        const int ti = arrangementModel.getSelectedTrackIndex();
        if (ti >= 0 && arrangementModel.duplicateTrack(ti))
        {
            refreshTrackList();
            updateStatusMessage("Duplicated track");
        }
        else
        {
            updateStatusMessage("Duplicate Track: select a track first");
        }
        break;
    }
    case 308:
    {
        const int last = engine.getTrackCount() - 1;
        if (last < 0) break;
        const juce::String trackName = engine.getSession().getTrack(last).name;
        engine.removeTrack(last);
        arrangementModel.clearSelection();
        if (mixerWindow) mixerWindow->refresh();
        refreshTrackList();
        arrangementModel.sendChangeMessage();
        updateStatusMessage("Removed track: " + trackName);
        break;
    }

    // ── Clip ────────────────────────────────────────────────────────────────
    case 401:
        if (arrangementModel.consolidateSelectedClips())
            updateStatusMessage("Consolidated clips");
        else
            updateStatusMessage("Consolidate: select 2+ unlocked audio clips on the same track");
        refreshTrackList();
        break;
    case 402:
        if (arrangementModel.normalizeSelectedClip())
            updateStatusMessage("Normalized selected clip");
        else
            updateStatusMessage("Normalize: no audio clip selected");
        refreshTrackList();
        break;
    case 403:
        if (arrangementModel.reverseSelectedClip())
            updateStatusMessage("Reversed selected clip");
        else
            updateStatusMessage("Reverse: no audio clip selected");
        refreshTrackList();
        break;
    case 404:
        promptClipFadeLength(true);
        break;
    case 405:
        promptClipFadeLength(false);
        break;
    case 406:
        promptBounceClipToAudio();
        break;
    case 407:
        setWorkspaceMode(0);
        alignPanel.setVisible(true);
        alignPanel.toFront(true);
        break;

    // ── AI / Nova Assistant ─────────────────────────────────────────────────
    case 801: runNovaAssistant();      break;
    case 802: alignBackgroundVocals(); break;
    case 803: organizeSession();       break;
    case 804: createVocalBuses();      break;
    case 805: promptExportStems();     break;
    case 806: exportForProTools();     break;
    case 807: findClippingTracks();    break;
    case 808: runAiMix();              break;

    // ── Audio ───────────────────────────────────────────────────────────────
    case 501: // Audio Settings...
    case 502: // Input Routing...   — exposed via the device selector's input-channel section
    case 503: // Output Routing...  — exposed via the device selector's output-channel section
    case 504: // Buffer Size        — exposed via the device selector's "Show advanced settings"
    case 505: // Sample Rate        — exposed via the device selector's "Show advanced settings"
        if (workspaceToolbar.onAudioSettings) workspaceToolbar.onAudioSettings();
        break;

    // ── Plugins ─────────────────────────────────────────────────────────────
    case 601: showPluginManager();      break;
    case 602: scanForPlugins(false);    break;
    case 603: scanForPlugins(true);     break;
    case 604: showPluginManager();      break; // Plugin Paths are managed from the same list component

    // ── View ────────────────────────────────────────────────────────────────
    case 701: setWorkspaceMode(0); break;
    case 702: setWorkspaceMode(1); break;
    case 703: // Piano Roll — jump to the Beat window's docked piano roll canvas
        setWorkspaceMode(2);
        if (beatWindow) beatWindow->showPianoRollView();
        break;
    case 704: // Step Sequencer — jump to the Beat window's Channel Rack
        setWorkspaceMode(2);
        if (beatWindow) beatWindow->showStepSequencerView();
        break;
    case 705: setWorkspaceMode(0); browserPanel.refresh(); break;
    case 706: setWorkspaceMode(0); break;
    case 707:
        setWorkspaceMode(0);
        alignPanel.setVisible(true);
        alignPanel.toFront(true);
        break;

    // ── Window ──────────────────────────────────────────────────────────────
    case 901: setWorkspaceMode(0); resized(); break;
    case 902:
        if (auto* peer = getPeer())
        {
            const bool goingFullScreen = ! peer->isFullScreen();
            peer->setFullScreen(goingFullScreen);
            updateStatusMessage(goingFullScreen ? "Entered full screen" : "Exited full screen");
        }
        break;
    case 903: popOutMixer();   updateStatusMessage("Floating Mixer");   break;
    case 904: popOutBrowser(); updateStatusMessage("Floating Browser"); break;

    // ── Help ────────────────────────────────────────────────────────────────
    case 1001: showManualDialog();             break;
    case 1002: showKeyboardShortcutsDialog();  break;
    case 1003: showAboutDialog();              break;

    default:
        updateStatusMessage("Coming soon.");
        break;
    }
}
