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
    mixerWindow = std::make_unique<NovaStudioUI::MixerWindow>(engine);
    addAndMakeVisible(*mixerWindow);
    mixerWindow->setVisible(false);
    beatWindow = std::make_unique<NovaStudioUI::BeatWindow>(transportState);
    addAndMakeVisible(*beatWindow);
    beatWindow->setVisible(false);
    addAndMakeVisible(mixerPanel);
    mixerPanel.setVisible(false);
    addAndMakeVisible(bottomDock);
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
    engine.addTrack("MIDI 1", NovaStudio::TrackType::Midi);
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
        alignPanel.toFront(true);
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

MainComponent::~MainComponent() = default;

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

    // Left browser sidebar
    auto leftSidebar = area.removeFromLeft(190);
    browserPanel.setBounds(leftSidebar);

    // Bottom dock: mixer channels + piano roll + step seq — secondary to timeline
    // Keep dock compact (≈25% of window) so the arrangement gets ≈65%
    const int dockH = juce::jmax(110, getHeight() / 4);
    auto bottomArea = area.removeFromBottom(dockH);
    bottomDock.setBounds(bottomArea);

    // Status label (overlay, bottom right)
    statusLabel.setBounds(getWidth() - 500, getHeight() - 26, 480, 22);

    // Edit mode: editWindow takes remaining center area
    if (editWindow)
        editWindow->setBounds(area);

    // Mixer mode: full screen minus header
    if (mixerWindow)
        mixerWindow->setBounds(getLocalBounds().withTrimmedTop(56));

    // Beat mode: full screen minus header
    if (beatWindow)
        beatWindow->setBounds(getLocalBounds().withTrimmedTop(56));

    // Nova Align: side panel over center
    alignPanel.setBounds(area.removeFromRight(360));

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

    return false;
}

void MainComponent::refreshTrackList()
{
    if (editWindow)
        editWindow->repaint();
}

void MainComponent::setWorkspaceMode(int mode)
{
    browserPanel.setVisible(true); // always visible

    switch (mode)
    {
    case 0: // Edit
        if (editWindow) editWindow->setVisible(true);
        if (mixerWindow) mixerWindow->setVisible(false);
        if (beatWindow) beatWindow->setVisible(false);
        alignPanel.setVisible(false);
        bottomDock.setVisible(true);
        break;
    case 1: // Mixer
        if (editWindow) editWindow->setVisible(false);
        if (mixerWindow) { mixerWindow->refresh(); mixerWindow->setVisible(true); }
        if (beatWindow) beatWindow->setVisible(false);
        alignPanel.setVisible(false);
        bottomDock.setVisible(false);
        break;
    case 2: // Browse / Split
        if (editWindow) editWindow->setVisible(true);
        if (mixerWindow) mixerWindow->setVisible(false);
        if (beatWindow) beatWindow->setVisible(false);
        alignPanel.setVisible(false);
        bottomDock.setVisible(true);
        break;
    case 3: // Beat
        if (editWindow) editWindow->setVisible(false);
        if (mixerWindow) mixerWindow->setVisible(false);
        if (beatWindow) beatWindow->setVisible(true);
        alignPanel.setVisible(false);
        bottomDock.setVisible(false);
        break;
    default:
        if (editWindow) editWindow->setVisible(true);
        if (mixerWindow) mixerWindow->setVisible(false);
        if (beatWindow) beatWindow->setVisible(false);
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
        menu.addItem(303, "New Instrument Track", false);
        menu.addItem(304, "New Aux Track",         false);
        menu.addItem(305, "New Bus",               false);
        menu.addItem(306, "New Folder Track",      false);
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
    {
        const auto type = (id == 301) ? NovaStudio::TrackType::Audio : NovaStudio::TrackType::Midi;
        const juce::String prefix = (id == 301) ? "Audio " : "MIDI ";
        const int n = engine.getTrackCount() + 1;
        engine.addTrack(prefix + juce::String(n), type);
        refreshTrackList();
        arrangementModel.sendChangeMessage();
        updateStatusMessage("Added " + prefix + juce::String(n));
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
