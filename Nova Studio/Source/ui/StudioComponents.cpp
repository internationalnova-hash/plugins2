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
        // Header
        g.setColour(juce::Colour::fromRGB(14, 16, 22));
        g.fillRect(getLocalBounds());
        g.setColour(juce::Colour::fromRGB(35, 38, 52));
        g.fillRect(0, 0, getWidth(), 28);
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("TRACKS", 10, 0, getWidth(), 28, juce::Justification::centredLeft);

        const int numTracks = session.getNumTracks();
        const int w = getWidth();

        // Button layout constants (right-aligned)
        const int btnW = 26, btnH = 18, btnGap = 3;
        const int soloX  = w - btnW - 6;
        const int muteX  = soloX - btnW - btnGap;
        const int armX   = muteX - btnW - btnGap;

        for (int i = 0; i < numTracks; ++i)
        {
            const auto& track = session.getTrack(i);
            const int y = 28 + i * kTrackHeight;

            // Row background
            g.setColour((i % 2 == 0) ? kTrackBg0 : kTrackBg1);
            g.fillRect(0, y, w, kTrackHeight);

            // Left color bar
            g.setColour(track.clips.size() > 0 ? track.clips[0].clipColor
                                               : juce::Colour::fromRGB(80, 80, 120));
            g.fillRect(0, y, 4, kTrackHeight);

            // Track name
            g.setColour(juce::Colours::white.withAlpha(0.92f));
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText(track.name, 12, y + 10, armX - 16, 18, juce::Justification::left);

            // Track type
            g.setColour(juce::Colours::white.withAlpha(0.38f));
            g.setFont(juce::Font(10.0f));
            g.drawText(track.type == NovaStudio::TrackType::Audio ? "Audio" : "MIDI",
                       12, y + 30, armX - 16, 14, juce::Justification::left);

            // ARM button
            const int btnY = y + (kTrackHeight - btnH) / 2;
            g.setColour(track.armed ? kArmedCol : kBtnDark);
            g.fillRoundedRectangle((float)armX, (float)btnY, (float)btnW, (float)btnH, 3.0f);
            g.setColour(track.armed ? juce::Colours::white : juce::Colours::white.withAlpha(0.45f));
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText("R", armX, btnY, btnW, btnH, juce::Justification::centred);

            // MUTE button
            g.setColour(track.muted ? kMuteCol : kBtnDark);
            g.fillRoundedRectangle((float)muteX, (float)btnY, (float)btnW, (float)btnH, 3.0f);
            g.setColour(track.muted ? juce::Colours::black : juce::Colours::white.withAlpha(0.45f));
            g.drawText("M", muteX, btnY, btnW, btnH, juce::Justification::centred);

            // SOLO button
            g.setColour(track.solo ? kSoloCol : kBtnDark);
            g.fillRoundedRectangle((float)soloX, (float)btnY, (float)btnW, (float)btnH, 3.0f);
            g.setColour(track.solo ? juce::Colours::black : juce::Colours::white.withAlpha(0.45f));
            g.drawText("S", soloX, btnY, btnW, btnH, juce::Justification::centred);

            // Row separator
            g.setColour(kTrackSep);
            g.drawLine(0, (float)(y + kTrackHeight - 1), (float)w, (float)(y + kTrackHeight - 1), 1.0f);
        }
    }

    TrackPanel::HitButton TrackPanel::hitTest(int trackIndex, juce::Point<int> pos) const
    {
        const int w = getWidth();
        const int btnW = 26, btnH = 18, btnGap = 3;
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
        g.fillAll(juce::Colour::fromRGB(20, 24, 34));
        g.setColour(Theme::panelEdge());
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 16.0f, 1.5f);
    }

    void InspectorPanel::resized()
    {
        auto area = getLocalBounds().reduced(16);
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

    ArrangementView::ArrangementView(NovaStudio::TransportState& transport,
                                     NovaStudio::TimelineModel& timelineModel,
                                     NovaStudio::ArrangementModel& arrangementModel)
        : transportState(transport), timelineModel(timelineModel), arrangementModel(arrangementModel)
    {
        transportState.addChangeListener(this);
        arrangementModel.addChangeListener(this);
        startTimerHz(30);
    }

    ArrangementView::~ArrangementView()
    {
        transportState.removeChangeListener(this);
        arrangementModel.removeChangeListener(this);
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

        // Track lanes — aligned with TrackPanel rows (28px header offset on left panel)
        const float trackHeight = (float)TrackPanel::kTrackHeight;
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

                juce::Colour fillColor = clip.clipColor;
                if (clip.muted)
                    fillColor = fillColor.withAlpha(0.25f);
                else if (clipSelectedSingle || clipMultiSelected)
                    fillColor = fillColor.brighter(0.4f);

                g.setColour(fillColor);
                g.fillRoundedRectangle((float)clipStartX, clipY, clipWidth, clipHeight, 8.0f);

                g.setColour(juce::Colours::black.withAlpha(0.12f));
                g.drawRoundedRectangle((float)clipStartX, clipY, clipWidth, clipHeight, 8.0f, 1.4f);

                const int stripeCount = juce::jmax(2, static_cast<int>(clipWidth / 22.0));
                g.setColour(juce::Colours::white.withAlpha(0.12f));
                for (int stripe = 0; stripe < stripeCount; ++stripe)
                {
                    const float lineX = (float)clipStartX + stripe * 22.0f;
                    g.drawLine(lineX, clipY + 4.0f, lineX, clipY + clipHeight - 4.0f, 1.0f);
                }

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
                            g.setColour(juce::Colours::white.withAlpha(0.12f));
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

        // Loop region
        if (timelineModel.isLooping())
        {
            const double loopStartX = timelineModel.getXForSamplePosition(timelineModel.getLoopStartSample(), width);
            const double loopEndX   = timelineModel.getXForSamplePosition(timelineModel.getLoopEndSample(), width);
            const auto loopRect = juce::Rectangle<float>((float)loopStartX, (float)rulerH,
                                                         (float)juce::jmax(1.0, loopEndX - loopStartX), (float)(H - rulerH));
            g.setColour(juce::Colours::yellow.withAlpha(0.09f));
            g.fillRect(loopRect);
            g.setColour(juce::Colours::yellow.withAlpha(0.32f));
            g.drawRect(loopRect, 1.4f);
        }

        // Playhead
        const float playheadX = (float)currentX;
        g.setColour(juce::Colour::fromRGB(255, 60, 60).withAlpha(0.9f));
        g.drawLine(playheadX, 0.0f, playheadX, (float)H, 2.0f);
        // Playhead triangle on ruler
        juce::Path arrow;
        arrow.addTriangle(playheadX - 6.0f, 0.0f, playheadX + 6.0f, 0.0f, playheadX, 10.0f);
        g.setColour(juce::Colour::fromRGB(255, 60, 60));
        g.fillPath(arrow);

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

    void ArrangementView::mouseDown(const juce::MouseEvent& event)
    {
        const auto clickPoint = event.position;
        const int width = getWidth() - 16;
        const auto trackArea = juce::Rectangle<float>(8.0f, 72.0f, (float)width, getHeight() - 88.0f);
        const float trackHeight = (float)TrackPanel::kTrackHeight;

        const int trackIndex = static_cast<int>((clickPoint.y - trackArea.getY()) / trackHeight);
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
            const double clipStartX = trackArea.getX() + timelineModel.getXForSamplePosition(clip.startSample, width);
            const double clipEndX = trackArea.getX() + timelineModel.getXForSamplePosition(clip.startSample + clip.lengthSamples, width);
            const auto clipRect = juce::Rectangle<float>((float)clipStartX, trackArea.getY() + trackIndex * trackHeight + 38.0f,
                                                        (float)juce::jmax(8.0, clipEndX - clipStartX), trackHeight - 48.0f);
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
                else
                {
                    arrangementModel.selectClip(trackIndex, clipIndex);
                    isDraggingClip = true;
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
            const int width = getWidth() - 16;
            const float trackHeight = (float)TrackPanel::kTrackHeight;
            const auto trackAreaY = 72.0f;
            for (int t = 0; t < arrangementModel.getSession().getNumTracks(); ++t)
            {
                const auto& track = arrangementModel.getSession().getTrack(t);
                for (int c = 0; c < track.clips.size(); ++c)
                {
                    const auto& clip = track.clips.getReference(c);
                    const double clipStartX = 8.0 + timelineModel.getXForSamplePosition(clip.startSample, width);
                    const double clipEndX = 8.0 + timelineModel.getXForSamplePosition(clip.startSample + clip.lengthSamples, width);
                    const auto clipRect = juce::Rectangle<float>((float)clipStartX, trackAreaY + t * trackHeight + 38.0f,
                                                                (float)juce::jmax(8.0, clipEndX - clipStartX), trackHeight - 48.0f);
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
        arrangementModel.moveClipsBySamples(targets, deltaSamples);
        // update baseline so further dragging accumulates
        dragStartX = event.x;
    }

    void ArrangementView::mouseUp(const juce::MouseEvent& event)
    {
        juce::ignoreUnused(event);
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

MixerPanel::MixerPanel() {}
MixerPanel::~MixerPanel() = default;

void MixerPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour::fromRGB(19, 22, 28));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 14.0f);
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("MIXER", 14, 12, getWidth() - 28, 18, juce::Justification::left);
}

void MixerPanel::resized() {}

BrowserPanel::BrowserPanel() {}
BrowserPanel::~BrowserPanel() = default;

void BrowserPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour::fromRGB(17, 20, 26));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 12.0f);
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("BROWSER", 14, 12, getWidth() - 28, 18, juce::Justification::left);
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
        addAndMakeVisible(mixerBtn);
        addAndMakeVisible(splitBtn);
        addAndMakeVisible(beatBtn);
        addAndMakeVisible(rackBtn);
        addAndMakeVisible(slipBtn);
        addAndMakeVisible(gridBtn);
        addAndMakeVisible(shuffleBtn);
        addAndMakeVisible(spotBtn);
        addAndMakeVisible(novaAlignBtn);
        addAndMakeVisible(saveBtn);
        addAndMakeVisible(loadBtn);
        addAndMakeVisible(audioBtn);

        editBtn.addListener(this);
        mixerBtn.addListener(this);
        splitBtn.addListener(this);
        beatBtn.addListener(this);
        rackBtn.addListener(this);
        slipBtn.addListener(this);
        gridBtn.addListener(this);
        shuffleBtn.addListener(this);
        spotBtn.addListener(this);
        novaAlignBtn.addListener(this);
        saveBtn.addListener(this);
        loadBtn.addListener(this);
        audioBtn.addListener(this);

        saveBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(30, 50, 30));
        saveBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(120, 220, 100));
        loadBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(28, 36, 50));
        loadBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(140, 180, 255));
        audioBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(30, 30, 50));
        audioBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(180, 160, 255));
    }

    WorkspaceToolbar::~WorkspaceToolbar() = default;

    void WorkspaceToolbar::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(8, 10, 14));
    }

    void WorkspaceToolbar::resized()
    {
        auto area = getLocalBounds().reduced(8);
        auto topRow = area.removeFromTop(32);
        auto bottomRow = area;

        // Reserve right-side buttons FIRST so workspace buttons share remaining space
        saveBtn.setBounds(topRow.removeFromRight(56).reduced(4));
        loadBtn.setBounds(topRow.removeFromRight(56).reduced(4));
        audioBtn.setBounds(topRow.removeFromRight(60).reduced(4));

        auto buttonWidth = topRow.getWidth() / 5;
        editBtn.setBounds(topRow.removeFromLeft(buttonWidth).reduced(4));
        mixerBtn.setBounds(topRow.removeFromLeft(buttonWidth).reduced(4));
        splitBtn.setBounds(topRow.removeFromLeft(buttonWidth).reduced(4));
        beatBtn.setBounds(topRow.removeFromLeft(buttonWidth).reduced(4));
        rackBtn.setBounds(topRow.removeFromLeft(buttonWidth).reduced(4));

        buttonWidth = bottomRow.getWidth() / 5;
        slipBtn.setBounds(bottomRow.removeFromLeft(buttonWidth).reduced(4));
        gridBtn.setBounds(bottomRow.removeFromLeft(buttonWidth).reduced(4));
        shuffleBtn.setBounds(bottomRow.removeFromLeft(buttonWidth).reduced(4));
        spotBtn.setBounds(bottomRow.removeFromLeft(buttonWidth).reduced(4));
        novaAlignBtn.setBounds(bottomRow.removeFromLeft(buttonWidth).reduced(4));
    }

    void WorkspaceToolbar::buttonClicked(juce::Button* b)
    {
        if (b == &editBtn && onModeSelected) onModeSelected(0);
        else if (b == &mixerBtn && onModeSelected) onModeSelected(1);
        else if (b == &splitBtn && onModeSelected) onModeSelected(2);
        else if (b == &beatBtn && onModeSelected) onModeSelected(3);
        else if (b == &rackBtn && onModeSelected) onModeSelected(4);
        else if (b == &slipBtn && onEditModeSelected) onEditModeSelected(NovaStudio::ArrangementModel::EditMode::Slip);
        else if (b == &gridBtn && onEditModeSelected) onEditModeSelected(NovaStudio::ArrangementModel::EditMode::Grid);
        else if (b == &shuffleBtn && onEditModeSelected) onEditModeSelected(NovaStudio::ArrangementModel::EditMode::Shuffle);
        else if (b == &spotBtn && onEditModeSelected) onEditModeSelected(NovaStudio::ArrangementModel::EditMode::Spot);
        else if (b == &novaAlignBtn && onNovaAlign) onNovaAlign();
        else if (b == &saveBtn && onSave) onSave();
        else if (b == &loadBtn && onLoad) onLoad();
        else if (b == &audioBtn && onAudioSettings) onAudioSettings();
    }
}
