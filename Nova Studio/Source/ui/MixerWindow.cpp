#include "MixerWindow.h"

using namespace NovaStudioUI;

// ─────────────────────────────────────────────────────────────────────────────
// Colours
// ─────────────────────────────────────────────────────────────────────────────

static juce::Colour kStripBg       = juce::Colour::fromRGB(18, 20, 28);
static juce::Colour kStripEdge     = juce::Colour::fromRGBA(255, 255, 255, 14);
static juce::Colour kMuteBtnOn     = juce::Colour::fromRGB(220, 140,  30);
static juce::Colour kSoloBtnOn     = juce::Colour::fromRGB(130,  70, 240);
static juce::Colour kArmBtnOn      = juce::Colour::fromRGB(210,  45,  45);
static juce::Colour kBtnOff        = juce::Colour::fromRGB(38,  42,  54);
static juce::Colour kFaderTrack    = juce::Colour::fromRGB(30,  33,  44);
static juce::Colour kFaderThumb    = juce::Colour::fromRGB(210, 210, 220);
static juce::Colour kMeterGreen    = juce::Colour::fromRGB( 60, 200,  90);
static juce::Colour kMeterYellow   = juce::Colour::fromRGB(230, 200,  40);
static juce::Colour kMeterRed      = juce::Colour::fromRGB(220,  55,  55);
static juce::Colour kSlotBg        = juce::Colour::fromRGB(24,  27,  36);
static juce::Colour kSlotEdge      = juce::Colour::fromRGBA(255, 255, 255, 22);
static juce::Colour kMasterEdge    = juce::Colour::fromRGB(180, 130,  50);
static juce::Colour kAuxEdge       = juce::Colour::fromRGB( 60, 110, 180);

// ─────────────────────────────────────────────────────────────────────────────
// LookAndFeel helpers
// ─────────────────────────────────────────────────────────────────────────────

static void applyFaderStyle(juce::Slider& s)
{
    s.setColour(juce::Slider::trackColourId,       kFaderTrack);
    s.setColour(juce::Slider::thumbColourId,        kFaderThumb);
    s.setColour(juce::Slider::backgroundColourId,   kFaderTrack);
    s.setColour(juce::Slider::textBoxTextColourId,  juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
}

static void applyPanStyle(juce::Slider& s)
{
    s.setColour(juce::Slider::trackColourId,  juce::Colour::fromRGB(50, 54, 70));
    s.setColour(juce::Slider::thumbColourId,  juce::Colour::fromRGB(180, 160, 220));
    s.setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGB(30, 33, 44));
    s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
}

static void styleButton(juce::TextButton& btn, bool active, juce::Colour onColour)
{
    auto bg = active ? onColour : kBtnOff;
    btn.setColour(juce::TextButton::buttonColourId,   bg);
    btn.setColour(juce::TextButton::buttonOnColourId, onColour);
    btn.setColour(juce::TextButton::textColourOffId,  active ? juce::Colours::white : juce::Colours::white.withAlpha(0.5f));
    btn.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
}

// ─────────────────────────────────────────────────────────────────────────────
// ChannelStrip
// ─────────────────────────────────────────────────────────────────────────────

ChannelStrip::ChannelStrip()
{
    fader.setSliderStyle(juce::Slider::LinearVertical);
    fader.setRange(-60.0, 12.0, 0.1);
    fader.setValue(0.0, juce::dontSendNotification);
    fader.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    applyFaderStyle(fader);
    addAndMakeVisible(fader);
    fader.addListener(this);

    panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setValue(0.0, juce::dontSendNotification);
    panSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    applyPanStyle(panSlider);
    addAndMakeVisible(panSlider);
    panSlider.addListener(this);

    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    {
        juce::Font f(juce::FontOptions(11.0f).withStyle("Bold"));
        nameLabel.setFont(f);
    }
    addAndMakeVisible(nameLabel);

    faderDbLabel.setJustificationType(juce::Justification::centred);
    faderDbLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.45f));
    {
        juce::Font f(juce::FontOptions(9.0f));
        faderDbLabel.setFont(f);
    }
    addAndMakeVisible(faderDbLabel);

    routingLabel.setText("Main", juce::dontSendNotification);
    routingLabel.setJustificationType(juce::Justification::centred);
    routingLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.35f));
    {
        juce::Font f(juce::FontOptions(9.0f));
        routingLabel.setFont(f);
    }
    addAndMakeVisible(routingLabel);

    for (auto* btn : { &muteBtn, &soloBtn, &armBtn })
    {
        btn->setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        addAndMakeVisible(*btn);
        btn->addListener(this);
    }

    styleButton(muteBtn, false, kMuteBtnOn);
    styleButton(soloBtn, false, kSoloBtnOn);
    styleButton(armBtn,  false, kArmBtnOn);
}

ChannelStrip::~ChannelStrip()
{
    fader.removeListener(this);
    panSlider.removeListener(this);
    muteBtn.removeListener(this);
    soloBtn.removeListener(this);
    armBtn.removeListener(this);
}

void ChannelStrip::updateFromTrack(const NovaStudio::Track& track)
{
    nameLabel.setText(track.name, juce::dontSendNotification);
    fader.setValue(track.volumeDb, juce::dontSendNotification);
    panSlider.setValue(track.pan, juce::dontSendNotification);

    styleButton(muteBtn, track.muted, kMuteBtnOn);
    styleButton(soloBtn, track.solo,  kSoloBtnOn);
    styleButton(armBtn,  track.armed, kArmBtnOn);

    armBtn.setVisible(true);
    faderDbLabel.setText(juce::String(track.volumeDb, 1) + " dB",
                         juce::dontSendNotification);
    repaint();
}

void ChannelStrip::updateAsMaster()
{
    nameLabel.setText("MASTER", juce::dontSendNotification);
    armBtn.setVisible(false);
    routingLabel.setText("Out", juce::dontSendNotification);
    faderDbLabel.setText(juce::String(fader.getValue(), 1) + " dB",
                         juce::dontSendNotification);
    repaint();
}

void ChannelStrip::updateAsAux(const juce::String& label)
{
    nameLabel.setText(label, juce::dontSendNotification);
    armBtn.setVisible(false);
    routingLabel.setText("Bus", juce::dontSendNotification);
    repaint();
}

void ChannelStrip::setMeterLevel(float left, float right)
{
    meterLeft  = juce::jlimit(0.0f, 1.0f, left);
    meterRight = juce::jlimit(0.0f, 1.0f, right);
    repaint();
}

void ChannelStrip::sliderValueChanged(juce::Slider* s)
{
    if (s == &fader)
    {
        auto db = (float)fader.getValue();
        faderDbLabel.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        if (onVolumeChanged) onVolumeChanged(db);
    }
    else if (s == &panSlider)
    {
        if (onPanChanged) onPanChanged((float)panSlider.getValue());
    }
}

void ChannelStrip::buttonClicked(juce::Button* b)
{
    if (b == &muteBtn)
    {
        bool nowMuted = !muteBtn.getToggleState();
        muteBtn.setToggleState(nowMuted, juce::dontSendNotification);
        styleButton(muteBtn, nowMuted, kMuteBtnOn);
        if (onMuteToggled) onMuteToggled(nowMuted);
    }
    else if (b == &soloBtn)
    {
        bool nowSolo = !soloBtn.getToggleState();
        soloBtn.setToggleState(nowSolo, juce::dontSendNotification);
        styleButton(soloBtn, nowSolo, kSoloBtnOn);
        if (onSoloToggled) onSoloToggled(nowSolo);
    }
    else if (b == &armBtn)
    {
        bool nowArmed = !armBtn.getToggleState();
        armBtn.setToggleState(nowArmed, juce::dontSendNotification);
        styleButton(armBtn, nowArmed, kArmBtnOn);
        if (onArmToggled) onArmToggled(nowArmed);
    }
}

void ChannelStrip::mouseDown(const juce::MouseEvent& e)
{
    // Determine insert slot area (mirrors paint() layout)
    auto r = getLocalBounds();
    r.removeFromBottom(22);   // name
    r.removeFromBottom(26);   // M/S/A
    r.removeFromBottom(16);   // dB label
    r.removeFromBottom(130);  // fader
    r.removeFromBottom(28);   // pan
    r.removeFromBottom(16);   // routing label
    r.removeFromBottom(76);   // meter
    r.removeFromBottom(40);   // sends
    // r is now insertsArea
    auto slotArea = r.reduced(4, 2);

    const int slotH = 16;
    const int gap   = 3;
    for (int i = 0; i < 3; ++i)
    {
        auto slot = slotArea.removeFromTop(slotH);
        slotArea.removeFromTop(gap);
        if (slot.contains(e.getPosition()))
        {
            if (onInsertClicked) onInsertClicked(i);
            return;
        }
    }
}

void ChannelStrip::drawInsertSlots(juce::Graphics& g, juce::Rectangle<int> area) const
{
    const int slotH = 16;
    const int gap   = 3;
    auto r = area.reduced(4, 2);

    for (int i = 0; i < 3; ++i)
    {
        auto slot = r.removeFromTop(slotH);
        r.removeFromTop(gap);
        g.setColour(kSlotBg);
        g.fillRoundedRectangle(slot.toFloat(), 3.0f);
        g.setColour(kSlotEdge);
        g.drawRoundedRectangle(slot.toFloat().reduced(0.5f), 3.0f, 0.8f);
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        {
            juce::Font f(juce::FontOptions(8.5f));
            g.setFont(f);
        }
        g.drawText("-- empty --", slot, juce::Justification::centred);
    }
}

void ChannelStrip::drawSendSlots(juce::Graphics& g, juce::Rectangle<int> area) const
{
    const int slotH = 15;
    const int gap   = 3;
    auto r = area.reduced(4, 2);

    for (int i = 0; i < 2; ++i)
    {
        auto slot = r.removeFromTop(slotH);
        r.removeFromTop(gap);
        g.setColour(kSlotBg);
        g.fillRoundedRectangle(slot.toFloat(), 3.0f);
        g.setColour(kSlotEdge);
        g.drawRoundedRectangle(slot.toFloat().reduced(0.5f), 3.0f, 0.8f);
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        {
            juce::Font f(juce::FontOptions(8.5f));
            g.setFont(f);
        }
        g.drawText("Send " + juce::String(i + 1), slot, juce::Justification::centred);
    }
}

void ChannelStrip::drawMeter(juce::Graphics& g, juce::Rectangle<int> area) const
{
    auto r = area.reduced(6, 2);
    const int barW = (r.getWidth() - 3) / 2;

    for (int ch = 0; ch < 2; ++ch)
    {
        float level = (ch == 0) ? meterLeft : meterRight;
        auto bar = juce::Rectangle<int>(r.getX() + ch * (barW + 3), r.getY(),
                                         barW, r.getHeight());

        g.setColour(kFaderTrack);
        g.fillRoundedRectangle(bar.toFloat(), 2.0f);

        if (level > 0.001f)
        {
            int fillH = juce::roundToInt(bar.getHeight() * level);
            auto fill = bar.removeFromBottom(fillH);

            juce::ColourGradient grad(kMeterGreen, 0.0f, (float)bar.getBottom(),
                                     kMeterRed,   0.0f, (float)bar.getY(), false);
            grad.addColour(0.75, kMeterYellow);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fill.toFloat(), 2.0f);
        }
    }

    // Clip indicator at top
    auto clipArea = juce::Rectangle<int>(r.getX(), r.getY(), r.getWidth(), 4);
    g.setColour((meterLeft > 0.95f || meterRight > 0.95f)
                    ? kMeterRed
                    : juce::Colour::fromRGB(40, 44, 58));
    g.fillRoundedRectangle(clipArea.toFloat(), 1.5f);
}

void ChannelStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.5f);

    // Strip background
    g.setColour(kStripBg);
    g.fillRoundedRectangle(bounds, 8.0f);

    // Edge colour depends on type
    juce::Colour edge = isMaster ? kMasterEdge
                       : isAux  ? kAuxEdge
                                : kStripEdge;
    g.setColour(edge);
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    // Top accent line
    g.setColour(edge.withAlpha(0.6f));
    g.fillRect(juce::Rectangle<float>(bounds.getX() + 8.0f, bounds.getY(),
                                       bounds.getWidth() - 16.0f, 2.0f));

    // Section labels
    auto r = getLocalBounds();
    r.removeFromBottom(22);   // name
    r.removeFromBottom(26);   // M/S/A
    r.removeFromBottom(16);   // dB label
    r.removeFromBottom(130);  // fader
    r.removeFromBottom(28);   // pan
    r.removeFromBottom(16);   // routing label

    auto meterArea   = r.removeFromBottom(76);
    auto sendsArea   = r.removeFromBottom(40);
    auto insertsArea = r;

    drawInsertSlots(g, insertsArea);
    drawSendSlots(g, sendsArea);
    drawMeter(g, meterArea);

    // Section separator lines
    g.setColour(kStripEdge);
    for (int y : { meterArea.getY(), sendsArea.getY(), insertsArea.getBottom() })
        g.drawHorizontalLine(y, bounds.getX() + 6.0f, bounds.getRight() - 6.0f);
}

void ChannelStrip::resized()
{
    auto r = getLocalBounds().reduced(3, 2);

    nameLabel.setBounds(r.removeFromBottom(22));
    auto msaRow = r.removeFromBottom(26).reduced(2, 3);
    {
        const int btnW = isMaster ? msaRow.getWidth() / 2
                                  : msaRow.getWidth() / 3;
        muteBtn.setBounds(msaRow.removeFromLeft(btnW));
        soloBtn.setBounds(msaRow.removeFromLeft(btnW));
        if (!isMaster)
            armBtn.setBounds(msaRow);
    }

    faderDbLabel.setBounds(r.removeFromBottom(16));
    fader.setBounds(r.removeFromBottom(130).reduced(4, 2));
    panSlider.setBounds(r.removeFromBottom(28).reduced(4, 4));
    routingLabel.setBounds(r.removeFromBottom(16));
    // remaining area handled by paint (meter, inserts, sends)
}

// ─────────────────────────────────────────────────────────────────────────────
// MixerWindow
// ─────────────────────────────────────────────────────────────────────────────

MixerWindow::MixerWindow(NovaStudio::StudioAudioEngine& engineRef)
    : engine(engineRef)
{
    viewport.setScrollBarsShown(false, true);
    viewport.setViewedComponent(&stripsContainer, false);
    addAndMakeVisible(viewport);

    buildStrips();
    startTimerHz(30);
}

MixerWindow::~MixerWindow()
{
    stopTimer();
}

void MixerWindow::refresh()
{
    buildStrips();
    resized();
    repaint();
}

void MixerWindow::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refresh();
}

void MixerWindow::buildStrips()
{
    // Detach old strips from container
    stripsContainer.removeAllChildren();
    trackStrips.clear();
    auxStrips.clear();
    masterStrip.reset();

    const auto& session = engine.getSession();
    const int numTracks = session.getNumTracks();

    // ── Track strips ──────────────────────────────────────────────────────
    for (int i = 0; i < numTracks; ++i)
    {
        auto* strip = trackStrips.add(new ChannelStrip());
        strip->setTrackIndex(i);
        strip->updateFromTrack(session.getTrack(i));
        stripsContainer.addAndMakeVisible(strip);

        strip->onVolumeChanged = [this, i](float db)
        {
            engine.setTrackVolume(i, db);
        };
        strip->onPanChanged = [this, i](float pan)
        {
            engine.setTrackPan(i, pan);
        };
        strip->onMuteToggled = [this, i](bool muted)
        {
            engine.setTrackMute(i, muted);
        };
        strip->onSoloToggled = [this, i](bool solo)
        {
            engine.setTrackSolo(i, solo);
        };
        strip->onArmToggled = [this, i](bool armed)
        {
            engine.setTrackArm(i, armed);
        };
        strip->onInsertClicked = [this, i](int slot)
        {
            openPluginEditor(i, slot);
        };
    }

    // ── Aux strips (placeholders) ─────────────────────────────────────────
    for (int i = 0; i < 2; ++i)
    {
        auto* strip = auxStrips.add(new ChannelStrip());
        strip->setAux(true);
        strip->updateAsAux("Aux " + juce::String(i + 1));
        stripsContainer.addAndMakeVisible(strip);
    }

    // ── Master strip ──────────────────────────────────────────────────────
    masterStrip = std::make_unique<ChannelStrip>();
    masterStrip->setMaster(true);
    masterStrip->updateAsMaster();
    stripsContainer.addAndMakeVisible(*masterStrip);
}

void MixerWindow::timerCallback()
{
    static constexpr float kDecay = 0.85f; // ballistic decay per 30Hz tick

    for (int i = 0; i < trackStrips.size(); ++i)
    {
        float L = engine.getTrackPeakLevel(i, 0);
        float R = engine.getTrackPeakLevel(i, 1);
        // Smooth decay on the display strip
        auto* strip = trackStrips[i];
        float prevL = strip->getMeterLeft();
        float prevR = strip->getMeterRight();
        strip->setMeterLevel(juce::jmax(L, prevL * kDecay),
                             juce::jmax(R, prevR * kDecay));
    }

    for (auto* strip : auxStrips)
        strip->setMeterLevel(strip->getMeterLeft() * kDecay, strip->getMeterRight() * kDecay);

    if (masterStrip)
        masterStrip->setMeterLevel(masterStrip->getMeterLeft() * kDecay, masterStrip->getMeterRight() * kDecay);
}

void MixerWindow::openPluginEditor(int trackIndex, int pluginSlot)
{
    auto* instance = engine.getTrackPlugin(trackIndex, pluginSlot);
    if (instance == nullptr)
        return;

    // Bring existing editor to front if already open
    for (auto* win : editorWindows)
    {
        if (win->isVisible())
        {
            win->toFront(true);
            return;
        }
    }

    editorWindows.add(new PluginEditorWindow(*instance, trackIndex, pluginSlot));
}

void MixerWindow::paint(juce::Graphics& g)
{
    // Window background
    g.fillAll(Theme::background());
    g.setColour(Theme::panelBackground());
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 10.0f);

    // Header bar
    auto header = getLocalBounds().removeFromTop(36).reduced(4, 0);
    g.setColour(juce::Colour::fromRGB(22, 24, 34));
    g.fillRoundedRectangle(header.toFloat(), 6.0f);
    g.setColour(Theme::glowAccent().withAlpha(0.25f));
    g.drawRoundedRectangle(header.toFloat().reduced(0.5f), 6.0f, 0.8f);

    g.setColour(juce::Colours::white.withAlpha(0.7f));
    {
        juce::Font f(juce::FontOptions(13.0f).withStyle("Bold"));
        g.setFont(f);
    }
    g.drawText("MIXER", header.reduced(12, 0), juce::Justification::centredLeft);

    // Track count info
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    {
        juce::Font f(juce::FontOptions(11.0f));
        g.setFont(f);
    }
    g.drawText(juce::String(engine.getTrackCount()) + " tracks",
               header.reduced(12, 0), juce::Justification::centredRight);
}

void MixerWindow::resized()
{
    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(40); // header

    const int numTrack  = trackStrips.size();
    const int numAux    = auxStrips.size();
    const int totalStrips = numTrack + numAux + 1; // +1 master

    const int trackWidth  = ChannelStrip::kWidth;
    const int masterWidth = ChannelStrip::kMasterWidth;
    const int gap         = 3;
    const int totalW = numTrack * (trackWidth + gap)
                     + numAux   * (trackWidth + gap)
                     + masterWidth + gap;

    stripsContainer.setBounds(0, 0,
                               juce::jmax(totalW, area.getWidth()),
                               ChannelStrip::kHeight + 4);
    viewport.setBounds(area);

    int x = 2;
    for (auto* strip : trackStrips)
    {
        strip->setBounds(x, 2, trackWidth, ChannelStrip::kHeight);
        x += trackWidth + gap;
    }
    // Thin separator before aux
    x += 6;
    for (auto* strip : auxStrips)
    {
        strip->setBounds(x, 2, trackWidth, ChannelStrip::kHeight);
        x += trackWidth + gap;
    }
    // Master always flush right
    const int masterX = juce::jmax(x + 6,
                                   stripsContainer.getWidth() - masterWidth - 4);
    if (masterStrip)
        masterStrip->setBounds(masterX, 2, masterWidth, ChannelStrip::kHeight);
}
