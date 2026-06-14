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

    addAndMakeVisible(workspaceToolbar);
    addAndMakeVisible(browserPanel);
    addAndMakeVisible(trackPanel);
    addAndMakeVisible(arrangementView);
    addAndMakeVisible(alignPanel);
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
    addAndMakeVisible(bottomTabs);
    addAndMakeVisible(statusLabel);

    bottomTabs.addTab("Mixer", juce::Colours::transparentBlack, &mixerPanel, false);
    bottomTabs.addTab("Piano Roll", juce::Colours::transparentBlack, &pianoRollPanel, false);
    bottomTabs.addTab("Step Seq", juce::Colours::transparentBlack, &stepSequencerPanel, false);

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

    // Unified top header
    workspaceToolbar.setBounds(area.removeFromTop(56));

    // Left browser sidebar
    auto leftSidebar = area.removeFromLeft(190);
    browserPanel.setBounds(leftSidebar);

    // Bottom tabs (Mixer / Piano Roll / Step Seq)
    auto bottomArea = area.removeFromBottom(200);
    bottomTabs.setBounds(bottomArea);

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
    trackPanel.repaint();
    arrangementView.repaint();
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
        bottomTabs.setVisible(true);
        break;
    case 1: // Mixer
        if (editWindow) editWindow->setVisible(false);
        if (mixerWindow) { mixerWindow->refresh(); mixerWindow->setVisible(true); }
        if (beatWindow) beatWindow->setVisible(false);
        alignPanel.setVisible(false);
        bottomTabs.setVisible(false);
        break;
    case 2: // Browse / Split
        if (editWindow) editWindow->setVisible(true);
        if (mixerWindow) mixerWindow->setVisible(false);
        if (beatWindow) beatWindow->setVisible(false);
        alignPanel.setVisible(false);
        bottomTabs.setVisible(true);
        break;
    case 3: // Beat
        if (editWindow) editWindow->setVisible(false);
        if (mixerWindow) mixerWindow->setVisible(false);
        if (beatWindow) beatWindow->setVisible(true);
        alignPanel.setVisible(false);
        bottomTabs.setVisible(false);
        break;
    default:
        if (editWindow) editWindow->setVisible(true);
        if (mixerWindow) mixerWindow->setVisible(false);
        if (beatWindow) beatWindow->setVisible(false);
        alignPanel.setVisible(false);
        bottomTabs.setVisible(true);
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
