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
        addAndMakeVisible(monitorButton);
        addAndMakeVisible(loopButton);
        addAndMakeVisible(tempoLabel);
        addAndMakeVisible(timeLabel);

        rtzButton.addListener(this);
        playButton.addListener(this);
        stopButton.addListener(this);
        recordButton.addListener(this);
        monitorButton.addListener(this);
        loopButton.addListener(this);
        punchButton.addListener(this);
        addAndMakeVisible(punchButton);

        punchButton.setTooltip("Auto-Punch: enable/disable punch recording. Set punch range by Cmd+dragging in the timeline ruler.");

        monitorButton.setTooltip("Input Monitor: hear your audio input live through the mix while recording");

        tempoLabel.setBPM(120);
        tempoLabel.setJustificationType(juce::Justification::centred);
        tempoLabel.setTooltip("Drag up/down to change BPM, or double-click to type");
        tempoLabel.onTempoChanged = [this](int bpm) { if (onTempoChanged) onTempoChanged(bpm); };
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

        // Draw icons at each button's position (avoids Unicode font issues on Linux)
        auto drawIcon = [&](juce::TextButton& btn, std::function<void(juce::Graphics&, juce::Rectangle<float>)> fn) {
            fn(g, btn.getBounds().toFloat());
        };

        // RTZ |◀
        drawIcon(rtzButton, [](juce::Graphics& g, juce::Rectangle<float> r) {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.fillRect(r.getX() + r.getWidth()*0.2f, r.getY() + r.getHeight()*0.25f, 2.0f, r.getHeight()*0.5f);
            juce::Path tri;
            float cx = r.getCentreX() + 2.0f, cy = r.getCentreY();
            float h = r.getHeight() * 0.35f;
            tri.addTriangle(cx + h, cy - h, cx + h, cy + h, cx - h*0.5f, cy);
            g.fillPath(tri);
        });

        // Play ▶
        drawIcon(playButton, [](juce::Graphics& g, juce::Rectangle<float> r) {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            juce::Path tri;
            float cx = r.getCentreX(), cy = r.getCentreY(), h = r.getHeight()*0.35f;
            tri.addTriangle(cx - h*0.6f, cy - h, cx - h*0.6f, cy + h, cx + h, cy);
            g.fillPath(tri);
        });

        // Stop ■
        drawIcon(stopButton, [](juce::Graphics& g, juce::Rectangle<float> r) {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            float s = r.getHeight() * 0.4f;
            g.fillRect(r.getCentreX() - s/2, r.getCentreY() - s/2, s, s);
        });

        // Record ●
        drawIcon(recordButton, [](juce::Graphics& g, juce::Rectangle<float> r) {
            g.setColour(juce::Colour::fromRGB(220, 55, 55).withAlpha(0.9f));
            float s = r.getHeight() * 0.38f;
            g.fillEllipse(r.getCentreX() - s, r.getCentreY() - s, s*2, s*2);
        });

        // Loop ↻ (arc + arrowhead)
        drawIcon(loopButton, [](juce::Graphics& g, juce::Rectangle<float> r) {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            float cx = r.getCentreX(), cy = r.getCentreY(), rad = r.getHeight()*0.3f;
            juce::Path arc;
            arc.addArc(cx - rad, cy - rad, rad*2, rad*2,
                       juce::MathConstants<float>::pi * 0.2f,
                       juce::MathConstants<float>::pi * 1.9f, true);
            g.strokePath(arc, juce::PathStrokeType(1.8f));
            float ax = cx + rad * std::cos(juce::MathConstants<float>::pi * 0.2f);
            float ay = cy + rad * std::sin(juce::MathConstants<float>::pi * 0.2f);
            juce::Path head;
            head.addTriangle(ax, ay - 3.0f, ax + 5.0f, ay + 1.0f, ax - 2.0f, ay + 3.0f);
            g.fillPath(head);
        });

        // Monitor ◎
        drawIcon(monitorButton, [](juce::Graphics& g, juce::Rectangle<float> r) {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            float cx = r.getCentreX(), cy = r.getCentreY(), rad = r.getHeight()*0.28f;
            g.drawEllipse(cx - rad, cy - rad, rad*2, rad*2, 1.5f);
            float ir = rad * 0.45f;
            g.fillEllipse(cx - ir, cy - ir, ir*2, ir*2);
        });
    }

    void TransportBar::resized()
    {
        const int H   = getHeight();
        const int btnSz = juce::jmin(H - 10, 36); // square icon buttons
        const int gap = 3;
        int x = 12;
        auto place = [&](juce::TextButton& btn, int w = -1) {
            const int bw = (w < 0) ? btnSz : w;
            btn.setBounds(x, (H - btnSz) / 2, bw, btnSz);
            x += bw + gap;
        };

        place(rtzButton, btnSz);       // ⏮  return to zero
        x += 4;                        // small spacer
        place(playButton, btnSz);      // ▶  play
        place(stopButton, btnSz);      // ■  stop
        place(recordButton, btnSz);    // ⏺  record
        x += 8;                        // separator
        place(loopButton, btnSz);      // ↻  loop
        place(monitorButton, btnSz);   // 🎧 input monitor
        x += 8;
        place(punchButton,    60);

        // Timecode + tempo on the right
        auto rightArea = getLocalBounds().withTrimmedLeft(x + 8).withTrimmedRight(8);
        const int midY = (H - btnSz) / 2;
        tempoLabel.setBounds(rightArea.removeFromRight(90).withY(midY).withHeight(btnSz));
        timeLabel.setBounds(rightArea.removeFromRight(120).withY(midY).withHeight(btnSz));

        // Playback preview indicator (below main row, compact)
        playbackLabel.setBounds(x + 8, H - 18, 200, 16);
        playbackToggleButton.setBounds(x + 214, H - 20, 72, 18);
    }


    void TransportBar::setTempo(int bpm)
    {
        tempoLabel.setBPM(bpm);
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

    void TransportBar::setMonitorState(bool enabled)
    {
        monitorButton.setColour(juce::TextButton::buttonColourId, enabled ? juce::Colours::skyblue.withAlpha(0.35f) : juce::Colours::transparentBlack);
    }

    void TransportBar::setPunchState(bool enabled)
    {
        punchButton.setColour(juce::TextButton::buttonColourId, enabled ? juce::Colours::orangered.withAlpha(0.45f) : juce::Colours::transparentBlack);
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
        else if (button == &monitorButton && onMonitor) onMonitor();
        else if (button == &loopButton && onLoop) onLoop();
        else if (button == &punchButton    && onPunchToggle) onPunchToggle();
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
        startTimerHz(24); // 24 fps meter refresh
    }

    TrackPanel::~TrackPanel()
    {
        stopTimer();
    }

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
            const int y = 28 + i * kTrackHeight - scrollY;
            if (y + kTrackHeight < 0 || y > getHeight()) continue;  // outside viewport
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
            g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
            g.drawText(track.name, 32, y + 4, armX - 36, 16, juce::Justification::left);

            // Track type badge
            g.setColour(trackColor.withAlpha(0.7f));
            g.setFont(juce::Font(juce::FontOptions(8.5f)));
            g.drawText(track.type == NovaStudio::TrackType::Audio ? "AUDIO" : "MIDI",
                       32, y + 20, 32, 11, juce::Justification::left);

            // Volume dB label (small, right of type)
            {
                juce::String volStr;
                if (track.volumeDb <= -60.0f) volStr = "-inf";
                else volStr = juce::String(track.volumeDb, 1) + "dB";
                g.setColour(juce::Colours::white.withAlpha(0.45f));
                g.setFont(juce::Font(juce::FontOptions(8.5f)));
                g.drawText(volStr, 66, y + 20, armX - 70, 11, juce::Justification::left);
            }

            // Pan indicator: thin horizontal bar (40px wide)
            {
                const int pBarW = 40, pBarH = 4;
                const int pBarX = 32;
                const int pBarY = y + 33;
                g.setColour(juce::Colour::fromRGB(30, 33, 48));
                g.fillRect(pBarX, pBarY, pBarW, pBarH);
                // center dot
                g.setColour(juce::Colour::fromRGB(60, 65, 90));
                g.fillRect(pBarX + pBarW / 2, pBarY, 1, pBarH);
                // pan fill
                const float panN = (track.pan + 1.0f) * 0.5f; // 0..1
                const int   cX   = pBarX + pBarW / 2;
                const int   pX   = pBarX + (int)(panN * pBarW);
                const int   rx   = juce::jmin(cX, pX), rw = std::abs(pX - cX);
                if (rw > 0)
                {
                    g.setColour(trackColor.withAlpha(0.8f));
                    g.fillRect(rx, pBarY, rw, pBarH);
                }
                // Pan label (L/C/R)
                juce::String panStr;
                if (track.pan < -0.02f)       panStr = "L" + juce::String((int)(-track.pan * 100));
                else if (track.pan > 0.02f)   panStr = "R" + juce::String((int)(track.pan * 100));
                else                           panStr = "C";
                g.setColour(juce::Colours::white.withAlpha(0.4f));
                g.setFont(juce::Font(juce::FontOptions(8.0f)));
                g.drawText(panStr, pBarX + pBarW + 3, pBarY - 1, 24, 11, juce::Justification::left);
            }

            // Input/Output bus labels (small, grey, at y+36)
            {
                const juce::String inStr  = track.inputBus.isEmpty()  ? "Default In" : track.inputBus;
                const juce::String outStr = track.outputBus.isEmpty() ? "Main Out"   : track.outputBus;
                g.setColour(juce::Colours::white.withAlpha(0.28f));
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.drawText(inStr,  32, y + 36, armX - 36, 11, juce::Justification::left);
                g.drawText(outStr, 32, y + 36, armX - 36, 11, juce::Justification::right);
            }

            // Buttons (bottom row)
            const int btnY = y + kTrackHeight - btnH - 6;

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

            // Inline L/R level meters (two 4px bars, right-aligned below buttons)
            {
                const float levelL = getTrackLevel ? getTrackLevel(i, 0) : 0.0f;
                const float levelR = getTrackLevel ? getTrackLevel(i, 1) : 0.0f;
                const int meterH   = kTrackHeight - 18;
                const int meterY   = y + 9;
                const int mRx      = W - 5;   // right bar x
                const int mLx      = mRx - 5; // left bar x
                const int barW     = 4;

                // Trough
                g.setColour(juce::Colour::fromRGB(18, 20, 28));
                g.fillRect(mLx, meterY, barW, meterH);
                g.fillRect(mRx, meterY, barW, meterH);

                // Level fill L
                const int fillHL = juce::jlimit(0, meterH, (int)(levelL * meterH));
                const int fillHR = juce::jlimit(0, meterH, (int)(levelR * meterH));
                if (fillHL > 0)
                {
                    g.setColour(levelL > 0.9f ? juce::Colour::fromRGB(220, 40, 40)
                                : levelL > 0.7f ? juce::Colour::fromRGB(200, 180, 30)
                                               : juce::Colour::fromRGB(50, 190, 100));
                    g.fillRect(mLx, meterY + meterH - fillHL, barW, fillHL);
                }
                if (fillHR > 0)
                {
                    g.setColour(levelR > 0.9f ? juce::Colour::fromRGB(220, 40, 40)
                                : levelR > 0.7f ? juce::Colour::fromRGB(200, 180, 30)
                                               : juce::Colour::fromRGB(50, 190, 100));
                    g.fillRect(mRx, meterY + meterH - fillHR, barW, fillHR);
                }
            }

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
        const int y     = 28 + trackIndex * kTrackHeight - scrollY;
        const int btnY  = y + kTrackHeight - btnH - 6;

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

        // "+" button in header bar (top 28px)
        if (pos.y < 28 && pos.x >= getWidth() - 30)
        {
            if (onAddTrackClicked) onAddTrackClicked();
            return;
        }

        for (int i = 0; i < numTracks; ++i)
        {
            const int y = 28 + i * kTrackHeight - scrollY;
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

    void TrackPanel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
    {
        const int totalH = session.getNumTracks() * kTrackHeight;
        const int maxScroll = juce::jmax(0, totalH - (getHeight() - 28));
        scrollY = juce::jlimit(0, maxScroll, scrollY - (int)(w.deltaY * 80.0f));
        repaint();
        if (onScrollChanged) onScrollChanged(scrollY);
    }

    void TrackPanel::mouseDoubleClick(const juce::MouseEvent& e)
    {
        const int kHdrH = 28;
        const int y = e.y - kHdrH + scrollY;
        if (y < 0) return;
        const int trackIndex = y / kTrackHeight;
        if (trackIndex < 0 || trackIndex >= session.getNumTracks()) return;

        auto& track = session.getTrack(trackIndex);
        auto* dialog = new juce::AlertWindow("Rename Track",
                                             "Enter new track name:",
                                             juce::MessageBoxIconType::NoIcon);
        dialog->addTextEditor("name", track.name, "Name:");
        dialog->addButton("OK",     1, juce::KeyPress(juce::KeyPress::returnKey));
        dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        dialog->setColour(juce::AlertWindow::backgroundColourId, juce::Colour::fromRGB(18, 20, 28));
        dialog->setColour(juce::AlertWindow::textColourId, juce::Colours::white);

        dialog->enterModalState(true,
            juce::ModalCallbackFunction::create([this, dialog, trackIndex](int result)
            {
                if (result == 1)
                {
                    const juce::String newName = dialog->getTextEditorContents("name").trim();
                    if (newName.isNotEmpty())
                    {
                        session.getTrack(trackIndex).name = newName;
                        if (onTrackRenamed) onTrackRenamed(trackIndex, newName);
                        repaint();
                    }
                }
            }), true);
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
        // Ctrl+L → quick-route selected track to a new send bus (auto-routing)
        if (kc == 'l' && key.getModifiers().isCommandDown())
        {
            if (onAutoRouteSelectedTrack) onAutoRouteSelectedTrack();
            return true;
        }
        // Escape → clear selection range
        if (kc == juce::KeyPress::escapeKey)
        {
            transportState.clearLoopRange();
            repaint();
            return true;
        }
        return false;
    }

    void ArrangementView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        if (e.mods.isCommandDown())
        {
            adjustHZoom(wheel.deltaY > 0 ? +1 : -1);
        }
        else if (e.mods.isShiftDown())
        {
            // Horizontal scroll
            juce::Component::mouseWheelMove(e, wheel);
        }
        else
        {
            // Vertical track scroll — clamp and sync with TrackPanel
            const int totalH = arrangementModel.getSession().getNumTracks() * trackHeightPx;
            const int maxScroll = juce::jmax(0, totalH - (getHeight() - 28));
            trackScrollY = juce::jlimit(0, maxScroll, trackScrollY - (int)(wheel.deltaY * 80.0f));
            repaint();
            if (onScrollChanged) onScrollChanged(trackScrollY);
        }
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

    // Display range/centre for an automation parameter — used to map values to
    // the 0..1 normalized vertical position drawn in the lane strip.
    static float automationParameterRange(const juce::String& parameterId)
    {
        if (parameterId == "volume") return 30.0f;   // ±30 dB
        if (parameterId == "pan")    return 1.0f;    // ±1.0
        if (parameterId.startsWith("send")) return 30.0f; // ±30 dB
        return 1.0f;
    }

    static float automationParameterCentre(const juce::String& parameterId)
    {
        if (parameterId == "volume") return -15.0f;
        if (parameterId.startsWith("send")) return -50.0f;
        return 0.0f;
    }

    static float automationValueToY(float value, const juce::String& parameterId, float stripTop, float stripBottom)
    {
        const float range  = automationParameterRange(parameterId);
        const float centre = automationParameterCentre(parameterId);
        const float norm = juce::jlimit(0.0f, 1.0f, 0.5f + (value - centre) / (2.0f * range));
        return stripBottom - 2.0f - norm * (stripBottom - stripTop - 4.0f);
    }

    static float automationYToValue(float y, const juce::String& parameterId, float stripTop, float stripBottom)
    {
        const float range  = automationParameterRange(parameterId);
        const float centre = automationParameterCentre(parameterId);
        const float h = stripBottom - stripTop - 4.0f;
        const float norm = h > 0.0f ? juce::jlimit(0.0f, 1.0f, (stripBottom - 2.0f - y) / h) : 0.5f;
        return centre + (norm - 0.5f) * (2.0f * range);
    }

    static void insertAutomationPointSorted(juce::Array<NovaStudio::AutomationPoint>& points, double timeSeconds, float value)
    {
        NovaStudio::AutomationPoint point;
        point.timeSeconds = timeSeconds;
        point.value = value;
        int insertIndex = points.size();
        for (int i = 0; i < points.size(); ++i)
        {
            if (timeSeconds < points.getReference(i).timeSeconds) { insertIndex = i; break; }
        }
        points.insert(insertIndex, point);
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

        // Session markers — named timeline positions (drawn over ruler)
        {
            const auto& sess = arrangementModel.getSession();
            const double sr = sess.getSampleRate();
            for (int m = 0; m < sess.getNumMarkers(); ++m)
            {
                const auto& marker = sess.getMarker(m);
                const auto markerSample = static_cast<int64_t>(marker.timeSeconds * sr);
                const double markerX = timelineModel.getXForSamplePosition(markerSample, W);
                if (markerX < -40.0 || markerX > (double)W) continue;

                const auto markerColour = juce::Colour::fromRGB(230, 180, 60);
                g.setColour(markerColour);
                g.drawLine((float)markerX, 0.0f, (float)markerX, (float)rulerH, 1.5f);

                // Flag/triangle pointing right at the top
                juce::Path flag;
                flag.startNewSubPath((float)markerX, 0.0f);
                flag.lineTo((float)markerX + 8.0f, 4.0f);
                flag.lineTo((float)markerX, 8.0f);
                flag.closeSubPath();
                g.fillPath(flag);

                g.setColour(markerColour.withAlpha(0.9f));
                g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
                g.drawText(marker.name, (int)markerX + 10, 1, 100, 12, juce::Justification::left);
            }
        }

        // Track lanes — height follows vertical zoom setting
        const float trackHeight = (float)trackHeightPx;
        const NovaStudio::Session& session = arrangementModel.getSession();
        const int trackCount = session.getNumTracks();
        const float lanesTop = (float)rulerH;

        for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
        {
            const float top = lanesTop + trackIndex * trackHeight - (float)trackScrollY;
            // skip if completely outside view
            if (top + trackHeight <= lanesTop || top >= (float)H) continue;
            const auto& track = session.getTrack(trackIndex);

            // Lane background
            g.setColour((trackIndex % 2 == 0) ? juce::Colour::fromRGB(16, 18, 26)
                                              : juce::Colour::fromRGB(14, 16, 22));
            g.fillRect(0.0f, top, (float)W, trackHeight);

            // Cross-track drag target highlight
            if (isDraggingClip && dragTargetTrackIndex == trackIndex && dragTargetTrackIndex != dragSourceTrackIndex)
            {
                g.setColour(juce::Colour::fromRGB(80, 160, 255).withAlpha(0.15f));
                g.fillRect(0.0f, top, (float)W, trackHeight);
                g.setColour(juce::Colour::fromRGB(80, 160, 255).withAlpha(0.6f));
                g.drawRect(0.0f, top, (float)W, trackHeight, 1.5f);
            }

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

                    const auto peaks  = waveformCache.getPeaks(clip.file);
                    const int  numCh  = waveformCache.getNumChannels(clip.file);
                    const int  stride = numCh * 2;
                    const int  blocks = (stride > 0) ? (int)peaks.size() / stride : 0;

                    if (blocks > 0)
                    {
                        // Normalize to the file's own peak so the waveform fills the clip
                        float filePeak = 0.001f;
                        for (float v : peaks) filePeak = juce::jmax(filePeak, std::abs(v));
                        const float invPeak = 1.0f / filePeak;

                        // Title text sits in top 18px; waveform occupies the rest
                        const float waveTop = clipY + 18.0f;

                        for (int ch = 0; ch < numCh; ++ch)
                        {
                            const float laneH      = (clipHeight - 18.0f) / (float)numCh;
                            const float laneTop2   = waveTop + ch * laneH;
                            const float laneBot2   = laneTop2 + laneH;
                            const float centerY    = (laneTop2 + laneBot2) * 0.5f;
                            const float amp        = laneH * 0.48f;

                            // Faint lane divider for stereo
                            if (numCh > 1 && ch > 0)
                            {
                                g.setColour(juce::Colours::white.withAlpha(0.12f));
                                g.drawHorizontalLine((int)laneTop2, (float)clipStartX, (float)clipStartX + clipWidth);
                            }

                            const float barW = juce::jmax(1.0f, clipWidth / (float)blocks);

                            for (int b = 0; b < blocks; ++b)
                            {
                                const int   idx  = b * stride + ch * 2;
                                const float minV = peaks[idx]     * invPeak;
                                const float maxV = peaks[idx + 1] * invPeak;
                                const float nx   = (float)clipStartX + b * barW;
                                const float y1   = centerY - (maxV * amp);
                                const float y2   = centerY - (minV * amp);
                                const float h    = juce::jmax(1.0f, y2 - y1);

                                // Filled solid bar — body colour with slight gradient tint by amplitude
                                const float energy = (maxV - minV) * 0.5f;
                                const float alpha  = 0.55f + energy * 0.35f;
                                g.setColour(juce::Colour::fromRGB(130, 200, 255).withAlpha(alpha));
                                g.fillRect(nx, y1, juce::jmax(1.0f, barW - 0.5f), h);

                                // Bright outline on top of bar
                                g.setColour(juce::Colour::fromRGB(200, 235, 255).withAlpha(0.6f));
                                g.drawLine(nx, y1, nx + juce::jmax(1.0f, barW - 0.5f), y1, 0.75f);
                            }
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

                // ── Clip gain line (always visible) ──────────────────────────
                {
                    const float waveH = clipHeight - 22.0f;
                    const float gainNorm = juce::jlimit(0.0f, 1.0f, (clip.gainDb + 24.0f) / 48.0f);
                    const float gainLineY = clipY + 20.0f + (1.0f - gainNorm) * juce::jmax(1.0f, waveH - 4.0f);
                    const bool isBeingDragged = (isDraggingClipGain && gainDragTrackIndex == trackIndex && gainDragClipIndex == clipIndex);

                    g.setColour(isBeingDragged ? juce::Colour::fromRGB(255, 210, 60).withAlpha(0.95f)
                                               : juce::Colour::fromRGB(180, 220, 255).withAlpha(0.55f));
                    g.drawLine((float)clipStartX + 2.0f, gainLineY, (float)clipStartX + clipWidth - 2.0f, gainLineY, 1.5f);

                    // Small grip circle at center of gain line
                    const float cx = (float)clipStartX + clipWidth * 0.5f;
                    g.fillEllipse(cx - 4.0f, gainLineY - 4.0f, 8.0f, 8.0f);

                    // dB label when gain is non-zero or being dragged
                    if (std::abs(clip.gainDb) > 0.05f || isBeingDragged)
                    {
                        juce::String gainStr = (clip.gainDb >= 0 ? "+" : "") + juce::String(clip.gainDb, 1) + " dB";
                        g.setColour(isBeingDragged ? juce::Colour::fromRGB(255, 210, 60) : juce::Colours::white.withAlpha(0.80f));
                        g.setFont(juce::FontOptions(9.0f));
                        g.drawText(gainStr, (int)clipStartX + 4, (int)gainLineY - 11, (int)clipWidth - 8, 11, juce::Justification::centredLeft);
                    }
                }
            }
        }

        // Automation lanes — drawn as translucent overlay strips along the
        // bottom of each track's lane (FL-style automation clips, simplified
        // to linear breakpoint curves overlaid on the arrangement).
        {
            automationStrips.clearQuick();
            const double sr = session.getSampleRate();
            const float laneStripH = 16.0f;

            for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
            {
                const auto& track = session.getTrack(trackIndex);
                if (track.automationLanes.isEmpty()) continue;

                const float top = lanesTop + trackIndex * trackHeight - (float)trackScrollY;
                if (top + trackHeight <= lanesTop || top >= (float)H) continue;

                int visibleLaneCount = 0;
                for (auto& lane : track.automationLanes)
                    if (lane.enabled) ++visibleLaneCount;
                if (visibleLaneCount == 0) continue;

                int laneSlot = 0;
                for (int laneIndex = 0; laneIndex < track.automationLanes.size(); ++laneIndex)
                {
                    const auto& lane = track.automationLanes.getReference(laneIndex);
                    if (!lane.enabled) continue;

                    const float stripBottom = top + trackHeight - laneSlot * laneStripH;
                    const float stripTop    = stripBottom - laneStripH;
                    ++laneSlot;
                    if (stripBottom <= lanesTop || stripTop >= (float)H) continue;

                    automationStrips.add({ trackIndex, laneIndex,
                        juce::Rectangle<float>(0.0f, stripTop, (float)W, laneStripH) });

                    const auto laneColour = juce::Colour::fromHSV(
                        (float)((laneIndex * 0.21) - std::floor(laneIndex * 0.21)), 0.55f, 0.95f, 1.0f);

                    g.setColour(juce::Colour::fromRGB(8, 9, 14).withAlpha(0.65f));
                    g.fillRect(0.0f, stripTop, (float)W, laneStripH);
                    g.setColour(laneColour.withAlpha(0.35f));
                    g.drawLine(0.0f, stripTop, (float)W, stripTop, 1.0f);

                    g.setColour(laneColour.withAlpha(0.7f));
                    g.setFont(juce::FontOptions(7.5f));
                    g.drawText(lane.parameterId, 2, (int)stripTop, 60, (int)laneStripH, juce::Justification::centredLeft);

                    if (lane.points.size() >= 1)
                    {
                        juce::Path curve;
                        for (int p = 0; p < lane.points.size(); ++p)
                        {
                            const auto& pt = lane.points.getReference(p);
                            const auto sampleAt = static_cast<int64_t>(pt.timeSeconds * sr);
                            const float x = (float)timelineModel.getXForSamplePosition(sampleAt, width);
                            const float y = automationValueToY(pt.value, lane.parameterId, stripTop, stripBottom);
                            if (p == 0) curve.startNewSubPath(x, y);
                            else        curve.lineTo(x, y);
                        }
                        g.setColour(laneColour.withAlpha(0.85f));
                        g.strokePath(curve, juce::PathStrokeType(1.5f));

                        for (int p = 0; p < lane.points.size(); ++p)
                        {
                            const auto& pt = lane.points.getReference(p);
                            const auto sampleAt = static_cast<int64_t>(pt.timeSeconds * sr);
                            const float x = (float)timelineModel.getXForSamplePosition(sampleAt, width);
                            const float y = automationValueToY(pt.value, lane.parameterId, stripTop, stripBottom);
                            if (x < -6.0f || x > (float)W + 6.0f) continue;
                            g.setColour(laneColour);
                            g.fillEllipse(x - 2.5f, y - 2.5f, 5.0f, 5.0f);
                        }
                    }
                }
            }
        }

        // Selection / loop range — always draw when a range exists
        if (transportState.hasLoopRange())
        {
            const double lsX = timelineModel.getXForSamplePosition(transportState.getLoopStartSample(), width);
            const double leX = timelineModel.getXForSamplePosition(transportState.getLoopEndSample(),   width);
            const float lsXf = (float)lsX, leXf = (float)leX;
            const float loopW = juce::jmax(2.0f, leXf - lsXf);

            const bool loopActive = transportState.isLooping();

            // Full-height lane overlay (brighter when loop is active)
            g.setColour(juce::Colour::fromRGBA(80, 120, 255, loopActive ? 60 : 30));
            g.fillRect(lsXf, (float)rulerH, loopW, (float)(H - rulerH));

            // Ruler brace fill
            g.setColour(juce::Colour::fromRGBA(80, 120, 255, 60));
            g.fillRect(lsXf, 2.0f, loopW, (float)rulerH - 2.0f);

            // Ruler brace border
            g.setColour(juce::Colour::fromRGB(80, 140, 255));
            g.drawRect(lsXf, 2.0f, loopW, (float)rulerH - 2.0f, 1.0f);

            // Left + right edge lines (handle bars)
            g.setColour(juce::Colour::fromRGB(130, 160, 255));
            g.fillRect(lsXf - 1.5f, 0.0f, 3.0f, (float)H);
            g.fillRect(leXf - 1.5f, 0.0f, 3.0f, (float)H);

            // Small triangle caps on ruler edges
            juce::Path leftCap, rightCap;
            leftCap.addTriangle (lsXf - 5.0f, 0.0f, lsXf + 5.0f, 0.0f, lsXf, 8.0f);
            rightCap.addTriangle(leXf - 5.0f, 0.0f, leXf + 5.0f, 0.0f, leXf, 8.0f);
            g.setColour(juce::Colour::fromRGB(140, 170, 255));
            g.fillPath(leftCap);
            g.fillPath(rightCap);

            // Duration label in ruler
            const double durationSamples = (double)(transportState.getLoopEndSample() - transportState.getLoopStartSample());
            const double durationSecs    = durationSamples / juce::jmax(1.0, transportState.getSampleRate());
            const int    mins  = (int)(durationSecs / 60.0);
            const double secs  = durationSecs - mins * 60.0;
            juce::String durStr = (mins > 0 ? juce::String(mins) + "m " : "") + juce::String(secs, 1) + "s";

            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::Font(juce::FontOptions(9.0f)));
            const float labelX = lsXf + 6.0f;
            const float labelW = juce::jmax(40.0f, loopW - 12.0f);
            g.drawText(durStr, (int)labelX, 6, (int)labelW, 14, juce::Justification::centredLeft, false);

            // Loop-active indicator
            if (loopActive)
            {
                g.setColour(juce::Colour::fromRGB(80, 110, 255).withAlpha(0.9f));
                g.setFont(juce::Font(juce::FontOptions(8.0f)));
                g.drawText(juce::CharPointer_UTF8("\xe2\x86\xba"), (int)(leXf - 18.0f), 6, 14, 14, juce::Justification::centred);
            }
        }

        // Punch range — drawn in ruler only, orange/red tint (Pro Tools style)
        if (transportState.isPunchEnabled() && transportState.getPunchOutSample() > transportState.getPunchInSample())
        {
            const float piX = (float)timelineModel.getXForSamplePosition(transportState.getPunchInSample(),  width);
            const float poX = (float)timelineModel.getXForSamplePosition(transportState.getPunchOutSample(), width);
            const float pw  = juce::jmax(2.0f, poX - piX);

            // Ruler fill — orange
            g.setColour(juce::Colour::fromRGBA(255, 100, 30, 70));
            g.fillRect(piX, 2.0f, pw, (float)rulerH - 2.0f);
            g.setColour(juce::Colour::fromRGB(255, 120, 40));
            g.drawRect(piX, 2.0f, pw, (float)rulerH - 2.0f, 1.0f);

            // Edge bars
            g.fillRect(piX - 1.5f, 2.0f, 3.0f, (float)rulerH - 2.0f);
            g.fillRect(poX - 1.5f, 2.0f, 3.0f, (float)rulerH - 2.0f);

            // "P" label
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.setFont(juce::Font(juce::FontOptions(9.0f)));
            g.drawText("P", (int)piX + 4, 8, 12, 12, juce::Justification::centredLeft);
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
        const int   trackIndex  = static_cast<int>((pos.y - rulerH + trackScrollY) / trackHeight);
        if (trackIndex < 0 || trackIndex >= session.getNumTracks()) return HoverZone::None;

        const auto& track = session.getTrack(trackIndex);
        for (int ci = 0; ci < track.clips.size(); ++ci)
        {
            const auto& clip    = track.clips.getReference(ci);
            const float cStartX = (float)timelineModel.getXForSamplePosition(clip.startSample, width);
            const float cEndX   = (float)timelineModel.getXForSamplePosition(clip.startSample + clip.lengthSamples, width);
            const float clipY   = (float)rulerH + trackIndex * trackHeight - (float)trackScrollY + 6.0f;
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
            const int width = getWidth();
            // Near playhead?
            const double phX = timelineModel.getXForSamplePosition(transportState.getPositionSamples(), width);
            if (std::abs(x - (float)phX) < 8.0f)
            { setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); return; }

            // Near existing loop handles?
            if (transportState.hasLoopRange())
            {
                const float lsX = (float)timelineModel.getXForSamplePosition(transportState.getLoopStartSample(), width);
                const float leX = (float)timelineModel.getXForSamplePosition(transportState.getLoopEndSample(),   width);
                if (std::abs(x - lsX) < 10.0f || std::abs(x - leX) < 10.0f)
                { setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); return; }
                if (x > lsX && x < leX)
                { setMouseCursor(juce::MouseCursor::DraggingHandCursor); return; }
            }

            // Default ruler cursor → crosshair signals "draw range here"
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
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

            // Right-click in ruler → marker context menu (add / rename / delete)
            if (event.mods.isPopupMenu())
            {
                const double sr = session.getSampleRate();
                int hitMarkerIndex = -1;
                for (int m = 0; m < session.getNumMarkers(); ++m)
                {
                    const auto markerSample = static_cast<int64_t>(session.getMarker(m).timeSeconds * sr);
                    const double markerX = timelineModel.getXForSamplePosition(markerSample, width);
                    if (std::abs(clickPoint.x - markerX) < 8.0)
                    {
                        hitMarkerIndex = m;
                        break;
                    }
                }

                const int64_t clickSample = juce::jmax<int64_t>(0, xToSample(clickPoint.x, timelineModel, snapNow, snapBeats, session));
                const double clickSeconds = sr > 0.0 ? (double)clickSample / sr : 0.0;

                juce::PopupMenu menu;
                if (hitMarkerIndex >= 0)
                {
                    menu.addItem(1, "Rename Marker...");
                    menu.addItem(2, "Delete Marker");
                }
                else
                {
                    menu.addItem(1, "Add Marker Here");
                }

                menu.showMenuAsync(juce::PopupMenu::Options(), [this, hitMarkerIndex, clickSeconds](int result)
                {
                    auto& sess = arrangementModel.getSession();
                    if (hitMarkerIndex < 0)
                    {
                        if (result == 1)
                        {
                            auto* aw = new juce::AlertWindow("Add Marker", "Marker name:", juce::MessageBoxIconType::NoIcon);
                            aw->addTextEditor("name", "Marker " + juce::String(sess.getNumMarkers() + 1));
                            aw->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
                            aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
                            aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw, clickSeconds](int r)
                            {
                                if (r == 1)
                                {
                                    NovaStudio::Marker marker;
                                    marker.name = aw->getTextEditorContents("name");
                                    marker.timeSeconds = clickSeconds;
                                    arrangementModel.getSession().addMarker(marker);
                                    arrangementModel.sendChangeMessage();
                                    repaint();
                                }
                                delete aw;
                            }), false);
                        }
                    }
                    else
                    {
                        if (result == 1)
                        {
                            auto& m2 = sess.getMarker(hitMarkerIndex);
                            auto* aw = new juce::AlertWindow("Rename Marker", "Marker name:", juce::MessageBoxIconType::NoIcon);
                            aw->addTextEditor("name", m2.name);
                            aw->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
                            aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
                            const int idx = hitMarkerIndex;
                            aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw, idx](int r)
                            {
                                if (r == 1)
                                {
                                    arrangementModel.getSession().renameMarker(idx, aw->getTextEditorContents("name"));
                                    arrangementModel.sendChangeMessage();
                                    repaint();
                                }
                                delete aw;
                            }), false);
                        }
                        else if (result == 2)
                        {
                            arrangementModel.getSession().removeMarker(hitMarkerIndex);
                            arrangementModel.sendChangeMessage();
                            repaint();
                        }
                    }
                });
                return;
            }

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

            // Double-click in ruler → return to zero
            if (event.getNumberOfClicks() == 2)
            {
                transportState.setPositionSamples(0, true);
                repaint();
                return;
            }

            // Cmd+drag in ruler → set punch in/out range (Pro Tools style)
            if (event.mods.isCommandDown())
            {
                isDraggingPunchRange = true;
                punchAnchorSample = juce::jmax<int64_t>(0, xToSample(clickPoint.x, timelineModel, snapNow, snapBeats, session));
                repaint();
                return;
            }

            // Click-drag in ruler → start range/selection drag
            // Set playhead immediately on mouseDown so single-click feels instant;
            // mouseUp will commit as loop range if drag distance exceeds threshold.
            isDraggingPlayhead    = true;
            isDraggingRangeSelect = true;
            rangeAnchorSample = juce::jmax<int64_t>(0, xToSample(clickPoint.x, timelineModel, snapNow, snapBeats, session));
            transportState.setPositionSamples(rangeAnchorSample, true);
            repaint();
            return;
        }

        // ── Automation lane strips: add / move / delete breakpoints ─────────
        for (auto& strip : automationStrips)
        {
            if (!strip.bounds.contains(clickPoint)) continue;

            const auto& sess = arrangementModel.getSession();
            if (!isPositiveAndBelow(strip.trackIndex, sess.getNumTracks())) break;
            const auto& lane = sess.getTrack(strip.trackIndex).automationLanes.getReference(strip.laneIndex);
            const double sr = sess.getSampleRate();

            // Hit-test existing points (within 6px)
            int hitPoint = -1;
            for (int p = 0; p < lane.points.size(); ++p)
            {
                const auto& pt = lane.points.getReference(p);
                const auto sampleAt = static_cast<int64_t>(pt.timeSeconds * sr);
                const float x = (float)timelineModel.getXForSamplePosition(sampleAt, width);
                const float y = automationValueToY(pt.value, lane.parameterId, strip.bounds.getY(), strip.bounds.getBottom());
                if (std::abs(clickPoint.x - x) < 6.0f && std::abs(clickPoint.y - y) < 6.0f)
                { hitPoint = p; break; }
            }

            if (event.mods.isPopupMenu())
            {
                juce::PopupMenu menu;
                if (hitPoint >= 0) menu.addItem(1, "Delete Point");
                menu.addItem(2, "Remove Automation Lane");
                const int trackIdx = strip.trackIndex, laneIdx = strip.laneIndex, pointIdx = hitPoint;
                menu.showMenuAsync(juce::PopupMenu::Options(), [this, trackIdx, laneIdx, pointIdx](int result)
                {
                    auto& tr = arrangementModel.getSession().getTrack(trackIdx);
                    if (result == 1 && pointIdx >= 0)
                        tr.automationLanes.getReference(laneIdx).points.remove(pointIdx);
                    else if (result == 2)
                        tr.automationLanes.remove(laneIdx);
                    else
                        return;
                    arrangementModel.sendChangeMessage();
                    repaint();
                });
                return;
            }

            if (hitPoint >= 0)
            {
                isDraggingAutomationPoint = true;
                dragAutomationTrack = strip.trackIndex;
                dragAutomationLane  = strip.laneIndex;
                dragAutomationPoint = hitPoint;
                return;
            }

            // Empty area click → add a new breakpoint here
            {
                const bool snapNow = (snapEnabled && editMode == EditModeToolbar::EditMode::Grid);
                const int64_t clickSample = juce::jmax<int64_t>(0, xToSample(clickPoint.x, timelineModel, snapNow, snapBeats, sess));
                const double timeSeconds = sr > 0.0 ? (double)clickSample / sr : 0.0;
                const float value = automationYToValue(clickPoint.y, lane.parameterId, strip.bounds.getY(), strip.bounds.getBottom());

                auto& tr = arrangementModel.getSession().getTrack(strip.trackIndex);
                insertAutomationPointSorted(tr.automationLanes.getReference(strip.laneIndex).points, timeSeconds, value);
                arrangementModel.sendChangeMessage();
                repaint();
            }
            return;
        }

        // ── Track lanes ──────────────────────────────────────────────────────
        const float trackHeight = (float)trackHeightPx;
        const float lanesTop = (float)rulerH;

        const int trackIndex = static_cast<int>((clickPoint.y - lanesTop + trackScrollY) / trackHeight);
        if (trackIndex < 0 || trackIndex >= arrangementModel.getSession().getNumTracks())
        {
            arrangementModel.clearSelection();
            return;
        }

        const auto& track = arrangementModel.getSession().getTrack(trackIndex);

        // Right-click on empty track-lane area (no clip under cursor) → add automation lane
        if (event.mods.isPopupMenu())
        {
            bool overClip = false;
            for (int ci = 0; ci < track.clips.size(); ++ci)
            {
                const auto& c = track.clips.getReference(ci);
                const double sx = timelineModel.getXForSamplePosition(c.startSample, width);
                const double ex = timelineModel.getXForSamplePosition(c.startSample + c.lengthSamples, width);
                const float cy = lanesTop + trackIndex * trackHeight - (float)trackScrollY + 6.0f;
                if (juce::Rectangle<float>((float)sx, cy, (float)juce::jmax(8.0, ex - sx), trackHeight - 12.0f).contains(clickPoint))
                { overClip = true; break; }
            }

            if (!overClip)
            {
                static const std::pair<const char*, const char*> kParams[] = {
                    { "volume", "Volume" }, { "pan", "Pan" },
                    { "send1", "Send 1" }, { "send2", "Send 2" }, { "send3", "Send 3" },
                    { "send4", "Send 4" }, { "send5", "Send 5" }, { "send6", "Send 6" },
                };
                juce::PopupMenu addMenu;
                for (int p = 0; p < (int)juce::numElementsInArray(kParams); ++p)
                    addMenu.addItem(p + 1, kParams[p].second);

                juce::PopupMenu menu;
                menu.addSubMenu("Add Automation Lane", addMenu);

                const int trackIdx = trackIndex;
                menu.showMenuAsync(juce::PopupMenu::Options(), [this, trackIdx](int result)
                {
                    if (result < 1 || result > (int)juce::numElementsInArray(kParams)) return;
                    NovaStudio::AutomationLane lane;
                    lane.parameterId = kParams[result - 1].first;
                    lane.enabled = true;
                    lane.points.add({ 0.0, 0.0f });
                    arrangementModel.getSession().getTrack(trackIdx).automationLanes.add(lane);
                    arrangementModel.sendChangeMessage();
                    repaint();
                });
                return;
            }
        }

        bool found = false;
        for (int clipIndex = 0; clipIndex < track.clips.size(); ++clipIndex)
        {
            const auto& clip = track.clips.getReference(clipIndex);
            const double clipStartX = timelineModel.getXForSamplePosition(clip.startSample, width);
            const double clipEndX = timelineModel.getXForSamplePosition(clip.startSample + clip.lengthSamples, width);
            const float clipY = lanesTop + trackIndex * trackHeight - (float)trackScrollY + 6.0f;
            const float clipH = trackHeight - 12.0f;
            const auto clipRect = juce::Rectangle<float>((float)clipStartX, clipY, (float)juce::jmax(8.0, clipEndX - clipStartX), clipH);
            const float handleW = 10.0f;
            const auto leftHandle = juce::Rectangle<float>((float)clipStartX - 1.0f, clipRect.getBottom() - 12.0f, handleW, 10.0f);
            const auto rightHandle = juce::Rectangle<float>((float)clipEndX - handleW + 1.0f, clipRect.getBottom() - 12.0f, handleW, 10.0f);
            const bool clipSelectedSingle = (trackIndex == arrangementModel.getSelectedTrackIndex() && clipIndex == arrangementModel.getSelectedClipIndex());

            // ── Clip gain drag (click within ±5px of gain line) ──────────────
            if (clipRect.contains(clickPoint) && !clip.locked
                && cursorTool == EditModeToolbar::CursorTool::Pointer)
            {
                const float waveH = clipH - 22.0f;
                const float gainNorm = juce::jlimit(0.0f, 1.0f, (clip.gainDb + 24.0f) / 48.0f);
                const float gainLineY = clipY + 20.0f + (1.0f - gainNorm) * juce::jmax(1.0f, waveH - 4.0f);
                if (std::abs(clickPoint.y - gainLineY) <= 6.0f)
                {
                    arrangementModel.selectClip(trackIndex, clipIndex);
                    isDraggingClipGain = true;
                    gainDragTrackIndex = trackIndex;
                    gainDragClipIndex  = clipIndex;
                    gainDragOrigDb     = clip.gainDb;
                    gainDragStartY     = clickPoint.y;
                    found = true;
                    break;
                }
            }

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
                    dragSourceTrackIndex = trackIndex;
                    dragTargetTrackIndex = trackIndex;
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

        // ── Automation point drag ─────────────────────────────────────────────
        if (isDraggingAutomationPoint)
        {
            for (auto& strip : automationStrips)
            {
                if (strip.trackIndex != dragAutomationTrack || strip.laneIndex != dragAutomationLane)
                    continue;
                if (!isPositiveAndBelow(dragAutomationTrack, session.getNumTracks())) break;

                auto& tr = arrangementModel.getSession().getTrack(dragAutomationTrack);
                auto& lane = tr.automationLanes.getReference(dragAutomationLane);
                if (!isPositiveAndBelow(dragAutomationPoint, lane.points.size())) break;

                const double sr = session.getSampleRate();
                const int64_t sample = juce::jmax<int64_t>(0, xToSample(event.position.x, timelineModel, snapNow, snapBeats, session));
                const double timeSeconds = sr > 0.0 ? (double)sample / sr : 0.0;
                const float value = automationYToValue(event.position.y, lane.parameterId, strip.bounds.getY(), strip.bounds.getBottom());

                lane.points.getReference(dragAutomationPoint).timeSeconds = timeSeconds;
                lane.points.getReference(dragAutomationPoint).value = value;
                repaint();
                break;
            }
            return;
        }

        // ── Playhead scrubbing ────────────────────────────────────────────────
        if (isDraggingPlayhead && !isDraggingRangeSelect)
        {
            const int64_t pos = xToSample(event.position.x, timelineModel, snapNow, snapBeats, session);
            transportState.setPositionSamples(juce::jmax<int64_t>(0, pos), true);
            repaint();
            return;
        }

        // ── Punch range drag (Cmd+drag in ruler) ─────────────────────────────
        if (isDraggingPunchRange)
        {
            const int64_t curSample = juce::jmax<int64_t>(0, xToSample(event.position.x, timelineModel, snapNow, snapBeats, session));
            const int64_t pIn  = juce::jmin(punchAnchorSample, curSample);
            const int64_t pOut = juce::jmax(punchAnchorSample, curSample);
            if (pOut > pIn && onPunchRangeChanged)
                onPunchRangeChanged(pIn, pOut);
            repaint();
            return;
        }

        // ── Timeline range / selection drag ──────────────────────────────────
        if (isDraggingRangeSelect)
        {
            // Commit to range mode as soon as we've moved more than a few pixels
            const float dragDistPx = std::abs(event.position.x - (float)getWidth() * (rangeAnchorSample / juce::jmax(1.0, timelineModel.getVisibleRangeSeconds(getWidth()) * transportState.getSampleRate())));
            const int64_t curSample = juce::jmax<int64_t>(0, xToSample(event.position.x, timelineModel, snapNow, snapBeats, session));
            const int64_t newStart  = juce::jmin(rangeAnchorSample, curSample);
            const int64_t newEnd    = juce::jmax(rangeAnchorSample, curSample);
            juce::ignoreUnused(dragDistPx);
            if (newEnd > newStart)
                transportState.setLoopRange(newStart, newEnd);
            // Scrub playhead to follow mouse during ruler drag
            transportState.setPositionSamples(curSample, true);
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
                    const auto clipRect = juce::Rectangle<float>((float)clipStartX, 28.0f + t * trackHeight - (float)trackScrollY + 6.0f,
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

        // ── Clip gain drag ───────────────────────────────────────────────────────
        if (isDraggingClipGain)
        {
            auto* clip = arrangementModel.getSelectedClip();
            if (clip != nullptr && !clip->locked)
            {
                const float trackH = (float)trackHeightPx;
                const float waveH = juce::jmax(1.0f, (trackH - 12.0f) - 22.0f - 4.0f);
                const float dbPerPixel = 48.0f / waveH;
                const float newGain = juce::jlimit(-24.0f, 24.0f,
                    gainDragOrigDb - (event.position.y - gainDragStartY) * dbPerPixel);
                arrangementModel.setSelectedClipGain(newGain);
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

            // Trim is slip-mode by default (pixel-accurate).
            // Only snap if Cmd/Ctrl is held.
            const bool snapTrim = event.mods.isCommandDown() && snapEnabled;
            const auto localX = static_cast<int>(event.x);
            const int64_t rawSample = static_cast<int64_t>(timelineModel.getSamplePositionForX(localX));
            const int64_t sampleAtX = snapTrim
                ? arrangementModel.getSnappedSamplePosition(rawSample)
                : juce::jmax<int64_t>(0, rawSample);

            if (isDraggingTrimLeft)
            {
                // Allow extending left (revealing audio before original start) as long as
                // fileOffsetSamples stays >= 0 and timeline position stays >= 0.
                const int64_t maxExtend = juce::jmin(clip->fileOffsetSamples, originalClipStartSample);
                const int64_t minStart  = originalClipStartSample - maxExtend;
                const int64_t maxStart  = originalClipStartSample + originalClipLength - 1;
                const int64_t newStart  = juce::jlimit<int64_t>(minStart, maxStart, sampleAtX);
                if (newStart != currentTrimSample)
                {
                    currentTrimSample = newStart;
                    const int64_t delta = newStart - originalClipStartSample;
                    NovaStudio::Clip previewClip = *clip;
                    previewClip.startSample       = newStart;
                    previewClip.fileOffsetSamples  = clip->fileOffsetSamples + delta;
                    previewClip.lengthSamples      = juce::jmax<int64_t>(1, originalClipLength - delta);
                    arrangementModel.replaceClipWithoutUndo(arrangementModel.getSelectedTrackIndex(), arrangementModel.getSelectedClipIndex(), previewClip);
                }
            }
            else if (isDraggingTrimRight)
            {
                // Right trim: free movement, no snap unless Cmd held
                const int64_t newEnd = juce::jmax<int64_t>(originalClipStartSample + 1, sampleAtX);
                if (newEnd != currentTrimSample)
                {
                    currentTrimSample = newEnd;
                    NovaStudio::Clip previewClip = *clip;
                    previewClip.lengthSamples = newEnd - originalClipStartSample;
                    arrangementModel.replaceClipWithoutUndo(arrangementModel.getSelectedTrackIndex(), arrangementModel.getSelectedClipIndex(), previewClip);
                }
            }
            return;
        }

        if (!isDraggingClip || arrangementModel.getSelectedClips().isEmpty())
            return;

        // Track which lane the mouse is over for cross-track drag
        {
            const int rulerH = 28;
            const int newTarget = static_cast<int>((event.y - rulerH + trackScrollY) / (float)trackHeightPx);
            const int numTracks = arrangementModel.getSession().getNumTracks();
            dragTargetTrackIndex = juce::jlimit(0, numTracks - 1, newTarget);
            repaint(); // keep target-lane highlight live
        }

        const int width = getWidth() - 16;
        const double sampleRate = transportState.getSampleRate();
        if (sampleRate <= 0.0)
            return;

        const double dx = static_cast<double>(event.x) - static_cast<double>(dragStartX);
        const double deltaSamplesF = dx / timelineModel.getPixelsPerSecond() * sampleRate;
        const int64_t deltaSamples = static_cast<int64_t>(deltaSamplesF);

        // Only update the baseline when we actually moved at least one sample.
        // This lets sub-pixel movements accumulate until they cross a sample boundary,
        // which is important at high sample rates (e.g. 96 kHz) where 1 sample < 1 pixel.
        if (deltaSamples == 0)
            return;

        const auto targets = arrangementModel.getSelectedClips();

        // Grid mode: snap clip start to grid after move
        if (editMode == EditModeToolbar::EditMode::Grid && snapEnabled)
        {
            arrangementModel.moveClipsBySamples(targets, deltaSamples);
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
            // Slip mode: free placement, no snap
            arrangementModel.moveClipsBySamples(targets, deltaSamples);
        }
        dragStartX = event.x;
        juce::ignoreUnused(width);
    }

    void ArrangementView::mouseUp(const juce::MouseEvent& event)
    {
        // ── Finalise automation point drag — re-sort by time and persist ─────
        if (isDraggingAutomationPoint)
        {
            isDraggingAutomationPoint = false;
            auto& session = arrangementModel.getSession();
            if (isPositiveAndBelow(dragAutomationTrack, session.getNumTracks()))
            {
                auto& tr = session.getTrack(dragAutomationTrack);
                if (isPositiveAndBelow(dragAutomationLane, tr.automationLanes.size()))
                {
                    auto& points = tr.automationLanes.getReference(dragAutomationLane).points;
                    if (isPositiveAndBelow(dragAutomationPoint, points.size()))
                    {
                        const auto moved = points.getReference(dragAutomationPoint);
                        points.remove(dragAutomationPoint);
                        insertAutomationPointSorted(points, moved.timeSeconds, moved.value);
                    }
                }
            }
            dragAutomationTrack = dragAutomationLane = dragAutomationPoint = -1;
            arrangementModel.sendChangeMessage();
            repaint();
            return;
        }

        // ── Finalise range-select drag ────────────────────────────────────────
        if (isDraggingRangeSelect)
        {
            isDraggingRangeSelect = false;
            const bool snapNow = (snapEnabled && editMode == EditModeToolbar::EditMode::Grid);
            const auto& session = arrangementModel.getSession();
            const int64_t curSample = juce::jmax<int64_t>(0, xToSample(event.position.x, timelineModel, snapNow, snapBeats, session));
            const int64_t newStart  = juce::jmin(rangeAnchorSample, curSample);
            const int64_t newEnd    = juce::jmax(rangeAnchorSample, curSample);

            const double minDurationSamples = transportState.getSampleRate() * 0.05; // 50 ms threshold
            if ((double)(newEnd - newStart) < minDurationSamples)
            {
                // Plain click in ruler: move playhead AND clear loop/selection range
                transportState.setPositionSamples(juce::jmax<int64_t>(0, rangeAnchorSample), true);
                transportState.clearLoopRange();
            }
            else
            {
                transportState.setLoopRange(newStart, newEnd);
                transportState.setPositionSamples(newStart, true);
            }
            repaint();
        }

        if (isDraggingPunchRange)
        {
            isDraggingPunchRange = false;
            repaint();
        }

        isDraggingPlayhead = false;
        isDraggingFadeIn   = false;
        isDraggingFadeOut  = false;

        if (isDraggingClipGain)
        {
            isDraggingClipGain = false;
            auto* clip = arrangementModel.getSelectedClip();
            if (clip != nullptr && !clip->locked)
                arrangementModel.replaceSelectedClipWithUndo(*clip, "Adjust Clip Gain");
            gainDragTrackIndex = -1;
            gainDragClipIndex  = -1;
        }
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
                // finalClip already reflects the live preview state (replaceClipWithoutUndo during drag)
                // Just commit with undo record; the clip data is already correct.
                arrangementModel.replaceSelectedClipWithUndo(finalClip, "Trim Clip Start");
            }
            else if (isDraggingTrimRight)
            {
                arrangementModel.replaceSelectedClipWithUndo(finalClip, "Trim Clip End");
            }
        }
        isDraggingTrimLeft = false;
        isDraggingTrimRight = false;
    }

    if (isDraggingClip)
    {
        isDraggingClip = false;
        // Cross-track: move clip to the target track lane
        if (dragTargetTrackIndex >= 0 && dragTargetTrackIndex != dragSourceTrackIndex)
        {
            const auto& selClips = arrangementModel.getSelectedClips();
            if (selClips.size() == 1)
            {
                const auto pt = selClips.getFirst();
                if (pt.x == dragSourceTrackIndex)
                {
                    const auto* clip = arrangementModel.getSelectedClip();
                    if (clip != nullptr)
                        arrangementModel.moveClipToTrack(pt.x, pt.y, dragTargetTrackIndex, clip->startSample);
                }
            }
        }
        dragSourceTrackIndex = -1;
        dragTargetTrackIndex = -1;
        repaint();
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

    // Read file metadata first — need channel count and length
    juce::AudioFormatManager fmt;
    fmt.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(file));

    const bool isStereo     = reader && reader->numChannels >= 2;
    const int64_t lengthSmp = reader ? reader->lengthInSamples : (int64_t)(44100 * 5);

    auto& session = arrangementModel.getSession();
    int trackIndex = (int)((dropY - rulerH + trackScrollY) / trackHeight);

    // If dropped below all tracks (or on empty space), create a new track
    if (trackIndex < 0 || trackIndex >= session.getNumTracks())
    {
        const juce::String trackName = (isStereo ? "Stereo - " : "Mono - ")
                                       + file.getFileNameWithoutExtension();
        if (onCreateAudioTrack)
            trackIndex = onCreateAudioTrack(trackName, isStereo);
        else
            trackIndex = session.getNumTracks() - 1;  // fallback: last track

        if (trackIndex < 0 || trackIndex >= session.getNumTracks()) return;
    }

    const int64_t samplePos = juce::jmax<int64_t>(0,
        (int64_t)timelineModel.getSamplePositionForX(dropX));

    NovaStudio::Clip clip;
    clip.file           = file;
    clip.startSample    = samplePos;
    clip.lengthSamples  = lengthSmp;
    clip.isMidi         = false;

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

        // Fader + meter layout: fader left, meter (8px) right
        const int faderTop  = 60;
        const int faderBot  = H - 36;
        const int meterW2   = 8;
        const int meterGap  = 3;
        const int meterX    = sx + (stripW - 4) - meterW2 - 2;
        const int faderRight = meterX - meterGap;
        const int faderLeft  = sx + 4;
        const int faderCX    = (faderLeft + faderRight) / 2;

        // Fader track
        g.setColour(juce::Colour::fromRGB(28, 30, 42));
        g.fillRect(faderCX - 2, faderTop, 4, faderBot - faderTop);

        // Fader handle (interactive via faderPositions)
        const int faderHandleY = faderTop + (int)((faderBot - faderTop) * 0.25f);
        g.setColour(juce::Colour::fromRGB(80, 84, 110));
        g.fillRoundedRectangle((float)faderLeft, (float)(faderHandleY - 6),
                               (float)(faderRight - faderLeft), 12.0f, 3.0f);
        g.setColour(juce::Colour::fromRGB(120, 124, 160));
        g.fillRect(faderLeft, faderHandleY - 1, faderRight - faderLeft, 2);

        // Level meter (static placeholder — green bar at moderate level)
        {
            // Background
            g.setColour(juce::Colour::fromRGB(14, 16, 24));
            g.fillRoundedRectangle((float)meterX, (float)faderTop,
                                   (float)meterW2, (float)(faderBot - faderTop), 2.0f);

            // Simulated level: ~60% green, 15% yellow at top
            const int totalH2 = faderBot - faderTop;
            const int greenH  = (int)(totalH2 * 0.58f);
            const int yellowH = (int)(totalH2 * 0.12f);
            const int fillTop = faderBot - greenH - yellowH;

            // Green section
            const int segH = 3, segGap = 1;
            for (int fy = faderBot - segH; fy >= fillTop; fy -= (segH + segGap))
            {
                const float t = 1.0f - (float)(fy - faderTop) / (float)totalH2;
                juce::Colour segCol;
                if (t < 0.70f)      segCol = juce::Colour::fromRGB(50, 200,  80);
                else if (t < 0.88f) segCol = juce::Colour::fromRGB(220, 200,  30);
                else                segCol = juce::Colour::fromRGB(220,  50,  50);
                g.setColour(segCol);
                g.fillRect(meterX + 1, fy, meterW2 - 2, segH);
            }
        }

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

    // Init fader/pan arrays to unity defaults
    for (int i = 0; i < kMaxStrips; ++i)
    {
        faderPositions[i] = 0.35f;
        panPositions[i]   = 0.5f;
    }
}

BottomDockPanel::~BottomDockPanel()
{
    stopTimer();
}

void BottomDockPanel::setEngine(NovaStudio::StudioAudioEngine& e)
{
    enginePtr = &e;
    startTimerHz(30);
}

void BottomDockPanel::timerCallback()
{
    if (!enginePtr) return;
    const int n = juce::jmin(enginePtr->getSession().getNumTracks() + 1, kMaxStrips); // +1 for master
    bool changed = false;
    for (int i = 0; i < n - 1; ++i)
    {
        const float newL = enginePtr->getTrackPeakLevel(i, 0);
        const float newR = enginePtr->getTrackPeakLevel(i, 1);
        peakLevelL[i] = juce::jmax(peakLevelL[i] * 0.92f, newL);
        peakLevelR[i] = juce::jmax(peakLevelR[i] * 0.92f, newR);
        if (newL > 0.001f || newR > 0.001f || peakLevelL[i] > 0.001f || peakLevelR[i] > 0.001f)
            changed = true;
    }
    // Master: peak across all tracks
    {
        const int mi = n - 1;
        float mL = 0.0f, mR = 0.0f;
        for (int i = 0; i < n - 1; ++i) { mL = juce::jmax(mL, peakLevelL[i]); mR = juce::jmax(mR, peakLevelR[i]); }
        peakLevelL[mi] = mL;
        peakLevelR[mi] = mR;
        if (mL > 0.001f || mR > 0.001f) changed = true;
    }
    if (changed) repaint();
}

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
    if (stripIdx < 0 || stripIdx >= kMaxStrips) return false;
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

    // ── Pan knob hit-test ─────────────────────────────────────────────────
    if (si >= 0 && si < kMaxStrips)
    {
        const int sw    = kStripW - 2;
        const float knobCX = (float)(si * kStripW) + sw * 0.5f;
        const float knobCY = (float)(28 + 28);   // matches paintMixerStrips: top + 28
        const float kr = 9.0f;
        const float dx = e.position.x - knobCX;
        const float dy = e.position.y - knobCY;
        if (dx * dx + dy * dy <= (kr + 2.0f) * (kr + 2.0f))
        {
            activePanStrip   = si;
            panDragStartX    = e.x;
            panDragStartPos  = panPositions[si];
            return;
        }
    }

    // ── Fader hit-test ────────────────────────────────────────────────────
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
        const int row = (e.y - 28 - 22) / juce::jmax(1, rowH); // 22px = step seq header height
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
    // ── Pan drag ─────────────────────────────────────────────────────────
    if (activePanStrip >= 0)
    {
        // Horizontal drag: right → more R, left → more L
        // 80px of drag = full sweep (L to R)
        const float delta = (float)(e.x - panDragStartX) / 80.0f;
        panPositions[activePanStrip] = juce::jlimit(0.0f, 1.0f, panDragStartPos + delta);
        repaint();
        return;
    }

    // ── Fader drag ───────────────────────────────────────────────────────
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
    activePanStrip   = -1;
}

void BottomDockPanel::paintMixerStrips(juce::Graphics& g, juce::Rectangle<int> area)
{
    const int H      = area.getHeight();
    const int top    = area.getY();

    // Build strip list: real session tracks + master, or fallback to palette names
    int numStrips = 0;
    bool hasEngine = (enginePtr != nullptr);
    int sessionTracks = hasEngine ? enginePtr->getSession().getNumTracks() : 0;
    int totalStrips = hasEngine ? sessionTracks + 1 : (int)std::size(kDockNames); // +1 for master
    numStrips = juce::jmin(totalStrips, area.getWidth() / kStripW, kMaxStrips);

    // dB scale labels reference (0 dB = 35% from top of fader travel)
    static const char* const dbMarks[] = { "+6", "0", "-6", "-12", "-24", "-inf" };

    for (int i = 0; i < numStrips; ++i)
    {
        const int sx    = area.getX() + i * kStripW;
        const int sw    = kStripW - 2;

        // Track colour from palette
        const juce::Colour col = kDockPalette[i % (int)std::size(kDockPalette)];

        // Track name: from session if available, otherwise hardcoded
        juce::String trackName;
        bool isMaster = (hasEngine && i == sessionTracks);
        if (hasEngine && !isMaster && i < sessionTracks)
            trackName = enginePtr->getSession().getTrack(i).name;
        else if (isMaster)
            trackName = "MASTER";
        else
            trackName = (i < (int)std::size(kDockNames)) ? kDockNames[i] : ("CH" + juce::String(i + 1));

        // ── Strip body ────────────────────────────────────────────────────
        g.setColour(juce::Colour::fromRGB(11, 12, 18));
        g.fillRect(sx, top, sw, H);

        // Track colour band at top
        g.setColour(isMaster ? juce::Colour::fromRGB(160, 140, 60) : col);
        g.fillRect(sx, top, sw, 4);

        // ── Track name ────────────────────────────────────────────────────
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions(8.5f).withStyle("Bold")));
        g.drawText(trackName, sx + 2, top + 6, sw - 4, 12, juce::Justification::centred);

        // ── Pan knob ──────────────────────────────────────────────────────
        const float knobCX = (float)sx + sw * 0.5f;
        const float knobCY = (float)top + 28.0f;
        const float kr = 9.0f;
        const float panVal = panPositions[i]; // 0.0 = L, 0.5 = C, 1.0 = R

        // Outer dark groove
        g.setColour(juce::Colour::fromRGB(8, 9, 14));
        g.fillEllipse(knobCX - kr, knobCY - kr, kr * 2.0f, kr * 2.0f);
        // Knob body
        g.setColour(juce::Colour::fromRGB(38, 42, 60));
        g.fillEllipse(knobCX - kr + 1.5f, knobCY - kr + 1.5f, (kr - 1.5f) * 2.0f, (kr - 1.5f) * 2.0f);
        // Colour rim — brighter when panned off-centre
        const float offCentre = std::abs(panVal - 0.5f) * 2.0f;
        g.setColour(col.withAlpha(0.35f + offCentre * 0.5f));
        g.drawEllipse(knobCX - kr, knobCY - kr, kr * 2.0f, kr * 2.0f, 1.5f);
        // Pan arc: from centre (top) sweeping left or right
        {
            const float startAngle = -juce::MathConstants<float>::pi * 0.75f;  // ~7 o'clock
            const float endAngle   =  juce::MathConstants<float>::pi * 0.75f;  // ~5 o'clock
            const float centreAngle = 0.0f; // 12 o'clock = centre
            const float panAngle    = startAngle + panVal * (endAngle - startAngle);
            juce::Path arc;
            arc.addArc(knobCX - kr + 1.0f, knobCY - kr + 1.0f, (kr - 1.0f) * 2.0f, (kr - 1.0f) * 2.0f,
                       juce::jmin(centreAngle, panAngle),
                       juce::jmax(centreAngle, panAngle), true);
            g.setColour(col.withAlpha(panVal != 0.5f ? 0.8f : 0.3f));
            g.strokePath(arc, juce::PathStrokeType(2.0f));
        }
        // Pointer line rotated to pan angle
        {
            const float startAngle = -juce::MathConstants<float>::pi * 0.75f;
            const float endAngle   =  juce::MathConstants<float>::pi * 0.75f;
            const float panAngle   = startAngle + panVal * (endAngle - startAngle);
            const float px = knobCX + (kr - 3.0f) * std::sin(panAngle);
            const float py = knobCY - (kr - 3.0f) * std::cos(panAngle);
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawLine(knobCX, knobCY, px, py, 1.8f);
        }
        // "PAN" label + L/R value
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        juce::String panStr;
        if (std::abs(panVal - 0.5f) < 0.01f)      panStr = "C";
        else if (panVal < 0.5f) panStr = "L" + juce::String((int)((0.5f - panVal) * 200.0f));
        else                    panStr = "R" + juce::String((int)((panVal - 0.5f) * 200.0f));
        g.drawText(panStr, sx + 2, (int)(knobCY + kr + 1.0f), sw - 4, 9, juce::Justification::centred);

        // ── Fader + level meter layout ────────────────────────────────────
        const int meterColW  = 9;   // width of vertical level meter column
        const int meterColX  = sx + sw - meterColW - 1;  // meter on right edge
        const int faderTop   = top + 44;
        const int faderBot   = top + H - 34;
        const int fTravel    = faderBot - faderTop;
        // Fader groove centred in the area left of the meter
        const int fCX        = sx + (meterColX - sx) / 2;

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

        // ── Level meter (right column, L+R averaged) ─────────────────────
        {
            g.setColour(juce::Colour::fromRGB(8, 10, 16));
            g.fillRoundedRectangle((float)meterColX, (float)faderTop,
                                   (float)meterColW, (float)fTravel, 2.0f);
            const float level = (i < kMaxStrips) ? (peakLevelL[i] + peakLevelR[i]) * 0.5f : 0.0f;
            const float levelNorm = juce::jlimit(0.0f, 1.0f, level);
            if (levelNorm > 0.001f)
            {
                const int fillH  = (int)(fTravel * levelNorm);
                const int segH = 3, segGap = 1;
                for (int fy = faderBot - segH; fy >= faderBot - fillH; fy -= (segH + segGap))
                {
                    const float t = 1.0f - (float)(fy - faderTop) / (float)fTravel;
                    juce::Colour segCol;
                    if (t < 0.70f)      segCol = juce::Colour::fromRGB(50, 200,  80);
                    else if (t < 0.88f) segCol = juce::Colour::fromRGB(220, 200,  30);
                    else                segCol = juce::Colour::fromRGB(220,  50,  50);
                    g.setColour(segCol);
                    g.fillRect(meterColX + 1, fy, meterColW - 2, segH);
                }
            }
        }

        // ── Fader thumb (analog-style, wide flat knob) ────────────────────
        const float fPos  = faderPositions[i];
        const int handleY = faderTop + (int)(fPos * fTravel);
        const int thumbH  = 18;
        const int thumbW  = meterColX - sx - 10;   // narrowed to leave meter space
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

    // Left mixer section: real session channels from engine
    paintMixerStrips(g, juce::Rectangle<int>(0, 28, splitX, getHeight() - 28));
    paintPianoRoll(g,     juce::Rectangle<int>(splitX + 1, 28, pianoW - 1, getHeight() - 28));
    paintStepSequencer(g, juce::Rectangle<int>(stepX + 1, 28, getWidth() - stepX - 1, getHeight() - 28));
}

// ─── BrowserPanel ─────────────────────────────────────────────────────────────

BrowserPanel::BrowserPanel()
{
    dirThread.startThread();
    navigateTo(juce::File::getSpecialLocation(juce::File::userHomeDirectory));

    // Style the file tree for dark theme
    fileTree.setColour(juce::FileTreeComponent::backgroundColourId,
                       juce::Colour::fromRGB(12, 14, 20));
    fileTree.setColour(juce::FileTreeComponent::selectedItemBackgroundColourId,
                       juce::Colour::fromRGB(50, 40, 90));
    fileTree.setColour(juce::ListBox::backgroundColourId,
                       juce::Colour::fromRGB(12, 14, 20));
    fileTree.setColour(juce::TreeView::backgroundColourId,
                       juce::Colour::fromRGB(12, 14, 20));
    fileTree.addListener(this);
    addAndMakeVisible(fileTree);

    // Search box
    searchBox.setTextToShowWhenEmpty("Search files...", juce::Colours::grey);
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(22, 26, 36));
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white.withAlpha(0.85f));
    searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    searchBox.setFont(juce::Font(juce::FontOptions(10.0f)));
    addAndMakeVisible(searchBox);

    // Bookmark buttons
    for (auto* btn : { &homeBtn, &desktopBtn, &docsBtn, &musicBtn, &recBtn })
    {
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(20, 22, 32));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.6f));
        btn->setColour(juce::TextButton::textColourOnId, juce::Colour::fromRGB(180, 155, 255));
        btn->addListener(this);
        addAndMakeVisible(btn);
    }
}

BrowserPanel::~BrowserPanel()
{
    fileTree.removeListener(this);
    dirThread.stopThread(2000);
}

void BrowserPanel::navigateTo(const juce::File& dir)
{
    dirContents.setDirectory(dir, true, true);
}

void BrowserPanel::refresh()
{
    dirContents.refresh();
}

void BrowserPanel::selectionChanged()
{
    selectedFile = fileTree.getSelectedFile(0);
    repaint();
}

void BrowserPanel::fileDoubleClicked(const juce::File& f)
{
    if (f.isDirectory())
        navigateTo(f);
}

void BrowserPanel::buttonClicked(juce::Button* b)
{
    if (b == &homeBtn)
        navigateTo(juce::File::getSpecialLocation(juce::File::userHomeDirectory));
    else if (b == &desktopBtn)
        navigateTo(juce::File::getSpecialLocation(juce::File::userDesktopDirectory));
    else if (b == &docsBtn)
        navigateTo(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));
    else if (b == &musicBtn)
        navigateTo(juce::File::getSpecialLocation(juce::File::userMusicDirectory));
    else if (b == &recBtn)
    {
        auto recFolder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                             .getChildFile("NovaStudio").getChildFile("Recordings");
        recFolder.createDirectory();
        navigateTo(recFolder);
    }
}

void BrowserPanel::paint(juce::Graphics& g)
{
    const int W = getWidth();
    g.fillAll(juce::Colour::fromRGB(12, 14, 20));

    // Header bar
    g.setColour(juce::Colour::fromRGB(18, 20, 30));
    g.fillRect(0, 0, W, kHeaderH);
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.setFont(juce::Font(juce::FontOptions(10.5f).withStyle("Bold")));
    g.drawText("BROWSER", 10, 0, W - 46, kHeaderH, juce::Justification::centredLeft);

    // "+" button hint (drawn as text, clickable via fileChooser in mouseDown)
    g.setColour(juce::Colour::fromRGB(150, 120, 255));
    g.setFont(juce::Font(juce::FontOptions(14.0f)));
    g.drawText("+", W - 40, 0, 18, kHeaderH, juce::Justification::centred);

    // Folder icon (open folder hint)
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(u8"\U0001F4C2", W - 22, 0, 20, kHeaderH, juce::Justification::centred);

    // Divider
    g.setColour(juce::Colour::fromRGB(30, 34, 48));
    g.fillRect(0, kHeaderH - 1, W, 1);

    // Current path label
    const int pathY = kHeaderH + kSearchH + kBookmarkH;
    g.setColour(juce::Colour::fromRGB(22, 24, 34));
    g.fillRect(0, pathY - 1, W, 1);

    // Preview footer
    const int previewY = getHeight() - kPreviewH;
    g.setColour(juce::Colour::fromRGB(14, 16, 24));
    g.fillRect(0, previewY, W, kPreviewH);
    g.setColour(juce::Colour::fromRGB(30, 34, 48));
    g.fillRect(0, previewY, W, 1);

    if (selectedFile.existsAsFile())
    {
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));
        g.drawText(selectedFile.getFileNameWithoutExtension(),
                   8, previewY + 5, W - 16, 14, juce::Justification::centredLeft, true);
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.setFont(juce::Font(juce::FontOptions(8.5f)));
        g.drawText(juce::File::descriptionOfSizeInBytes(selectedFile.getSize())
                       + "  " + selectedFile.getFileExtension().toUpperCase(),
                   8, previewY + 21, W - 16, 12, juce::Justification::centredLeft);
        g.setColour(juce::Colour::fromRGB(150, 120, 255).withAlpha(0.7f));
        g.setFont(juce::Font(juce::FontOptions(8.0f)));
        g.drawText(u8"⟵ Drag to timeline", 8, previewY + 35, W - 16, 12,
                   juce::Justification::centredLeft);
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.drawText("Select a file  •  Drag to timeline",
                   0, previewY + 14, W, 14, juce::Justification::centred);
    }
}

void BrowserPanel::resized()
{
    const int W = getWidth();
    int y = kHeaderH;

    searchBox.setBounds(6, y + 2, W - 12, kSearchH - 4);
    y += kSearchH;

    // Bookmark strip — 5 equal buttons
    const int bw = W / 5;
    homeBtn   .setBounds(0,          y, bw,     kBookmarkH);
    desktopBtn.setBounds(bw,         y, bw,     kBookmarkH);
    docsBtn   .setBounds(bw * 2,     y, bw,     kBookmarkH);
    musicBtn  .setBounds(bw * 3,     y, bw,     kBookmarkH);
    recBtn    .setBounds(bw * 4,     y, W - bw * 4, kBookmarkH);
    y += kBookmarkH;

    const int treeH = getHeight() - y - kPreviewH;
    fileTree.setBounds(0, y, W, treeH);
}

// mouseDown on the header area handles the "+" open folder picker
void BrowserPanel::mouseDown(const juce::MouseEvent& e)
{
    // "+" button is drawn at (W-40, 0, 18, kHeaderH)
    if (e.y < kHeaderH && e.x >= getWidth() - 42)
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Choose a folder to browse",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory));
        chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result.isDirectory())
                    navigateTo(result);
            });
    }
}

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
        addAndMakeVisible(beatBtn);
        addAndMakeVisible(rtzBtn);
        addAndMakeVisible(rewindBtn);
        addAndMakeVisible(playBtn);
        addAndMakeVisible(stopBtn);
        addAndMakeVisible(recordBtn);
        addAndMakeVisible(ffBtn);
        addAndMakeVisible(loopBtn);
        addAndMakeVisible(punchBtn);
        addAndMakeVisible(timecodeLabel);
        addAndMakeVisible(tempoLabel);
        addAndMakeVisible(novaAlignBtn);
        addAndMakeVisible(hZoomOutBtn);
        addAndMakeVisible(hZoomInBtn);
        addAndMakeVisible(vZoomOutBtn);
        addAndMakeVisible(vZoomInBtn);

        editBtn.addListener(this);
        mixBtn.addListener(this);
        beatBtn.addListener(this);
        rtzBtn.addListener(this);
        rewindBtn.addListener(this);
        playBtn.addListener(this);
        stopBtn.addListener(this);
        recordBtn.addListener(this);
        ffBtn.addListener(this);
        loopBtn.addListener(this);
        punchBtn.setTooltip("Auto-Punch: Cmd+drag in the timeline ruler to set punch range, then enable here");
        punchBtn.addListener(this);
        novaAlignBtn.addListener(this);
        hZoomOutBtn.addListener(this);
        hZoomInBtn.addListener(this);
        vZoomOutBtn.addListener(this);
        vZoomInBtn.addListener(this);

        // Zoom button styling — subtle, smaller than transport
        for (auto* btn : {&hZoomOutBtn, &hZoomInBtn, &vZoomOutBtn, &vZoomInBtn})
        {
            btn->setColour(juce::TextButton::buttonColourId,  juce::Colour::fromRGB(28, 30, 42));
            btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.55f));
        }
        hZoomOutBtn.setTooltip("Zoom out (horizontal)");
        hZoomInBtn.setTooltip("Zoom in (horizontal)");
        vZoomOutBtn.setTooltip("Zoom out (vertical / track height)");
        vZoomInBtn.setTooltip("Zoom in (vertical / track height)");

        // Mode tab styling — dark base, will highlight active
        for (auto* btn : {&editBtn, &mixBtn, &beatBtn})
        {
            btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(18, 20, 28));
            btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.65f));
            btn->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        }
        // Edit is default active — highlight it
        editBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(30, 28, 50));
        editBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(180, 155, 255));

        // Transport styling — all transparent so paint() icons show cleanly
        for (auto* btn : {&rtzBtn, &rewindBtn, &playBtn, &stopBtn, &recordBtn, &ffBtn, &loopBtn})
        {
            btn->setColour(juce::TextButton::buttonColourId,     juce::Colour::fromRGB(20, 22, 30));
            btn->setColour(juce::TextButton::buttonOnColourId,   juce::Colour::fromRGB(35, 38, 52));
            btn->setColour(juce::TextButton::textColourOffId,    juce::Colours::transparentBlack);
            btn->setColour(juce::TextButton::textColourOnId,     juce::Colours::transparentBlack);
        }

        // Timecode label — large amber
        timecodeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(255, 190, 60));
        timecodeLabel.setFont(juce::Font(juce::FontOptions(20.0f).withStyle("Bold")));
        timecodeLabel.setJustificationType(juce::Justification::centred);
        timecodeLabel.setText("00:00:00:00", juce::dontSendNotification);

        // Tempo label — amber, draggable/editable
        tempoLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(220, 170, 60));
        tempoLabel.setJustificationType(juce::Justification::centred);
        tempoLabel.setTooltip("Drag up/down to change BPM, or double-click to type");
        tempoLabel.setBPM(120);
        tempoLabel.onTempoChanged = [this](int bpm) { if (onTempoChanged) onTempoChanged(bpm); };

        // Right utility buttons
        novaAlignBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(35, 25, 55));
        novaAlignBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(190, 150, 255));
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

    void WorkspaceToolbar::paintOverChildren(juce::Graphics& g)
    {
        // Icons drawn here (AFTER children paint) so they appear on top of button backgrounds
        const juce::Colour iconWhite = juce::Colours::white.withAlpha(0.92f);
        const juce::Colour iconRed   = juce::Colour::fromRGB(235, 55, 55);

        auto drawIcon = [&](juce::TextButton& btn,
                            std::function<void(juce::Graphics&, juce::Rectangle<float>)> fn)
        {
            if (btn.isVisible())
                fn(g, btn.getBounds().toFloat());
        };

        // RTZ  |◀  (bar + single left triangle — matches mock up)
        drawIcon(rtzBtn, [iconWhite](juce::Graphics& g2, juce::Rectangle<float> r) {
            g2.setColour(iconWhite);
            float cy = r.getCentreY(), h = r.getHeight() * 0.32f;
            float barX = r.getCentreX() - h * 1.2f;
            g2.fillRect(barX, cy - h, 2.5f, h * 2.0f);
            juce::Path tri;
            tri.addTriangle(barX + 2.5f + h, cy - h, barX + 2.5f + h, cy + h, barX + 2.5f, cy);
            g2.fillPath(tri);
        });

        // Rewind  ◀◀
        drawIcon(rewindBtn, [iconWhite](juce::Graphics& g2, juce::Rectangle<float> r) {
            g2.setColour(iconWhite);
            float cx = r.getCentreX(), cy = r.getCentreY(), h = r.getHeight() * 0.30f;
            juce::Path t1, t2;
            t1.addTriangle(cx - 1.0f, cy - h, cx - 1.0f, cy + h, cx - 1.0f - h, cy);
            t2.addTriangle(cx + h - 1.0f, cy - h, cx + h - 1.0f, cy + h, cx - 1.0f, cy);
            g2.fillPath(t1);
            g2.fillPath(t2);
        });

        // Play  ▶
        drawIcon(playBtn, [iconWhite](juce::Graphics& g2, juce::Rectangle<float> r) {
            g2.setColour(iconWhite);
            float cx = r.getCentreX() - 1.5f, cy = r.getCentreY(), h = r.getHeight() * 0.34f;
            juce::Path tri;
            tri.addTriangle(cx - h * 0.5f, cy - h, cx - h * 0.5f, cy + h, cx + h, cy);
            g2.fillPath(tri);
        });

        // Stop  ■  — filled white square, clearly visible
        drawIcon(stopBtn, [iconWhite](juce::Graphics& g2, juce::Rectangle<float> r) {
            g2.setColour(iconWhite);
            float s = r.getHeight() * 0.38f;
            g2.fillRect(r.getCentreX() - s * 0.5f, r.getCentreY() - s * 0.5f, s, s);
        });

        // Record  ●  — solid red circle
        drawIcon(recordBtn, [iconRed](juce::Graphics& g2, juce::Rectangle<float> r) {
            g2.setColour(iconRed);
            float s = r.getHeight() * 0.34f;
            g2.fillEllipse(r.getCentreX() - s, r.getCentreY() - s, s * 2.0f, s * 2.0f);
        });

        // Fast-forward  ▶▶
        drawIcon(ffBtn, [iconWhite](juce::Graphics& g2, juce::Rectangle<float> r) {
            g2.setColour(iconWhite);
            float cx = r.getCentreX(), cy = r.getCentreY(), h = r.getHeight() * 0.30f;
            juce::Path t1, t2;
            t1.addTriangle(cx - h, cy - h, cx - h, cy + h, cx, cy);
            t2.addTriangle(cx, cy - h, cx, cy + h, cx + h, cy);
            g2.fillPath(t1);
            g2.fillPath(t2);
        });

        // Loop  ↻
        drawIcon(loopBtn, [iconWhite](juce::Graphics& g2, juce::Rectangle<float> r) {
            g2.setColour(iconWhite);
            float cx = r.getCentreX(), cy = r.getCentreY(), rad = r.getHeight() * 0.28f;
            juce::Path arc;
            arc.addArc(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f,
                       juce::MathConstants<float>::pi * 0.3f,
                       juce::MathConstants<float>::pi * 1.9f, true);
            g2.strokePath(arc, juce::PathStrokeType(1.8f));
            float endA = juce::MathConstants<float>::pi * 0.3f;
            float ax = cx + rad * std::cos(endA), ay = cy + rad * std::sin(endA);
            juce::Path head;
            head.addTriangle(ax - 4.0f, ay - 3.0f, ax + 3.0f, ay + 1.0f, ax - 1.0f, ay + 4.0f);
            g2.fillPath(head);
        });
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
        beatBtn.setBounds(x, btnY, 48, btnH); x += 52;
        // x now at ~395, past divider at 390

        x = 398;
        // Transport — all same-size square buttons (btnH × btnH) with small gaps
        const int tSz = btnH;  // square transport buttons
        rtzBtn.setBounds   (x, btnY, tSz, tSz); x += tSz + 3;
        rewindBtn.setBounds(x, btnY, tSz, tSz); x += tSz + 3;
        x += 4;  // small spacer before play
        playBtn.setBounds  (x, btnY, tSz, tSz); x += tSz + 3;
        stopBtn.setBounds  (x, btnY, tSz, tSz); x += tSz + 3;
        recordBtn.setBounds(x, btnY, tSz, tSz); x += tSz + 3;
        x += 4;  // small spacer after record
        ffBtn.setBounds    (x, btnY, tSz, tSz); x += tSz + 3;
        x += 6;
        loopBtn.setBounds  (x, btnY, tSz, tSz); x += tSz + 3;
        x += 6;
        punchBtn.setBounds   (x, btnY, 54, tSz); x += 58;

        x = juce::jmax(x + 4, 860);  // timecode after punch buttons
        timecodeLabel.setBounds(x, btnY - 2, 160, btnH + 4); x += 164;
        tempoLabel.setBounds(x, btnY, 100, btnH); x += 108;

        // Zoom controls — right of tempo
        const int zSz = 30;
        hZoomOutBtn.setBounds(x, btnY, zSz, btnH); x += zSz + 2;
        hZoomInBtn.setBounds (x, btnY, zSz, btnH); x += zSz + 6;
        vZoomOutBtn.setBounds(x, btnY, zSz, btnH); x += zSz + 2;
        vZoomInBtn.setBounds (x, btnY, zSz, btnH);

        // Right buttons (from right edge)
        int rx = getWidth() - 6;
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

    void WorkspaceToolbar::setArmState(bool /*armed*/) {}

    void WorkspaceToolbar::setMonitorState(bool /*enabled*/) {}

    void WorkspaceToolbar::setPunchState(bool enabled)
    {
        punchBtn.setColour(juce::TextButton::buttonColourId,
            enabled ? juce::Colours::orangered.withAlpha(0.45f) : juce::Colours::transparentBlack);
    }

    void WorkspaceToolbar::setTempo(int bpm)
    {
        tempoLabel.setBPM(bpm);
    }

    void WorkspaceToolbar::setTimecode(const juce::String& tc)
    {
        timecodeLabel.setText(tc, juce::dontSendNotification);
    }

    void WorkspaceToolbar::setPlaybackState(bool /*previewEnabled*/, bool /*hasPreview*/) {}

    void WorkspaceToolbar::buttonClicked(juce::Button* b)
    {
        const auto kActive   = juce::Colour::fromRGB(30, 28, 50);
        const auto kInactive = juce::Colour::fromRGB(18, 20, 28);
        const auto kActiveText   = juce::Colour::fromRGB(180, 155, 255);
        const auto kInactiveText = juce::Colours::white.withAlpha(0.65f);

        auto setTabActive = [&](juce::TextButton* active) {
            for (auto* btn : {&editBtn, &mixBtn, &beatBtn}) {
                const bool on = (btn == active);
                btn->setColour(juce::TextButton::buttonColourId, on ? kActive : kInactive);
                btn->setColour(juce::TextButton::textColourOffId, on ? kActiveText : kInactiveText);
            }
        };

        if (b == &editBtn)  { setTabActive(&editBtn);  if (onModeSelected) onModeSelected(0); }
        else if (b == &mixBtn)  { setTabActive(&mixBtn);   if (onModeSelected) onModeSelected(1); }
        else if (b == &beatBtn) { setTabActive(&beatBtn);  if (onModeSelected) onModeSelected(2); }
        else if (b == &rtzBtn && onReturnToZero) onReturnToZero();
        else if (b == &playBtn && onPlay) onPlay();
        else if (b == &stopBtn && onStop) onStop();
        else if (b == &recordBtn && onRecord) onRecord();
        else if (b == &loopBtn && onLoop) onLoop();
        else if (b == &punchBtn    && onPunchToggle) onPunchToggle();
        else if (b == &novaAlignBtn && onNovaAlign) onNovaAlign();
        else if (b == &hZoomOutBtn && onHZoomChanged) onHZoomChanged(-1);
        else if (b == &hZoomInBtn  && onHZoomChanged) onHZoomChanged(+1);
        else if (b == &vZoomOutBtn && onVZoomChanged) onVZoomChanged(-1);
        else if (b == &vZoomInBtn  && onVZoomChanged) onVZoomChanged(+1);
    }

    // =========================================================================
    // ProductionPanel implementation
    // =========================================================================

    static const juce::Colour kPanelBg      = juce::Colour::fromRGB(14, 16, 22);
    static const juce::Colour kSlotBg       = juce::Colour::fromRGB(22, 24, 34);
    static const juce::Colour kAccent       = juce::Colour::fromRGB(100, 80, 200);
    static const juce::Colour kBypassOn     = juce::Colour::fromRGB(60, 200, 90);
    static const juce::Colour kBypassOff    = juce::Colour::fromRGB(40, 44, 58);
    static const juce::Colour kEQGraphBg    = juce::Colour::fromRGB(10, 12, 18);
    static const juce::Colour kEQCurve      = juce::Colour::fromRGB(80, 200, 100);
    static const juce::Colour kHeaderText   = juce::Colours::white;

    ProductionPanel::ProductionPanel(NovaStudio::ArrangementModel& model)
        : arrangementModel(model)
    {
        // ---- Section 1 ----
        trackNameLabel.setFont(juce::Font(16.0f, juce::Font::bold));
        trackNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        trackNameLabel.setText("No Track", juce::dontSendNotification);
        contentComp.addAndMakeVisible(trackNameLabel);

        inputLabel.setFont(juce::Font(10.0f));
        inputLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        inputLabel.setText("IN: Default", juce::dontSendNotification);
        contentComp.addAndMakeVisible(inputLabel);

        outputLabel.setFont(juce::Font(10.0f));
        outputLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        outputLabel.setText("OUT: Main", juce::dontSendNotification);
        contentComp.addAndMakeVisible(outputLabel);

        gainDbLabel.setFont(juce::Font(11.0f));
        gainDbLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        gainDbLabel.setText("0.0 dB", juce::dontSendNotification);
        gainDbLabel.setJustificationType(juce::Justification::centred);
        contentComp.addAndMakeVisible(gainDbLabel);

        volumeKnob.setRange(-60.0, 12.0, 0.1);
        volumeKnob.setValue(0.0, juce::dontSendNotification);
        volumeKnob.setDoubleClickReturnValue(true, 0.0);
        volumeKnob.setTooltip("Track volume (dB) — double-click to reset to 0 dB");
        volumeKnob.setColour(juce::Slider::trackColourId,         juce::Colour::fromRGB(40,44,60));
        volumeKnob.setColour(juce::Slider::thumbColourId,         juce::Colour::fromRGB(130,140,200));
        volumeKnob.setColour(juce::Slider::backgroundColourId,    juce::Colour::fromRGB(18,20,30));
        volumeKnob.addListener(this);
        contentComp.addAndMakeVisible(volumeKnob);

        for (auto* btn : { &muteBtn, &soloBtn, &armBtn })
        {
            btn->setColour(juce::TextButton::buttonColourId, kSlotBg);
            btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            btn->addListener(this);
            contentComp.addAndMakeVisible(*btn);
        }
        muteBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(200, 140, 40));
        soloBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(220, 200, 40));
        armBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(200, 60, 60));
        muteBtn.setClickingTogglesState(true);
        soloBtn.setClickingTogglesState(true);
        armBtn.setClickingTogglesState(true);

        // ---- Section 2: Inserts ----
        for (int i = 0; i < kNumInserts; ++i)
        {
            auto& slot = insertSlots[i];
            slot.bypassBtn.setColour(juce::TextButton::buttonColourId, kBypassOn);
            slot.bypassBtn.setColour(juce::TextButton::buttonOnColourId, kBypassOff);
            slot.bypassBtn.setClickingTogglesState(true);
            slot.bypassBtn.addListener(this);
            contentComp.addAndMakeVisible(slot.bypassBtn);

            slot.nameBtn.setColour(juce::TextButton::buttonColourId, kSlotBg);
            slot.nameBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(90, 90, 110));
            slot.nameBtn.addListener(this);
            contentComp.addAndMakeVisible(slot.nameBtn);
        }

        // ---- Section 3: EQ ----
        static const char* kBandNames[kNumBands] = { "HPF", "LF", "LMF", "HMF", "HF", "LPF" };
        static const float kDefaultFreqs[kNumBands] = { 80.0f, 200.0f, 800.0f, 3000.0f, 8000.0f, 16000.0f };

        for (int i = 0; i < kNumBands; ++i)
        {
            eqBands[i].freq = kDefaultFreqs[i];

            auto& bc = bandControls[i];
            bc.bandLabel.setFont(juce::Font(9.0f, juce::Font::bold));
            bc.bandLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            bc.bandLabel.setText(kBandNames[i], juce::dontSendNotification);
            bc.bandLabel.setJustificationType(juce::Justification::centred);
            contentComp.addAndMakeVisible(bc.bandLabel);

            bc.freqSlider.setRange(20.0, 20000.0, 1.0);
            bc.freqSlider.setSkewFactorFromMidPoint(1000.0);
            bc.freqSlider.setValue(kDefaultFreqs[i], juce::dontSendNotification);
            bc.freqSlider.addListener(this);
            contentComp.addAndMakeVisible(bc.freqSlider);

            // HPF and LPF have no gain slider — we still add it but will hide
            bc.gainSlider.setRange(-12.0, 12.0, 0.1);
            bc.gainSlider.setValue(0.0, juce::dontSendNotification);
            bc.gainSlider.addListener(this);
            contentComp.addAndMakeVisible(bc.gainSlider);

            bc.qSlider.setRange(0.1, 10.0, 0.01);
            bc.qSlider.setSkewFactorFromMidPoint(1.0);
            bc.qSlider.setValue(0.707, juce::dontSendNotification);
            bc.qSlider.addListener(this);
            contentComp.addAndMakeVisible(bc.qSlider);

            bc.enableBtn.setClickingTogglesState(true);
            bc.enableBtn.setToggleState(false, juce::dontSendNotification);
            bc.enableBtn.setColour(juce::TextButton::buttonColourId, kBypassOff);
            bc.enableBtn.setColour(juce::TextButton::buttonOnColourId, kBypassOn);
            bc.enableBtn.addListener(this);
            contentComp.addAndMakeVisible(bc.enableBtn);
        }

        // ---- Section 4: Sends ----
        for (int i = 0; i < kNumSends; ++i)
        {
            auto& row = sendRows[i];

            // Bus assignment button — click to pick bus
            row.busBtn.setButtonText("—");
            row.busBtn.setColour(juce::TextButton::buttonColourId, kSlotBg);
            row.busBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
            row.busBtn.setTooltip("Click to assign a send bus");
            const int sendIdx = i;
            row.busBtn.onClick = [this, sendIdx]() {
                juce::PopupMenu m;
                m.addItem(1, "Unassign");
                m.addSeparator();
                for (int b = 0; b < 8; ++b)
                    m.addItem(b + 2, "Bus " + juce::String(b * 2 + 1) + "-" + juce::String(b * 2 + 2));
                m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sendRows[sendIdx].busBtn),
                    [this, sendIdx](int result) {
                        const int busIndex = (result <= 1) ? -1 : result - 2;
                        sendRows[sendIdx].assignedBus = busIndex;
                        const juce::String label = busIndex < 0 ? "—"
                            : ("B" + juce::String(busIndex * 2 + 1) + "-" + juce::String(busIndex * 2 + 2));
                        sendRows[sendIdx].busBtn.setButtonText(label);
                        sendRows[sendIdx].busBtn.setColour(juce::TextButton::buttonColourId,
                            busIndex >= 0 ? kAccent.withAlpha(0.5f) : kSlotBg);
                        if (onSendBusChanged) onSendBusChanged(sendIdx, busIndex);
                        // Raise level to 0 dB when bus first assigned
                        if (busIndex >= 0 && sendRows[sendIdx].levelSlider.getValue() < -90.0)
                        {
                            sendRows[sendIdx].levelSlider.setValue(0.0, juce::sendNotification);
                        }
                    });
            };
            contentComp.addAndMakeVisible(row.busBtn);

            // Level fader: -inf to +6 dB
            row.levelSlider.setRange(-100.0, 6.0, 0.1);
            row.levelSlider.setValue(-100.0, juce::dontSendNotification);
            row.levelSlider.setSkewFactorFromMidPoint(-18.0);
            row.levelSlider.setTooltip("Send level (dB)");
            row.levelSlider.addListener(this);
            contentComp.addAndMakeVisible(row.levelSlider);

            // PRE/POST fader toggle
            row.preBtn.setClickingTogglesState(true);
            row.preBtn.setColour(juce::TextButton::buttonColourId, kSlotBg);
            row.preBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange.withAlpha(0.5f));
            row.preBtn.setTooltip("Toggle Pre/Post fader send");
            row.preBtn.onClick = [this, sendIdx]() {
                const bool pre = sendRows[sendIdx].preBtn.getToggleState();
                sendRows[sendIdx].preBtn.setButtonText(pre ? "PRE" : "POST");
                sendRows[sendIdx].isPreFader = pre;
                if (onSendPreFaderChanged) onSendPreFaderChanged(sendIdx, pre);
            };
            contentComp.addAndMakeVisible(row.preBtn);
        }

        viewport.setViewedComponent(&contentComp, false);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);
        startTimerHz(30);
    }

    ProductionPanel::~ProductionPanel()
    {
        stopTimer();
        volumeKnob.removeListener(this);
        for (auto* btn : { &muteBtn, &soloBtn, &armBtn })
            btn->removeListener(this);
        for (auto& slot : insertSlots)
        {
            slot.bypassBtn.removeListener(this);
            slot.nameBtn.removeListener(this);
        }
        for (auto& bc : bandControls)
        {
            bc.freqSlider.removeListener(this);
            bc.gainSlider.removeListener(this);
            bc.qSlider.removeListener(this);
            bc.enableBtn.removeListener(this);
        }
        for (auto& row : sendRows)
            row.levelSlider.removeListener(this);
    }

    void ProductionPanel::updateFromTrack(const NovaStudio::Track& track)
    {
        trackNameLabel.setText(track.name, juce::dontSendNotification);
        currentVolDb  = track.volumeDb;
        currentMuted  = track.muted;
        currentSoloed = track.solo;
        currentArmed  = track.armed;

        gainDbLabel.setText(juce::String(track.volumeDb, 1) + " dB", juce::dontSendNotification);
        volumeKnob.setValue(track.volumeDb, juce::dontSendNotification);
        muteBtn.setToggleState(track.muted, juce::dontSendNotification);
        soloBtn.setToggleState(track.solo, juce::dontSendNotification);
        armBtn.setToggleState(track.armed, juce::dontSendNotification);

        // Restore send state for this track
        for (int i = 0; i < kNumSends; ++i)
        {
            auto& row = sendRows[i];
            const int busIdx  = track.sendBusIndex[i];
            const float level = track.sendLevels[i];
            const bool pre    = track.sendPreFader[i];

            row.assignedBus  = busIdx;
            row.isPreFader   = pre;

            const juce::String label = busIdx < 0 ? "—"
                : ("B" + juce::String(busIdx * 2 + 1) + "-" + juce::String(busIdx * 2 + 2));
            row.busBtn.setButtonText(label);
            row.busBtn.setColour(juce::TextButton::buttonColourId,
                busIdx >= 0 ? kAccent.withAlpha(0.5f) : kSlotBg);

            row.levelSlider.setValue(level, juce::dontSendNotification);
            row.preBtn.setToggleState(pre, juce::dontSendNotification);
            row.preBtn.setButtonText(pre ? "PRE" : "POST");
        }

        // Always reset EQ to flat/disabled when switching tracks.
        // EQ state lives in the engine's TrackPlayer, not in the Track struct,
        // so we can't restore it here — just guarantee a flat starting state.
        static const float kDefaultFreqs[kNumBands] = { 80.0f, 200.0f, 800.0f, 3000.0f, 8000.0f, 16000.0f };
        for (int i = 0; i < kNumBands; ++i)
        {
            eqBands[i].freq    = kDefaultFreqs[i];
            eqBands[i].gain    = 0.0f;
            eqBands[i].q       = 0.707f;
            eqBands[i].enabled = false;
            bandControls[i].enableBtn.setToggleState(false, juce::dontSendNotification);
            bandControls[i].enableBtn.setColour(juce::TextButton::buttonColourId, kBypassOff);
        }

        repaint();
    }

    void ProductionPanel::setInsertSlotName(int slot, const juce::String& name)
    {
        if (slot < 0 || slot >= kNumInserts) return;
        insertSlots[slot].pluginName = name;
        if (name.isEmpty())
        {
            insertSlots[slot].nameBtn.setButtonText("[ Empty Slot ]");
            insertSlots[slot].nameBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(90, 90, 110));
        }
        else
        {
            insertSlots[slot].nameBtn.setButtonText(name);
            insertSlots[slot].nameBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        }
    }

    void ProductionPanel::paintSectionHeader(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour(juce::Colour::fromRGB(35, 38, 52));
        g.fillRect(r);
        g.setColour(juce::Colour::fromRGB(55, 58, 75));
        g.fillRect(r.getX(), r.getBottom() - 1, r.getWidth(), 1);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.setColour(kHeaderText);
        g.drawText(title, r.reduced(6, 0), juce::Justification::centredLeft);
    }

    void ProductionPanel::paintMeter(juce::Graphics& g, juce::Rectangle<int> r)
    {
        // Horizontal L/R meter bars
        // r is split: top half = L, bottom half = R (with 2px gap)
        const int barH  = (r.getHeight() - 6) / 2;  // height of each bar
        const int lY    = r.getY() + 1;
        const int rY    = lY + barH + 4;
        const int barW  = r.getWidth() - 22;         // 22px for label on left
        const int barX  = r.getX() + 20;

        auto drawHBar = [&](int bx, int by, int bw, int bh, float level, const char* label)
        {
            // Label
            g.setColour(juce::Colours::white.withAlpha(0.45f));
            g.setFont(juce::FontOptions(7.5f));
            g.drawText(label, r.getX(), by, 18, bh, juce::Justification::centredRight, false);

            // Dark background track
            g.setColour(juce::Colour::fromRGB(14, 16, 24));
            g.fillRoundedRectangle((float)bx, (float)by, (float)bw, (float)bh, 2.0f);

            if (level < 0.001f) return;

            const float db      = 20.0f * std::log10(juce::jmax(level, 1e-6f));
            // Map dB to fill fraction: -60..+3 → 0..1
            const float norm    = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 63.0f);
            const int   fillW   = (int)(norm * bw);

            // Segmented horizontal bars (green→yellow→red left to right)
            const int segW = 3, segGap = 1;
            for (int fx = bx; fx < bx + fillW - segW; fx += segW + segGap)
            {
                const float t = (float)(fx - bx) / (float)bw;
                juce::Colour segCol;
                if (t < 0.70f)       segCol = juce::Colour::fromRGB(50, 200,  80);
                else if (t < 0.88f)  segCol = juce::Colour::fromRGB(220, 200,  30);
                else                 segCol = juce::Colour::fromRGB(220,  50,  50);
                g.setColour(segCol);
                g.fillRect(fx, by + 1, juce::jmin(segW, bx + fillW - fx), bh - 2);
            }

            // dB value overlay on right
            g.setColour(juce::Colours::white.withAlpha(0.55f));
            g.setFont(juce::FontOptions(6.5f));
            const juce::String dbStr = (db >= 0.0f ? "+" : "") + juce::String(db, 1) + "dB";
            g.drawText(dbStr, bx + bw - 36, by, 36, bh, juce::Justification::centredRight, false);
        };

        drawHBar(barX, lY, barW, barH, meterLevelL, "L");
        drawHBar(barX, rY, barW, barH, meterLevelR, "R");
    }

    double ProductionPanel::calcBiquadMagnitude(int bandIdx, double normFreq) const
    {
        // Simplified magnitude response for display only
        const auto& band = eqBands[bandIdx];
        if (!band.enabled) return 1.0;

        double sampleRate = 44100.0;
        double w0 = 2.0 * juce::MathConstants<double>::pi * band.freq / sampleRate;
        double cosw0 = std::cos(w0);
        double sinw0 = std::sin(w0);
        double alpha = sinw0 / (2.0 * band.q);

        double b0, b1, b2, a0, a1, a2;

        if (bandIdx == 0) // HPF
        {
            b0 = (1.0 + cosw0) / 2.0;
            b1 = -(1.0 + cosw0);
            b2 = (1.0 + cosw0) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw0;
            a2 = 1.0 - alpha;
        }
        else if (bandIdx == kNumBands - 1) // LPF
        {
            b0 = (1.0 - cosw0) / 2.0;
            b1 = 1.0 - cosw0;
            b2 = (1.0 - cosw0) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw0;
            a2 = 1.0 - alpha;
        }
        else // Peaking EQ
        {
            double A = std::pow(10.0, band.gain / 40.0);
            b0 = 1.0 + alpha * A;
            b1 = -2.0 * cosw0;
            b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A;
            a1 = -2.0 * cosw0;
            a2 = 1.0 - alpha / A;
        }

        // Evaluate at normFreq (0..pi) using z = e^(jw)
        double w = normFreq * juce::MathConstants<double>::pi;
        double cosW = std::cos(w);
        double sinW = std::sin(w);
        double cos2W = std::cos(2.0 * w);
        double sin2W = std::sin(2.0 * w);

        double bRe = b0 + b1 * cosW + b2 * cos2W;
        double bIm = -b1 * sinW - b2 * sin2W;
        double aRe = a0 + a1 * cosW + a2 * cos2W;
        double aIm = -a1 * sinW - a2 * sin2W;

        double bMag2 = bRe * bRe + bIm * bIm;
        double aMag2 = aRe * aRe + aIm * aIm;

        return std::sqrt(bMag2 / (aMag2 + 1e-30));
    }

    static juce::Point<float> eqNodePos(const juce::Rectangle<int>&, float, float); // forward decl

    void ProductionPanel::paintEQGraph(juce::Graphics& g, juce::Rectangle<int> r)
    {
        g.setColour(kEQGraphBg);
        g.fillRect(r);
        g.setColour(juce::Colour::fromRGB(30, 35, 45));
        g.drawRect(r, 1);

        // Grid lines
        g.setColour(juce::Colour::fromRGB(25, 30, 40));
        for (int db : { -12, -6, 0, 6, 12 })
        {
            float y = r.getY() + r.getHeight() * (1.0f - (db + 12.0f) / 24.0f);
            g.drawHorizontalLine((int)y, (float)r.getX(), (float)r.getRight());
        }

        // Draw summed frequency response curve
        juce::Path curve;
        const int numPoints = r.getWidth();
        bool first = true;
        for (int px = 0; px < numPoints; ++px)
        {
            double normFreq = (double)px / numPoints;
            // Sum magnitude in dB across all bands
            double totalMag = 1.0;
            for (int i = 0; i < kNumBands; ++i)
                totalMag *= calcBiquadMagnitude(i, normFreq);

            double dB = 20.0 * std::log10(totalMag + 1e-30);
            dB = juce::jlimit(-12.0, 12.0, dB);
            float y = r.getY() + r.getHeight() * (float)(1.0 - (dB + 12.0) / 24.0);
            float x = (float)(r.getX() + px);

            if (first) { curve.startNewSubPath(x, y); first = false; }
            else        curve.lineTo(x, y);
        }

        // Filled gradient under curve
        {
            juce::Path filled = curve;
            filled.lineTo((float)(r.getRight()), (float)r.getBottom());
            filled.lineTo((float)r.getX(), (float)r.getBottom());
            filled.closeSubPath();
            juce::ColourGradient grad(kAccent.withAlpha(0.15f), 0.0f, (float)r.getY(),
                                      juce::Colours::transparentBlack, 0.0f, (float)r.getBottom(), false);
            g.setGradientFill(grad);
            g.fillPath(filled);
        }

        g.setColour(kEQCurve);
        g.strokePath(curve, juce::PathStrokeType(1.5f));

        // 0 dB center line (slightly brighter)
        {
            float zeroY = r.getY() + r.getHeight() * 0.5f;
            g.setColour(juce::Colour::fromRGB(45, 52, 65));
            g.drawHorizontalLine((int)zeroY, (float)r.getX(), (float)r.getRight());
        }

        // Frequency axis labels at bottom
        {
            static const struct { float hz; const char* label; } kFreqLabels[] = {
                { 50.0f, "50" }, { 200.0f, "200" }, { 1000.0f, "1k" },
                { 5000.0f, "5k" }, { 20000.0f, "20k" }
            };
            const float logMin = std::log10(20.0f), logMax = std::log10(20000.0f);
            g.setFont(juce::FontOptions(7.0f));
            for (auto& fl : kFreqLabels)
            {
                float nx = (std::log10(fl.hz) - logMin) / (logMax - logMin);
                float fx = r.getX() + nx * r.getWidth();
                g.setColour(juce::Colours::white.withAlpha(0.35f));
                g.drawText(fl.label, (int)(fx - 12), r.getBottom() - 11, 24, 10,
                           juce::Justification::centred, false);
                g.setColour(juce::Colour::fromRGB(30, 35, 45));
                g.drawVerticalLine((int)fx, (float)r.getY(), (float)(r.getBottom() - 11));
            }
        }

        // dB labels on right edge
        {
            static const struct { float db; const char* label; } kDbEdge[] = {
                { 12.0f, "+12" }, { 0.0f, "0" }, { -12.0f, "-12" }
            };
            g.setFont(juce::FontOptions(7.0f));
            for (auto& dl : kDbEdge)
            {
                float dy = r.getY() + r.getHeight() * (1.0f - (dl.db + 12.0f) / 24.0f);
                g.setColour(juce::Colours::white.withAlpha(0.35f));
                g.drawText(dl.label, r.getRight() - 20, (int)(dy - 4), 19, 9,
                           juce::Justification::centredRight, false);
            }
        }

        // Draw draggable band nodes
        static const juce::Colour nodeColours[] = {
            juce::Colour::fromRGB(80, 160, 255),   // HPF  blue
            juce::Colour::fromRGB(80, 200, 120),   // LF   green
            juce::Colour::fromRGB(200, 160, 60),   // LMF  amber
            juce::Colour::fromRGB(200, 100, 200),  // HMF  purple
            juce::Colour::fromRGB(200, 80,  80),   // HF   red
            juce::Colour::fromRGB(80, 200, 200),   // LPF  cyan
        };
        static const char* kBandShortNames[] = { "HP","LF","LM","HM","HF","LP" };
        for (int i = 0; i < kNumBands; ++i)
        {
            auto node = eqNodePos(r, eqBands[i].freq, eqBands[i].gain);
            const bool dragging  = (i == dragBandIndex);
            const bool isEnabled = eqBands[i].enabled;
            const float radius   = dragging ? 10.0f : 8.0f;
            const juce::Colour col = nodeColours[i % 6];

            // Glow halo for enabled nodes
            if (isEnabled)
            {
                g.setColour(col.withAlpha(0.18f));
                g.fillEllipse(node.x - radius - 4, node.y - radius - 4,
                              (radius + 4) * 2, (radius + 4) * 2);
            }

            // Outer ring
            g.setColour(isEnabled ? col : col.withAlpha(0.3f));
            g.fillEllipse(node.x - radius, node.y - radius, radius * 2, radius * 2);

            // Inner highlight
            g.setColour(juce::Colours::white.withAlpha(isEnabled ? 0.25f : 0.10f));
            g.fillEllipse(node.x - radius * 0.5f, node.y - radius * 0.8f,
                          radius * 0.7f, radius * 0.5f);

            // Border
            g.setColour(juce::Colours::white.withAlpha(dragging ? 1.0f : 0.7f));
            g.drawEllipse(node.x - radius, node.y - radius, radius * 2, radius * 2,
                          dragging ? 2.0f : 1.2f);

            // Band label inside node
            g.setFont(juce::FontOptions(juce::Font::plain).withHeight(7.5f).withStyle("Bold"));
            g.setColour(juce::Colours::white.withAlpha(isEnabled ? 0.9f : 0.4f));
            g.drawText(kBandShortNames[i], (int)(node.x - radius), (int)(node.y - 5),
                       (int)(radius * 2), 10, juce::Justification::centred);

            // Frequency value below node
            g.setFont(juce::FontOptions(6.5f));
            g.setColour(col.withAlpha(isEnabled ? 0.8f : 0.4f));
            const juce::String freqLabel = eqBands[i].freq >= 1000.0f
                ? juce::String(eqBands[i].freq / 1000.0f, 1) + "k"
                : juce::String((int)eqBands[i].freq);
            g.drawText(freqLabel, (int)(node.x - 14), (int)(node.y + radius + 2), 28, 9,
                       juce::Justification::centred);
        }
    }

    void ProductionPanel::paint(juce::Graphics& g)
    {
        g.fillAll(kPanelBg);
    }

    void ProductionPanel::paintOverChildren(juce::Graphics& g)
    {
        // Convert contentComp-local bounds to ProductionPanel coordinates.
        // contentComp.getBounds() returns its position within viewport (parent).
        // viewport is at {0,0} in ProductionPanel, so panel coord = contentComp pos + contentComp-local.
        const auto vOffset = contentComp.getBounds().getPosition();

        // Horizontal L/R level meter (near top of content, scrolls with content)
        if (meterBounds.getWidth() > 0)
        {
            const auto meterInPanel = meterBounds.translated(vOffset.x, vOffset.y);
            // Only draw if visible within panel bounds
            if (meterInPanel.getBottom() > 0 && meterInPanel.getY() < getHeight())
                paintMeter(g, meterInPanel);
        }

        // EQ graph
        if (eqGraphBounds.getWidth() > 0)
        {
            const auto eqInPanel = eqGraphBounds.translated(vOffset.x, vOffset.y);
            if (eqInPanel.getBottom() > 0 && eqInPanel.getY() < getHeight())
                paintEQGraph(g, eqInPanel);
        }
    }

    void ProductionPanel::resized()
    {
        viewport.setBounds(getLocalBounds());

        const int W = getWidth();
        const int totalH = 180 + 18 + 8*22 + 14 + 80 + 14 + 18 + 8*22 + 10 + 200 + 14 + 4*28 + 20;
        contentComp.setSize(W, juce::jmax(getHeight(), totalH));

        int y = 0;

        // ---- Section 1: Track Channel ----
        const int headerH = 18;

        // Track name row
        trackNameLabel.setBounds(4, y + 2, W - 8, 22);
        y += 26;

        // Horizontal L/R meter (full width, 28px tall)
        const int meterH = 28;
        meterBounds = { 4, y, W - 8, meterH };
        y += meterH + 4;

        // Horizontal volume fader (full width, 20px)
        volumeKnob.setBounds(4, y, W - 8, 20);
        y += 24;

        // IN/OUT labels
        inputLabel.setBounds(4, y, W/2 - 6, 14);
        outputLabel.setBounds(W/2 + 2, y, W/2 - 6, 14);
        y += 18;

        // MSR buttons + dB readout
        const int btnW = 28;
        muteBtn.setBounds(4, y, btnW, 22);
        soloBtn.setBounds(4 + btnW + 3, y, btnW, 22);
        armBtn.setBounds(4 + (btnW + 3) * 2, y, btnW, 22);
        gainDbLabel.setBounds(4 + (btnW + 3) * 3, y, W - 4 - (btnW + 3) * 3 - 4, 22);
        y += 28;

        // ---- Section 2: Inserts (header 18 + 8*22 + gap 8) ----
        // Header painted in paint()
        y += 8; // gap before section

        // Section header
        int sec2HeaderY = y;
        y += headerH;

        const int slotH = 22;
        const int bypassW = 18;
        for (int i = 0; i < kNumInserts; ++i)
        {
            insertSlots[i].bypassBtn.setBounds(4, y, bypassW, slotH - 2);
            insertSlots[i].nameBtn.setBounds(4 + bypassW + 2, y, W - 4 - bypassW - 6, slotH - 2);
            y += slotH;
        }
        y += 8;

        // ---- Section 3: EQ (header + graph + band controls) ----
        int sec3HeaderY = y;
        y += headerH;

        // EQ graph
        const int graphH = 80;
        eqGraphBounds = juce::Rectangle<int>(4, y, W - 8, graphH);
        y += graphH + 4;

        // Band controls — 6 bands arranged horizontally
        const int bandW = (W - 8) / kNumBands;
        const int knobSz = 28;
        const int smallKnobSz = 20;
        for (int i = 0; i < kNumBands; ++i)
        {
            int bx = 4 + i * bandW;
            auto& bc = bandControls[i];
            bc.bandLabel.setBounds(bx, y, bandW, 12);
            bc.enableBtn.setBounds(bx + (bandW - 14) / 2, y + 12, 14, 14);
            bc.freqSlider.setBounds(bx + (bandW - knobSz) / 2, y + 28, knobSz, knobSz);
            // HPF and LPF hide gain slider
            bool hasGain = (i != 0 && i != kNumBands - 1);
            bc.gainSlider.setVisible(hasGain);
            if (hasGain)
                bc.gainSlider.setBounds(bx + (bandW - smallKnobSz) / 2, y + 28 + knobSz + 2, smallKnobSz, smallKnobSz);
            bc.qSlider.setBounds(bx + (bandW - smallKnobSz) / 2, y + 28 + knobSz + (hasGain ? smallKnobSz + 4 : 2), smallKnobSz, smallKnobSz);
        }
        y += 28 + knobSz + smallKnobSz + smallKnobSz + 8;
        y += 8;

        // ---- Section 4: Sends ----
        int sec4HeaderY = y;
        y += headerH;

        const int sendRowH = 32;
        for (int i = 0; i < kNumSends; ++i)
        {
            // [BusBtn 70px] [Fader 8px wide full height] [PRE/POST btn 40px]
            sendRows[i].busBtn.setBounds(4, y + 2, 70, 22);
            sendRows[i].levelSlider.setBounds(78, y + 2, 8, sendRowH - 6);
            sendRows[i].preBtn.setBounds(W - 44, y + 4, 40, 18);
            y += sendRowH;
        }
        y += 10;

        contentComp.setSize(W, juce::jmax(getHeight(), y));

        // Store header y positions for paint
        // We'll draw headers in paint() by reading contentComp position
        // Store as member vars via a small trick: repaint after layout
        repaint();

        juce::ignoreUnused(sec2HeaderY, sec3HeaderY, sec4HeaderY);
    }

    // ── EQ graph mouse: drag nodes to adjust freq (X) and gain (Y) ────────────

    static juce::Point<float> eqNodePos(const juce::Rectangle<int>& r,
                                        float freq, float gain)
    {
        // X: log scale 20Hz–20kHz
        const float logMin = std::log10(20.0f), logMax = std::log10(20000.0f);
        const float nx = (std::log10(juce::jlimit(20.0f, 20000.0f, freq)) - logMin)
                         / (logMax - logMin);
        // Y: gain ±12 dB linear
        const float ny = 1.0f - (juce::jlimit(-12.0f, 12.0f, gain) + 12.0f) / 24.0f;
        return { r.getX() + nx * r.getWidth(), r.getY() + ny * r.getHeight() };
    }

    void ProductionPanel::mouseDown(const juce::MouseEvent& e)
    {
        if (eqGraphBounds.getWidth() <= 0) return;
        const auto offset = contentComp.getBounds().getPosition() - viewport.getViewPosition();
        const auto graphInThis = eqGraphBounds.translated(viewport.getX() + offset.x,
                                                          viewport.getY() + offset.y);
        if (!graphInThis.contains(e.getPosition())) return;

        // Hit-test each band node (14px radius) — dragging a disabled band auto-enables it
        dragBandIndex = -1;
        for (int i = 0; i < kNumBands; ++i)
        {
            auto node = eqNodePos(graphInThis, eqBands[i].freq, eqBands[i].gain);
            if (e.position.getDistanceFrom(node) < 14.0f)
            {
                dragBandIndex  = i;
                dragStartFreq  = eqBands[i].freq;
                dragStartGain  = eqBands[i].gain;
                dragStartPos   = e.position;
                // Auto-enable band when user grabs the node
                if (!eqBands[i].enabled)
                {
                    eqBands[i].enabled = true;
                    bandControls[i].enableBtn.setToggleState(true, juce::dontSendNotification);
                    bandControls[i].enableBtn.setColour(juce::TextButton::buttonColourId, kBypassOn);
                    if (onEQChanged) onEQChanged(i, eqBands[i].freq, eqBands[i].gain, eqBands[i].q);
                }
                return;
            }
        }
    }

    void ProductionPanel::mouseDrag(const juce::MouseEvent& e)
    {
        if (dragBandIndex < 0 || eqGraphBounds.getWidth() <= 0) return;
        const auto offset = contentComp.getBounds().getPosition() - viewport.getViewPosition();
        const auto graphInThis = eqGraphBounds.translated(viewport.getX() + offset.x,
                                                          viewport.getY() + offset.y);

        // X → freq (log)
        const float logMin = std::log10(20.0f), logMax = std::log10(20000.0f);
        const float nx = juce::jlimit(0.0f, 1.0f,
            (e.position.x - graphInThis.getX()) / (float)graphInThis.getWidth());
        const float newFreq = std::pow(10.0f, logMin + nx * (logMax - logMin));

        // Y → gain (linear ±12 dB)
        const float ny = juce::jlimit(0.0f, 1.0f,
            (e.position.y - graphInThis.getY()) / (float)graphInThis.getHeight());
        const float newGain = 12.0f - ny * 24.0f;

        eqBands[dragBandIndex].freq = newFreq;
        eqBands[dragBandIndex].gain = newGain;

        // Sync knob positions
        bandControls[dragBandIndex].freqSlider.setValue(newFreq, juce::dontSendNotification);
        bandControls[dragBandIndex].gainSlider.setValue(newGain, juce::dontSendNotification);

        if (onEQChanged)
            onEQChanged(dragBandIndex, newFreq, newGain, eqBands[dragBandIndex].q);

        repaint();
    }

    void ProductionPanel::mouseUp(const juce::MouseEvent&)
    {
        dragBandIndex = -1;
    }

    void ProductionPanel::sliderValueChanged(juce::Slider* s)
    {
        for (int i = 0; i < kNumBands; ++i)
        {
            auto& bc = bandControls[i];
            if (s == &bc.freqSlider)
            {
                eqBands[i].freq = (float)s->getValue();
                if (onEQChanged) onEQChanged(i, eqBands[i].freq, eqBands[i].gain, eqBands[i].q);
                repaint();
                return;
            }
            if (s == &bc.gainSlider)
            {
                eqBands[i].gain = (float)s->getValue();
                if (onEQChanged) onEQChanged(i, eqBands[i].freq, eqBands[i].gain, eqBands[i].q);
                repaint();
                return;
            }
            if (s == &bc.qSlider)
            {
                eqBands[i].q = (float)s->getValue();
                if (onEQChanged) onEQChanged(i, eqBands[i].freq, eqBands[i].gain, eqBands[i].q);
                repaint();
                return;
            }
        }
        for (int i = 0; i < kNumSends; ++i)
        {
            if (s == &sendRows[i].levelSlider)
            {
                if (onSendLevelChanged) onSendLevelChanged(i, (float)s->getValue());
                return;
            }
        }

        if (s == &volumeKnob)
        {
            currentVolDb = (float)s->getValue();
            gainDbLabel.setText(juce::String(currentVolDb, 1) + " dB", juce::dontSendNotification);
            if (onVolumeChanged) onVolumeChanged(currentVolDb);
            return;
        }
    }

    void ProductionPanel::buttonClicked(juce::Button* b)
    {
        for (int i = 0; i < kNumInserts; ++i)
        {
            if (b == &insertSlots[i].nameBtn)
            {
                if (insertSlots[i].pluginName.isNotEmpty())
                {
                    // Right-click / context: show popup menu
                    juce::PopupMenu menu;
                    menu.addItem(1, "Open Editor");
                    menu.addItem(2, "Change Plugin...");
                    menu.addItem(3, "Remove");
                    menu.showMenuAsync(juce::PopupMenu::Options{}, [this, i](int result)
                    {
                        if (result == 1 && onInsertClicked)       onInsertClicked(i);
                        else if (result == 2 && onInsertChangePlugin) onInsertChangePlugin(i);
                        else if (result == 3 && onInsertRemovePlugin) onInsertRemovePlugin(i);
                    });
                }
                else
                {
                    if (onInsertClicked) onInsertClicked(i);
                }
                return;
            }
            if (b == &insertSlots[i].bypassBtn)
            {
                insertSlots[i].bypassed = insertSlots[i].bypassBtn.getToggleState();
                repaint();
                return;
            }
        }
        for (int i = 0; i < kNumBands; ++i)
        {
            if (b == &bandControls[i].enableBtn)
            {
                eqBands[i].enabled = bandControls[i].enableBtn.getToggleState();
                if (onEQChanged) onEQChanged(i, eqBands[i].freq, eqBands[i].gain, eqBands[i].q);
                repaint();
                return;
            }
        }
    }
}

