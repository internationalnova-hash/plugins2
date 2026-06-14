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
        const float cx     = (float)x + width  * 0.5f;
        const float top    = (float)y + 6.0f;
        const float bottom = (float)(y + height) - 6.0f;
        const float range  = bottom - top;

        // dB scale Y mapping: +12→top, 0 dB at 75% down, -∞ at bottom
        auto dbToY = [&](float db) -> float {
            if (db >= 0.0f)
                return top + (12.0f - db) / 12.0f * 0.75f * range;
            return top + 0.75f * range + juce::jmin((-db) / 60.0f, 1.0f) * 0.25f * range;
        };

        // ── dB scale (right side ticks + labels) ────────────────────────────
        static const float kDbMarks[] = { 12.f, 6.f, 0.f, -6.f, -12.f, -18.f, -24.f, -36.f, -48.f };
        const float tickX  = cx + 5.0f;   // right of trough
        const float lblX   = tickX + 5.0f;
        g.setFont(juce::FontOptions(7.0f));
        for (float db : kDbMarks)
        {
            float fy = dbToY(db);
            g.setColour(juce::Colours::white.withAlpha(db == 0.0f ? 0.45f : 0.18f));
            g.fillRect(tickX, fy - 0.5f, 4.0f, 1.0f);
            g.setColour(juce::Colours::white.withAlpha(0.30f));
            juce::String lbl = (db > 0) ? "+" + juce::String((int)db) : juce::String((int)db);
            g.drawText(lbl, juce::Rectangle<float>(lblX, fy - 5.0f, 18.0f, 10.0f),
                       juce::Justification::centredLeft, false);
        }

        // ── Thin recessed trough (3 px wide, centred) ────────────────────────
        const float troughW = 4.0f;
        const float troughX = cx - troughW * 0.5f;
        juce::Rectangle<float> trough(troughX, top, troughW, range);

        // shadow behind trough
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.fillRoundedRectangle(trough.expanded(1.5f, 1.0f), 3.0f);
        // very dark groove
        g.setColour(juce::Colour::fromRGB(10, 12, 18));
        g.fillRoundedRectangle(trough, 2.0f);
        // recessed inner highlight on left edge
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(juce::Rectangle<float>(troughX, top, 1.0f, range), 1.0f);

        // 0 dB reference notch on left side of trough
        const float unityY = dbToY(0.0f);
        g.setColour(juce::Colour::fromRGBA(160, 120, 255, 180));
        g.fillRect(troughX - 4.0f, unityY - 0.5f, troughW + 8.0f, 1.5f);

        // ── Fader cap — SSL-style: slim pill on a thin trough ────────────────
        // Cap is ~52% of component width, 13 px tall — matches SSL proportions
        const float capW = (float)width * 0.52f;
        const float capH = 13.0f;
        const float capX = cx - capW * 0.5f;
        const float capY = sliderPos - capH * 0.5f;
        const float capR = 3.5f;   // corner radius

        // drop shadow (offset down-right)
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(capX + 1.0f, capY + 2.5f, capW, capH, capR);

        // SSL-style metallic body: left-to-right gradient simulating rounded 3-D form
        //   bright near-white left → mid grey centre → slightly lighter right edge
        juce::ColourGradient bodyGrad(
            juce::Colour::fromRGB(200, 204, 218), capX,         capY + capH * 0.5f,
            juce::Colour::fromRGB(115, 118, 133), capX + capW,  capY + capH * 0.5f,
            false);
        bodyGrad.addColour(0.12, juce::Colour::fromRGB(228, 232, 244));
        bodyGrad.addColour(0.38, juce::Colour::fromRGB(190, 193, 207));
        bodyGrad.addColour(0.62, juce::Colour::fromRGB(155, 158, 172));
        bodyGrad.addColour(0.88, juce::Colour::fromRGB(128, 131, 146));
        g.setGradientFill(bodyGrad);
        g.fillRoundedRectangle(capX, capY, capW, capH, capR);

        // top-edge bevel highlight
        juce::ColourGradient topBevel(
            juce::Colours::white.withAlpha(0.60f), capX, capY,
            juce::Colours::white.withAlpha(0.0f),  capX, capY + 3.5f,
            false);
        g.setGradientFill(topBevel);
        g.fillRoundedRectangle(capX, capY, capW, 3.5f, capR);

        // bottom-edge shadow
        juce::ColourGradient botBevel(
            juce::Colours::black.withAlpha(0.0f),  capX, capY + capH - 3.0f,
            juce::Colours::black.withAlpha(0.40f), capX, capY + capH,
            false);
        g.setGradientFill(botBevel);
        g.fillRoundedRectangle(capX, capY + capH - 3.0f, capW, 3.0f, capR);

        // outline
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawRoundedRectangle(capX + 0.5f, capY + 0.5f, capW - 1.0f, capH - 1.0f, capR, 0.7f);

        // Centre position indicator line (the white stripe SSL faders have in the middle of the cap)
        const float midY = capY + capH * 0.5f;
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRect(capX + 4.0f, midY,           capW - 8.0f, 1.5f);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.fillRect(capX + 4.0f, midY - 0.5f,   capW - 8.0f, 1.0f);
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
    // ── Main fader ────────────────────────────────────────────────────────
    fader.setSliderStyle(juce::Slider::LinearVertical);
    fader.setRange(-60.0, 12.0, 0.1);
    fader.setValue(0.0, juce::dontSendNotification);
    fader.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    fader.setLookAndFeel(&sProFaderLF);
    applyFaderStyle(fader);
    addAndMakeVisible(fader);
    fader.addListener(this);

    // ── Pan — rotary knob ──────────────────────────────────────────────────
    panKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    panKnob.setRange(-1.0, 1.0, 0.01);
    panKnob.setValue(0.0, juce::dontSendNotification);
    panKnob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    panKnob.setColour(juce::Slider::rotarySliderFillColourId,    juce::Colour::fromRGB(100, 80, 200));
    panKnob.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB(30, 33, 46));
    panKnob.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    addAndMakeVisible(panKnob);
    panKnob.addListener(this);

    // ── Track name ─────────────────────────────────────────────────────────
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
    nameLabel.setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));
    addAndMakeVisible(nameLabel);

    // ── Fader dB readout ───────────────────────────────────────────────────
    faderDbLabel.setJustificationType(juce::Justification::centred);
    faderDbLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    faderDbLabel.setFont(juce::Font(juce::FontOptions(9.0f)));
    faderDbLabel.setText("0.0 dB", juce::dontSendNotification);
    addAndMakeVisible(faderDbLabel);

    // ── I/O labels (painted as hit targets in paint(); stored as strings only)
    inputLabel .setText("Input 1",  juce::dontSendNotification);
    outputLabel.setText("Main Out", juce::dontSendNotification);
    // Do NOT addAndMakeVisible — we paint them manually so mouseDown reaches us

    // ── M S R buttons ──────────────────────────────────────────────────────
    for (auto* btn : { &muteBtn, &soloBtn, &armBtn })
    {
        btn->setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        addAndMakeVisible(*btn);
        btn->addListener(this);
    }
    styleButton(muteBtn, false, kMuteBtnOn);
    styleButton(soloBtn, false, kSoloBtnOn);
    styleButton(armBtn,  false, kArmBtnOn);

    // ── PRE button ─────────────────────────────────────────────────────────
    preBtn.setColour(juce::TextButton::buttonColourId,   juce::Colour::fromRGB(38, 30, 70));
    preBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(90, 60, 200));
    preBtn.setColour(juce::TextButton::textColourOffId,  juce::Colours::white.withAlpha(0.6f));
    preBtn.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
    preBtn.setClickingTogglesState(true);
    addAndMakeVisible(preBtn);
    preBtn.addListener(this);

    // ── Read (automation) button ───────────────────────────────────────────
    readBtn.setColour(juce::TextButton::buttonColourId,  juce::Colour::fromRGB(20, 90, 20));
    readBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(80, 220, 80));
    addAndMakeVisible(readBtn);
    readBtn.addListener(this);

    // ── Send knobs (Verb / Delay) ──────────────────────────────────────────
    for (int i = 0; i < kNumSends; ++i)
    {
        auto& k = sendKnobs[i];
        k.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        k.setRange(-60.0, 0.0, 0.1);
        k.setValue(-20.0, juce::dontSendNotification);
        k.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        k.setColour(juce::Slider::rotarySliderFillColourId,    juce::Colour::fromRGB(70, 130, 220));
        k.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB(28, 30, 44));
        k.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        addAndMakeVisible(k);
        k.addListener(this);

        auto& lbl = sendDbLabels[i];
        lbl.setFont(juce::Font(juce::FontOptions(7.5f)));
        lbl.setJustificationType(juce::Justification::centredLeft);
        lbl.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.55f));
        lbl.setText("-20.0", juce::dontSendNotification);
        addAndMakeVisible(lbl);

        // sendBusNames[i] stays empty — user assigns via click
    }
}

ChannelStrip::~ChannelStrip()
{
    fader.setLookAndFeel(nullptr);
    fader.removeListener(this);
    panKnob.removeListener(this);
    for (auto& k : sendKnobs) k.removeListener(this);
    for (auto* btn : { &muteBtn, &soloBtn, &armBtn, &preBtn, &readBtn })
        btn->removeListener(this);
}

void ChannelStrip::updateFromTrack(const NovaStudio::Track& track)
{
    nameLabel.setText(track.name, juce::dontSendNotification);
    fader.setValue(track.volumeDb, juce::dontSendNotification);
    panKnob.setValue(track.pan, juce::dontSendNotification);

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
    outputLabel.setText("Main Out", juce::dontSendNotification);
    faderDbLabel.setText(juce::String(fader.getValue(), 1) + " dB",
                         juce::dontSendNotification);
    repaint();
}

void ChannelStrip::updateAsAux(const juce::String& label)
{
    nameLabel.setText(label, juce::dontSendNotification);
    armBtn.setVisible(false);
    outputLabel.setText("Bus", juce::dontSendNotification);
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
    else if (s == &panKnob)
    {
        if (onPanChanged) onPanChanged((float)panKnob.getValue());
    }
    else
    {
        for (int i = 0; i < kNumSends; ++i)
        {
            if (s == &sendKnobs[i])
            {
                const float db = (float)sendKnobs[i].getValue();
                sendDbLabels[i].setText(juce::String(db, 1), juce::dontSendNotification);
                if (onSendChanged) onSendChanged(i, db);
                break;
            }
        }
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

void ChannelStrip::showIOPopup(bool isInput)
{
    juce::StringArray options;
    if (isInput && getAvailableInputs)
        options = getAvailableInputs();
    else if (!isInput && getAvailableOutputs)
        options = getAvailableOutputs();

    if (options.isEmpty())
    {
        if (isInput)
            for (int i = 1; i <= 8; ++i) options.add("Input " + juce::String(i));
        else
        {
            options.add("Main Out");
            options.add("Headphones");
        }
    }

    juce::PopupMenu menu;
    for (int i = 0; i < options.size(); ++i)
        menu.addItem(i + 1, options[i]);

    const auto& targetBounds = isInput ? inputLabelBounds : outputLabelBounds;
    const auto screenPt = localPointToGlobal(targetBounds.getBottomLeft());
    menu.showMenuAsync(juce::PopupMenu::Options()
                           .withTargetScreenArea(juce::Rectangle<int>(screenPt.x, screenPt.y, targetBounds.getWidth(), 1)),
        [this, isInput, options](int result)
        {
            if (result <= 0) return;
            const juce::String chosen = options[result - 1];
            if (isInput)
            {
                inputLabel.setText(chosen, juce::dontSendNotification);
                if (onInputChanged) onInputChanged(chosen);
            }
            else
            {
                outputLabel.setText(chosen, juce::dontSendNotification);
                if (onOutputChanged) onOutputChanged(chosen);
            }
            repaint();
        });
}

void ChannelStrip::mouseDown(const juce::MouseEvent& e)
{
    // I/O popup
    if (inputLabelBounds .contains(e.getPosition())) { showIOPopup(true);  return; }
    if (outputLabelBounds.contains(e.getPosition())) { showIOPopup(false); return; }

    // Send row click — assign or re-assign destination bus
    {
        auto sndArea = getSendsRect();
        for (int i = 0; i < kNumSends; ++i)
        {
            auto row = sndArea.removeFromTop(30);
            if (row.contains(e.getPosition()))
            {
                // Build popup: available outputs + "Remove" if already assigned
                juce::StringArray outputs;
                if (getAvailableOutputs) outputs = getAvailableOutputs();
                if (outputs.isEmpty()) { outputs.add("Main Out"); }

                juce::PopupMenu menu;
                for (int j = 0; j < outputs.size(); ++j)
                    menu.addItem(j + 1, outputs[j],
                                 /*enabled=*/true,
                                 /*ticked=*/outputs[j] == sendBusNames[i]);
                if (sendBusNames[i].isNotEmpty())
                {
                    menu.addSeparator();
                    menu.addItem(1000 + i, "Remove Send");
                }

                const int captureI = i;
                const auto rowScreenPt = localPointToGlobal(row.getBottomLeft());
                menu.showMenuAsync(juce::PopupMenu::Options()
                    .withTargetScreenArea(juce::Rectangle<int>(rowScreenPt.x, rowScreenPt.y, row.getWidth(), 1)),
                    [this, captureI, outputs](int result)
                    {
                        if (result == 1000 + captureI)
                        {
                            sendBusNames[captureI] = {};
                            resized();
                            repaint();
                        }
                        else if (result > 0 && result <= outputs.size())
                        {
                            sendBusNames[captureI] = outputs[result - 1];
                            resized();
                            repaint();
                        }
                    });
                return;
            }
        }
    }

    // Insert slot hit-test uses getInsertRect()
    auto slotArea = getInsertRect().reduced(4, 2);

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

            const auto slotScreenPt = localPointToGlobal(slot.getBottomLeft());
            menu.showMenuAsync(juce::PopupMenu::Options()
                .withTargetScreenArea(juce::Rectangle<int>(slotScreenPt.x, slotScreenPt.y, slot.getWidth(), 1)),
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

    // Hit-test slots 6..9 (only if expanded)
    if (insertsExpanded)
    {
        for (int i = 6; i < 10; ++i)
            if (handleSlotClick(i)) return;
    }
}

void ChannelStrip::drawInsertSlots(juce::Graphics& g, juce::Rectangle<int> area) const
{
    const int slotH = 16;
    const int gap   = 3;
    auto r = area.reduced(4, 2);

    const int visibleSlots = insertsExpanded ? 10 : 6;

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
            g.drawText("Empty Slot", slot, juce::Justification::centred);
        }
    }

    // Expand/collapse button
    auto expandRow = r.removeFromTop(16);
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.setFont(juce::FontOptions(8.0f));
    g.drawText(insertsExpanded ? u8"▲ less" : u8"▼ +4 slots", expandRow, juce::Justification::centred);
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
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const int W = getWidth();

    // Strip background
    g.setColour(kStripBg);
    g.fillRoundedRectangle(bounds, 6.0f);

    juce::Colour edge = isMaster ? kMasterEdge : isAux ? kAuxEdge : kStripEdge;
    g.setColour(edge);
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    // ── Header bar: colour swatch + track number ───────────────────────────
    {
        const int hdrH = 20;
        g.setColour(trackColour.withAlpha(0.85f));
        juce::Path hdrPath;
        hdrPath.addRoundedRectangle(bounds.getX(), bounds.getY(), bounds.getWidth(), (float)hdrH, 6.0f, 6.0f, true, true, false, false);
        g.fillPath(hdrPath);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        if (!isMaster && trackNumber > 0)
            g.drawText(juce::String(trackNumber), 4, 2, W - 8, hdrH - 4, juce::Justification::centredLeft);
        else
            g.drawText(isMaster ? "M" : "A", 4, 2, W - 8, hdrH - 4, juce::Justification::centredLeft);
    }

    // ── Pan value label (painted beside panKnob) ──────────────────────────
    {
        const float pan = (float)panKnob.getValue();
        juce::String panStr;
        if (std::abs(pan) < 0.02f) panStr = "C";
        else if (pan < 0) panStr = "L" + juce::String((int)(-pan * 100));
        else              panStr = "R" + juce::String((int)(pan * 100));
        // Draw to the right of the knob
        auto kb = panKnob.getBounds();
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.setFont(juce::FontOptions(8.0f));
        g.drawText(panStr, kb.getRight() + 2, kb.getY(), W - kb.getRight() - 4, kb.getHeight(),
                   juce::Justification::centredLeft);
    }

    // ── INSERTS section header ─────────────────────────────────────────────
    {
        auto insHdr = getInsertRect().withHeight(12).translated(0, -12);
        g.setColour(juce::Colour::fromRGB(20, 22, 32));
        g.fillRect(insHdr);
        g.setColour(kStripEdge);
        g.drawHorizontalLine(insHdr.getY(), 4.0f, (float)W - 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.setFont(juce::FontOptions(7.0f).withStyle("Bold"));
        g.drawText("INSERTS", insHdr, juce::Justification::centred);
    }

    // ── Insert slots ───────────────────────────────────────────────────────
    drawInsertSlots(g, getInsertRect());

    // ── SENDS section header ───────────────────────────────────────────────
    {
        auto sndHdr = getSendsRect().withHeight(12).translated(0, -12);
        g.setColour(juce::Colour::fromRGB(20, 22, 32));
        g.fillRect(sndHdr);
        g.setColour(kStripEdge);
        g.drawHorizontalLine(sndHdr.getY(), 4.0f, (float)W - 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.setFont(juce::FontOptions(7.0f).withStyle("Bold"));
        g.drawText("SENDS", sndHdr, juce::Justification::centred);
    }

    // ── Send rows: each row is a click target ─────────────────────────────
    {
        auto sndArea = getSendsRect();
        const char* letters[] = { "A", "B" };
        for (int i = 0; i < kNumSends; ++i)
        {
            auto row = sndArea.removeFromTop(30);
            const bool assigned = sendBusNames[i].isNotEmpty();

            // Row background
            g.setColour(juce::Colour::fromRGB(22, 24, 36));
            g.fillRoundedRectangle(row.toFloat().reduced(1.0f), 3.0f);
            g.setColour(kStripEdge);
            g.drawRoundedRectangle(row.toFloat().reduced(1.0f), 3.0f, 0.7f);

            if (assigned)
            {
                // Letter badge
                g.setColour(juce::Colour::fromRGB(50, 60, 120));
                g.fillRoundedRectangle(juce::Rectangle<float>((float)row.getX() + 3, (float)row.getCentreY() - 6, 12.0f, 12.0f), 2.0f);
                g.setColour(juce::Colours::white.withAlpha(0.9f));
                g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
                g.drawText(letters[i], row.getX() + 3, row.getCentreY() - 6, 12, 12, juce::Justification::centred);
                // Bus name
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                g.setFont(juce::FontOptions(8.5f));
                g.drawText(sendBusNames[i], row.getX() + 17, row.getY() + 2, row.getWidth() - 48, row.getHeight() - 4, juce::Justification::centredLeft);
            }
            else
            {
                // Unassigned: dim prompt
                g.setColour(juce::Colours::white.withAlpha(0.22f));
                g.setFont(juce::FontOptions(7.5f));
                g.drawText(juce::String(letters[i]) + "  -- click to assign --", row.reduced(4, 0), juce::Justification::centredLeft);
            }
        }
    }

    // ── I/O rows (painted as clickable pill buttons) ───────────────────────
    {
        auto paintIO = [&](const juce::Rectangle<int>& b, const juce::String& text, bool isIn)
        {
            g.setColour(juce::Colour::fromRGB(22, 25, 38));
            g.fillRoundedRectangle(b.toFloat(), 3.0f);
            g.setColour(kStripEdge.withAlpha(0.6f));
            g.drawRoundedRectangle(b.toFloat().reduced(0.5f), 3.0f, 0.7f);
            // prefix label
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.setFont(juce::FontOptions(7.0f));
            g.drawText(isIn ? "IN" : "OUT", b.getX() + 2, b.getY(), 18, b.getHeight(), juce::Justification::centredLeft);
            // value + dropdown arrow
            g.setColour(juce::Colours::white.withAlpha(0.75f));
            g.setFont(juce::FontOptions(8.0f));
            g.drawText(text, b.getX() + 20, b.getY(), b.getWidth() - 28, b.getHeight(), juce::Justification::centredLeft);
            // ▼ arrow
            g.setColour(juce::Colours::white.withAlpha(0.4f));
            const float ax = (float)(b.getRight() - 9);
            const float ay = (float)b.getCentreY() - 1.0f;
            juce::Path tri;
            tri.addTriangle(ax - 3.0f, ay - 1.0f, ax + 3.0f, ay - 1.0f, ax, ay + 3.0f);
            g.fillPath(tri);
        };
        paintIO(inputLabelBounds,  inputLabel .getText(), true);
        paintIO(outputLabelBounds, outputLabel.getText(), false);
    }

    // ── Fader area: draw meter bars on right side of fader ────────────────
    {
        const auto fr = getFaderRect();
        // Meter occupies right 14px of fader area
        const int meterW = 14;
        auto meterArea = fr.withLeft(fr.getRight() - meterW);
        drawMeter(g, meterArea);
        // dB scale to right of fader (left of meter)
        auto scaleArea = fr.withLeft(fr.getRight() - meterW - 22).withRight(fr.getRight() - meterW);
        // Use ProFaderLookAndFeel's built-in scale — just add 0dB notch marker
        g.setColour(juce::Colour::fromRGBA(160, 120, 255, 140));
        // 0dB sits at 75% down the fader
        const int zeroY = fr.getY() + (int)(fr.getHeight() * 0.75f);
        g.fillRect(fr.getX(), zeroY, fr.getWidth() - meterW, 1);
    }
}

// ── Layout helpers ────────────────────────────────────────────────────────────
// These must match what resized() allocates exactly.

juce::Rectangle<int> ChannelStrip::getFaderRect() const
{
    // Must match resized() layout exactly (top-down order)
    auto r = getLocalBounds().reduced(2, 2);
    r.removeFromTop(20);                // header bar
    r.removeFromTop(18);                // name label
    r.removeFromTop(18);                // PRE button
    r.removeFromTop(46);                // pan knob
    r.removeFromTop(12);                // INSERTS header
    r.removeFromTop(6 * 19 + 14);      // insert slots (always 6 visible)
    r.removeFromTop(12);                // SENDS header
    r.removeFromTop(kNumSends * 30);    // send rows
    r.removeFromTop(4);                 // gap
    r.removeFromTop(15);                // input label
    r.removeFromTop(15);                // output label
    r.removeFromTop(18);                // read button
    r.removeFromTop(20);                // MSR buttons
    return r.removeFromTop(190).reduced(4, 2);
}

juce::Rectangle<int> ChannelStrip::getMeterRect() const
{
    auto fr = getFaderRect();
    return fr.withLeft(fr.getRight() - 14);
}

juce::Rectangle<int> ChannelStrip::getInsertRect() const
{
    auto r = getLocalBounds().reduced(2, 2);
    r.removeFromTop(20);  // header bar
    r.removeFromTop(18);  // name
    r.removeFromTop(18);  // PRE
    r.removeFromTop(46);  // pan knob
    r.removeFromTop(12);  // INSERTS header
    // Insert area: 6 visible * 19px + expand row
    const int visH = 6 * 19 + 14;
    return r.removeFromTop(visH);
}

juce::Rectangle<int> ChannelStrip::getSendsRect() const
{
    auto r = getLocalBounds().reduced(2, 2);
    r.removeFromTop(20);   // header
    r.removeFromTop(18);   // name
    r.removeFromTop(18);   // PRE
    r.removeFromTop(46);   // pan
    r.removeFromTop(12);   // INSERTS header
    const int insH = 6 * 19 + 14;
    r.removeFromTop(insH); // inserts
    r.removeFromTop(12);   // SENDS header
    return r.removeFromTop(kNumSends * 30);
}

void ChannelStrip::resized()
{
    auto r = getLocalBounds().reduced(2, 2);

    // ── TOP: colour header bar (painted only) ─────────────────────────────
    r.removeFromTop(20);

    // ── Track name ─────────────────────────────────────────────────────────
    nameLabel.setBounds(r.removeFromTop(18));

    // ── PRE button ─────────────────────────────────────────────────────────
    preBtn.setBounds(r.removeFromTop(18).reduced(2, 1));

    // ── Pan knob (rotary, left-aligned; pan label painted to right) ────────
    panKnob.setBounds(r.removeFromTop(46).withWidth(42).reduced(2, 2));

    // ── INSERTS header (painted) then slots (painted) ─────────────────────
    r.removeFromTop(12); // INSERTS header
    const int insH = 6 * 19 + 14;
    r.removeFromTop(insH);

    // ── SENDS header (painted) then send rows ─────────────────────────────
    r.removeFromTop(12); // SENDS header
    {
        for (int i = 0; i < kNumSends; ++i)
        {
            auto row = r.removeFromTop(30).reduced(1, 1);
            const bool assigned = sendBusNames[i].isNotEmpty();
            if (assigned)
            {
                // Knob on the right, dB label below knob
                auto knobArea = row.removeFromRight(30).reduced(1, 1);
                sendKnobs[i]   .setBounds(knobArea.withHeight(20));
                sendDbLabels[i].setBounds(knobArea.withTop(knobArea.getY() + 20));
                sendKnobs[i]   .setVisible(true);
                sendDbLabels[i].setVisible(true);
            }
            else
            {
                sendKnobs[i]   .setVisible(false);
                sendDbLabels[i].setVisible(false);
            }
        }
    }

    // ── I/O rows (painted, hit-tested via stored bounds) ───────────────────
    r.removeFromTop(4);
    inputLabelBounds  = r.removeFromTop(15).reduced(2, 1);
    outputLabelBounds = r.removeFromTop(15).reduced(2, 1);

    // ── Read button ────────────────────────────────────────────────────────
    readBtn.setBounds(r.removeFromTop(18).reduced(2, 1));

    // ── M S R buttons ──────────────────────────────────────────────────────
    {
        auto row = r.removeFromTop(20).reduced(2, 1);
        const int btnW = isMaster ? row.getWidth() / 2 : row.getWidth() / 3;
        muteBtn.setBounds(row.removeFromLeft(btnW));
        soloBtn.setBounds(row.removeFromLeft(btnW));
        if (!isMaster) armBtn.setBounds(row);
    }

    // ── Main fader ─────────────────────────────────────────────────────────
    // Leave right 14px for meter (painted)
    auto faderRow = r.removeFromTop(190);
    fader.setBounds(faderRow.withTrimmedRight(14).reduced(4, 2));

    // ── Fader dB label ─────────────────────────────────────────────────────
    faderDbLabel.setBounds(r.removeFromTop(14));
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

void MixerWindow::setArrangementModel(NovaStudio::ArrangementModel& model)
{
    if (arrangementModelPtr)
        arrangementModelPtr->removeChangeListener(this);
    arrangementModelPtr = &model;
    arrangementModelPtr->addChangeListener(this);
}

void MixerWindow::notifyPluginChainChanged()
{
    // Refresh our own insert labels immediately
    refreshInsertSlotNames();
    // Broadcast so EditWindow's ProductionPanel also refreshes
    if (arrangementModelPtr)
        arrangementModelPtr->sendChangeMessage();
}

void MixerWindow::changeListenerCallback(juce::ChangeBroadcaster*)
{
    // Only rebuild strips if track count changed; otherwise just refresh inserts + meters
    const int numTracks = engine.getSession().getNumTracks();
    const int numStrips = trackStrips.size() + auxStrips.size() + (masterStrip ? 1 : 0);
    if (numTracks != numStrips)
        refresh();
    else
        refreshInsertSlotNames();
}

juce::StringArray MixerWindow::buildBusList(bool includeHardware) const
{
    juce::StringArray list;

    // Internal buses — one per aux track, named by channel count
    int ch = 1;
    const auto& sess = engine.getSession();
    for (int i = 0; i < sess.getNumTracks(); ++i)
    {
        const auto& t = sess.getTrack(i);
        if (t.type != NovaStudio::TrackType::Aux) continue;
        if (t.isStereo)
        {
            list.add("Bus " + juce::String(ch) + "-" + juce::String(ch + 1)
                     + "  (" + t.name + ")");
            ch += 2;
        }
        else
        {
            list.add("Bus " + juce::String(ch) + "  (" + t.name + ")");
            ch += 1;
        }
    }

    if (includeHardware)
    {
        list.add("Main Out");
        for (int n = 1; n <= 8; ++n)
            list.add("Output " + juce::String(n) + "-" + juce::String(n + 1));
    }

    return list;
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
            notifyPluginChainChanged();
            openPluginBrowser(i, slot);
        };
        strip->onInsertRemovePlugin = [this, i](int slot)
        {
            engine.removePluginFromTrack(i, slot);
            notifyPluginChainChanged();
        };
    };

    // Sends start unassigned — user picks the destination via click

    static const juce::Colour kTrackPalette[] = {
        juce::Colour::fromRGB(120, 80, 200),
        juce::Colour::fromRGB(80, 100, 200),
        juce::Colour::fromRGB(50, 160, 160),
        juce::Colour::fromRGB(60, 120, 200),
        juce::Colour::fromRGB(40, 180, 160),
        juce::Colour::fromRGB(60, 180, 80),
        juce::Colour::fromRGB(160, 190, 50),
        juce::Colour::fromRGB(190, 110, 40),
    };

    int stripNum = 1;

    // ── Track strips (Audio / MIDI / Instrument) ──────────────────────────
    for (int i = 0; i < numTracks; ++i)
    {
        const auto& track = session.getTrack(i);
        if (track.type == NovaStudio::TrackType::Aux || track.type == NovaStudio::TrackType::Master)
            continue;

        auto* strip = trackStrips.add(new ChannelStrip());
        strip->setTrackIndex(i);
        strip->setTrackNumber(stripNum);
        strip->setTrackColour(kTrackPalette[(stripNum - 1) % 8]);
        stripNum++;
        strip->updateFromTrack(track);
        // sends start unassigned
        stripsContainer.addAndMakeVisible(strip);

        strip->onVolumeChanged    = [this, i](float db)   { engine.setTrackVolume(i, db); };
        strip->onPanChanged       = [this, i](float pan)   { engine.setTrackPan(i, pan); };
        strip->onMuteToggled      = [this, i](bool muted)  { engine.setTrackMute(i, muted); };
        strip->onSoloToggled      = [this, i](bool solo)   { engine.setTrackSolo(i, solo); };
        strip->onArmToggled       = [this, i](bool armed)  { engine.setTrackArm(i, armed); };
        strip->onSendChanged      = [this, i](int send, float db) { engine.setTrackSendLevel(i, send, db); };
        // Available hardware inputs
        strip->getAvailableInputs = [this]() -> juce::StringArray {
            juce::StringArray inputs;
            for (int n = 1; n <= 8; ++n) inputs.add("Input " + juce::String(n));
            return inputs;
        };
        // Available outputs: hardware + internal buses (one per aux track)
        strip->getAvailableOutputs = [this]() -> juce::StringArray {
            return buildBusList(true);
        };
        wireInserts(strip, i);
    }

    // ── Aux strips (engine tracks of type Aux) ────────────────────────────
    int auxBusChannel = 1; // running channel counter for bus naming
    for (int i = 0; i < numTracks; ++i)
    {
        const auto& track = session.getTrack(i);
        if (track.type != NovaStudio::TrackType::Aux)
            continue;

        // Generate the bus name this aux receives on
        juce::String busName;
        if (track.isStereo)
        {
            busName = "Bus " + juce::String(auxBusChannel) + "-" + juce::String(auxBusChannel + 1);
            auxBusChannel += 2;
        }
        else
        {
            busName = "Bus " + juce::String(auxBusChannel);
            auxBusChannel += 1;
        }

        auto* strip = auxStrips.add(new ChannelStrip());
        strip->setTrackIndex(i);
        strip->setTrackNumber(stripNum++);
        strip->setAux(true);
        strip->updateAsAux(track.name);
        strip->setInputName(track.inputBus.isEmpty() ? busName : track.inputBus);
        strip->setOutputName(track.outputBus.isEmpty() ? "Main Out" : track.outputBus);
        stripsContainer.addAndMakeVisible(strip);

        strip->onVolumeChanged = [this, i](float db)   { engine.setTrackVolume(i, db); };
        strip->onPanChanged    = [this, i](float pan)   { engine.setTrackPan(i, pan); };
        strip->onMuteToggled   = [this, i](bool muted)  { engine.setTrackMute(i, muted); };
        strip->onSoloToggled   = [this, i](bool solo)   { engine.setTrackSolo(i, solo); };
        strip->onSendChanged   = [](int, float) {};
        strip->onInputChanged  = [this, i](const juce::String& bus) {
            engine.getSession().getTrack(i).inputBus = bus;
        };
        strip->onOutputChanged = [this, i](const juce::String& bus) {
            engine.getSession().getTrack(i).outputBus = bus;
        };
        // Aux inputs = internal buses; outputs = hardware + other buses
        strip->getAvailableInputs  = [this]() -> juce::StringArray { return buildBusList(false); };
        strip->getAvailableOutputs = [this]() -> juce::StringArray { return buildBusList(true);  };
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
    static constexpr float kDecay = 0.92f; // ballistic decay per 30Hz tick

    // Track strips — use each strip's stored track index (not the loop counter)
    // so aux tracks interspersed in the session don't offset the peak reads.
    for (auto* strip : trackStrips)
    {
        const int ti = strip->getTrackIndex();
        const float L = engine.getTrackPeakLevel(ti, 0);
        const float R = engine.getTrackPeakLevel(ti, 1);
        strip->setMeterLevel(juce::jmax(L, strip->getMeterLeft()  * kDecay),
                             juce::jmax(R, strip->getMeterRight() * kDecay));
    }

    for (auto* strip : auxStrips)
    {
        const int ti = strip->getTrackIndex();
        const float L = (ti >= 0) ? engine.getTrackPeakLevel(ti, 0) : 0.0f;
        const float R = (ti >= 0) ? engine.getTrackPeakLevel(ti, 1) : 0.0f;
        strip->setMeterLevel(juce::jmax(L, strip->getMeterLeft()  * kDecay),
                             juce::jmax(R, strip->getMeterRight() * kDecay));
    }

    if (masterStrip)
    {
        // Master level = peak across all tracks (summed mix proxy)
        float L = 0.0f, R = 0.0f;
        const int nt = engine.getSession().getNumTracks();
        for (int i = 0; i < nt; ++i)
        {
            L = juce::jmax(L, engine.getTrackPeakLevel(i, 0));
            R = juce::jmax(R, engine.getTrackPeakLevel(i, 1));
        }
        masterStrip->setMeterLevel(juce::jmax(L, masterStrip->getMeterLeft()  * kDecay),
                                   juce::jmax(R, masterStrip->getMeterRight() * kDecay));
    }
}

void MixerWindow::refreshInsertSlotNames()
{
    auto refresh = [this](juce::OwnedArray<ChannelStrip>& strips)
    {
        for (auto* strip : strips)
        {
            const int t = strip->getTrackIndex();
            for (int s = 0; s < 10; ++s)
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
            // Fired after successful load — update strip display, broadcast, open editor
            if (isPositiveAndBelow(track, trackStrips.size()))
                trackStrips[track]->setInsertSlotName(slot, name);
            notifyPluginChainChanged();
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
