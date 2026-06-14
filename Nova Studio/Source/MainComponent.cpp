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
    addAndMakeVisible(transportBar);
    addAndMakeVisible(trackPanel);
    addAndMakeVisible(arrangementView);
    addAndMakeVisible(alignPanel);
    editWindow = std::make_unique<NovaStudioUI::EditWindow>(transportState, timelineModel, arrangementModel);
    addAndMakeVisible(*editWindow);
    mixerWindow = std::make_unique<NovaStudioUI::MixerWindow>(engine);
    addAndMakeVisible(*mixerWindow);
    mixerWindow->setVisible(false);
    beatWindow = std::make_unique<NovaStudioUI::BeatWindow>(transportState);
    addAndMakeVisible(*beatWindow);
    beatWindow->setVisible(false);
    addAndMakeVisible(mixerPanel);
    addAndMakeVisible(bottomTabs);
    addAndMakeVisible(statusLabel);

    bottomTabs.addTab("Browser", juce::Colours::transparentBlack, &browserPanel, false);
    bottomTabs.addTab("Piano Roll", juce::Colours::transparentBlack, &pianoRollPanel, false);
    bottomTabs.addTab("Step Seq", juce::Colours::transparentBlack, &stepSequencerPanel, false);

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
    statusLabel.setText("Nova Studio Lite prototype ready", juce::dontSendNotification);

    transportBar.onPlay = [this] {
        engine.play();
        transportBar.setPlayState(true, engine.isRecording());
        updateStatusMessage("Playback engaged.");
    };

    transportBar.onStop = [this] {
        bool wasRecording = engine.isRecording();
        engine.stop();
        transportBar.setPlayState(false, false);
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

    transportBar.onRecord = [this] {
        engine.toggleRecord();
        transportBar.setPlayState(engine.getTransportState().isPlaying(), engine.isRecording());
        updateStatusMessage(engine.isRecording() ? "Recording active." : "Record stopped.");
    };

    transportBar.onArm = [this] {
        const bool armed = !engine.getTransportState().isRecordArmed();
        engine.getTransportState().setRecordArmed(armed);
        transportBar.setArmState(armed);
        updateStatusMessage(armed ? "Record armed." : "Record disarmed.");
    };

    transportBar.onMonitor = [this] {
        const bool monitor = !engine.getTransportState().isInputMonitoring();
        engine.getTransportState().setInputMonitoring(monitor);
        transportBar.setMonitorState(monitor);
        updateStatusMessage(monitor ? "Input monitoring enabled." : "Input monitoring disabled.");
    };

    transportBar.onLoop = [this] {
        const bool looping = !engine.getTransportState().isLooping();
        engine.getTransportState().setLooping(looping);
        transportBar.setLoopState(looping);
        updateStatusMessage(looping ? "Loop enabled." : "Loop disabled.");
    };

    transportBar.setPlayState(false, false);
    transportBar.setArmState(engine.getTransportState().isRecordArmed());
    transportBar.setMonitorState(engine.getTransportState().isInputMonitoring());
    transportBar.setLoopState(engine.getTransportState().isLooping());

    if (!engine.initialize())
        updateStatusMessage("Audio initialization failed.");

    transportBar.setTempo(static_cast<int>(engine.getSession().getTempo()));
    transportBar.setTimecode(transportState.getTimecodeString(transportState.getPositionSamples()));

    // initial playback indicator and toggle hookup
    transportBar.setPlaybackState(engine.getSession().isPreviewPlaybackEnabled(), arrangementModel.hasAlignmentPreview());
    transportBar.onTogglePreview = [this](bool wantsPreview) {
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
        transportBar.setPlaybackState(engine.getSession().isPreviewPlaybackEnabled(), arrangementModel.hasAlignmentPreview());
    }
}

MainComponent::~MainComponent() = default;

void MainComponent::paint(juce::Graphics& g)
{
    juce::Colour top = juce::Colour::fromRGB(10, 12, 18);
    juce::Colour bottom = juce::Colour::fromRGB(18, 24, 36);
    g.setGradientFill({ top, 0.0f, 0.0f, bottom, 0.0f, (float)getHeight(), false });
    g.fillAll();

    g.setColour(juce::Colours::white.withAlpha(0.65f));
    {
        juce::Font font(juce::FontOptions(28.0f).withStyle("Bold"));
        g.setFont(font);
    }
    g.drawText("Nova Studio", 24, 20, 320, 32, juce::Justification::left);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(18);
    auto toolbarArea = area.removeFromTop(56);
    workspaceToolbar.setBounds(toolbarArea.reduced(0, 6));

    auto transportArea = area.removeFromTop(112);
    transportBar.setBounds(transportArea);

    auto bottomArea = area.removeFromBottom(260);
    bottomTabs.setBounds(bottomArea);

    auto centreArea = area;
    auto leftPane = centreArea.removeFromLeft(280);
    trackPanel.setBounds(leftPane.reduced(0, 4));
    auto rightPane = centreArea.removeFromRight(360);
    auto alignArea = rightPane.removeFromTop(340);
    alignPanel.setBounds(alignArea.reduced(0, 4));
    mixerPanel.setBounds(rightPane.reduced(0, 4));
    arrangementView.setBounds(centreArea.reduced(0, 4));
    if (editWindow)
        editWindow->setBounds(centreArea.reduced(0, 4));
    if (mixerWindow)
        mixerWindow->setBounds(getLocalBounds().reduced(18).withTrimmedTop(56 + 12 + 112 + 4));
    if (beatWindow)
        beatWindow->setBounds(getLocalBounds().reduced(18).withTrimmedTop(56 + 12 + 112 + 4));

    statusLabel.setBounds(getWidth() - 520, getHeight() - 32, 500, 24);
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
    if (editWindow)
        editWindow->repaint();
}

void MainComponent::setWorkspaceMode(int mode)
{
    // 0=Edit,1=Mixer,2=Split,3=Beat,4=Rack
    switch (mode)
    {
        case 0: // Edit
            if (editWindow)
                editWindow->setVisible(true);
            if (mixerWindow)
                mixerWindow->setVisible(false);
            if (beatWindow)
                beatWindow->setVisible(false);
            arrangementView.setVisible(false);
            trackPanel.setVisible(false);
            alignPanel.setVisible(true);
            mixerPanel.setVisible(false);
            bottomTabs.setVisible(true);
            break;
        case 1: // Mixer
            if (editWindow)
                editWindow->setVisible(false);
            if (beatWindow)
                beatWindow->setVisible(false);
            arrangementView.setVisible(false);
            trackPanel.setVisible(false);
            alignPanel.setVisible(false);
            mixerPanel.setVisible(false);
            if (mixerWindow)
            {
                mixerWindow->refresh();
                mixerWindow->setVisible(true);
            }
            bottomTabs.setVisible(false);
            break;
        case 2: // Split
            if (editWindow)
                editWindow->setVisible(false);
            if (mixerWindow)
                mixerWindow->setVisible(false);
            if (beatWindow)
                beatWindow->setVisible(false);
            arrangementView.setVisible(true);
            trackPanel.setVisible(true);
            alignPanel.setVisible(true);
            mixerPanel.setVisible(false);
            bottomTabs.setVisible(true);
            break;
        case 3: // Beat
            if (editWindow)
                editWindow->setVisible(false);
            if (mixerWindow)
                mixerWindow->setVisible(false);
            if (beatWindow)
                beatWindow->setVisible(true);
            arrangementView.setVisible(false);
            trackPanel.setVisible(false);
            alignPanel.setVisible(false);
            mixerPanel.setVisible(false);
            bottomTabs.setVisible(false);
            break;
        case 4: // Rack
            if (editWindow)
                editWindow->setVisible(false);
            if (mixerWindow)
                mixerWindow->setVisible(false);
            if (beatWindow)
                beatWindow->setVisible(false);
            arrangementView.setVisible(false);
            trackPanel.setVisible(true);
            alignPanel.setVisible(true);
            mixerPanel.setVisible(false);
            bottomTabs.setVisible(true);
            break;
    }

    // persist
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("mode", mode);
    juce::var root(obj.get());
    auto content = juce::JSON::toString(root);
    juce::File cfgDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("NovaStudio");
    cfgDir.createDirectory();
    juce::File cfg = cfgDir.getChildFile("workspace.json");
    cfg.replaceWithText(content);
    resized();
}
