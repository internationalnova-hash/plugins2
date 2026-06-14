#include "StudioComponents.h"
#include "WaveformCache.h"
#include "Theme.h"

namespace NovaStudioUI
{
    TransportBar::TransportBar()
    {
        addAndMakeVisible(rtzButton);
        addAndMakeVisible(playButton);
        addAndMakeVisible(stopButton);
        addAndMakeVisible(recordButton);
        addAndMakeVisible(armButton);
        addAndMakeVisible(monitorButton);
        addAndMakeVisible(loopButton);
        addAndMakeVisible(tempoLabel);
        addAndMakeVisible(timeLabel);

        rtzButton.addListener(this);
        playButton.addListener(this);
        stopButton.addListener(this);
        recordButton.addListener(this);
        armButton.addListener(this);
        monitorButton.addListener(this);
        loopButton.addListener(this);

        tempoLabel.setText("120 BPM", juce::dontSendNotification);
        tempoLabel.setJustificationType(juce::Justification::centred);
        timeLabel.setText("00:00:00", juce::dontSendNotification);
        timeLabel.setJustificationType(juce::Justification::centred);

        addAndMakeVisible(playbackLabel);
        playbackLabel.setJustificationType(juce::Justification::centredLeft);
        playbackLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        playbackLabel.setText("Hearing: Committed", juce::dontSendNotification);
        addAndMakeVisible(playbackToggleButton);
        playbackToggleButton.addListener(this);
        playbackToggleButton.setButtonText("Preview");
        playbackToggleButton.setTooltip("Toggle hearing Original/Preview when a Nova Align preview exists");
    }

    TransportBar::~TransportBar() = default;

    void TransportBar::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        juce::Colour gradientStart = juce::Colour::fromRGB(18, 20, 28);
        juce::Colour gradientEnd = juce::Colour::fromRGB(36, 42, 58);
        g.setGradientFill({ gradientStart, 0.0f, 0.0f, gradientEnd, 0.0f, bounds.getHeight(), false });
        g.fillRoundedRectangle(bounds.reduced(1.0f), 14.0f);

        g.setColour(juce::Colours::white.withAlpha(0.72f));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("TRANSPORT", 16, 12, 120, 16, juce::Justification::left);
    }

    void TransportBar::resized()
    {
        auto area = getLocalBounds().reduced(14);
        auto buttonWidth = 88;
        auto tall = 32;
        rtzButton.setBounds(area.removeFromLeft(44).removeFromTop(tall).reduced(4));
        playButton.setBounds(area.removeFromLeft(buttonWidth).removeFromTop(tall).reduced(4));
        stopButton.setBounds(area.removeFromLeft(buttonWidth).removeFromTop(tall).reduced(4));
        recordButton.setBounds(area.removeFromLeft(buttonWidth).removeFromTop(tall).reduced(4));
        armButton.setBounds(area.removeFromLeft(buttonWidth).removeFromTop(tall).reduced(4));
        monitorButton.setBounds(area.removeFromLeft(buttonWidth).removeFromTop(tall).reduced(4));
        loopButton.setBounds(area.removeFromLeft(buttonWidth).removeFromTop(tall).reduced(4));

        auto rightArea = getLocalBounds().reduced(14).removeFromRight(260);
        tempoLabel.setBounds(rightArea.removeFromLeft(120).reduced(4));
        timeLabel.setBounds(rightArea.removeFromLeft(140).reduced(4));

        // Playback indicator near title
        playbackLabel.setBounds(16, 34, 220, 18);
        playbackToggleButton.setBounds(240, 30, 80, 22);
    }


    void TransportBar::setTempo(int bpm)
    {
        tempoLabel.setText(juce::String(bpm) + " BPM", juce::dontSendNotification);
    }

    void TransportBar::setTimecode(const juce::String& timecode)
    {
        timeLabel.setText(timecode, juce::dontSendNotification);
    }

    void TransportBar::setPlayState(bool isPlaying, bool isRecording)
    {
        playButton.setColour(juce::TextButton::buttonColourId, isPlaying ? juce::Colours::lightgreen.withAlpha(0.35f) : juce::Colours::transparentBlack);
        recordButton.setColour(juce::TextButton::buttonColourId, isRecording ? juce::Colours::red.withAlpha(0.55f) : juce::Colours::transparentBlack);
    }

    void TransportBar::setLoopState(bool enabled)
    {
        loopButton.setColour(juce::TextButton::buttonColourId, enabled ? juce::Colours::yellow.withAlpha(0.35f) : juce::Colours::transparentBlack);
    }

    void TransportBar::setArmState(bool armed)
    {
        armButton.setColour(juce::TextButton::buttonColourId, armed ? juce::Colours::orange.withAlpha(0.45f) : juce::Colours::transparentBlack);
    }

    void TransportBar::setMonitorState(bool enabled)
    {
        monitorButton.setColour(juce::TextButton::buttonColourId, enabled ? juce::Colours::skyblue.withAlpha(0.35f) : juce::Colours::transparentBlack);
    }

    

    void TransportBar::setPlaybackState(bool previewEnabled, bool hasPreview)
    {
        if (!hasPreview)
        {
            playbackLabel.setText("Hearing: Committed", juce::dontSendNotification);
            playbackLabel.setColour(juce::Label::backgroundColourId, juce::Colours::darkgrey);
            playbackLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
            playbackLabel.setTooltip("No Nova Align preview available — hearing committed audio.");
            playbackToggleButton.setEnabled(false);
            playbackToggleButton.setToggleState(false, juce::dontSendNotification);
            return;
        }

        if (previewEnabled)
        {
            playbackLabel.setText("Hearing: Preview", juce::dontSendNotification);
            playbackLabel.setColour(juce::Label::backgroundColourId, juce::Colours::orange.withAlpha(0.25f));
            playbackLabel.setColour(juce::Label::textColourId, juce::Colours::black);
            playbackLabel.setTooltip("Hearing Nova Align preview. Click 'Preview' to switch to Original.");
            playbackToggleButton.setEnabled(true);
            playbackToggleButton.setButtonText("Original");
        }
        else
        {
            playbackLabel.setText("Hearing: Original", juce::dontSendNotification);
            playbackLabel.setColour(juce::Label::backgroundColourId, juce::Colours::skyblue.withAlpha(0.25f));
            playbackLabel.setColour(juce::Label::textColourId, juce::Colours::black);
            playbackLabel.setTooltip("Hearing original audio. Click 'Preview' to switch to Nova Align preview.");
            playbackToggleButton.setEnabled(true);
            playbackToggleButton.setButtonText("Preview");
        }
    }

    // Button listener handles playback toggle
    void TransportBar::buttonClicked(juce::Button* button)
    {
        if (button == &rtzButton && onReturnToZero) onReturnToZero();
        else if (button == &playButton && onPlay) onPlay();
        else if (button == &stopButton && onStop) onStop();
        else if (button == &recordButton && onRecord) onRecord();
        else if (button == &armButton && onArm) onArm();
        else if (button == &monitorButton && onMonitor) onMonitor();
        else if (button == &loopButton && onLoop) onLoop();
        else if (button == &playbackToggleButton)
        {
            if (onTogglePreview)
            {
                // Toggle intent: if button text is "Preview" user wants to enable preview, else disable
                const bool wantsPreview = (playbackToggleButton.getButtonText() == "Preview");
                onTogglePreview(wantsPreview);
            }
        }
    }

    void TransportBar::timerCallback() {}

    TrackPanel::TrackPanel(NovaStudio::Session& sessionRef)
        : session(sessionRef)
    {
    }

    TrackPanel::~TrackPanel() = default;

    static const juce::Colour kTrackBg0   = juce::Colour::fromRGB(22, 24, 34);
    static const juce::Colour kTrackBg1   = juce::Colour::fromRGB(19, 21, 30);
    static const juce::Colour kTrackSep   = juce::Colour::fromRGB(32, 35, 48);
    static const juce::Colour kArmedCol   = juce::Colour::fromRGB(210, 45, 45);
    static const juce::Colour kMuteCol    = juce::Colour::fromRGB(210, 155, 25);
    static const juce::Colour kSoloCol    = juce::Colour::fromRGB(35, 175, 175);
    static const juce::Colour kBtnDark    = juce::Colour::fromRGB(30, 33, 46);

    void TrackPanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(10, 11, 16));

        // Header bar
        g.setColour(juce::Colour::fromRGB(14, 15, 22));
        g.fillRect(0, 0, getWidth(), 28);
        g.setColour(juce::Colour::fromRGB(40, 44, 60));
        g.fillRect(0, 27, getWidth(), 1);
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
        g.drawText("TRACKS", 36, 0, 80, 28, juce::Justification::centredLeft);

        // Add button (+) in header
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.setFont(juce::Font(juce::FontOptions(16.0f)));
        g.drawText("+", getWidth() - 28, 0, 22, 28, juce::Justification::centred);

        // Palette of track colors matching mockup
        static const juce::Colour trackPalette[] = {
            juce::Colour::fromRGB(120, 80, 200),   // purple — vocal lead
            juce::Colour::fromRGB(80, 100, 200),   // blue-purple — harmony
            juce::Colour::fromRGB(50, 160, 160),   // teal — adlibs
            juce::Colour::fromRGB(60, 120, 200),   // blue — beat
            juce::Colour::fromRGB(40, 180, 160),   // teal-green — bass
            juce::Colour::fromRGB(60, 180, 80),    // green — keys
            juce::Colour::fromRGB(160, 190, 50),   // yellow-green — guitar
            juce::Colour::fromRGB(190, 110, 40),   // orange — FX
        };
        const int numPalette = 8;

        const int numTracks = session.getNumTracks();
        const int W = getWidth();
        const int btnW = 24, btnH = 16, btnGap = 2;
        const int soloX = W - btnW - 6;
        const int muteX = soloX - btnW - btnGap;
        const int armX  = muteX - btnW - btnGap;

        for (int i = 0; i < numTracks; ++i)
        {
            const auto& track = session.getTrack(i);
            const int y = 28 + i * kTrackHeight;
            const juce::Colour trackColor = trackPalette[i % numPalette];

            // Alternating row background
            g.setColour((i % 2 == 0) ? juce::Colour::fromRGB(14, 15, 22)
                                     : juce::Colour::fromRGB(12, 13, 19));
            g.fillRect(0, y, W, kTrackHeight);

            // Armed track: subtle red tint
            if (track.armed)
            {
                g.setColour(juce::Colour::fromRGB(180, 30, 30).withAlpha(0.08f));
                g.fillRect(0, y, W, kTrackHeight);
            }

            // Track number
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.drawText(juce::String(i + 1), 4, y, 18, kTrackHeight, juce::Justification::centred);

            // Color swatch/badge (left side, full height, 4px wide)
            g.setColour(trackColor);
            g.fillRect(22, y + 4, 4, kTrackHeight - 8);

            // Track name
            g.setColour(juce::Colours::white.withAlpha(0.92f));
            g.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
            g.drawText(track.name, 32, y + 8, armX - 36, 18, juce::Justification::left);

            // Track type label
            g.setColour(trackColor.withAlpha(0.7f));
            g.setFont(juce::Font(juce::FontOptions(9.5f)));
            g.drawText(track.type == NovaStudio::TrackType::Audio ? "AUDIO" : "MIDI",
                       32, y + 28, armX - 36, 14, juce::Justification::left);

            // Buttons
            const int btnY = y + (kTrackHeight - btnH) / 2;

            // ARM button
            g.setColour(track.armed ? juce::Colour::fromRGB(220, 50, 50)
                                    : juce::Colour::fromRGB(28, 30, 42));
            g.fillRoundedRectangle((float)armX, (float)btnY, (float)btnW, (float)btnH, 3.0f);
            g.setColour(track.armed ? juce::Colours::white : juce::Colours::white.withAlpha(0.4f));
            g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
            g.drawText("R", armX, btnY, btnW, btnH, juce::Justification::centred);

            // MUTE button
            g.setColour(track.muted ? juce::Colour::fromRGB(200, 150, 20)
                                    : juce::Colour::fromRGB(28, 30, 42));
            g.fillRoundedRectangle((float)muteX, (float)btnY, (float)btnW, (float)btnH, 3.0f);
            g.setColour(track.muted ? juce::Colours::black : juce::Colours::white.withAlpha(0.4f));
            g.drawText("M", muteX, btnY, btnW, btnH, juce::Justification::centred);

            // SOLO button
            g.setColour(track.solo ? juce::Colour::fromRGB(30, 170, 170)
                                   : juce::Colour::fromRGB(28, 30, 42));
            g.fillRoundedRectangle((float)soloX, (float)btnY, (float)btnW, (float)btnH, 3.0f);
            g.setColour(track.solo ? juce::Colours::black : juce::Colours::white.withAlpha(0.4f));
            g.drawText("S", soloX, btnY, btnW, btnH, juce::Justification::centred);

            // Row separator
            g.setColour(juce::Colour::fromRGB(25, 27, 38));
            g.fillRect(0, y + kTrackHeight - 1, W, 1);
        }

        // Right border
        g.setColour(juce::Colour::fromRGB(35, 38, 52));
        g.fillRect(W - 1, 0, 1, getHeight());
    }

    TrackPanel::HitButton TrackPanel::hitTest(int trackIndex, juce::Point<int> pos) const
    {
        const int w = getWidth();
        const int btnW = 24, btnH = 16, btnGap = 2;
        const int soloX = w - btnW - 6;
        const int muteX = soloX - btnW - btnGap;
        const int armX  = muteX - btnW - btnGap;
        const int y     = 28 + trackIndex * kTrackHeight;
        const int btnY  = y + (kTrackHeight - btnH) / 2;

        juce::Rectangle<int> armR  { armX,  btnY, btnW, btnH };
        juce::Rectangle<int> muteR { muteX, btnY, btnW, btnH };
        juce::Rectangle<int> soloR { soloX, btnY, btnW, btnH };

        if (armR.contains(pos))  return HitButton::Arm;
        if (muteR.contains(pos)) return HitButton::Mute;
        if (soloR.contains(pos)) return HitButton::Solo;
        return HitButton::None;
    }

    void TrackPanel::mouseDown(const juce::MouseEvent& e)
    {
        const int numTracks = session.getNumTracks();
        const auto pos = e.getPosition();

        for (int i = 0; i < numTracks; ++i)
        {
            const int y = 28 + i * kTrackHeight;
            if (pos.y >= y && pos.y < y + kTrackHeight)
            {
                switch (hitTest(i, pos))
                {
                    case HitButton::Arm:
                        if (onTrackArm) onTrackArm(i, !session.getTrack(i).armed);
                        break;
                    case HitButton::Mute:
                        if (onTrackMute) onTrackMute(i, !session.getTrack(i).muted);
                        break;
                    case HitButton::Solo:
                        if (onTrackSolo) onTrackSolo(i, !session.getTrack(i).solo);
                        break;
                    default: break;
                }
                repaint();
                return;
            }
        }
    }

    InspectorPanel::InspectorPanel(NovaStudio::ArrangementModel& arrangementModelRef)
        : arrangementModel(arrangementModelRef)
    {
        addAndMakeVisible(tabs);
        tabs.addTab("CHANNEL", juce::Colours::transparentBlack, new juce::Component(), true);
        tabs.addTab("INSERTS", juce::Colours::transparentBlack, new juce::Component(), true);
        tabs.addTab("SENDS",   juce::Colours::transparentBlack, new juce::Component(), true);
        tabs.setColour(juce::TabbedButtonBar::tabTextColourId, juce::Colours::white.withAlpha(0.7f));
        tabs.setColour(juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
        tabs.setColour(juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);

        addAndMakeVisible(titleLabel);
        addAndMakeVisible(selectedClipLabel);
        addAndMakeVisible(trackInfoLabel);
        addAndMakeVisible(gainLabel);
        addAndMakeVisible(gainSlider);
        addAndMakeVisible(fadeInLabel);
        addAndMakeVisible(fadeInSlider);
        addAndMakeVisible(fadeOutLabel);
        addAndMakeVisible(fadeOutSlider);
        addAndMakeVisible(muteToggle);
        addAndMakeVisible(lockToggle);

        titleLabel.setText("Inspector", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        selectedClipLabel.setJustificationType(juce::Justification::centredLeft);
        trackInfoLabel.setJustificationType(juce::Justification::centredLeft);

        gainLabel.setText("Gain", juce::dontSendNotification);
        gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        gainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
        gainSlider.setRange(-24.0, 24.0, 0.1);
        gainSlider.addListener(this);

        fadeInLabel.setText("Fade In", juce::dontSendNotification);
        fadeInSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        fadeInSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
        fadeInSlider.setRange(0.0, 10.0, 0.01);
        fadeInSlider.addListener(this);

        fadeOutLabel.setText("Fade Out", juce::dontSendNotification);
        fadeOutSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        fadeOutSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
        fadeOutSlider.setRange(0.0, 10.0, 0.01);
        fadeOutSlider.addListener(this);

        muteToggle.addListener(this);
        lockToggle.addListener(this);

        arrangementModel.addChangeListener(this);
        refreshContent();
    }

    InspectorPanel::~InspectorPanel()
    {
        arrangementModel.removeChangeListener(this);
    }

    void InspectorPanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(11, 12, 18));
        // left border
        g.setColour(juce::Colour::fromRGB(35, 38, 52));
        g.fillRect(0, 0, 1, getHeight());
    }

    void InspectorPanel::resized()
    {
        auto area = getLocalBounds();
        tabs.setBounds(area.removeFromTop(32));
        area = area.reduced(16);
        titleLabel.setBounds(area.removeFromTop(28));
        area.removeFromTop(12);
        selectedClipLabel.setBounds(area.removeFromTop(24));
        trackInfoLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(12);
        gainLabel.setBounds(area.removeFromTop(18));
        gainSlider.setBounds(area.removeFromTop(36));
        fadeInLabel.setBounds(area.removeFromTop(18));
        fadeInSlider.setBounds(area.removeFromTop(36));
        fadeOutLabel.setBounds(area.removeFromTop(18));
        fadeOutSlider.setBounds(area.removeFromTop(36));
        area.removeFromTop(12);
        muteToggle.setBounds(area.removeFromTop(24));
        lockToggle.setBounds(area.removeFromTop(24));
    }

    void InspectorPanel::changeListenerCallback(juce::ChangeBroadcaster* source)
    {
        if (source == &arrangementModel)
            refreshContent();
    }

    void InspectorPanel::refreshContent()
    {
        const auto* clip = arrangementModel.getSelectedClip();
        if (clip == nullptr)
        {
            selectedClipLabel.setText("No clip selected", juce::dontSendNotification);
            trackInfoLabel.setText("Select a clip to edit properties", juce::dontSendNotification);
            gainSlider.setEnabled(false);
            fadeInSlider.setEnabled(false);
            fadeOutSlider.setEnabled(false);
            muteToggle.setEnabled(false);
            lockToggle.setEnabled(false);
            return;
        }

        selectedClipLabel.setText("Selected: " + clip->file.getFileNameWithoutExtension(), juce::dontSendNotification);
        trackInfoLabel.setText("Start: " + juce::String(clip->startSample) + "   Length: " + juce::String(clip->lengthSamples), juce::dontSendNotification);
        gainSlider.setEnabled(true);
        gainSlider.setValue(clip->gainDb, juce::dontSendNotification);
        fadeInSlider.setEnabled(true);
        fadeInSlider.setValue(clip->fadeInSamples / 44100.0, juce::dontSendNotification);
        fadeOutSlider.setEnabled(true);
        fadeOutSlider.setValue(clip->fadeOutSamples / 44100.0, juce::dontSendNotification);
        muteToggle.setEnabled(true);
        muteToggle.setToggleState(clip->muted, juce::dontSendNotification);
        lockToggle.setEnabled(true);
        lockToggle.setToggleState(clip->locked, juce::dontSendNotification);
    }

    void InspectorPanel::sliderValueChanged(juce::Slider* slider)
    {
        auto* clip = arrangementModel.getSelectedClip();
        if (clip == nullptr)
            return;

        if (slider == &gainSlider)
        {
            arrangementModel.setSelectedClipGain((float)gainSlider.getValue());
        }
        else if (slider == &fadeInSlider)
        {
            const int64_t samples = static_cast<int64_t>(fadeInSlider.getValue() * 44100.0);
            arrangementModel.setSelectedClipFadeIn(samples);
        }
        else if (slider == &fadeOutSlider)
        {
            const int64_t samples = static_cast<int64_t>(fadeOutSlider.getValue() * 44100.0);
            arrangementModel.setSelectedClipFadeOut(samples);
        }
    }

    void InspectorPanel::buttonClicked(juce::Button* button)
    {
        auto* clip = arrangementModel.getSelectedClip();
        if (clip == nullptr)
            return;

        if (button == &muteToggle)
        {
            clip->muted = !clip->muted;
            arrangementModel.sendChangeMessage();
        }
        else if (button == &lockToggle)
        {
            arrangementModel.lockSelectedClip(!clip->locked);
        }
    }

    // ── EditModeToolbar ──────────────────────────────────────────────────────

    // ── EditModeToolbar ──────────────────────────────────────────────────────

    static void styleResBox(juce::ComboBox& box)
    {
        box.setColour(juce::ComboBox::backgroundColourId,  juce::Colour::fromRGB(24, 27, 38));
        box.setColour(juce::ComboBox::outlineColourId,     juce::Colour::fromRGB(50, 55, 72));
        box.setColour(juce::ComboBox::textColourId,        juce::Colours::white.withAlpha(0.85f));
        box.setColour(juce::ComboBox::arrowColourId,       juce::Colours::white.withAlpha(0.5f));
    }

    static void populateResBox(juce::ComboBox& box)
    {
        box.addItem("1 Bar",     1);
        box.addItem("1/2 Note",  2);
        box.addItem("1/4 Note",  3);
        box.addItem("1/8 Note",  4);
        box.addItem("1/16 Note", 5);
        box.addItem("1/32 Note", 6);
        box.addItem("1/64 Note", 7);
        box.addItem("Samples",   8);
    }

    double EditModeToolbar::beatsForResolutionId(int id)
    {
        switch (id)
        {
            case 1: return 4.0;     // 1 Bar  (4 quarter-note beats)
            case 2: return 2.0;     // 1/2
            case 3: return 1.0;     // 1/4
            case 4: return 0.5;     // 1/8
            case 5: return 0.25;    // 1/16
            case 6: return 0.125;   // 1/32
            case 7: return 0.0625;  // 1/64
            default: return 0.0;    // Samples — no beat snap
        }
    }

    EditModeToolbar::EditModeToolbar()
    {
        // Edit mode buttons
        for (auto* b : { &slipBtn, &gridBtn, &spotBtn, &shuffleBtn })
        { addAndMakeVisible(b); b->addListener(this); }

        // Cursor tool buttons
        for (auto* b : { &pointerBtn, &trimBtn, &splitBtn, &fadeBtn })
        { addAndMakeVisible(b); b->addListener(this); }

        // Snap / grid
        addAndMakeVisible(snapBtn);
        snapBtn.addListener(this);
        populateResBox(gridBox);  styleResBox(gridBox);
        gridBox.setSelectedId(1, juce::dontSendNotification);
        gridBox.addListener(this); addAndMakeVisible(gridBox);

        // Nudge
        populateResBox(nudgeBox); styleResBox(nudgeBox);
        nudgeBox.setSelectedId(5, juce::dontSendNotification);
        nudgeBox.addListener(this); addAndMakeVisible(nudgeBox);

        // Zoom
        for (auto* b : { &hZoomInBtn, &hZoomOutBtn, &vZoomInBtn, &vZoomOutBtn })
        { addAndMakeVisible(b); b->addListener(this); }

        // Section labels
        auto setupLabel = [&](juce::Label& l, const char* text) {
            l.setText(text, juce::dontSendNotification);
            l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.45f));
            l.setFont(juce::Font(juce::FontOptions(9.0f)));
            addAndMakeVisible(l);
        };
        setupLabel(modeLabel,  "MODE");
        setupLabel(toolLabel,  "TOOL");
        setupLabel(gridLabel,  "Grid:");
        setupLabel(nudgeLabel, "Nudge:");
        setupLabel(zoomLabel,  "Zoom:");

        shuffleBtn.setEnabled(false);
        shuffleBtn.setAlpha(0.4f);
        shuffleBtn.setTooltip("Shuffle/ripple editing — coming soon");

        setEditMode(EditMode::Slip);
        setCursorTool(CursorTool::Pointer);
    }

    EditModeToolbar::~EditModeToolbar()
    {
        for (auto* b : { &slipBtn, &gridBtn, &spotBtn, &shuffleBtn,
                         &pointerBtn, &trimBtn, &splitBtn, &fadeBtn,
                         &snapBtn, &hZoomInBtn, &hZoomOutBtn, &vZoomInBtn, &vZoomOutBtn })
            b->removeListener(this);
        gridBox.removeListener(this);
        nudgeBox.removeListener(this);
    }

    void EditModeToolbar::paint(juce::Graphics& g)
    {
        g.setColour(juce::Colour::fromRGB(11, 13, 19));
        g.fillAll();
        // Divider below
        g.setColour(juce::Colour::fromRGB(35, 38, 52));
        g.fillRect(0, getHeight() - 1, getWidth(), 1);

        // Section dividers between groups
        const juce::Colour div = juce::Colour::fromRGB(35, 38, 52);
        // (The paint-only divider lines between groups — positions determined in resized())
    }

    void EditModeToolbar::resized()
    {
        auto area = getLocalBounds().reduced(4, 3);
        const int g1 = 2, g2 = 10;
        const int bW = 38, sW = 28;

        // ── Mode group ───────────────────────────────────────────────
        modeLabel.setBounds(area.removeFromLeft(38)); area.removeFromLeft(g1);
        slipBtn.setBounds(area.removeFromLeft(bW));    area.removeFromLeft(g1);
        gridBtn.setBounds(area.removeFromLeft(bW));    area.removeFromLeft(g1);
        spotBtn.setBounds(area.removeFromLeft(bW));    area.removeFromLeft(g1);
        shuffleBtn.setBounds(area.removeFromLeft(bW)); area.removeFromLeft(g2);

        // ── Tool group ───────────────────────────────────────────────
        toolLabel.setBounds(area.removeFromLeft(32));  area.removeFromLeft(g1);
        pointerBtn.setBounds(area.removeFromLeft(bW)); area.removeFromLeft(g1);
        trimBtn.setBounds(area.removeFromLeft(bW));    area.removeFromLeft(g1);
        splitBtn.setBounds(area.removeFromLeft(bW));   area.removeFromLeft(g1);
        fadeBtn.setBounds(area.removeFromLeft(bW));    area.removeFromLeft(g2);

        // ── Snap/grid ─────────────────────────────────────────────────
        snapBtn.setBounds(area.removeFromLeft(bW));    area.removeFromLeft(g1);
        gridLabel.setBounds(area.removeFromLeft(28));  area.removeFromLeft(g1);
        gridBox.setBounds(area.removeFromLeft(82));    area.removeFromLeft(g2);

        // ── Nudge ──────────────────────────────────────────────────────
        nudgeLabel.setBounds(area.removeFromLeft(36)); area.removeFromLeft(g1);
        nudgeBox.setBounds(area.removeFromLeft(82));   area.removeFromLeft(g2);

        // ── Zoom ───────────────────────────────────────────────────────
        zoomLabel.setBounds(area.removeFromLeft(30));  area.removeFromLeft(g1);
        hZoomOutBtn.setBounds(area.removeFromLeft(sW)); area.removeFromLeft(g1);
        hZoomInBtn.setBounds(area.removeFromLeft(sW));  area.removeFromLeft(g1);
        vZoomOutBtn.setBounds(area.removeFromLeft(sW)); area.removeFromLeft(g1);
        vZoomInBtn.setBounds(area.removeFromLeft(sW));
    }

    void EditModeToolbar::setEditMode(EditMode m)
    {
        currentMode = m;
        const auto gold    = juce::Colour::fromRGB(180, 140, 60);
        const auto dim     = juce::Colour::fromRGB(24, 28, 42);
        const auto snapOn  = juce::Colour::fromRGB(40, 150, 70);

        auto style = [&](juce::TextButton& btn, bool active, juce::Colour ac) {
            btn.setColour(juce::TextButton::buttonColourId,  active ? ac : dim);
            btn.setColour(juce::TextButton::buttonOnColourId, ac);
            btn.setColour(juce::TextButton::textColourOffId,  juce::Colours::white);
            btn.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
        };

        style(slipBtn,    m == EditMode::Slip,  gold);
        style(gridBtn,    m == EditMode::Grid,  gold);
        style(spotBtn,    m == EditMode::Spot,  gold);
        style(shuffleBtn, false, dim); // always dim

        snapBtn.setColour(juce::TextButton::buttonColourId, snapEnabled ? snapOn : dim);
        snapBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }

    void EditModeToolbar::setCursorTool(CursorTool t)
    {
        currentTool = t;
        const auto teal = juce::Colour::fromRGB(40, 140, 180);
        const auto dim  = juce::Colour::fromRGB(24, 28, 42);

        auto style = [&](juce::TextButton& btn, bool active) {
            btn.setColour(juce::TextButton::buttonColourId,  active ? teal : dim);
            btn.setColour(juce::TextButton::buttonOnColourId, teal);
            btn.setColour(juce::TextButton::textColourOffId,  juce::Colours::white);
            btn.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
        };

        style(pointerBtn, t == CursorTool::Pointer);
        style(trimBtn,    t == CursorTool::Trim);
        style(splitBtn,   t == CursorTool::Split);
        style(fadeBtn,    t == CursorTool::Fade);
    }

    void EditModeToolbar::buttonClicked(juce::Button* b)
    {
        // Edit mode
        if      (b == &slipBtn)    { setEditMode(EditMode::Slip);  if (onEditModeChanged) onEditModeChanged(EditMode::Slip); }
        else if (b == &gridBtn)    { setEditMode(EditMode::Grid);  if (onEditModeChanged) onEditModeChanged(EditMode::Grid); }
        else if (b == &spotBtn)    { setEditMode(EditMode::Spot);  if (onEditModeChanged) onEditModeChanged(EditMode::Spot); }
        else if (b == &shuffleBtn) { /* placeholder */ }
        // Cursor tools
        else if (b == &pointerBtn) { setCursorTool(CursorTool::Pointer); if (onCursorToolChanged) onCursorToolChanged(CursorTool::Pointer); }
        else if (b == &trimBtn)    { setCursorTool(CursorTool::Trim);    if (onCursorToolChanged) onCursorToolChanged(CursorTool::Trim); }
        else if (b == &splitBtn)   { setCursorTool(CursorTool::Split);   if (onCursorToolChanged) onCursorToolChanged(CursorTool::Split); }
        else if (b == &fadeBtn)    { setCursorTool(CursorTool::Fade);    if (onCursorToolChanged) onCursorToolChanged(CursorTool::Fade); }
        // Snap
        else if (b == &snapBtn)    { snapEnabled = !snapEnabled; setEditMode(currentMode); if (onSnapEnabled) onSnapEnabled(snapEnabled); }
        // Zoom
        else if (b == &hZoomInBtn)  { if (onHZoomChanged) onHZoomChanged(+1); }
        else if (b == &hZoomOutBtn) { if (onHZoomChanged) onHZoomChanged(-1); }
        else if (b == &vZoomInBtn)  { if (onVZoomChanged) onVZoomChanged(+1); }
        else if (b == &vZoomOutBtn) { if (onVZoomChanged) onVZoomChanged(-1); }
    }

    void EditModeToolbar::comboBoxChanged(juce::ComboBox* c)
    {
        if (c == &gridBox)
        {
            snapBeats = beatsForResolutionId(gridBox.getSelectedId());
            if (onSnapResolutionChanged) onSnapResolutionChanged(snapBeats);
        }
        else if (c == &nudgeBox)
        {
            nudgeBeats = beatsForResolutionId(nudgeBox.getSelectedId());
            if (onNudgeResolutionChanged) onNudgeResolutionChanged(nudgeBeats);
        }
    }

    // ── ArrangementView ──────────────────────────────────────────────────────

    ArrangementView::ArrangementView(NovaStudio::TransportState& transport,
                                     NovaStudio::TimelineModel& timelineModel,
                                     NovaStudio::ArrangementModel& arrangementModel)
        : transportState(transport), timelineModel(timelineModel), arrangementModel(arrangementModel)
    {
        transportState.addChangeListener(this);
        arrangementModel.addChangeListener(this);
        startTimerHz(30);
        setWantsKeyboardFocus(true);
    }

    ArrangementView::~ArrangementView()
    {
        transportState.removeChangeListener(this);
        arrangementModel.removeChangeListener(this);
    }

    void ArrangementView::adjustHZoom(int direction)
    {
        if (direction > 0)
            zoomFactor = juce::jmin(zoomFactor * 1.5, 64.0);
        else
            zoomFactor = juce::jmax(zoomFactor / 1.5, 0.05);
        timelineModel.setZoomFactor(zoomFactor);
        repaint();
    }

    void ArrangementView::adjustVZoom(int direction)
    {
        if (direction > 0)
            trackHeightPx = juce::jmin(trackHeightPx + 16, 192);
        else
            trackHeightPx = juce::jmax(trackHeightPx - 16, 32);
        repaint();
    }

    void ArrangementView::nudgeSelected(int direction)
    {
        const auto selected = arrangementModel.getSelectedClips();
        if (selected.isEmpty()) return;

        const double sr    = arrangementModel.getSession().getSampleRate();
        const double tempo = arrangementModel.getSession().getTempo();
        if (sr <= 0.0 || tempo <= 0.0) return;

        int64_t deltaSamples = 0;
        if (nudgeBeats > 0.0)
        {
            const double samplesPerBeat = (60.0 / tempo) * sr;
            deltaSamples = static_cast<int64_t>(nudgeBeats * samplesPerBeat);
        }
        else
        {
            deltaSamples = 1; // sample-accurate mode
        }

        arrangementModel.moveClipsBySamples(selected, direction * deltaSamples);
        repaint();
    }

    bool ArrangementView::keyPressed(const juce::KeyPress& key)
    {
        const int kc = key.getKeyCode();

        // Nudge right: + or = (with or without shift)
        if (kc == '+' || kc == '=' || kc == juce::KeyPress::numberPadAdd)
        {
            nudgeSelected(+1);
            return true;
        }
        // Nudge left: -
        if (kc == '-' || kc == juce::KeyPress::numberPadSubtract)
        {
            nudgeSelected(-1);
            return true;
        }
        // Also support Alt+Arrow
        if (key.getModifiers().isAltDown())
        {
            if (kc == juce::KeyPress::rightKey) { nudgeSelected(+1); return true; }
            if (kc == juce::KeyPress::leftKey)  { nudgeSelected(-1); return true; }
        }
        return false;
    }

    void ArrangementView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        if (e.mods.isCommandDown())
            adjustHZoom(wheel.deltaY > 0 ? +1 : -1);
        else
            juce::Component::mouseWheelMove(e, wheel);
    }

    int64_t ArrangementView::snapToGrid(int64_t sample) const
    {
        if (!snapEnabled || snapBeats <= 0.0)
            return sample;
        const double sr    = arrangementModel.getSession().getSampleRate();
        const double tempo = arrangementModel.getSession().getTempo();
        if (sr <= 0.0 || tempo <= 0.0) return sample;
        const double samplesPerBeat = (60.0 / tempo) * sr;
        return static_cast<int64_t>(std::round(static_cast<double>(sample) / (snapBeats * samplesPerBeat))
                                    * (snapBeats * samplesPerBeat));
    }

    void ArrangementView::showSpotDialog(int trackIndex, int clipIndex)
    {
        const auto* clip = &arrangementModel.getSession().getTrack(trackIndex).clips.getReference(clipIndex);
        const double sr    = arrangementModel.getSession().getSampleRate();
        const double tempo = arrangementModel.getSession().getTempo();
        if (sr <= 0.0 || tempo <= 0.0) return;

        const double samplesPerBeat = (60.0 / tempo) * sr;
        const double currentBeat    = static_cast<double>(clip->startSample) / samplesPerBeat;
        const int    currentBar     = static_cast<int>(currentBeat / 4) + 1;
        const int    beatInBar      = static_cast<int>(currentBeat) % 4 + 1;

        auto* dialog = new juce::AlertWindow("Spot — Move Clip to Position",
                                             "Enter the destination Bar and Beat:",
                                             juce::MessageBoxIconType::NoIcon);
        dialog->addTextEditor("bar",  juce::String(currentBar),  "Bar:");
        dialog->addTextEditor("beat", juce::String(beatInBar),   "Beat (1–4):");
        dialog->addButton("Move",   1);
        dialog->addButton("Cancel", 0);
        dialog->setColour(juce::AlertWindow::backgroundColourId, juce::Colour::fromRGB(18, 20, 28));
        dialog->setColour(juce::AlertWindow::textColourId,       juce::Colours::white);

        dialog->enterModalState(true,
            juce::ModalCallbackFunction::create([this, dialog, trackIndex, clipIndex,
                                                 samplesPerBeat](int result)
            {
                if (result == 1)
                {
                    const int bar  = juce::jmax(1, dialog->getTextEditorContents("bar").getIntValue());
                    const int beat = juce::jlimit(1, 4, dialog->getTextEditorContents("beat").getIntValue());
                    const double targetBeat = (bar - 1) * 4.0 + (beat - 1);
                    const int64_t targetSample = static_cast<int64_t>(targetBeat * samplesPerBeat);

                    auto clipCopy = arrangementModel.getSession()
                                                    .getTrack(trackIndex)
                                                    .clips.getReference(clipIndex);
                    clipCopy.startSample = juce::jmax<int64_t>(0, targetSample);
                    arrangementModel.replaceClipWithoutUndo(trackIndex, clipIndex, clipCopy);
                    arrangementModel.sendChangeMessage();
                    repaint();
                }
            }), true);
    }

    // In paint, draw waveform using cache where available and simple handles for selected clips

    void ArrangementView::paint(juce::Graphics& g)
    {
        const int W = getWidth();
        const int H = getHeight();

        // Overall background
        g.setColour(juce::Colour::fromRGB(12, 13, 19));
        g.fillRect(0, 0, W, H);

        const int rulerH = 28;  // matches TrackPanel header height
        const double pixelsPerBeat = timelineModel.getPixelsPerBeat();
        const int beatsPerBar = timelineModel.getBeatsPerBar();
        const int64_t currentSample = transportState.getPositionSamples();
        const double currentX = timelineModel.getXForSamplePosition(currentSample, W);
        const int width = W;

        // Ruler
        g.setColour(juce::Colour::fromRGB(20, 23, 33));
        g.fillRect(0, 0, W, rulerH);
        g.setColour(juce::Colour::fromRGB(35, 38, 52));
        g.fillRect(0, rulerH - 1, W, 1);

        const int numBars = static_cast<int>(std::ceil((W / pixelsPerBeat) / beatsPerBar)) + 2;
        for (int bar = 0; bar < numBars; ++bar)
        {
            const double x = bar * beatsPerBar * pixelsPerBeat;
            // Bar line
            g.setColour(juce::Colour::fromRGB(50, 55, 72));
            g.drawLine((float)x, 0.0f, (float)x, (float)rulerH, 1.0f);
            // Bar label
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText(juce::String(bar + 1), (int)x + 4, 6, 40, 14, juce::Justification::left);
            // Beat subdivisions
            for (int sub = 1; sub < beatsPerBar; ++sub)
            {
                const double beatX = x + sub * pixelsPerBeat;
                g.setColour(juce::Colour::fromRGB(38, 42, 58));
                g.drawLine((float)beatX, (float)(rulerH / 2), (float)beatX, (float)rulerH, 0.8f);
            }
        }

        // Section markers (drawn over ruler — decorative)
        if (arrangementModel.getSession().getNumTracks() > 0)
        {
            struct Section { const char* name; float beatPos; juce::Colour color; };
            static const Section sections[] = {
                {"INTRO",    0.0f,  juce::Colour::fromRGB(100, 100, 140)},
                {"VERSE 1",  8.0f,  juce::Colour::fromRGB(80, 120, 160)},
                {"HOOK",     24.0f, juce::Colour::fromRGB(140, 80, 160)},
                {"VERSE 2",  40.0f, juce::Colour::fromRGB(80, 120, 160)},
                {"OUTRO",    56.0f, juce::Colour::fromRGB(100, 100, 140)},
            };
            for (auto& sec : sections)
            {
                const double secX = sec.beatPos * pixelsPerBeat;
                if (secX < 0 || secX > W) continue;
                g.setColour(sec.color.withAlpha(0.6f));
                g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
                g.drawText(sec.name, (int)secX + 4, 2, 80, 12, juce::Justification::left);
            }
        }

        // Track lanes — height follows vertical zoom setting
        const float trackHeight = (float)trackHeightPx;
        const NovaStudio::Session& session = arrangementModel.getSession();
        const int trackCount = session.getNumTracks();
        const float lanesTop = (float)rulerH;
        const int visibleTracks = juce::jmin(trackCount, static_cast<int>((H - rulerH) / trackHeight));

        for (int trackIndex = 0; trackIndex < visibleTracks; ++trackIndex)
        {
            const float top = lanesTop + trackIndex * trackHeight;
            const auto& track = session.getTrack(trackIndex);

            // Lane background
            g.setColour((trackIndex % 2 == 0) ? juce::Colour::fromRGB(16, 18, 26)
                                              : juce::Colour::fromRGB(14, 16, 22));
            g.fillRect(0.0f, top, (float)W, trackHeight);

            // Armed track tint
            if (track.armed)
            {
                g.setColour(juce::Colour::fromRGB(200, 30, 30).withAlpha(0.06f));
                g.fillRect(0.0f, top, (float)W, trackHeight);
            }

            // Beat grid lines within lane
            for (int bar = 0; bar < numBars; ++bar)
            {
                const double x = bar * beatsPerBar * pixelsPerBeat;
                g.setColour(juce::Colour::fromRGB(28, 31, 44));
                g.drawLine((float)x, top, (float)x, top + trackHeight, 1.0f);
                for (int sub = 1; sub < beatsPerBar; ++sub)
                {
                    const double beatX = x + sub * pixelsPerBeat;
                    g.setColour(juce::Colour::fromRGB(22, 24, 35));
                    g.drawLine((float)beatX, top, (float)beatX, top + trackHeight, 0.5f);
                }
            }

            // Lane separator
            g.setColour(juce::Colour::fromRGB(28, 31, 44));
            g.drawLine(0.0f, top + trackHeight - 1.0f, (float)W, top + trackHeight - 1.0f, 1.0f);

            for (int clipIndex = 0; clipIndex < track.clips.size(); ++clipIndex)
            {
                const auto& clip = track.clips.getReference(clipIndex);
                const double clipStartX = timelineModel.getXForSamplePosition(clip.startSample, width);
                const double clipEndX   = timelineModel.getXForSamplePosition(clip.startSample + clip.lengthSamples, width);
                const float clipWidth  = (float)juce::jmax(8.0, clipEndX - clipStartX);
                const float clipY      = top + 6.0f;
                const float clipHeight = trackHeight - 12.0f;
                const float rowHeight  = clipHeight;
                const bool clipSelectedSingle = (trackIndex == arrangementModel.getSelectedTrackIndex() && clipIndex == arrangementModel.getSelectedClipIndex());
                const auto& selected = arrangementModel.getSelectedClips();
                const bool clipMultiSelected = selected.contains(juce::Point<int>(trackIndex, clipIndex));

                static const juce::Colour palette[] = {
                    juce::Colour::fromRGB(100, 65, 175),
                    juce::Colour::fromRGB(65, 85, 175),
                    juce::Colour::fromRGB(40, 140, 140),
                    juce::Colour::fromRGB(50, 100, 175),
                    juce::Colour::fromRGB(35, 155, 140),
                    juce::Colour::fromRGB(50, 155, 70),
                    juce::Colour::fromRGB(140, 165, 45),
                    juce::Colour::fromRGB(165, 95, 35),
                };
                juce::Colour fillColor = palette[trackIndex % 8];
                if (clip.muted) fillColor = fillColor.withAlpha(0.25f);
                else if (clipSelectedSingle || clipMultiSelected) fillColor = fillColor.brighter(0.3f);

                g.setColour(fillColor);
                g.fillRoundedRectangle((float)clipStartX, clipY, clipWidth, clipHeight, 8.0f);

                g.setColour(juce::Colours::black.withAlpha(0.12f));
                g.drawRoundedRectangle((float)clipStartX, clipY, clipWidth, clipHeight, 8.0f, 1.4f);

                const bool isGuideClip = arrangementModel.isGuideClip(trackIndex, clipIndex);
                const bool isTargetClip = arrangementModel.isAlignTargetClip(trackIndex, clipIndex);
                const bool isPreviewClip = arrangementModel.isPreviewClip(trackIndex, clipIndex);

                if (clipSelectedSingle || clipMultiSelected)
                {
                    g.setColour(juce::Colours::gold.withAlpha(0.9f));
                    const float stroke = clipMultiSelected && !clipSelectedSingle ? 3.0f : 2.0f;
                    g.drawRoundedRectangle((float)clipStartX, clipY, clipWidth, clipHeight, 8.0f, stroke);
                    if (clipMultiSelected && !clipSelectedSingle)
                    {
                        g.setColour(juce::Colours::gold.withAlpha(0.16f));
                        g.fillRoundedRectangle((float)clipStartX + 2.0f, clipY + 2.0f, clipWidth - 4.0f, clipHeight - 4.0f, 8.0f);
                    }
                }

                if (isGuideClip)
                {
                    g.setColour(juce::Colours::lime.withAlpha(0.88f));
                    g.drawRoundedRectangle((float)clipStartX + 2.0f, clipY + 2.0f, clipWidth - 4.0f, clipHeight - 4.0f, 8.0f, 2.0f);
                }
                else if (isPreviewClip)
                {
                    g.setColour(juce::Colours::aqua.withAlpha(0.75f));
                    g.drawRoundedRectangle((float)clipStartX + 2.0f, clipY + 2.0f, clipWidth - 4.0f, clipHeight - 4.0f, 8.0f, 2.0f);
                }
                else if (isTargetClip)
                {
                    g.setColour(juce::Colours::mediumvioletred.withAlpha(0.7f));
                    g.drawRoundedRectangle((float)clipStartX + 2.0f, clipY + 2.0f, clipWidth - 4.0f, clipHeight - 4.0f, 8.0f, 2.0f);
                }

                juce::String clipTitle = clip.file.existsAsFile() ? clip.file.getFileNameWithoutExtension() : "Clip";
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                g.setFont(juce::Font(11.0f, juce::Font::bold));
                g.drawText(clipTitle, (int)clipStartX + 8, (int)clipY + 6, (int)clipWidth - 16, 16, juce::Justification::left);

                // Draw waveform using cache if available
                if (clip.file.existsAsFile() && clipWidth > 24.0f)
                {
                    const int samplesPerPixel = juce::jmax<int>(1, static_cast<int>(clip.lengthSamples / clipWidth));
                    if (!waveformCache.isCached(clip.file))
                        waveformCache.ensureCachedAsync(clip.file, samplesPerPixel);

                    const auto peaks = waveformCache.getPeaks(clip.file);
                    const int blocks = (int)peaks.size() / 2;
                    if (blocks > 0)
                    {
                        const float centerY = clipY + clipHeight * 0.5f;
                        const float amp = clipHeight * 0.45f;
                        for (int b = 0; b < blocks; ++b)
                        {
                            const float minV = peaks[b * 2];
                            const float maxV = peaks[b * 2 + 1];
                            const float nx = (float)clipStartX + (b / (float)blocks) * clipWidth;
                            const float x2 = nx + (clipWidth / (float)blocks);
                            const float y1 = centerY - (maxV * amp);
                            const float y2 = centerY - (minV * amp);
                            g.setColour(juce::Colours::white.withAlpha(0.35f));
                            g.drawLine(nx, y1, nx, y2, juce::jmax(1.0f, clipWidth / blocks * 0.6f));
                        }
                    }
                }

                // Draw clip fade regions and trim handles for selected clips
                if (clipSelectedSingle || clipMultiSelected)
                {
                    const float handleW = 10.0f;
                    const float handleH = 12.0f;
                    const float leftHandleX = (float)clipStartX - 1.0f;
                    const float rightHandleX = (float)clipStartX + clipWidth - handleW + 1.0f;

                    g.setColour(juce::Colours::white.withAlpha(0.9f));
                    g.fillRoundedRectangle(leftHandleX, clipY + clipHeight - handleH, handleW, handleH, 2.0f);
                    g.fillRoundedRectangle(rightHandleX, clipY + clipHeight - handleH, handleW, handleH, 2.0f);

                    const float fadeInX = (float)clipStartX + juce::jmin<float>((float)clip.fadeInSamples / (float)clip.lengthSamples, 1.0f) * clipWidth;
                    const float fadeOutX = (float)clipStartX + clipWidth - juce::jmin<float>((float)clip.fadeOutSamples / (float)clip.lengthSamples, 1.0f) * clipWidth;
                    g.setColour(juce::Colours::white.withAlpha(0.14f));
                    {
                        juce::Path fadeInPath;
                        fadeInPath.startNewSubPath((float)clipStartX, clipY + clipHeight);
                        fadeInPath.lineTo(fadeInX, clipY);
                        fadeInPath.lineTo(fadeInX, clipY + clipHeight);
                        fadeInPath.closeSubPath();
                        g.fillPath(fadeInPath);
                    }
                    {
                        juce::Path fadeOutPath;
                        fadeOutPath.startNewSubPath((float)clipStartX + clipWidth, clipY + clipHeight);
                        fadeOutPath.lineTo(fadeOutX, clipY);
                        fadeOutPath.lineTo(fadeOutX, clipY + clipHeight);
                        fadeOutPath.closeSubPath();
                        g.fillPath(fadeOutPath);
                    }
                    g.setColour(juce::Colours::white.withAlpha(0.22f));
                    g.drawLine(fadeInX, clipY, fadeInX, clipY + clipHeight, 1.2f);
                    g.drawLine(fadeOutX, clipY, fadeOutX, clipY + clipHeight, 1.2f);
                }
            }
        }

        // Loop region — drawn in ruler bar + shaded lane area
        if (transportState.isLooping() && transportState.hasLoopRange())
        {
            const double lsX = timelineModel.getXForSamplePosition(transportState.getLoopStartSample(), width);
            const double leX = timelineModel.getXForSamplePosition(transportState.getLoopEndSample(),   width);
            const float lsXf = (float)lsX, leXf = (float)leX;
            const float loopW = juce::jmax(2.0f, leXf - lsXf);

            // Shaded lane region
            g.setColour(juce::Colour::fromRGB(180, 140, 60).withAlpha(0.07f));
            g.fillRect(lsXf, (float)rulerH, loopW, (float)(H - rulerH));

            // Ruler brace bar
            g.setColour(juce::Colour::fromRGB(220, 170, 50).withAlpha(0.75f));
            g.fillRect(lsXf, 4.0f, loopW, 10.0f);

            // Start / end bracket handles (draggable)
            g.setColour(juce::Colour::fromRGB(255, 200, 60));
            g.fillRect(lsXf - 2.0f, 0.0f, 4.0f, (float)rulerH);
            g.fillRect(leXf - 2.0f, 0.0f, 4.0f, (float)rulerH);

            // Labels
            g.setColour(juce::Colour::fromRGB(10, 10, 10));
            g.setFont(juce::Font(juce::FontOptions(8.0f).withStyle("Bold")));
            g.drawText("IN",  (int)lsXf + 3, 5, 20, 10, juce::Justification::left);
            g.drawText("OUT", (int)leXf - 22, 5, 22, 10, juce::Justification::right);
        }

        // Playhead — red line with glow + triangular scrub head
        const float playheadX = (float)currentX;
        // Soft glow behind the line
        g.setColour(juce::Colour::fromRGB(255, 50, 50).withAlpha(0.12f));
        g.fillRect(playheadX - 4.0f, 0.0f, 8.0f, (float)H);
        // Line
        g.setColour(juce::Colour::fromRGB(255, 55, 55).withAlpha(0.92f));
        g.drawLine(playheadX, 0.0f, playheadX, (float)H, 1.8f);
        // Triangle scrub head — acts as grab target
        juce::Path arrow;
        arrow.addTriangle(playheadX - 7.0f, 0.0f, playheadX + 7.0f, 0.0f, playheadX, 12.0f);
        g.setColour(juce::Colour::fromRGB(255, 55, 55));
        g.fillPath(arrow);
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.strokePath(arrow, juce::PathStrokeType(0.8f));

        // Marquee visual
        if (isMarqueeSelecting && marqueeRect.getWidth() > 0 && marqueeRect.getHeight() > 0)
        {
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.fillRect(marqueeRect);
            g.setColour(juce::Colours::white.withAlpha(0.45f));
            g.drawRect(marqueeRect.getX(), marqueeRect.getY(), marqueeRect.getWidth(), marqueeRect.getHeight(), 1.2f);
        }
    }

    void ArrangementView::resized() {}

    // Helper: convert X pixel to sample position (with optional grid snap)
    static int64_t xToSample(float x, const NovaStudio::TimelineModel& tm, bool snapEnabled,
                              double snapBeats, const NovaStudio::Session& session)
    {
        const int64_t raw = static_cast<int64_t>(tm.getSamplePositionForX(static_cast<int>(x)));
        if (!snapEnabled || snapBeats <= 0.0) return juce::jmax<int64_t>(0, raw);
        const double sr    = session.getSampleRate();
        const double tempo = session.getTempo();
        if (sr <= 0.0 || tempo <= 0.0) return juce::jmax<int64_t>(0, raw);
        const double snapSamples = snapBeats * (60.0 / tempo) * sr;
        return static_cast<int64_t>(std::round(static_cast<double>(raw) / snapSamples) * snapSamples);
    }

    void ArrangementView::updateCursorForTool()
    {
        switch (cursorTool)
        {
            case EditModeToolbar::CursorTool::Trim:
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); break;
            case EditModeToolbar::CursorTool::Split:
                setMouseCursor(juce::MouseCursor::CrosshairCursor); break;
            case EditModeToolbar::CursorTool::Fade:
                setMouseCursor(juce::MouseCursor::UpDownLeftRightResizeCursor); break;
            default:
                setMouseCursor(juce::MouseCursor::NormalCursor); break;
        }
    }

    ArrangementView::HoverZone ArrangementView::hoverZone(juce::Point<float> pos, int& outTrack, int& outClip) const
    {
        outTrack = -1;
        outClip  = -1;
        const int rulerH = 28;
        if (pos.y < (float)rulerH) return HoverZone::None;

        const float trackHeight = (float)trackHeightPx;
        const int   width       = getWidth();
        const auto& session     = arrangementModel.getSession();
        const int   trackIndex  = static_cast<int>((pos.y - rulerH) / trackHeight);
        if (trackIndex < 0 || trackIndex >= session.getNumTracks()) return HoverZone::None;

        const auto& track = session.getTrack(trackIndex);
        for (int ci = 0; ci < track.clips.size(); ++ci)
        {
            const auto& clip    = track.clips.getReference(ci);
            const float cStartX = (float)timelineModel.getXForSamplePosition(clip.startSample, width);
            const float cEndX   = (float)timelineModel.getXForSamplePosition(clip.startSample + clip.lengthSamples, width);
            const float clipY   = (float)rulerH + trackIndex * trackHeight + 6.0f;
            const float clipH   = trackHeight - 12.0f;
            const auto  cRect   = juce::Rectangle<float>(cStartX, clipY, cEndX - cStartX, clipH);
            if (!cRect.contains(pos)) continue;

            outTrack = trackIndex;
            outClip  = ci;

            const float edgeTol = 8.0f;
            if (pos.x < cStartX + edgeTol) return HoverZone::ClipLeftEdge;
            if (pos.x > cEndX   - edgeTol) return HoverZone::ClipRightEdge;

            const float clipW = cEndX - cStartX;
            if (clip.fadeInSamples > 0)
            {
                const float fadeInX = cStartX + juce::jmin(1.0f, (float)clip.fadeInSamples / (float)clip.lengthSamples) * clipW;
                if (pos.x < fadeInX) return HoverZone::ClipFadeIn;
            }
            if (clip.fadeOutSamples > 0)
            {
                const float fadeOutX = cEndX - juce::jmin(1.0f, (float)clip.fadeOutSamples / (float)clip.lengthSamples) * clipW;
                if (pos.x > fadeOutX) return HoverZone::ClipFadeOut;
            }
            return HoverZone::ClipBody;
        }
        return HoverZone::None;
    }

    void ArrangementView::mouseMove(const juce::MouseEvent& event)
    {
        const float y = event.position.y;
        const float x = event.position.x;
        const int   rulerH = 28;

        if (y < (float)rulerH)
        {
            const double phX = timelineModel.getXForSamplePosition(transportState.getPositionSamples(), getWidth());
            if (std::abs(x - (float)phX) < 8.0f)
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            else
                setMouseCursor(juce::MouseCursor::PointingHandCursor);
            return;
        }

        int ht = -1, hc = -1;
        const auto zone = hoverZone(event.position, ht, hc);

        switch (cursorTool)
        {
            case EditModeToolbar::CursorTool::Trim:
                setMouseCursor((zone == HoverZone::ClipLeftEdge || zone == HoverZone::ClipRightEdge)
                               ? juce::MouseCursor::LeftRightResizeCursor
                               : juce::MouseCursor::NormalCursor);
                break;
            case EditModeToolbar::CursorTool::Split:
                setMouseCursor((zone != HoverZone::None)
                               ? juce::MouseCursor::CrosshairCursor
                               : juce::MouseCursor::NormalCursor);
                break;
            case EditModeToolbar::CursorTool::Fade:
                setMouseCursor((zone != HoverZone::None)
                               ? juce::MouseCursor::UpDownLeftRightResizeCursor
                               : juce::MouseCursor::NormalCursor);
                break;
            default:
                setMouseCursor(juce::MouseCursor::NormalCursor);
                break;
        }
    }

    void ArrangementView::mouseDown(const juce::MouseEvent& event)
    {
        grabKeyboardFocus();

        const auto clickPoint = event.position;
        const int  rulerH  = 28;
        const int  width   = getWidth();

        // ── Ruler area: playhead placement / loop brace drag ─────────────────
        if (clickPoint.y < (float)rulerH)
        {
            const bool snapNow = (snapEnabled && editMode == EditModeToolbar::EditMode::Grid);
            const auto& session = arrangementModel.getSession();

            // Check loop brace handles first (only when loop is active)
            if (transportState.isLooping() && transportState.hasLoopRange())
            {
                const int64_t ls = transportState.getLoopStartSample();
                const int64_t le = transportState.getLoopEndSample();
                const float lsX  = (float)timelineModel.getXForSamplePosition(ls, width);
                const float leX  = (float)timelineModel.getXForSamplePosition(le, width);

                if (std::abs(clickPoint.x - lsX) < 10.0f)
                {
                    loopDragHandle = LoopDragHandle::Start;
                    loopOrigStart  = ls;
                    loopOrigEnd    = le;
                    loopDragStartSample = xToSample(clickPoint.x, timelineModel, snapNow, snapBeats, session);
                    return;
                }
                if (std::abs(clickPoint.x - leX) < 10.0f)
                {
                    loopDragHandle = LoopDragHandle::End;
                    loopOrigStart  = ls;
                    loopOrigEnd    = le;
                    loopDragStartSample = xToSample(clickPoint.x, timelineModel, snapNow, snapBeats, session);
                    return;
                }
                if (clickPoint.x > lsX && clickPoint.x < leX)
                {
                    loopDragHandle = LoopDragHandle::Body;
                    loopOrigStart  = ls;
                    loopOrigEnd    = le;
                    loopDragStartSample = xToSample(clickPoint.x, timelineModel, snapNow, snapBeats, session);
                    return;
                }
            }

            // Click in ruler → move playhead + start scrub
            isDraggingPlayhead = true;
            const int64_t pos = xToSample(clickPoint.x, timelineModel, snapNow, snapBeats, session);
            transportState.setPositionSamples(juce::jmax<int64_t>(0, pos), true);
            repaint();
            return;
        }

        // ── Track lanes ──────────────────────────────────────────────────────
        const float trackHeight = (float)trackHeightPx;
        const float lanesTop = (float)rulerH;

        const int trackIndex = static_cast<int>((clickPoint.y - lanesTop) / trackHeight);
        if (trackIndex < 0 || trackIndex >= arrangementModel.getSession().getNumTracks())
        {
            arrangementModel.clearSelection();
            return;
        }

        const auto& track = arrangementModel.getSession().getTrack(trackIndex);
        bool found = false;
        for (int clipIndex = 0; clipIndex < track.clips.size(); ++clipIndex)
        {
            const auto& clip = track.clips.getReference(clipIndex);
            const double clipStartX = timelineModel.getXForSamplePosition(clip.startSample, width);
            const double clipEndX = timelineModel.getXForSamplePosition(clip.startSample + clip.lengthSamples, width);
            const float clipY = lanesTop + trackIndex * trackHeight + 6.0f;
            const float clipH = trackHeight - 12.0f;
            const auto clipRect = juce::Rectangle<float>((float)clipStartX, clipY, (float)juce::jmax(8.0, clipEndX - clipStartX), clipH);
            const float handleW = 10.0f;
            const auto leftHandle = juce::Rectangle<float>((float)clipStartX - 1.0f, clipRect.getBottom() - 12.0f, handleW, 10.0f);
            const auto rightHandle = juce::Rectangle<float>((float)clipEndX - handleW + 1.0f, clipRect.getBottom() - 12.0f, handleW, 10.0f);
            const bool clipSelectedSingle = (trackIndex == arrangementModel.getSelectedTrackIndex() && clipIndex == arrangementModel.getSelectedClipIndex());

            if (clipSelectedSingle && leftHandle.contains(clickPoint))
            {
                isDraggingTrimLeft = true;
                originalClipStartSample = clip.startSample;
                originalClipLength = clip.lengthSamples;
                currentTrimSample = clip.startSample;
                found = true;
                break;
            }
            else if (clipSelectedSingle && rightHandle.contains(clickPoint))
            {
                isDraggingTrimRight = true;
                originalClipStartSample = clip.startSample;
                originalClipLength = clip.lengthSamples;
                currentTrimSample = clip.startSample + clip.lengthSamples;
                found = true;
                break;
            }

            if (clipRect.contains(clickPoint))
            {
                if (event.mods.isAltDown())
                {
                    arrangementModel.setGuideClip(trackIndex, clipIndex);
                }
                else if (event.mods.isCtrlDown())
                {
                    arrangementModel.toggleAlignTargetClip(trackIndex, clipIndex);
                }
                else if (event.mods.isShiftDown())
                {
                    arrangementModel.toggleClipSelection(trackIndex, clipIndex);
                }
                else if (cursorTool == EditModeToolbar::CursorTool::Split)
                {
                    arrangementModel.selectClip(trackIndex, clipIndex);
                    const int64_t splitSample = xToSample(clickPoint.x, timelineModel, snapEnabled, snapBeats, arrangementModel.getSession());
                    arrangementModel.splitSelectedClip(splitSample);
                }
                else if (cursorTool == EditModeToolbar::CursorTool::Fade)
                {
                    arrangementModel.selectClip(trackIndex, clipIndex);
                    const float cStartX = (float)(timelineModel.getXForSamplePosition(clip.startSample, width));
                    const float cEndX   = (float)(timelineModel.getXForSamplePosition(clip.startSample + clip.lengthSamples, width));
                    const float clipW   = cEndX - cStartX;
                    const float fadeInX = cStartX + juce::jmin(1.0f, (float)clip.fadeInSamples / (float)juce::jmax<int64_t>(1, clip.lengthSamples)) * clipW;
                    if (clickPoint.x <= fadeInX + 16.0f)
                    {
                        isDraggingFadeIn = true;
                        fadeOrigIn = clip.fadeInSamples;
                    }
                    else
                    {
                        isDraggingFadeOut = true;
                        fadeOrigOut = clip.fadeOutSamples;
                    }
                }
                else if (cursorTool == EditModeToolbar::CursorTool::Trim)
                {
                    arrangementModel.selectClip(trackIndex, clipIndex);
                    const float cStartX = (float)(timelineModel.getXForSamplePosition(clip.startSample, width));
                    const float cEndX   = (float)(timelineModel.getXForSamplePosition(clip.startSample + clip.lengthSamples, width));
                    originalClipStartSample = clip.startSample;
                    originalClipLength = clip.lengthSamples;
                    if (clickPoint.x < cStartX + (cEndX - cStartX) * 0.5f)
                    {
                        isDraggingTrimLeft = true;
                        currentTrimSample = clip.startSample;
                    }
                    else
                    {
                        isDraggingTrimRight = true;
                        currentTrimSample = clip.startSample + clip.lengthSamples;
                    }
                }
                else if (editMode == EditModeToolbar::EditMode::Spot)
                {
                    // Spot mode: select then immediately open position dialog
                    arrangementModel.selectClip(trackIndex, clipIndex);
                    showSpotDialog(trackIndex, clipIndex);
                }
                else
                {
                    // Slip / Grid: select and start drag
                    arrangementModel.selectClip(trackIndex, clipIndex);
                    isDraggingClip = (editMode != EditModeToolbar::EditMode::Spot);
                    dragStartX = event.x;
                    originalClipStartSample = clip.startSample;
                }

                found = true;
                break;
            }
        }

        if (!found)
        {
            // start marquee selection on empty area
            if (!event.mods.isShiftDown())
                arrangementModel.clearSelection();
            isMarqueeSelecting = true;
            marqueeStart = event.position;
            marqueeRect = juce::Rectangle<float>(marqueeStart.x, marqueeStart.y, 0, 0);
        }
    }

    void ArrangementView::mouseDrag(const juce::MouseEvent& event)
    {
        const bool snapNow = (snapEnabled && editMode == EditModeToolbar::EditMode::Grid);
        const auto& session = arrangementModel.getSession();

        // ── Playhead scrubbing ────────────────────────────────────────────────
        if (isDraggingPlayhead)
        {
            const int64_t pos = xToSample(event.position.x, timelineModel, snapNow, snapBeats, session);
            transportState.setPositionSamples(juce::jmax<int64_t>(0, pos), true);
            repaint();
            return;
        }

        // ── Loop brace drag ───────────────────────────────────────────────────
        if (loopDragHandle != LoopDragHandle::None)
        {
            const int64_t currentSample = xToSample(event.position.x, timelineModel, snapNow, snapBeats, session);
            const int64_t delta = currentSample - loopDragStartSample;

            if (loopDragHandle == LoopDragHandle::Start)
            {
                const int64_t newStart = juce::jmax<int64_t>(0, loopOrigStart + delta);
                const int64_t newEnd   = juce::jmax(newStart + 1, loopOrigEnd);
                transportState.setLoopRange(newStart, newEnd);
            }
            else if (loopDragHandle == LoopDragHandle::End)
            {
                const int64_t newEnd = juce::jmax(loopOrigStart + 1, loopOrigEnd + delta);
                transportState.setLoopRange(loopOrigStart, newEnd);
            }
            else // Body
            {
                const int64_t newStart = juce::jmax<int64_t>(0, loopOrigStart + delta);
                const int64_t newEnd   = newStart + (loopOrigEnd - loopOrigStart);
                transportState.setLoopRange(newStart, newEnd);
            }
            repaint();
            return;
        }

        if (isMarqueeSelecting)
        {
            const auto current = event.position;
            const float x = juce::jmin(marqueeStart.x, current.x);
            const float y = juce::jmin(marqueeStart.y, current.y);
            const float w = std::abs(current.x - marqueeStart.x);
            const float h = std::abs(current.y - marqueeStart.y);
            marqueeRect = juce::Rectangle<float>(x, y, w, h);

            // determine intersecting clips
            juce::Array<juce::Point<int>> hits;
            const int width = getWidth();
            const float trackHeight = (float)trackHeightPx;
            for (int t = 0; t < arrangementModel.getSession().getNumTracks(); ++t)
            {
                const auto& track = arrangementModel.getSession().getTrack(t);
                for (int c = 0; c < track.clips.size(); ++c)
                {
                    const auto& clip = track.clips.getReference(c);
                    const double clipStartX = timelineModel.getXForSamplePosition(clip.startSample, width);
                    const double clipEndX = timelineModel.getXForSamplePosition(clip.startSample + clip.lengthSamples, width);
                    const auto clipRect = juce::Rectangle<float>((float)clipStartX, 28.0f + t * trackHeight + 6.0f,
                                                                (float)juce::jmax(8.0, clipEndX - clipStartX), trackHeight - 12.0f);
                    if (clipRect.intersects(marqueeRect))
                        hits.add(juce::Point<int>(t, c));
                }
            }

            // update selection
            if (event.mods.isShiftDown())
            {
                // additive marquee: add hits to existing selection
                for (auto& p : hits)
                    arrangementModel.addClipToSelection(p.x, p.y);
            }
            else
            {
                arrangementModel.clearSelection();
                for (auto& p : hits)
                    arrangementModel.addClipToSelection(p.x, p.y);
            }

            repaint();
            return;
        }

        // ── Fade handle drag ─────────────────────────────────────────────────────
        if (isDraggingFadeIn || isDraggingFadeOut)
        {
            auto* clip = arrangementModel.getSelectedClip();
            if (clip == nullptr || clip->locked) return;

            const int width2 = getWidth();
            const float cStartX = (float)timelineModel.getXForSamplePosition(clip->startSample, width2);
            const float cEndX   = (float)timelineModel.getXForSamplePosition(clip->startSample + clip->lengthSamples, width2);
            const float clipW   = juce::jmax(1.0f, cEndX - cStartX);

            if (isDraggingFadeIn)
            {
                const float ratio = juce::jlimit(0.0f, 1.0f, (event.position.x - cStartX) / clipW);
                const int64_t newFade = static_cast<int64_t>(ratio * (float)clip->lengthSamples);
                arrangementModel.setSelectedClipFadeIn(newFade);
            }
            else
            {
                const float ratio = juce::jlimit(0.0f, 1.0f, (cEndX - event.position.x) / clipW);
                const int64_t newFade = static_cast<int64_t>(ratio * (float)clip->lengthSamples);
                arrangementModel.setSelectedClipFadeOut(newFade);
            }
            repaint();
            return;
        }

        if (isDraggingTrimLeft || isDraggingTrimRight)
        {
            auto* clip = arrangementModel.getSelectedClip();
            if (clip == nullptr || clip->locked)
                return;

            const auto localX = static_cast<int>(event.x - 8.0f);
            const int64_t sampleAtX = static_cast<int64_t>(timelineModel.getSamplePositionForX(localX));
            if (isDraggingTrimLeft)
            {
                const int64_t newStart = arrangementModel.getSnappedSamplePosition(juce::jlimit<int64_t>(originalClipStartSample, originalClipStartSample + originalClipLength - 1, sampleAtX));
                if (newStart != currentTrimSample)
                {
                    currentTrimSample = newStart;
                    NovaStudio::Clip previewClip = *clip;
                    previewClip.startSample = newStart;
                    previewClip.lengthSamples = juce::jmax<int64_t>(1, originalClipLength - (newStart - originalClipStartSample));
                    arrangementModel.replaceClipWithoutUndo(arrangementModel.getSelectedTrackIndex(), arrangementModel.getSelectedClipIndex(), previewClip);
                }
            }
            else if (isDraggingTrimRight)
            {
                const int64_t newEnd = arrangementModel.getSnappedSamplePosition(juce::jlimit<int64_t>(originalClipStartSample + 1, originalClipStartSample + originalClipLength, sampleAtX));
                if (newEnd != currentTrimSample)
                {
                    currentTrimSample = newEnd;
                    NovaStudio::Clip previewClip = *clip;
                    previewClip.lengthSamples = juce::jmax<int64_t>(1, newEnd - originalClipStartSample);
                    arrangementModel.replaceClipWithoutUndo(arrangementModel.getSelectedTrackIndex(), arrangementModel.getSelectedClipIndex(), previewClip);
                }
            }
            return;
        }

        if (!isDraggingClip || arrangementModel.getSelectedClips().isEmpty())
            return;

        const int width = getWidth() - 16;
        const double sampleRate = transportState.getSampleRate();
        if (sampleRate <= 0.0)
            return;

        const int dx = event.x - dragStartX;
        const double seconds = dx / timelineModel.getPixelsPerSecond();
        const int64_t deltaSamples = static_cast<int64_t>(seconds * sampleRate);

        const auto targets = arrangementModel.getSelectedClips();

        // In Grid mode with snap enabled, snap clip start to grid after move
        if (editMode == EditModeToolbar::EditMode::Grid && snapEnabled && deltaSamples != 0)
        {
            arrangementModel.moveClipsBySamples(targets, deltaSamples);
            // Snap the selected clip's start to grid
            auto* clip = arrangementModel.getSelectedClip();
            if (clip != nullptr)
            {
                const int64_t snapped = snapToGrid(clip->startSample);
                const int64_t snapDelta = snapped - clip->startSample;
                if (snapDelta != 0)
                    arrangementModel.moveClipsBySamples(targets, snapDelta);
            }
        }
        else
        {
            arrangementModel.moveClipsBySamples(targets, deltaSamples);
        }
        // update baseline so further dragging accumulates
        dragStartX = event.x;
    }

    void ArrangementView::mouseUp(const juce::MouseEvent& event)
    {
        juce::ignoreUnused(event);

        isDraggingPlayhead = false;
        isDraggingFadeIn   = false;
        isDraggingFadeOut  = false;
        loopDragHandle = LoopDragHandle::None;
        updateCursorForTool();

        if (isMarqueeSelecting)
        {
            isMarqueeSelecting = false;
            marqueeRect = {};
            repaint();
        }

    if (isDraggingTrimLeft || isDraggingTrimRight)
    {
        auto* clip = arrangementModel.getSelectedClip();
        if (clip != nullptr && !clip->locked)
        {
            NovaStudio::Clip finalClip = *clip;
            if (isDraggingTrimLeft)
            {
                finalClip.startSample = currentTrimSample;
                finalClip.lengthSamples = juce::jmax<int64_t>(1, originalClipLength - (currentTrimSample - originalClipStartSample));
                arrangementModel.replaceSelectedClipWithUndo(finalClip, "Trim Clip Start");
            }
            else if (isDraggingTrimRight)
            {
                finalClip.lengthSamples = juce::jmax<int64_t>(1, currentTrimSample - originalClipStartSample);
                arrangementModel.replaceSelectedClipWithUndo(finalClip, "Trim Clip End");
            }
        }
        isDraggingTrimLeft = false;
        isDraggingTrimRight = false;
    }
}

void ArrangementView::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &transportState || source == &arrangementModel)
        repaint();
}

void ArrangementView::timerCallback()
{
    repaint();
}

bool ArrangementView::isInterestedInDragSource(const SourceDetails& details)
{
    const juce::String path = details.description.toString();
    return path.endsWithIgnoreCase(".wav")  || path.endsWithIgnoreCase(".aif")
        || path.endsWithIgnoreCase(".aiff") || path.endsWithIgnoreCase(".mp3")
        || path.endsWithIgnoreCase(".flac");
}

void ArrangementView::itemDropped(const SourceDetails& details)
{
    const juce::File file(details.description.toString());
    if (!file.existsAsFile()) return;

    const int rulerH = 28;
    const float trackHeight = (float)trackHeightPx;
    const int dropY = (int)details.localPosition.y;
    const int dropX = (int)details.localPosition.x;

    const int trackIndex = juce::jmax(0, (int)((dropY - rulerH) / trackHeight));
    auto& session = arrangementModel.getSession();
    if (trackIndex >= session.getNumTracks()) return;

    const int64_t samplePos = juce::jmax<int64_t>(0,
        (int64_t)timelineModel.getSamplePositionForX(dropX));

    // Read actual duration from file
    juce::AudioFormatManager fmt;
    fmt.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(file));

    NovaStudio::Clip clip;
    clip.file         = file;
    clip.startSample  = samplePos;
    clip.lengthSamples = reader ? reader->lengthInSamples : (int64_t)(44100 * 5);
    clip.isMidi       = false;

    session.getTrack(trackIndex).clips.add(clip);
    arrangementModel.sendChangeMessage();
}

MixerPanel::MixerPanel() {}
MixerPanel::~MixerPanel() = default;

void MixerPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(9, 10, 15));

    // Header bar
    g.setColour(juce::Colour::fromRGB(14, 15, 22));
    g.fillRect(0, 0, getWidth(), 28);
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    g.drawText("MIXER", 12, 0, 80, 28, juce::Justification::centredLeft);

    // Separator
    g.setColour(juce::Colour::fromRGB(35, 38, 52));
    g.fillRect(0, 27, getWidth(), 1);

    // Draw channel strip placeholders
    static const juce::Colour palette[] = {
        juce::Colour::fromRGB(100, 65, 175),
        juce::Colour::fromRGB(65, 85, 175),
        juce::Colour::fromRGB(40, 140, 140),
        juce::Colour::fromRGB(50, 100, 175),
        juce::Colour::fromRGB(35, 155, 140),
        juce::Colour::fromRGB(50, 155, 70),
        juce::Colour::fromRGB(140, 165, 45),
        juce::Colour::fromRGB(165, 95, 35),
    };
    static const char* names[] = {"VOC LEAD","HARMONY","ADLIBS","BEAT","BASS","KEYS","GUITAR","FX"};

    const int stripW = 72;
    const int numStrips = juce::jmin(8, (getWidth() - 80) / stripW);
    const int H = getHeight();

    for (int i = 0; i < numStrips; ++i)
    {
        const int sx = 8 + i * stripW;
        const juce::Colour col = palette[i];

        // Strip background
        g.setColour(juce::Colour::fromRGB(14, 15, 22));
        g.fillRect(sx, 32, stripW - 4, H - 36);

        // Color indicator top
        g.setColour(col);
        g.fillRect(sx, 32, stripW - 4, 3);

        // Track name
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(juce::Font(juce::FontOptions(8.5f).withStyle("Bold")));
        g.drawText(names[i], sx, 38, stripW - 4, 14, juce::Justification::centred);

        // Fader track (vertical line)
        const int faderX = sx + (stripW - 4) / 2;
        const int faderTop = 60;
        const int faderBot = H - 36;
        g.setColour(juce::Colour::fromRGB(28, 30, 42));
        g.fillRect(faderX - 2, faderTop, 4, faderBot - faderTop);

        // Fader handle at ~80% position
        const int faderHandleY = faderTop + (int)((faderBot - faderTop) * 0.25f);
        g.setColour(juce::Colour::fromRGB(80, 84, 110));
        g.fillRoundedRectangle((float)(sx + 6), (float)(faderHandleY - 6), (float)(stripW - 16), 12.0f, 3.0f);
        g.setColour(juce::Colour::fromRGB(120, 124, 160));
        g.fillRect(sx + 6, faderHandleY - 1, stripW - 16, 2);

        // M / S buttons at bottom
        g.setColour(juce::Colour::fromRGB(28, 30, 42));
        g.fillRoundedRectangle((float)sx + 2, (float)(H - 32), 20.0f, 14.0f, 2.0f);
        g.fillRoundedRectangle((float)sx + 26, (float)(H - 32), 20.0f, 14.0f, 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.setFont(juce::Font(juce::FontOptions(8.0f).withStyle("Bold")));
        g.drawText("M", sx + 2, H - 32, 20, 14, juce::Justification::centred);
        g.drawText("S", sx + 26, H - 32, 20, 14, juce::Justification::centred);

        // dB label at bottom
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.setFont(juce::Font(juce::FontOptions(8.0f)));
        g.drawText("-3.2", sx, H - 16, stripW - 4, 14, juce::Justification::centred);

        // Strip separator
        g.setColour(juce::Colour::fromRGB(22, 24, 34));
        g.fillRect(sx + stripW - 4, 32, 1, H - 36);
    }
}

void MixerPanel::resized() {}

// ─── BottomDockPanel ────────────────────────────────────────────────────────

BottomDockPanel::BottomDockPanel()
{
    for (auto* b : {&mixerTab, &channelsTab, &effectsTab, &metersTab})
    {
        addAndMakeVisible(b);
        b->addListener(this);
        b->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b->setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.55f));
    }
    mixerTab.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
}

BottomDockPanel::~BottomDockPanel() = default;

void BottomDockPanel::resized()
{
    const int tabH = 28;
    auto area = getLocalBounds();
    auto tabRow = area.removeFromTop(tabH);
    const int tabW = 80;
    mixerTab.setBounds(tabRow.removeFromLeft(tabW));
    channelsTab.setBounds(tabRow.removeFromLeft(tabW));
    effectsTab.setBounds(tabRow.removeFromLeft(tabW));
    metersTab.setBounds(tabRow.removeFromLeft(tabW));
}

static const juce::Colour kDockPalette[] = {
    juce::Colour::fromRGB(100, 65, 175),
    juce::Colour::fromRGB(65,  85, 175),
    juce::Colour::fromRGB(40, 140, 140),
    juce::Colour::fromRGB(50, 100, 175),
    juce::Colour::fromRGB(35, 155, 140),
    juce::Colour::fromRGB(50, 155,  70),
    juce::Colour::fromRGB(140,165,  45),
    juce::Colour::fromRGB(165, 95,  35),
    juce::Colour::fromRGB( 80, 80, 120),  // BUS 1
    juce::Colour::fromRGB( 80, 80, 120),  // BUS 2
    juce::Colour::fromRGB( 60, 80, 140),  // REVERB
    juce::Colour::fromRGB( 60, 80, 140),  // DELAY
    juce::Colour::fromRGB(160,140,  60),  // MASTER
};
static const char* kDockNames[] = {
    "VOC LEAD","HARMONY","ADLIBS","BEAT","BASS","KEYS","GUITAR","FX",
    "BUS 1","BUS 2","REVERB","DELAY","MASTER"
};
static const float kFaderPos[] = {
    0.22f, 0.28f, 0.32f, 0.26f, 0.30f, 0.35f, 0.30f, 0.50f,
    0.25f, 0.28f, 0.55f, 0.62f, 0.18f
};

int BottomDockPanel::stripIndexAt(int x) const
{
    // Only checks within the mixer region (left ~62% of panel)
    const int mixW = static_cast<int>(getWidth() * 0.62f);
    if (x < 0 || x >= mixW) return -1;
    return x / kStripW;
}

bool BottomDockPanel::faderHitTest(int stripIdx, juce::Point<int> pos,
                                    juce::Rectangle<int> mixerArea) const
{
    if (stripIdx < 0 || stripIdx >= kNumStrips) return false;
    const int H      = mixerArea.getHeight();
    const int top    = mixerArea.getY();
    const int sx     = mixerArea.getX() + stripIdx * kStripW;
    const int fTop   = top + 44;
    const int fBot   = top + H - 34;
    const float fPos = faderPositions[stripIdx];
    const int hY     = fTop + (int)(fPos * (fBot - fTop));
    const auto handleRect = juce::Rectangle<int>(sx + 6, hY - 9, kStripW - 14, 18);
    return handleRect.contains(pos);
}

void BottomDockPanel::mouseDown(const juce::MouseEvent& e)
{
    const int mixH = getHeight() - 28;
    const auto mixArea = juce::Rectangle<int>(0, 28, static_cast<int>(getWidth() * 0.62f), mixH);
    const int si = stripIndexAt(e.x);
    if (si >= 0 && faderHitTest(si, e.getPosition(), mixArea))
    {
        activeFaderStrip  = si;
        faderDragStartY   = e.y;
        faderDragStartPos = faderPositions[si];
        return;
    }

    // Step sequencer click
    const int splitX = (int)(getWidth() * 0.62f);
    const int pianoW = (int)((getWidth() - splitX) * 0.42f);
    const int stepX  = splitX + pianoW;
    if (e.x > stepX && e.y > 28)
    {
        const int area_h = getHeight() - 28;
        const int numRows = 6;
        const int numSteps = 16;
        const int labelW = 44;
        const int rowH = area_h / numRows;
        const int stepW = (getWidth() - stepX - 1 - labelW) / numSteps;
        const int row = (e.y - 28) / juce::jmax(1, rowH);
        const int col = (e.x - stepX - 1 - labelW) / juce::jmax(1, stepW);
        if (row >= 0 && row < numRows && col >= 0 && col < numSteps)
        {
            stepStates[row][col] = !stepStates[row][col];
            repaint();
        }
        return;
    }
}

void BottomDockPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (activeFaderStrip < 0) return;
    const int mixH   = getHeight() - 28;
    const int fRange = mixH - 78; // fader travel distance
    const float delta = (float)(e.y - faderDragStartY) / (float)juce::jmax(1, fRange);
    faderPositions[activeFaderStrip] = juce::jlimit(0.0f, 1.0f, faderDragStartPos + delta);
    repaint();
}

void BottomDockPanel::mouseUp(const juce::MouseEvent&)
{
    activeFaderStrip = -1;
}

void BottomDockPanel::paintMixerStrips(juce::Graphics& g, juce::Rectangle<int> area)
{
    const int H      = area.getHeight();
    const int top    = area.getY();
    const int numStrips = juce::jmin(kNumStrips, area.getWidth() / kStripW);

    // dB scale labels reference (0 dB = 35% from top of fader travel)
    static const char* const dbMarks[] = { "+6", "0", "-6", "-12", "-24", "-inf" };

    for (int i = 0; i < numStrips; ++i)
    {
        const int sx    = area.getX() + i * kStripW;
        const int sw    = kStripW - 2;
        const juce::Colour col = kDockPalette[i];

        // ── Strip body ────────────────────────────────────────────────────
        g.setColour(juce::Colour::fromRGB(11, 12, 18));
        g.fillRect(sx, top, sw, H);

        // Track colour band at top
        g.setColour(col);
        g.fillRect(sx, top, sw, 4);

        // ── Track name ────────────────────────────────────────────────────
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions(8.5f).withStyle("Bold")));
        g.drawText(kDockNames[i], sx + 2, top + 6, sw - 4, 12, juce::Justification::centred);

        // ── Pan knob ──────────────────────────────────────────────────────
        const float knobCX = (float)sx + sw * 0.5f;
        const float knobCY = (float)top + 28.0f;
        const float kr = 9.0f;
        // Outer arc ring (dark groove)
        g.setColour(juce::Colour::fromRGB(8, 9, 14));
        g.fillEllipse(knobCX - kr, knobCY - kr, kr * 2.0f, kr * 2.0f);
        // Knob body
        g.setColour(juce::Colour::fromRGB(38, 42, 60));
        g.fillEllipse(knobCX - kr + 1.5f, knobCY - kr + 1.5f, (kr - 1.5f) * 2.0f, (kr - 1.5f) * 2.0f);
        // Colour rim
        g.setColour(col.withAlpha(0.55f));
        g.drawEllipse(knobCX - kr, knobCY - kr, kr * 2.0f, kr * 2.0f, 1.5f);
        // Centre pointer (pan = 0)
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.drawLine(knobCX, knobCY - kr + 3.0f, knobCX, knobCY - 3.0f, 1.8f);
        // "PAN" label
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        g.drawText("PAN", sx + 2, (int)(knobCY + kr + 1.0f), sw - 4, 9, juce::Justification::centred);

        // ── Fader track ───────────────────────────────────────────────────
        const int faderTop = top + 44;
        const int faderBot = top + H - 34;
        const int fTravel  = faderBot - faderTop;
        const int fCX      = sx + sw / 2;

        // Background groove
        const juce::Colour grooveCol = juce::Colour::fromRGB(6, 7, 11);
        g.setColour(grooveCol);
        g.fillRoundedRectangle((float)(fCX - 3), (float)faderTop, 6.0f, (float)fTravel, 3.0f);
        g.setColour(juce::Colour::fromRGB(30, 34, 48));
        g.drawRoundedRectangle((float)(fCX - 3), (float)faderTop, 6.0f, (float)fTravel, 3.0f, 0.8f);

        // Unity (0 dB) tick mark — at 35% position
        const int unityY = faderTop + (int)(0.35f * fTravel);
        g.setColour(col.withAlpha(0.45f));
        g.fillRect(fCX - 8, unityY - 1, 16, 2);

        // Scale ticks on the left
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.setFont(juce::Font(juce::FontOptions(6.0f)));
        for (int m = 0; m < 6; ++m)
        {
            const int tickY = faderTop + m * fTravel / 5;
            g.drawLine((float)(sx + 4), (float)tickY, (float)(fCX - 5), (float)tickY, 0.7f);
            g.drawText(dbMarks[m], sx + 2, tickY - 4, 18, 8, juce::Justification::right);
        }

        // ── Fader thumb (analog-style, wide flat knob) ────────────────────
        const float fPos  = faderPositions[i];
        const int handleY = faderTop + (int)(fPos * fTravel);
        const int thumbH  = 18;
        const int thumbW  = sw - 12;
        const int thumbX  = sx + 6;

        // Shadow under thumb
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle((float)thumbX + 1.0f, (float)(handleY - thumbH/2) + 2.0f,
                               (float)thumbW, (float)thumbH, 4.0f);

        // Thumb body
        juce::ColourGradient thumbGrad(juce::Colour::fromRGB(72, 78, 110),
                                        (float)thumbX, (float)(handleY - thumbH/2),
                                        juce::Colour::fromRGB(44, 48, 70),
                                        (float)thumbX, (float)(handleY + thumbH/2), false);
        g.setGradientFill(thumbGrad);
        g.fillRoundedRectangle((float)thumbX, (float)(handleY - thumbH/2),
                               (float)thumbW, (float)thumbH, 4.0f);

        // Centre line (white stripe = the actual indicator)
        g.setColour(juce::Colours::white.withAlpha(0.75f));
        g.fillRect(thumbX + 4, handleY - 1, thumbW - 8, 2);

        // Thumb border
        g.setColour(col.withAlpha(0.35f));
        g.drawRoundedRectangle((float)thumbX, (float)(handleY - thumbH/2),
                               (float)thumbW, (float)thumbH, 4.0f, 1.0f);

        // ── dB readout below fader ────────────────────────────────────────
        const float dbVal = (fPos < 0.35f)
                            ? juce::jmap(fPos, 0.0f, 0.35f, -96.0f, 0.0f)
                            : juce::jmap(fPos, 0.35f, 1.0f, 0.0f, 6.0f);
        const juce::String dbStr = (dbVal < -90.0f) ? "-inf" : (juce::String(dbVal, 1) + " dB");
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.drawText(dbStr, sx + 2, faderBot + 2, sw - 4, 12, juce::Justification::centred);

        // ── M / S / R buttons ────────────────────────────────────────────
        const int btnY  = top + H - 18;
        const int btnH2 = 14;
        const int btnW2 = (sw - 10) / 3;

        auto drawBtn = [&](const char* lbl, int bx, juce::Colour bc) {
            g.setColour(bc);
            g.fillRoundedRectangle((float)bx, (float)btnY, (float)btnW2, (float)btnH2, 2.5f);
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
            g.drawText(lbl, bx, btnY, btnW2, btnH2, juce::Justification::centred);
        };

        drawBtn("M", sx + 3,               juce::Colour::fromRGB(100, 30, 30));
        drawBtn("S", sx + 3 + btnW2 + 2,   juce::Colour::fromRGB(30, 90, 30));
        drawBtn("R", sx + 3 + (btnW2 + 2)*2, juce::Colour::fromRGB(80, 30, 80));

        // ── Strip separator ───────────────────────────────────────────────
        g.setColour(juce::Colour::fromRGB(6, 7, 11));
        g.fillRect(sx + sw, top, 2, H);
    }
}

void BottomDockPanel::paintPianoRoll(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour::fromRGB(12, 13, 19));
    g.fillRect(area);

    // Header
    g.setColour(juce::Colour::fromRGB(16, 18, 26));
    g.fillRect(area.getX(), area.getY(), area.getWidth(), 22);
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));
    g.drawText("PIANO ROLL", area.getX() + 8, area.getY(), 90, 22, juce::Justification::centredLeft);
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(juce::Font(juce::FontOptions(8.5f)));
    g.drawText("Snap: 1/16", area.getRight() - 70, area.getY(), 64, 22, juce::Justification::centredRight);

    // Piano keys on left
    const int keyW = 18;
    const int noteH = 8;
    const int numKeys = (area.getHeight() - 22) / noteH;
    const bool isBlack[] = {false,true,false,true,false,false,true,false,true,false,true,false};
    for (int k = 0; k < numKeys; ++k)
    {
        const int ky = area.getY() + 22 + k * noteH;
        const bool black = isBlack[k % 12];
        g.setColour(black ? juce::Colour::fromRGB(18, 20, 28) : juce::Colour::fromRGB(40, 44, 58));
        g.fillRect(area.getX(), ky, keyW, noteH - 1);
    }

    // Note grid
    const int gridX = area.getX() + keyW;
    const int gridW = area.getWidth() - keyW;
    const int gridH = area.getHeight() - 22;

    // Beat lines
    const int beatsVisible = 8;
    const int beatW = gridW / beatsVisible;
    for (int b = 0; b < beatsVisible; ++b)
    {
        const int bx = gridX + b * beatW;
        g.setColour(b % 4 == 0 ? juce::Colour::fromRGB(30, 33, 48)
                                : juce::Colour::fromRGB(22, 24, 34));
        g.fillRect(bx, area.getY() + 22, 1, gridH);
    }

    // Row lines
    for (int k = 0; k < numKeys; ++k)
    {
        const int ky = area.getY() + 22 + k * noteH;
        const bool black = isBlack[k % 12];
        g.setColour(black ? juce::Colour::fromRGB(14, 15, 22)
                          : juce::Colour::fromRGB(16, 18, 26));
        g.fillRect(gridX, ky, gridW, noteH - 1);
    }

    // Sample notes (decorative purple blobs)
    struct Note { int beat, key, len; };
    static const Note notes[] = {
        {0,5,2},{2,5,1},{3,7,2},{6,5,3},{0,8,4},{4,10,2},{7,3,1}
    };
    g.setColour(juce::Colour::fromRGB(130, 80, 200).withAlpha(0.85f));
    for (auto& n : notes)
    {
        const int nx = gridX + n.beat * beatW;
        const int ny = area.getY() + 22 + n.key * noteH;
        g.fillRoundedRectangle((float)nx + 1.0f, (float)ny + 1.0f,
                               (float)(n.len * beatW - 3), (float)(noteH - 2), 2.0f);
    }
}

void BottomDockPanel::paintStepSequencer(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour::fromRGB(11, 12, 17));
    g.fillRect(area);

    // Header
    g.setColour(juce::Colour::fromRGB(15, 17, 24));
    g.fillRect(area.getX(), area.getY(), area.getWidth(), 22);
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));
    g.drawText("STEP SEQUENCER", area.getX() + 8, area.getY(), 130, 22, juce::Justification::centredLeft);
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(juce::Font(juce::FontOptions(8.5f)));
    g.drawText("Pattern 01", area.getRight() - 70, area.getY(), 64, 22, juce::Justification::centredRight);

    const char* rows[] = {"Kick","Snare","Hi Hat","808","Clap","Perc"};
    const int numRows = 6;
    const int numSteps = 16;
    const int labelW = 44;
    const int rowH = (area.getHeight() - 22) / numRows;
    const int stepW = (area.getWidth() - labelW) / numSteps;

    for (int r = 0; r < numRows; ++r)
    {
        const int ry = area.getY() + 22 + r * rowH;

        // Row label
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.setFont(juce::Font(juce::FontOptions(8.5f)));
        g.drawText(rows[r], area.getX() + 2, ry, labelW - 4, rowH, juce::Justification::centredLeft);

        for (int s = 0; s < numSteps; ++s)
        {
            const int sx = area.getX() + labelW + s * stepW;
            const bool active = stepStates[r][s];
            const bool beat   = (s % 4 == 0);

            g.setColour(active ? juce::Colour::fromRGB(200, 140, 40).withAlpha(beat ? 1.0f : 0.75f)
                               : juce::Colour::fromRGB(24, 26, 36));
            g.fillRoundedRectangle((float)sx + 1.0f, (float)ry + 2.0f,
                                   (float)(stepW - 2), (float)(rowH - 4), 2.0f);

            // Beat group divider
            if (s % 4 == 0 && s > 0)
            {
                g.setColour(juce::Colour::fromRGB(30, 33, 48));
                g.fillRect(sx, ry, 1, rowH);
            }
        }
    }
}

void BottomDockPanel::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour::fromRGB(10, 11, 16));

    // Top tab bar background
    g.setColour(juce::Colour::fromRGB(13, 14, 20));
    g.fillRect(0, 0, getWidth(), 28);
    g.setColour(juce::Colour::fromRGB(35, 38, 52));
    g.fillRect(0, 27, getWidth(), 1);

    // Active tab underline (MIXER)
    g.setColour(juce::Colour::fromRGB(150, 110, 255));
    g.fillRect(0, 25, 80, 2);

    // Divider between mixer and piano roll sections
    const int splitX = (int)(getWidth() * 0.62f);
    g.setColour(juce::Colour::fromRGB(30, 33, 48));
    g.fillRect(splitX, 28, 1, getHeight() - 28);

    // Divider between piano roll and step seq
    const int pianoW  = (int)((getWidth() - splitX) * 0.42f);
    const int stepX   = splitX + pianoW;
    g.setColour(juce::Colour::fromRGB(30, 33, 48));
    g.fillRect(stepX, 28, 1, getHeight() - 28);

    // Paint sections
    paintMixerStrips(g, juce::Rectangle<int>(0, 28, splitX, getHeight() - 28));
    paintPianoRoll(g,   juce::Rectangle<int>(splitX + 1, 28, pianoW - 1, getHeight() - 28));
    paintStepSequencer(g, juce::Rectangle<int>(stepX + 1, 28, getWidth() - stepX - 1, getHeight() - 28));
}

BrowserPanel::BrowserPanel()
{
    refresh();
    setRepaintsOnMouseActivity(true);
}

BrowserPanel::~BrowserPanel() = default;

void BrowserPanel::refresh()
{
    files.clear();
    auto folder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                      .getChildFile("NovaStudio").getChildFile("Recordings");
    if (folder.isDirectory())
        folder.findChildFiles(files, juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.mp3;*.flac");
    files.sort();
    repaint();
}

int BrowserPanel::fileIndexAt(int y) const
{
    const int listTop = kHeaderH + kSearchH + kTabsH + 2;
    const int listBot = getHeight() - kPreviewH;
    if (y < listTop || y >= listBot) return -1;
    const int idx = (y - listTop) / kItemH;
    return (idx >= 0 && idx < files.size()) ? idx : -1;
}

void BrowserPanel::mouseDown(const juce::MouseEvent& e)
{
    selectedIndex = fileIndexAt(e.y);
    repaint();
}

void BrowserPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (selectedIndex < 0 || selectedIndex >= files.size()) return;
    if (e.getDistanceFromDragStart() < 6) return;

    if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor(this))
        dc->startDragging(files[selectedIndex].getFullPathName(), this);
}

void BrowserPanel::paint(juce::Graphics& g)
{
    const int W = getWidth();
    const int H = getHeight();

    g.fillAll(juce::Colour::fromRGB(12, 14, 20));

    // Header
    g.setColour(juce::Colour::fromRGB(18, 20, 28));
    g.fillRect(0, 0, W, kHeaderH);
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
    g.drawText("BROWSER", 12, 0, W - 24, kHeaderH, juce::Justification::centredLeft);
    // Refresh icon hint
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.drawText(u8"↻", W - 26, 0, 22, kHeaderH, juce::Justification::centred);

    // Search bar
    g.setColour(juce::Colour::fromRGB(22, 26, 36));
    g.fillRoundedRectangle(8.0f, (float)kHeaderH + 2.0f, W - 16.0f, kSearchH - 4.0f, 5.0f);
    g.setColour(juce::Colours::white.withAlpha(0.28f));
    g.setFont(juce::Font(juce::FontOptions(10.5f)));
    g.drawText("Search recordings...", 16, kHeaderH + 2, W - 32, kSearchH - 4, juce::Justification::centredLeft);

    // Category tabs
    const int tabsY = kHeaderH + kSearchH;
    g.setColour(juce::Colour::fromRGB(16, 18, 26));
    g.fillRect(0, tabsY, W, kTabsH);
    g.setColour(juce::Colour::fromRGB(35, 38, 52));
    g.fillRect(0, tabsY + kTabsH - 1, W, 1);
    g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
    g.setColour(juce::Colour::fromRGB(150, 120, 255));
    g.drawText("RECORDINGS", 8, tabsY, W - 16, kTabsH, juce::Justification::centredLeft);

    // File list
    const int listTop = kHeaderH + kSearchH + kTabsH + 2;
    const int listBot = H - kPreviewH;
    const int visCount = (listBot - listTop) / kItemH;

    for (int i = 0; i < files.size() && i < visCount; ++i)
    {
        const int iy = listTop + i * kItemH;
        const bool sel = (i == selectedIndex);

        if (sel)
        {
            g.setColour(juce::Colour::fromRGB(50, 40, 90));
            g.fillRect(0, iy, W, kItemH);
        }

        // Waveform icon dot
        g.setColour(juce::Colour::fromRGB(100, 80, 180).withAlpha(0.8f));
        g.fillEllipse(8.0f, (float)iy + 7.0f, 5.0f, 5.0f);

        // File name
        g.setColour(sel ? juce::Colours::white : juce::Colours::white.withAlpha(0.72f));
        g.setFont(juce::Font(juce::FontOptions(10.5f)));
        g.drawText(files[i].getFileNameWithoutExtension(), 18, iy, W - 26, kItemH,
                   juce::Justification::centredLeft, true);

        // Row separator
        g.setColour(juce::Colour::fromRGB(22, 24, 34));
        g.fillRect(0, iy + kItemH - 1, W, 1);
    }

    if (files.isEmpty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.28f));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText("No recordings found", 0, listTop + 20, W, 20, juce::Justification::centred);
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.drawText("Record something first", 0, listTop + 42, W, 18, juce::Justification::centred);
    }

    // Preview area
    g.setColour(juce::Colour::fromRGB(14, 16, 22));
    g.fillRect(0, listBot, W, kPreviewH);
    g.setColour(juce::Colour::fromRGB(30, 34, 48));
    g.fillRect(0, listBot, W, 1);

    if (selectedIndex >= 0 && selectedIndex < files.size())
    {
        const auto& f = files[selectedIndex];
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));
        g.drawText(f.getFileNameWithoutExtension(), 8, listBot + 6, W - 16, 14,
                   juce::Justification::centredLeft, true);
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        const auto size = f.getSize();
        g.drawText(juce::File::descriptionOfSizeInBytes(size) + "  •  WAV",
                   8, listBot + 22, W - 16, 14, juce::Justification::centredLeft);
        // Drag hint
        g.setColour(juce::Colour::fromRGB(150, 120, 255).withAlpha(0.55f));
        g.setFont(juce::Font(juce::FontOptions(8.5f)));
        g.drawText("Drag to timeline to place", 8, listBot + 40, W - 16, 14, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(0.28f));
        g.setFont(juce::Font(juce::FontOptions(9.5f)));
        g.drawText("Click to select  •  Drag to place", 0, listBot + 26, W, 18, juce::Justification::centred);
    }

    // NOVA AUDIO branding
    g.setColour(juce::Colour::fromRGB(200, 155, 60).withAlpha(0.45f));
    g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
    g.drawText("NOVA AUDIO", 0, H - 16, W, 14, juce::Justification::centred);
}

void BrowserPanel::resized() {}

PianoRollPanel::PianoRollPanel() {}
PianoRollPanel::~PianoRollPanel() = default;

void PianoRollPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour::fromRGB(13, 15, 20));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 12.0f);
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("PIANO ROLL", 14, 12, getWidth() - 28, 18, juce::Justification::left);
}

void PianoRollPanel::resized() {}

StepSequencerPanel::StepSequencerPanel() {}
StepSequencerPanel::~StepSequencerPanel() = default;

void StepSequencerPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour::fromRGB(13, 15, 20));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 12.0f);
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("STEP SEQUENCER", 14, 12, getWidth() - 28, 18, juce::Justification::left);
}

void StepSequencerPanel::resized() {}

NovaAlignPanel::NovaAlignPanel(NovaStudio::ArrangementModel& arrangementModelRef)
        : arrangementModel(arrangementModelRef)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(guideLabel);
        addAndMakeVisible(targetClipList);
        addAndMakeVisible(alignAmountLabel);
        addAndMakeVisible(alignAmountSlider);
        addAndMakeVisible(naturalnessLabel);
        addAndMakeVisible(naturalnessSlider);
        addAndMakeVisible(tightnessLabel);
        addAndMakeVisible(tightnessSlider);
        addAndMakeVisible(phraseSensitivityLabel);
        addAndMakeVisible(phraseSensitivitySlider);
        addAndMakeVisible(consonantPriorityLabel);
        addAndMakeVisible(consonantPrioritySlider);
        addAndMakeVisible(createNewVersionToggle);
        addAndMakeVisible(bypassPreviewToggle);
        bypassPreviewToggle.setTooltip("Toggle hearing Original/Preview (Shortcut: Cmd/Ctrl + Shift + P)");
        addAndMakeVisible(revertButton);
        addAndMakeVisible(statusLabel);
        addAndMakeVisible(previewButton);
        addAndMakeVisible(commitButton);
        addAndMakeVisible(resetButton);

        titleLabel.setText("Nova Align", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));

        guideLabel.setText("Guide: None", juce::dontSendNotification);
        guideLabel.setJustificationType(juce::Justification::left);

        targetClipList.setModel(this);
        targetClipList.setColour(juce::ListBox::backgroundColourId, juce::Colour::fromRGB(24, 28, 38));
        targetClipList.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);

        alignAmountLabel.setText("Align Amount", juce::dontSendNotification);
        alignAmountSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        alignAmountSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 20);
        alignAmountSlider.setRange(0.0, 1.0, 0.01);
        alignAmountSlider.setValue(1.0);
        alignAmountSlider.addListener(this);

        naturalnessLabel.setText("Naturalness", juce::dontSendNotification);
        naturalnessSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        naturalnessSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 20);
        naturalnessSlider.setRange(0.0, 1.0, 0.01);
        naturalnessSlider.setValue(0.82);
        naturalnessSlider.addListener(this);

        tightnessLabel.setText("Tightness", juce::dontSendNotification);
        tightnessSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        tightnessSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 20);
        tightnessSlider.setRange(0.0, 1.0, 0.01);
        tightnessSlider.setValue(0.62);
        tightnessSlider.addListener(this);

        phraseSensitivityLabel.setText("Phrase Sensitivity", juce::dontSendNotification);
        phraseSensitivitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
        phraseSensitivitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
        phraseSensitivitySlider.setRange(0.0, 1.0, 0.01);
        phraseSensitivitySlider.setValue(0.75);
        phraseSensitivitySlider.addListener(this);

        consonantPriorityLabel.setText("Consonant Priority", juce::dontSendNotification);
        consonantPrioritySlider.setSliderStyle(juce::Slider::LinearHorizontal);
        consonantPrioritySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
        consonantPrioritySlider.setRange(0.0, 1.0, 0.01);
        consonantPrioritySlider.setValue(0.5);
        consonantPrioritySlider.addListener(this);

        createNewVersionToggle.setButtonText("Create New Version");
        createNewVersionToggle.setToggleState(true, juce::dontSendNotification);

        bypassPreviewToggle.setToggleState(false, juce::dontSendNotification);
        bypassPreviewToggle.addListener(this);

        revertButton.addListener(this);

        statusLabel.setJustificationType(juce::Justification::centredLeft);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        statusLabel.setText("Ready", juce::dontSendNotification);

        previewButton.addListener(this);
        commitButton.addListener(this);
        resetButton.addListener(this);

        addAndMakeVisible(shortcutLabel);
        shortcutLabel.setText("Shortcut: Cmd/Ctrl + Shift + P toggles Original/Preview", juce::dontSendNotification);
        shortcutLabel.setJustificationType(juce::Justification::centredLeft);
        shortcutLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.55f));
        juce::Font smallFont(11.0f);
        shortcutLabel.setFont(smallFont);

        arrangementModel.addChangeListener(this);
        refreshContent();
    }

    NovaAlignPanel::~NovaAlignPanel()
    {
        arrangementModel.removeChangeListener(this);
    }

    void NovaAlignPanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(18, 22, 30));
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.drawRect(getLocalBounds().reduced(2), 1);
    }

    void NovaAlignPanel::resized()
    {
        auto area = getLocalBounds().reduced(12);
        titleLabel.setBounds(area.removeFromTop(30));
        guideLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(10);

        targetClipList.setBounds(area.removeFromTop(110));
        area.removeFromTop(10);

        auto row = area.removeFromTop(130);
        auto rotaryArea = row.removeFromLeft(row.getWidth() / 3).reduced(8);
        alignAmountLabel.setBounds(rotaryArea.removeFromTop(20));
        alignAmountSlider.setBounds(rotaryArea.removeFromTop(90));

        rotaryArea = row.removeFromLeft(row.getWidth() / 2).reduced(8);
        naturalnessLabel.setBounds(rotaryArea.removeFromTop(20));
        naturalnessSlider.setBounds(rotaryArea.removeFromTop(90));

        tightnessLabel.setBounds(row.reduced(8).removeFromTop(20));
        tightnessSlider.setBounds(row.reduced(8).removeFromTop(90));

        area.removeFromTop(6);
        phraseSensitivityLabel.setBounds(area.removeFromTop(20));
        phraseSensitivitySlider.setBounds(area.removeFromTop(30));
        area.removeFromTop(8);
        consonantPriorityLabel.setBounds(area.removeFromTop(20));
        consonantPrioritySlider.setBounds(area.removeFromTop(30));
        area.removeFromTop(8);

        createNewVersionToggle.setBounds(area.removeFromTop(28));
        area.removeFromTop(10);

        bypassPreviewToggle.setBounds(area.removeFromTop(28));
        area.removeFromTop(10);

        // place subtle shortcut hint below the bypass toggle
        shortcutLabel.setBounds(area.removeFromTop(18));

        statusLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(8);

        auto buttonRow = area.removeFromTop(38);
        previewButton.setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 3).reduced(4));
        commitButton.setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 2).reduced(4));
        resetButton.setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 2).reduced(4));
        revertButton.setBounds(buttonRow.reduced(4));
    }

    void NovaAlignPanel::buttonClicked(juce::Button* button)
    {
        NovaStudio::ArrangementModel::AlignSettings settings;
        settings.amount = static_cast<float>(alignAmountSlider.getValue());
        settings.naturalness = static_cast<float>(naturalnessSlider.getValue());
        settings.tightness = static_cast<float>(tightnessSlider.getValue());
        settings.phraseSensitivity = static_cast<float>(phraseSensitivitySlider.getValue());
        settings.consonantPriority = static_cast<float>(consonantPrioritySlider.getValue());
        settings.maxShiftMs = 1000;

        if (button == &previewButton)
        {
            if (arrangementModel.previewSelectedTargetsToGuide(settings))
            {
                if (onStatusMessage) onStatusMessage("Nova Align preview created.");
                statusLabel.setText("Preview ready", juce::dontSendNotification);
                arrangementModel.setPreviewBypassed(bypassPreviewToggle.getToggleState());
                // save last used settings to session
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("amount", settings.amount);
                obj->setProperty("naturalness", settings.naturalness);
                obj->setProperty("tightness", settings.tightness);
                obj->setProperty("phraseSensitivity", settings.phraseSensitivity);
                obj->setProperty("consonantPriority", settings.consonantPriority);
                arrangementModel.getSession().setNovaAlignSettings(juce::var(obj.get()));
            }
            else if (onStatusMessage)
            {
                onStatusMessage("Nova Align preview failed; check guide and targets.");
            }
        }
        else if (button == &bypassPreviewToggle)
        {
            // User toggled bypass in Nova Align panel: update session via ArrangementModel
            arrangementModel.setPreviewBypassed(bypassPreviewToggle.getToggleState());
            arrangementModel.sendChangeMessage();
            statusLabel.setText(bypassPreviewToggle.getToggleState() ? "Preview bypassed" : "Preview active", juce::dontSendNotification);
        }
        else if (button == &commitButton)
        {
            if (arrangementModel.commitAlignmentPreview(createNewVersionToggle.getToggleState()))
            {
                if (onStatusMessage) onStatusMessage("Nova Align commit completed.");
                statusLabel.setText("Committed", juce::dontSendNotification);
            }
            else if (onStatusMessage)
            {
                onStatusMessage("No Nova Align preview available to commit.");
            }
        }
        else if (button == &resetButton)
        {
            arrangementModel.clearAlignmentPreview();
            if (onStatusMessage) onStatusMessage("Nova Align preview reset.");
            statusLabel.setText("Ready", juce::dontSendNotification);
            arrangementModel.setPreviewBypassed(false);
            bypassPreviewToggle.setToggleState(false, juce::dontSendNotification);
        }
        else if (button == &revertButton)
        {
            // Revert selected target clips to original where possible
            const auto& targets = arrangementModel.getAlignTargetClips();
            for (int i = 0; i < targets.size(); ++i)
            {
                const auto ref = targets.getReference(i);
                arrangementModel.revertAlignedClip(ref.x, ref.y);
            }
            arrangementModel.sendChangeMessage();
            if (onStatusMessage) onStatusMessage("Reverted selected targets to original.");
            statusLabel.setText("Reverted", juce::dontSendNotification);
        }
    }

    void NovaAlignPanel::sliderValueChanged(juce::Slider* slider)
    {
        juce::ignoreUnused(slider);
    }

    int NovaAlignPanel::getNumRows()
    {
        return arrangementModel.getAlignTargetClips().size();
    }

    void NovaAlignPanel::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
    {
        const auto& targets = arrangementModel.getAlignTargetClips();
        if (rowNumber < 0 || rowNumber >= targets.size())
            return;

        const auto clipRef = targets.getReference(rowNumber);
        const auto& session = arrangementModel.getSession();
        if (!isPositiveAndBelow(clipRef.x, session.getNumTracks()))
            return;

        const auto& track = session.getTrack(clipRef.x);
        if (!isPositiveAndBelow(clipRef.y, track.clips.size()))
            return;

        const auto& clip = track.clips.getReference(clipRef.y);

        if (rowIsSelected)
            g.fillAll(juce::Colours::white.withAlpha(0.08f));

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText(clip.file.getFileNameWithoutExtension(), 8, 8, width - 16, 16, juce::Justification::left);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.setFont(juce::Font(11.0f));
        g.drawText("Track " + juce::String(clipRef.x + 1) + "  •  " + juce::String(clip.startSample), 8, 24, width - 16, 14, juce::Justification::left);
    }

    void NovaAlignPanel::changeListenerCallback(juce::ChangeBroadcaster* source)
    {
        if (source == &arrangementModel)
            refreshContent();
    }

    void NovaAlignPanel::refreshContent()
    {
        if (arrangementModel.hasGuideClip())
            guideLabel.setText("Guide: " + arrangementModel.getSession().getTrack(arrangementModel.getGuideTrackIndex()).clips.getReference(arrangementModel.getGuideClipIndex()).file.getFileNameWithoutExtension(), juce::dontSendNotification);
        else
            guideLabel.setText("Guide: None", juce::dontSendNotification);

        targetClipList.updateContent();
        targetClipList.repaint();

        // Sync bypass toggle to session preview state (Session is single source of truth)
        const bool previewEnabled = arrangementModel.getSession().isPreviewPlaybackEnabled();
        bypassPreviewToggle.setToggleState(!previewEnabled, juce::dontSendNotification);
    }

    WorkspaceToolbar::WorkspaceToolbar()
    {
        addAndMakeVisible(editBtn);
        addAndMakeVisible(mixBtn);
        addAndMakeVisible(browseBtn);
        addAndMakeVisible(rtzBtn);
        addAndMakeVisible(playBtn);
        addAndMakeVisible(stopBtn);
        addAndMakeVisible(recordBtn);
        addAndMakeVisible(armBtn);
        addAndMakeVisible(monitorBtn);
        addAndMakeVisible(loopBtn);
        addAndMakeVisible(timecodeLabel);
        addAndMakeVisible(tempoLabel);
        addAndMakeVisible(saveBtn);
        addAndMakeVisible(loadBtn);
        addAndMakeVisible(audioBtn);
        addAndMakeVisible(novaAlignBtn);

        editBtn.addListener(this);
        mixBtn.addListener(this);
        browseBtn.addListener(this);
        rtzBtn.addListener(this);
        playBtn.addListener(this);
        stopBtn.addListener(this);
        recordBtn.addListener(this);
        armBtn.addListener(this);
        monitorBtn.addListener(this);
        loopBtn.addListener(this);
        saveBtn.addListener(this);
        loadBtn.addListener(this);
        audioBtn.addListener(this);
        novaAlignBtn.addListener(this);

        // Mode tab styling — dark base, will highlight active
        for (auto* btn : {&editBtn, &mixBtn, &browseBtn})
        {
            btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(18, 20, 28));
            btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.65f));
            btn->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        }
        // Edit is default active — highlight it
        editBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(30, 28, 50));
        editBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(180, 155, 255));

        // Transport styling
        for (auto* btn : {&rtzBtn, &playBtn, &stopBtn, &recordBtn, &armBtn, &monitorBtn, &loopBtn})
        {
            btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(20, 22, 30));
            btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
        }
        recordBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(255, 80, 80));

        // Timecode label — large amber
        timecodeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(255, 190, 60));
        timecodeLabel.setFont(juce::Font(juce::FontOptions(20.0f).withStyle("Bold")));
        timecodeLabel.setJustificationType(juce::Justification::centred);
        timecodeLabel.setText("00:00:00:00", juce::dontSendNotification);

        // Tempo label — amber, smaller
        tempoLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(220, 170, 60));
        tempoLabel.setJustificationType(juce::Justification::centred);
        tempoLabel.setText("120 BPM", juce::dontSendNotification);

        // Right utility buttons
        novaAlignBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(35, 25, 55));
        novaAlignBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(190, 150, 255));
        saveBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(20, 40, 22));
        saveBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(100, 210, 90));
        loadBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(20, 30, 48));
        loadBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(120, 170, 255));
        audioBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(22, 20, 40));
        audioBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(160, 140, 255));
    }

    WorkspaceToolbar::~WorkspaceToolbar() = default;

    void WorkspaceToolbar::paint(juce::Graphics& g)
    {
        // Very dark gradient background
        g.setGradientFill(juce::ColourGradient(
            juce::Colour::fromRGB(8, 9, 14), 0.0f, 0.0f,
            juce::Colour::fromRGB(12, 14, 20), 0.0f, (float)getHeight(), false));
        g.fillRect(getLocalBounds());

        // Bottom border — subtle gold line
        g.setColour(juce::Colour::fromRGB(100, 82, 40).withAlpha(0.4f));
        g.fillRect(0, getHeight() - 1, getWidth(), 1);

        // Gold circular N badge
        const float badgeX = 14.0f, badgeY = 9.0f, badgeD = 36.0f;
        // Outer gold ring
        g.setColour(juce::Colour::fromRGB(180, 140, 50));
        g.drawEllipse(badgeX, badgeY, badgeD, badgeD, 1.5f);
        // Inner dark fill
        g.setColour(juce::Colour::fromRGB(10, 10, 15));
        g.fillEllipse(badgeX + 2.0f, badgeY + 2.0f, badgeD - 4.0f, badgeD - 4.0f);
        // N letter in gold
        g.setColour(juce::Colour::fromRGB(200, 160, 60));
        g.setFont(juce::Font(juce::FontOptions(18.0f).withStyle("Bold")));
        g.drawText("N", (int)badgeX, (int)badgeY, (int)badgeD, (int)badgeD, juce::Justification::centred);

        // "NOVA STUDIO" text
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
        g.drawText("NOVA", 56, 9, 52, 16, juce::Justification::centredLeft);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText("STUDIO", 56, 26, 52, 14, juce::Justification::centredLeft);

        // Vertical dividers between sections
        auto divColor = juce::Colour::fromRGB(40, 44, 58);
        g.setColour(divColor);
        g.fillRect(185, 6, 1, getHeight() - 12);  // after logo
        g.fillRect(390, 6, 1, getHeight() - 12);  // after mode tabs
        g.fillRect(760, 6, 1, getHeight() - 12);  // after transport
        g.fillRect(940, 6, 1, getHeight() - 12);  // after timecode
    }

    void WorkspaceToolbar::resized()
    {
        const int H = getHeight();
        const int btnH = 26;
        const int btnY = (H - btnH) / 2;

        // Logo area: 0-185 (painted)
        int x = 193;

        // Mode tabs
        editBtn.setBounds(x, btnY, 52, btnH); x += 56;
        mixBtn.setBounds(x, btnY, 44, btnH); x += 48;
        browseBtn.setBounds(x, btnY, 60, btnH); x += 64;
        // x now at ~395, past divider at 390

        x = 398;
        // Transport
        rtzBtn.setBounds(x, btnY, 30, btnH); x += 34;
        playBtn.setBounds(x, btnY, 56, btnH); x += 60;
        stopBtn.setBounds(x, btnY, 52, btnH); x += 56;
        recordBtn.setBounds(x, btnY, 44, btnH); x += 48;
        x += 6;
        armBtn.setBounds(x, btnY, 44, btnH); x += 48;
        monitorBtn.setBounds(x, btnY, 44, btnH); x += 48;
        loopBtn.setBounds(x, btnY, 44, btnH); x += 48;
        // x ~760

        x = 768;
        timecodeLabel.setBounds(x, btnY - 2, 160, btnH + 4); x += 164;
        tempoLabel.setBounds(x, btnY, 100, btnH); x += 104;

        // Right buttons (from right edge)
        int rx = getWidth() - 6;
        audioBtn.setBounds(rx - 58, btnY, 56, btnH); rx -= 62;
        loadBtn.setBounds(rx - 50, btnY, 48, btnH); rx -= 54;
        saveBtn.setBounds(rx - 50, btnY, 48, btnH); rx -= 54;
        novaAlignBtn.setBounds(rx - 80, btnY, 78, btnH);
    }

    void WorkspaceToolbar::setPlayState(bool isPlaying, bool isRecording)
    {
        playBtn.setColour(juce::TextButton::buttonColourId,
            isPlaying ? juce::Colours::lightgreen.withAlpha(0.35f) : juce::Colours::transparentBlack);
        recordBtn.setColour(juce::TextButton::buttonColourId,
            isRecording ? juce::Colours::red.withAlpha(0.55f) : juce::Colours::transparentBlack);
    }

    void WorkspaceToolbar::setLoopState(bool enabled)
    {
        loopBtn.setColour(juce::TextButton::buttonColourId,
            enabled ? juce::Colours::yellow.withAlpha(0.35f) : juce::Colours::transparentBlack);
    }

    void WorkspaceToolbar::setArmState(bool armed)
    {
        armBtn.setColour(juce::TextButton::buttonColourId,
            armed ? juce::Colours::orange.withAlpha(0.45f) : juce::Colours::transparentBlack);
    }

    void WorkspaceToolbar::setMonitorState(bool enabled)
    {
        monitorBtn.setColour(juce::TextButton::buttonColourId,
            enabled ? juce::Colours::skyblue.withAlpha(0.35f) : juce::Colours::transparentBlack);
    }

    void WorkspaceToolbar::setTempo(int bpm)
    {
        tempoLabel.setText(juce::String(bpm) + " BPM", juce::dontSendNotification);
    }

    void WorkspaceToolbar::setTimecode(const juce::String& tc)
    {
        timecodeLabel.setText(tc, juce::dontSendNotification);
    }

    void WorkspaceToolbar::setPlaybackState(bool /*previewEnabled*/, bool /*hasPreview*/) {}

    void WorkspaceToolbar::buttonClicked(juce::Button* b)
    {
        if (b == &editBtn && onModeSelected) onModeSelected(0);
        else if (b == &mixBtn && onModeSelected) onModeSelected(1);
        else if (b == &browseBtn && onModeSelected) onModeSelected(2);
        else if (b == &rtzBtn && onReturnToZero) onReturnToZero();
        else if (b == &playBtn && onPlay) onPlay();
        else if (b == &stopBtn && onStop) onStop();
        else if (b == &recordBtn && onRecord) onRecord();
        else if (b == &armBtn && onArm) onArm();
        else if (b == &monitorBtn && onMonitor) onMonitor();
        else if (b == &loopBtn && onLoop) onLoop();
        else if (b == &novaAlignBtn && onNovaAlign) onNovaAlign();
        else if (b == &saveBtn && onSave) onSave();
        else if (b == &loadBtn && onLoad) onLoad();
        else if (b == &audioBtn && onAudioSettings) onAudioSettings();
    }
}
