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
static constexpr int kDropArrowW   = 14; // width of the ▼ dropdown arrow on loaded slots
static juce::Colour kMasterEdge    = juce::Colour::fromRGB(180, 130,  50);
static juce::Colour kAuxEdge       = juce::Colour::fromRGB( 60, 110, 180);

// ─────────────────────────────────────────────────────────────────────────────
// Pro Fader LookAndFeel  (SSL / Pro Tools style)
// ─────────────────────────────────────────────────────────────────────────────

class ProFaderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minPos*/, float /*maxPos*/,
                          const juce::Slider::SliderStyle, juce::Slider&) override
    {
        const float cx     = (float)x + width * 0.5f;
        const float top    = (float)y + 4.0f;
        const float bottom = (float)(y + height) - 4.0f;
        const float range  = bottom - top;

        // dB → Y: +12 at top, 0 dB at 75% from top, -60 at bottom
        auto dbToY = [&](float db) -> float {
            // piecewise: +12→0%, 0→75%, -60→100%
            if (db >= 0.0f)
                return top + (12.0f - db) / 12.0f * 0.75f * range;
            else
                return top + 0.75f * range + (-db) / 60.0f * 0.25f * range;
        };

        // ── dB scale labels ──────────────────────────────────────────────────
        const float labelX = (float)x;
        const float labelW = 12.0f;
        static const float kDbMarks[] = { 12.f, 6.f, 0.f, -6.f, -12.f, -18.f, -24.f, -36.f, -48.f, -60.f };
        g.setFont(juce::FontOptions(7.0f));
        for (float db : kDbMarks)
        {
            float fy = dbToY(db);
            // tick
            g.setColour(juce::Colours::white.withAlpha(0.22f));
            g.drawHorizontalLine(juce::roundToInt(fy), labelX + labelW, labelX + labelW + 4.0f);
            // label
            g.setColour(juce::Colours::white.withAlpha(0.30f));
            juce::String lbl = (db > 0) ? juce::String("+") + juce::String((int)db)
                                         : juce::String((int)db);
            g.drawText(lbl, juce::Rectangle<float>(labelX, fy - 4.0f, labelW, 9.0f),
                       juce::Justification::centredRight, false);
        }

        // ── Recessed trough ──────────────────────────────────────────────────
        const float troughX = cx - 3.5f;
        const float troughW = 7.0f;
        juce::Rectangle<float> trough(troughX, top, troughW, range);

        // outer shadow
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(trough.expanded(1.2f, 0.5f), 3.0f);
        // groove fill (very dark)
        g.setColour(juce::Colour::fromRGB(14, 16, 22));
        g.fillRoundedRectangle(trough, 2.5f);
        // inner top highlight (recessed effect)
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawRoundedRectangle(trough.reduced(0.5f), 2.5f, 0.8f);
        // subtle inner glow at bottom
        g.setColour(juce::Colours::white.withAlpha(0.04f));
        g.fillRoundedRectangle(trough.withTrimmedTop(trough.getHeight() * 0.7f), 2.0f);

        // unity (0 dB) tick line on trough
        const float unityY = dbToY(0.0f);
        g.setColour(juce::Colour::fromRGBA(140, 100, 220, 160));
        g.drawHorizontalLine(juce::roundToInt(unityY), troughX - 2.0f, troughX + troughW + 2.0f);

        // ── Fader cap ────────────────────────────────────────────────────────
        const float capH = 22.0f;
        const float capW = (float)width - 10.0f;
        const float capX = (float)x + 5.0f;
        const float capY = sliderPos - capH * 0.5f;

        // drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.50f));
        g.fillRoundedRectangle(capX + 1.5f, capY + 2.5f, capW, capH, 3.0f);

        // metallic gradient body — top-to-bottom (light top edge → dark body → light bottom edge)
        juce::ColourGradient metalGrad(
            juce::Colour::fromRGB(235, 238, 248), capX, capY,
            juce::Colour::fromRGB(130, 133, 148), capX, capY + capH,
            false);
        metalGrad.addColour(0.08, juce::Colour::fromRGB(215, 218, 230));
        metalGrad.addColour(0.40, juce::Colour::fromRGB(175, 178, 192));
        metalGrad.addColour(0.55, juce::Colour::fromRGB(155, 158, 173));
        metalGrad.addColour(0.88, juce::Colour::fromRGB(140, 143, 158));
        g.setGradientFill(metalGrad);
        g.fillRoundedRectangle(capX, capY, capW, capH, 3.0f);

        // horizontal centre highlight strip (the chrome "ridge" on SSL-style caps)
        float ridgeY = capY + capH * 0.35f;
        juce::ColourGradient ridgeGrad(
            juce::Colours::white.withAlpha(0.0f), capX, ridgeY,
            juce::Colours::white.withAlpha(0.0f), capX, ridgeY + 6.0f,
            false);
        ridgeGrad.addColour(0.5, juce::Colours::white.withAlpha(0.38f));
        g.setGradientFill(ridgeGrad);
        g.fillRoundedRectangle(capX + 2.0f, ridgeY, capW - 4.0f, 6.0f, 1.5f);

        // top bevel (light)
        juce::ColourGradient topBevel(
            juce::Colours::white.withAlpha(0.55f), capX, capY,
            juce::Colours::white.withAlpha(0.0f),  capX, capY + 5.0f,
            false);
        g.setGradientFill(topBevel);
        g.fillRoundedRectangle(capX, capY, capW, 5.0f, 2.0f);

        // bottom shadow
        juce::ColourGradient botBevel(
            juce::Colours::black.withAlpha(0.0f),  capX, capY + capH - 5.0f,
            juce::Colours::black.withAlpha(0.35f), capX, capY + capH,
            false);
        g.setGradientFill(botBevel);
        g.fillRoundedRectangle(capX, capY + capH - 5.0f, capW, 5.0f, 2.0f);

        // outline
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawRoundedRectangle(capX + 0.5f, capY + 0.5f, capW - 1.0f, capH - 1.0f, 3.0f, 0.8f);

        // 3 grip lines in the centre
        const float midX = capX + capW * 0.5f;
        const float midY = capY + capH * 0.5f;
        const float gripW = capW * 0.4f;
        g.setColour(juce::Colours::black.withAlpha(0.30f));
        for (int k = -1; k <= 1; ++k)
        {
            float gy = midY + (float)k * 3.0f;
            g.drawHorizontalLine(juce::roundToInt(gy), midX - gripW * 0.5f, midX + gripW * 0.5f);
        }
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        for (int k = -1; k <= 1; ++k)
        {
            float gy = midY + (float)k * 3.0f - 0.5f;
            g.drawHorizontalLine(juce::roundToInt(gy), midX - gripW * 0.5f, midX + gripW * 0.5f);
        }
    }
};

static ProFaderLookAndFeel sProFaderLF;

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
    fader.setLookAndFeel(&sProFaderLF);
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
    fader.setLookAndFeel(nullptr);
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
    auto slotArea = r.reduced(4, 2);

    const int slotH = 16;
    const int gap   = 3;

    auto handleSlotClick = [&](int i) -> bool
    {
        auto slot = slotArea.removeFromTop(slotH);
        slotArea.removeFromTop(gap);
        if (!slot.contains(e.getPosition()))
            return false;

        bool hasPlugin = insertSlotNames[i].isNotEmpty();
        if (!hasPlugin)
        {
            if (onInsertClicked) onInsertClicked(i);
            return true;
        }

        bool onArrow = (e.position.x >= slot.getRight() - kDropArrowW);
        if (onArrow)
        {
            const int slotCapture = i;
            juce::PopupMenu menu;
            menu.addItem(1, "Open Editor");
            menu.addItem(2, "Change Plugin...");
            menu.addSeparator();
            menu.addItem(3, "Remove");

            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                [this, slotCapture](int result)
                {
                    if (result == 1 && onInsertClicked)           onInsertClicked(slotCapture);
                    else if (result == 2 && onInsertChangePlugin) onInsertChangePlugin(slotCapture);
                    else if (result == 3 && onInsertRemovePlugin) onInsertRemovePlugin(slotCapture);
                });
        }
        else
        {
            if (onInsertClicked) onInsertClicked(i);
        }
        return true;
    };

    // Hit-test slots 0..5 (always visible)
    for (int i = 0; i < 6; ++i)
        if (handleSlotClick(i)) return;

    // Hit-test expand/collapse button
    {
        auto expandRow = slotArea.removeFromTop(slotH);
        if (expandRow.contains(e.getPosition()))
        {
            insertsExpanded = !insertsExpanded;
            repaint();
            return;
        }
    }

    // Hit-test slots 6..8 (only if expanded)
    if (insertsExpanded)
    {
        for (int i = 6; i < 9; ++i)
            if (handleSlotClick(i)) return;
    }
}

void ChannelStrip::drawInsertSlots(juce::Graphics& g, juce::Rectangle<int> area) const
{
    const int slotH = 16;
    const int gap   = 3;
    auto r = area.reduced(4, 2);

    const int visibleSlots = insertsExpanded ? 9 : 6;

    for (int i = 0; i < visibleSlots; ++i)
    {
        auto slot = r.removeFromTop(slotH);
        r.removeFromTop(gap);

        bool hasPlugin = insertSlotNames[i].isNotEmpty();
        g.setColour(hasPlugin ? juce::Colour::fromRGB(30, 38, 70) : kSlotBg);
        g.fillRoundedRectangle(slot.toFloat(), 3.0f);
        g.setColour(hasPlugin ? juce::Colour::fromRGBA(90, 120, 255, 140) : kSlotEdge);
        g.drawRoundedRectangle(slot.toFloat().reduced(0.5f), 3.0f, 0.8f);

        g.setFont(juce::FontOptions(8.5f));
        if (hasPlugin)
        {
            // Plugin name (leaves room for ▼ on right)
            auto textArea = slot.withTrimmedRight(kDropArrowW);
            g.setColour(juce::Colours::white.withAlpha(0.88f));
            g.drawText(insertSlotNames[i], textArea.reduced(3, 0), juce::Justification::centredLeft, true);

            // ▼ dropdown arrow region
            auto arrowArea = slot.removeFromRight(kDropArrowW);
            g.setColour(juce::Colour::fromRGBA(90, 120, 255, 80));
            g.fillRect(arrowArea.withTrimmedLeft(1));
            g.setColour(juce::Colours::white.withAlpha(0.55f));
            // Small triangle
            const float ax = arrowArea.getCentreX();
            const float ay = arrowArea.getCentreY() - 1.0f;
            juce::Path tri;
            tri.addTriangle(ax - 3.0f, ay - 1.0f, ax + 3.0f, ay - 1.0f, ax, ay + 3.0f);
            g.fillPath(tri);
        }
        else
        {
            g.setColour(juce::Colours::white.withAlpha(0.18f));
            g.drawText("-- empty --", slot, juce::Justification::centred);
        }
    }

    // Expand/collapse button
    auto expandRow = r.removeFromTop(16);
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.setFont(juce::FontOptions(8.0f));
    g.drawText(insertsExpanded ? u8"▲ less" : u8"▼ +3 slots", expandRow, juce::Justification::centred);
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
    auto r = area.reduced(5, 2);

    // Clip indicator row at top
    auto clipRow = r.removeFromTop(5);
    bool isClipping = (meterLeft > 0.95f || meterRight > 0.95f);
    g.setColour(isClipping ? kMeterRed : juce::Colour::fromRGB(40, 44, 58));
    g.fillRoundedRectangle(clipRow.toFloat(), 1.5f);
    r.removeFromTop(2);

    const int barW = (r.getWidth() - 3) / 2;
    const int segH = 3;
    const int segGap = 1;
    const int segStep = segH + segGap;
    const int totalH = r.getHeight();
    const int numSegs = totalH / segStep;

    for (int ch = 0; ch < 2; ++ch)
    {
        float level = (ch == 0) ? meterLeft : meterRight;
        int litSegs = juce::roundToInt(level * (float)numSegs);

        int barX = r.getX() + ch * (barW + 3);
        int barBottom = r.getBottom();

        for (int s = 0; s < numSegs; ++s)
        {
            // s=0 is bottom, s=numSegs-1 is top
            float frac = (float)s / (float)(numSegs - 1);  // 0=bottom,1=top
            int segY = barBottom - (s + 1) * segStep + segGap;
            juce::Rectangle<float> seg((float)barX, (float)segY, (float)barW, (float)segH);

            bool lit = (s < litSegs);
            juce::Colour segColour;
            if (!lit)
            {
                segColour = juce::Colour::fromRGB(22, 25, 34);
            }
            else if (frac > 0.88f)
                segColour = kMeterRed;
            else if (frac > 0.70f)
                segColour = kMeterYellow;
            else
                segColour = kMeterGreen;

            // Lit segments get a subtle inner highlight
            g.setColour(segColour);
            g.fillRoundedRectangle(seg, 1.0f);
            if (lit)
            {
                g.setColour(segColour.brighter(0.35f).withAlpha(0.5f));
                g.fillRoundedRectangle(seg.withHeight(1.0f), 0.5f);
            }
        }
    }
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
    stripsContainer.removeAllChildren();
    trackStrips.clear();
    auxStrips.clear();
    masterStrip.reset();

    const auto& session = engine.getSession();
    const int numTracks = session.getNumTracks();

    // Helper: wire all plugin-insert callbacks for a strip at engine track index i
    auto wireInserts = [this](ChannelStrip* strip, int i)
    {
        strip->onInsertClicked = [this, i](int slot)
        {
            if (engine.getTrackPlugin(i, slot) != nullptr)
                openPluginEditor(i, slot);
            else
                openPluginBrowser(i, slot);
        };
        strip->onInsertChangePlugin = [this, i](int slot)
        {
            engine.removePluginFromTrack(i, slot);
            refreshInsertSlotNames();
            openPluginBrowser(i, slot);
        };
        strip->onInsertRemovePlugin = [this, i](int slot)
        {
            engine.removePluginFromTrack(i, slot);
            refreshInsertSlotNames();
            repaint();
        };
    };

    // ── Track strips (Audio / MIDI / Instrument) ──────────────────────────
    for (int i = 0; i < numTracks; ++i)
    {
        const auto& track = session.getTrack(i);
        if (track.type == NovaStudio::TrackType::Aux || track.type == NovaStudio::TrackType::Master)
            continue;

        auto* strip = trackStrips.add(new ChannelStrip());
        strip->setTrackIndex(i);
        strip->updateFromTrack(track);
        stripsContainer.addAndMakeVisible(strip);

        strip->onVolumeChanged = [this, i](float db)  { engine.setTrackVolume(i, db); };
        strip->onPanChanged    = [this, i](float pan)  { engine.setTrackPan(i, pan); };
        strip->onMuteToggled   = [this, i](bool muted) { engine.setTrackMute(i, muted); };
        strip->onSoloToggled   = [this, i](bool solo)  { engine.setTrackSolo(i, solo); };
        strip->onArmToggled    = [this, i](bool armed)  { engine.setTrackArm(i, armed); };
        wireInserts(strip, i);
    }

    // ── Aux strips (engine tracks of type Aux) ────────────────────────────
    for (int i = 0; i < numTracks; ++i)
    {
        const auto& track = session.getTrack(i);
        if (track.type != NovaStudio::TrackType::Aux)
            continue;

        auto* strip = auxStrips.add(new ChannelStrip());
        strip->setTrackIndex(i);
        strip->setAux(true);
        strip->updateAsAux(track.name);
        stripsContainer.addAndMakeVisible(strip);

        strip->onVolumeChanged = [this, i](float db)  { engine.setTrackVolume(i, db); };
        strip->onPanChanged    = [this, i](float pan)  { engine.setTrackPan(i, pan); };
        strip->onMuteToggled   = [this, i](bool muted) { engine.setTrackMute(i, muted); };
        strip->onSoloToggled   = [this, i](bool solo)  { engine.setTrackSolo(i, solo); };
        wireInserts(strip, i);
    }

    // ── Master strip ──────────────────────────────────────────────────────
    masterStrip = std::make_unique<ChannelStrip>();
    masterStrip->setMaster(true);
    masterStrip->updateAsMaster();
    stripsContainer.addAndMakeVisible(*masterStrip);

    refreshInsertSlotNames();
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

void MixerWindow::refreshInsertSlotNames()
{
    auto refresh = [this](juce::OwnedArray<ChannelStrip>& strips)
    {
        for (auto* strip : strips)
        {
            const int t = strip->getTrackIndex();
            for (int s = 0; s < 9; ++s)
            {
                auto* plugin = engine.getTrackPlugin(t, s);
                strip->setInsertSlotName(s, plugin ? plugin->getName() : juce::String());
            }
        }
    };
    refresh(trackStrips);
    refresh(auxStrips);
}

void MixerWindow::openPluginBrowser(int trackIndex, int slotIndex)
{
    if (pluginBrowserWindow != nullptr && pluginBrowserWindow->isVisible())
    {
        // Retarget existing browser window
        pluginBrowserWindow->getPanel()->setTargetTrackAndSlot(trackIndex, slotIndex);
        pluginBrowserWindow->toFront(true);
        return;
    }

    pluginBrowserWindow = std::make_unique<PluginBrowserWindow>(
        engine, trackIndex, slotIndex,
        [this](int track, int slot, const juce::String& name)
        {
            // Fired after successful load — update strip display then open editor
            if (isPositiveAndBelow(track, trackStrips.size()))
                trackStrips[track]->setInsertSlotName(slot, name);
            openPluginEditor(track, slot);
        });
}

void MixerWindow::openPluginEditor(int trackIndex, int pluginSlot)
{
    auto* instance = engine.getTrackPlugin(trackIndex, pluginSlot);
    if (instance == nullptr) return;

    // Find existing editor for this exact track+slot
    for (auto* win : editorWindows)
    {
        if (win->getTrackIndex() == trackIndex && win->getPluginSlot() == pluginSlot)
        {
            win->setVisible(true);
            win->toFront(true);
            return;
        }
    }

    // No existing window — create one
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
