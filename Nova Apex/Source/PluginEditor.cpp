#include "PluginEditor.h"

#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// ApexLookAndFeel
// ─────────────────────────────────────────────────────────────────────────────

ApexLookAndFeel::ApexLookAndFeel()
{
    // Base colours
    setColour (juce::Slider::rotarySliderFillColourId,
               juce::Colour (ApexColours::champagne));
    setColour (juce::Slider::rotarySliderOutlineColourId,
               juce::Colour (ApexColours::panelBorder));
    setColour (juce::Slider::thumbColourId,
               juce::Colour (ApexColours::white));

    setColour (juce::TextButton::buttonColourId,
               juce::Colour (ApexColours::panel));
    setColour (juce::TextButton::buttonOnColourId,
               juce::Colour (ApexColours::champagne).withAlpha (0.25f));
    setColour (juce::TextButton::textColourOffId,
               juce::Colour (ApexColours::textMuted));
    setColour (juce::TextButton::textColourOnId,
               juce::Colour (ApexColours::champagne));

    setColour (juce::Label::textColourId,
               juce::Colour (ApexColours::textMuted));

    setColour (juce::ToggleButton::textColourId,
               juce::Colour (ApexColours::textMuted));
    setColour (juce::ToggleButton::tickColourId,
               juce::Colour (ApexColours::champagne));
    setColour (juce::ToggleButton::tickDisabledColourId,
               juce::Colour (ApexColours::panelBorder));
}

void ApexLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPos,
                                         float rotaryStartAngle, float rotaryEndAngle,
                                         juce::Slider&)
{
    const float radius = juce::jmin (width, height) * 0.5f - 4.0f;
    const float centreX = static_cast<float> (x) + static_cast<float> (width)  * 0.5f;
    const float centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;

    // Body
    g.setColour (juce::Colour (ApexColours::panel));
    g.fillEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

    // Outer ring
    g.setColour (juce::Colour (ApexColours::panelBorder));
    g.drawEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    // Arc track
    const float arcRadius = radius - 3.0f;
    juce::Path trackPath;
    trackPath.addCentredArc (centreX, centreY, arcRadius, arcRadius,
                              0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (ApexColours::panelBorder).withAlpha (0.5f));
    g.strokePath (trackPath, juce::PathStrokeType (2.0f));

    // Filled arc (value)
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    juce::Path valuePath;
    valuePath.addCentredArc (centreX, centreY, arcRadius, arcRadius,
                              0.0f, rotaryStartAngle, angle, true);
    g.setColour (juce::Colour (ApexColours::champagne));
    g.strokePath (valuePath, juce::PathStrokeType (2.5f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

    // Pointer dot
    const float pointerLen = radius * 0.55f;
    const float px = centreX + std::sin (angle) * pointerLen;
    const float py = centreY - std::cos (angle) * pointerLen;
    g.setColour (juce::Colour (ApexColours::white));
    g.fillEllipse (px - 3.0f, py - 3.0f, 6.0f, 6.0f);

    // Small centre fill
    const float innerR = radius * 0.22f;
    g.setColour (juce::Colour (ApexColours::background));
    g.fillEllipse (centreX - innerR, centreY - innerR, innerR * 2.0f, innerR * 2.0f);
}

void ApexLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                             const juce::Colour&,
                                             bool, bool isDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool isOn   = button.getToggleState();

    if (isOn)
    {
        g.setColour (juce::Colour (ApexColours::champagne).withAlpha (0.18f));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (juce::Colour (ApexColours::champagne).withAlpha (0.6f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }
    else
    {
        g.setColour (isDown ? juce::Colour (ApexColours::panelBorder).withAlpha (0.5f)
                            : juce::Colour (ApexColours::panel));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (juce::Colour (ApexColours::panelBorder));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }
}

void ApexLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                       bool, bool)
{
    const bool isOn = button.getToggleState();
    g.setColour (isOn ? juce::Colour (ApexColours::champagne)
                      : juce::Colour (ApexColours::textMuted));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    g.drawFittedText (button.getButtonText(),
                      button.getLocalBounds(),
                      juce::Justification::centred, 1);
}

void ApexLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setColour (juce::Colour (ApexColours::textMuted));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f)));
    g.drawFittedText (label.getText(),
                      label.getLocalBounds(),
                      label.getJustificationType(), 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// GRDisplayComponent
// ─────────────────────────────────────────────────────────────────────────────

GRDisplayComponent::GRDisplayComponent()
{
    grHistory.fill (0.0f);
}

void GRDisplayComponent::pushGRValue (float grDb)
{
    grHistory[writePos] = juce::jlimit (-40.0f, 0.0f, grDb);
    writePos = (writePos + 1) % kHistorySize;
}

void GRDisplayComponent::setInputLevel (float l, float r) noexcept
{
    inputL = l;
    inputR = r;
}

void GRDisplayComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // Background
    g.setColour (juce::Colour (ApexColours::background));
    g.fillRoundedRectangle (bounds, 4.0f);

    // Grid lines
    g.setColour (juce::Colour (ApexColours::panelBorder).withAlpha (0.4f));
    for (int db = -10; db >= -40; db -= 10)
    {
        const float y = h * (1.0f - static_cast<float> (-db) / 40.0f);
        g.drawHorizontalLine (juce::roundToInt (y), bounds.getX(), bounds.getRight());
    }

    // Clipping line at 0 dB GR (top)
    g.setColour (juce::Colour (ApexColours::red).withAlpha (0.5f));
    g.drawHorizontalLine (1, bounds.getX(), bounds.getRight());

    // GR fill shape
    if (w > 0.0f)
    {
        juce::Path grPath;
        grPath.startNewSubPath (0.0f, h);

        for (int i = 0; i < kHistorySize; ++i)
        {
            const int idx = (writePos + i) % kHistorySize;
            const float xPos = static_cast<float> (i) / static_cast<float> (kHistorySize - 1) * w;
            const float grDb = grHistory[idx]; // 0..-40
            const float yPos = h * (static_cast<float> (-grDb) / 40.0f);
            grPath.lineTo (xPos, h - yPos);
        }

        grPath.lineTo (w, h);
        grPath.closeSubPath();

        g.setColour (juce::Colour (ApexColours::champagne).withAlpha (0.35f));
        g.fillPath (grPath);

        // Outline
        juce::Path outline;
        outline.startNewSubPath (0.0f, h);
        for (int i = 0; i < kHistorySize; ++i)
        {
            const int idx = (writePos + i) % kHistorySize;
            const float xPos = static_cast<float> (i) / static_cast<float> (kHistorySize - 1) * w;
            const float grDb = grHistory[idx];
            const float yPos = h * (static_cast<float> (-grDb) / 40.0f);
            outline.lineTo (xPos, h - yPos);
        }
        g.setColour (juce::Colour (ApexColours::champagne).withAlpha (0.7f));
        g.strokePath (outline, juce::PathStrokeType (1.0f));
    }

    // Input level bar (left side indicator)
    const float levelDbL = juce::Decibels::gainToDecibels (inputL + 1.0e-9f);
    const float levelNormL = juce::jlimit (0.0f, 1.0f, (levelDbL + 60.0f) / 60.0f);
    const float barH = levelNormL * h;
    g.setColour (juce::Colour (ApexColours::champagne).withAlpha (0.5f));
    g.fillRect (bounds.getX(), h - barH, 4.0f, barH);

    // Border
    g.setColour (juce::Colour (ApexColours::panelBorder));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    // Label
    g.setColour (juce::Colour (ApexColours::textMuted));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.0f)));
    g.drawText ("GR", bounds.reduced (6.0f, 4.0f).removeFromTopLeft (20, 12),
                juce::Justification::topLeft);
}

// ─────────────────────────────────────────────────────────────────────────────
// TruePeakMeter
// ─────────────────────────────────────────────────────────────────────────────

TruePeakMeter::TruePeakMeter() = default;

void TruePeakMeter::setLevel (float lin) noexcept
{
    levelLinear = juce::jlimit (0.0f, 2.0f, lin); // allow a bit over 0 dBFS
}

void TruePeakMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // Background
    g.setColour (juce::Colour (ApexColours::background));
    g.fillRoundedRectangle (bounds, 3.0f);

    // Segments
    constexpr int kSegments = 20;
    const float segH = (h - 2.0f) / static_cast<float> (kSegments);
    const float levelDb = juce::Decibels::gainToDecibels (levelLinear + 1.0e-9f);
    // Map: 0 dBFS = top, -40 dBFS = bottom
    const float normLevel = juce::jlimit (0.0f, 1.0f, (levelDb + 40.0f) / 40.0f);
    const int activeSegs  = juce::roundToInt (normLevel * static_cast<float> (kSegments));

    for (int i = 0; i < kSegments; ++i)
    {
        const float segY = bounds.getBottom() - 1.0f - static_cast<float> (i + 1) * segH;
        const juce::Rectangle<float> seg (bounds.getX() + 2.0f, segY, w - 4.0f, segH - 1.5f);

        if (i < activeSegs)
        {
            const float t = static_cast<float> (i) / static_cast<float> (kSegments - 1);
            // champagne → red gradient near top
            juce::Colour col = (t > 0.85f)
                               ? juce::Colour (ApexColours::red)
                               : juce::Colour (ApexColours::champagne).withAlpha (0.6f + t * 0.4f);
            g.setColour (col);
            g.fillRoundedRectangle (seg, 1.5f);
        }
        else
        {
            g.setColour (juce::Colour (ApexColours::panelBorder).withAlpha (0.3f));
            g.fillRoundedRectangle (seg, 1.5f);
        }
    }

    // Border
    g.setColour (juce::Colour (ApexColours::panelBorder));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);

    // Label
    g.setColour (juce::Colour (ApexColours::textMuted));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (8.5f)));
    g.drawText ("TP", bounds.reduced (2, 2), juce::Justification::centredBottom);
}

// ─────────────────────────────────────────────────────────────────────────────
// NovaApexAudioProcessorEditor
// ─────────────────────────────────────────────────────────────────────────────

NovaApexAudioProcessorEditor::NovaApexAudioProcessorEditor (NovaApexAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&laf);
    setSize (900, 540);

    // ─── GR display
    addAndMakeVisible (grDisplay);

    // ─── True-peak meter
    addAndMakeVisible (tpMeter);

    // ─── Main knobs
    buildKnob (gainKnob,    "gain",    gainAtt);
    buildKnob (driveKnob,   "drive",   driveAtt);
    buildKnob (ceilingKnob, "ceiling", ceilingAtt);
    buildKnob (outputKnob,  "outputGain", outputAtt);

    // ─── Bottom knobs
    buildSmallKnob (attackKnob,    "attack",    attackAtt);
    buildSmallKnob (releaseKnob,   "release",   releaseAtt);
    buildSmallKnob (lookaheadKnob, "lookahead", lookaheadAtt);

    // ─── Style buttons
    static const char* styleNames[] = { "Clean", "Punch", "Smooth", "Loud", "Analog", "Master" };
    for (int i = 0; i < 6; ++i)
    {
        styleButtons[i].setButtonText (styleNames[i]);
        styleButtons[i].setClickingTogglesState (true);
        styleButtons[i].setRadioGroupId (1);
        styleButtons[i].setLookAndFeel (&laf);
        addAndMakeVisible (styleButtons[i]);

        styleButtons[i].onClick = [this, i]()
        {
            if (auto* param = processorRef.apvts.getParameter ("style"))
                param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (i)));
        };
    }
    // Set initial state from param
    {
        const int currentStyle = juce::roundToInt (
            processorRef.apvts.getRawParameterValue ("style")->load());
        if (currentStyle >= 0 && currentStyle < 6)
            styleButtons[currentStyle].setToggleState (true, juce::dontSendNotification);
    }

    // ─── Oversample button cycle
    oversampleButton.setLookAndFeel (&laf);
    addAndMakeVisible (oversampleButton);
    oversampleButton.onClick = [this]()
    {
        currentOversampleIdx = (currentOversampleIdx + 1) % 4;
        const char* labels[] = { "1x", "2x", "4x", "8x" };
        oversampleButton.setButtonText (labels[currentOversampleIdx]);
        if (auto* param = processorRef.apvts.getParameter ("oversample"))
            param->setValueNotifyingHost (param->convertTo0to1 (
                static_cast<float> (currentOversampleIdx)));
    };

    // ─── Link toggle
    linkButton.setButtonText ("Link");
    linkButton.setLookAndFeel (&laf);
    addAndMakeVisible (linkButton);

    // ─── Dither toggle
    ditherButton.setButtonText ("Dither");
    ditherButton.setLookAndFeel (&laf);
    addAndMakeVisible (ditherButton);

    // ─── Match button
    matchButton.setLookAndFeel (&laf);
    addAndMakeVisible (matchButton);
    matchButton.onClick = [this]()
    {
        // Loudness match: set gain so LUFS reaches target
        const float lufs    = processorRef.getLUFS();
        const float target  = processorRef.apvts.getRawParameterValue ("loudnessTarget")->load();
        const float delta   = target - lufs;
        const float currentGain = processorRef.apvts.getRawParameterValue ("gain")->load();
        const float newGain = juce::jlimit (0.0f, 24.0f, currentGain + delta);
        if (auto* param = processorRef.apvts.getParameter ("gain"))
            param->setValueNotifyingHost (param->convertTo0to1 (newGain));
    };

    startTimerHz (30);
}

NovaApexAudioProcessorEditor::~NovaApexAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NovaApexAudioProcessorEditor::buildKnob (juce::Slider& s, const juce::String& paramId,
                                               std::unique_ptr<SliderAtt>& att)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setLookAndFeel (&laf);
    addAndMakeVisible (s);
    att = std::make_unique<SliderAtt> (processorRef.apvts, paramId, s);
}

void NovaApexAudioProcessorEditor::buildSmallKnob (juce::Slider& s, const juce::String& paramId,
                                                    std::unique_ptr<SliderAtt>& att)
{
    buildKnob (s, paramId, att);
}

void NovaApexAudioProcessorEditor::timerCallback()
{
    cachedLufs     = processorRef.getLUFS();
    cachedTruePeak = processorRef.getTruePeak();
    cachedGR       = processorRef.gainReductionDb.load();

    grDisplay.pushGRValue (cachedGR);
    grDisplay.setInputLevel (processorRef.inputLevelL.load(),
                              processorRef.inputLevelR.load());

    tpMeter.setLevel (processorRef.outputLevelL.load());

    grDisplay.repaint();
    tpMeter.repaint();
    repaint (0, 0, getWidth(), 50); // header only for readouts
}

void NovaApexAudioProcessorEditor::resized()
{
    const int W = getWidth();
    const int H = getHeight();

    // ─── Header 50px
    // ─── Style strip at bottom of content area (36px)
    // ─── Bottom strip (60px)
    // ─── Content area in between

    constexpr int headerH  = 50;
    constexpr int styleH   = 36;
    constexpr int bottomH  = 60;
    const int contentTop    = headerH;
    const int contentBottom = H - styleH - bottomH;
    const int contentH      = contentBottom - contentTop;

    // Left panel: 160px
    // Right panel: 120px
    // Centre: remainder
    constexpr int leftW  = 160;
    constexpr int rightW = 150;
    const int centreX = leftW;
    const int centreW = W - leftW - rightW;

    // GR display (centre)
    grDisplay.setBounds (centreX + 4, contentTop + 8,
                          centreW - 8, contentH - 16);

    // TP meter (right panel upper)
    tpMeter.setBounds (W - rightW + 10, contentTop + 8,
                        30, contentH - 16);

    // Large knobs in left panel
    const int knobSize  = 68;
    const int knobY1 = contentTop + 20;
    const int knobY2 = contentTop + contentH / 2 + 10;

    gainKnob.setBounds  (leftW / 2 - knobSize / 2 - 30, knobY1, knobSize, knobSize);
    driveKnob.setBounds (leftW / 2 - knobSize / 2 + 30, knobY1, knobSize, knobSize);

    // Right panel knobs
    const int rKnobX = W - rightW + 50;
    ceilingKnob.setBounds (rKnobX, knobY1, knobSize, knobSize);
    outputKnob.setBounds  (rKnobX, knobY2, knobSize, knobSize);

    // Style strip
    const int styleY = contentBottom;
    const int btnW = W / 6;
    for (int i = 0; i < 6; ++i)
        styleButtons[i].setBounds (i * btnW, styleY, btnW, styleH);

    // Bottom strip
    const int bottomY   = H - bottomH;
    const int smallKnob = 44;
    const int kPad = 10;

    attackKnob.setBounds   (kPad,                     bottomY + 8, smallKnob, smallKnob);
    releaseKnob.setBounds  (kPad + smallKnob + 10,    bottomY + 8, smallKnob, smallKnob);
    lookaheadKnob.setBounds (kPad + (smallKnob + 10) * 2, bottomY + 8, smallKnob, smallKnob);

    const int rightBtnX = W - 290;
    oversampleButton.setBounds (rightBtnX,       bottomY + 14, 50, 28);
    linkButton.setBounds       (rightBtnX + 60,  bottomY + 14, 58, 28);
    ditherButton.setBounds     (rightBtnX + 128, bottomY + 14, 60, 28);
    matchButton.setBounds      (rightBtnX + 198, bottomY + 14, 60, 28);
}

void NovaApexAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Full background
    g.fillAll (juce::Colour (ApexColours::background));

    paintHeader (g);
    paintLeftPanel (g);
    paintRightPanel (g);
    paintStyleStrip (g);
    paintBottomStrip (g);
}

void NovaApexAudioProcessorEditor::paintHeader (juce::Graphics& g)
{
    const int W = getWidth();

    // Header bar
    g.setColour (juce::Colour (ApexColours::panel));
    g.fillRect (0, 0, W, 50);

    g.setColour (juce::Colour (ApexColours::panelBorder));
    g.drawHorizontalLine (50, 0.0f, static_cast<float> (W));

    // Plugin name
    g.setColour (juce::Colour (ApexColours::champagne));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (18.0f)));
    g.drawText ("NOVA APEX", 18, 0, 160, 50, juce::Justification::centredLeft);

    // Tagline
    g.setColour (juce::Colour (ApexColours::textMuted));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.5f)));
    g.drawText ("true-peak loudness mastering",
                18, 32, 200, 14, juce::Justification::centredLeft);

    // LUFS readout
    const juce::String lufsStr = (cachedLufs <= -69.0f)
                                  ? "LUFS: ---"
                                  : "LUFS: " + juce::String (cachedLufs, 1);
    g.setColour (juce::Colour (ApexColours::champagne));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
    g.drawText (lufsStr, W - 280, 0, 130, 50, juce::Justification::centred);

    // True-peak readout
    const juce::String tpStr = (cachedTruePeak <= -69.0f)
                                ? "TP: ---"
                                : "TP: " + juce::String (cachedTruePeak, 1) + " dBFS";
    g.drawText (tpStr, W - 150, 0, 130, 50, juce::Justification::centred);

    // GR readout (red if reducing)
    if (cachedGR < -0.2f)
    {
        g.setColour (juce::Colour (ApexColours::red));
        const juce::String grStr = "GR: " + juce::String (cachedGR, 1) + " dB";
        g.drawText (grStr, W / 2 - 60, 0, 120, 50, juce::Justification::centred);
    }
}

void NovaApexAudioProcessorEditor::paintLeftPanel (juce::Graphics& g)
{
    constexpr int headerH = 50;
    constexpr int styleH  = 36;
    constexpr int bottomH = 60;
    const int contentH = getHeight() - headerH - styleH - bottomH;

    // Panel background
    g.setColour (juce::Colour (ApexColours::panel));
    g.fillRect (0, headerH, 160, contentH);

    g.setColour (juce::Colour (ApexColours::panelBorder));
    g.drawVerticalLine (160, static_cast<float> (headerH),
                         static_cast<float> (headerH + contentH));

    // Knob labels
    g.setColour (juce::Colour (ApexColours::textMuted));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f)));

    const auto gainBounds  = gainKnob.getBounds();
    const auto driveBounds = driveKnob.getBounds();

    g.drawText ("INPUT GAIN",
                gainBounds.getX() - 10, gainBounds.getBottom() + 4, 88, 12,
                juce::Justification::centred);
    g.drawText ("DRIVE",
                driveBounds.getX() - 10, driveBounds.getBottom() + 4, 88, 12,
                juce::Justification::centred);

    // Value readout under each knob
    g.setColour (juce::Colour (ApexColours::champagne));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.5f)));

    const float gainDb = processorRef.apvts.getRawParameterValue ("gain")->load();
    const float driveVal = processorRef.apvts.getRawParameterValue ("drive")->load();

    g.drawText (juce::String (gainDb, 1) + " dB",
                gainBounds.getX() - 10, gainBounds.getBottom() + 16, 88, 12,
                juce::Justification::centred);
    g.drawText (juce::String (driveVal, 1),
                driveBounds.getX() - 10, driveBounds.getBottom() + 16, 88, 12,
                juce::Justification::centred);
}

void NovaApexAudioProcessorEditor::paintRightPanel (juce::Graphics& g)
{
    constexpr int headerH = 50;
    constexpr int styleH  = 36;
    constexpr int bottomH = 60;
    const int W = getWidth();
    const int contentH = getHeight() - headerH - styleH - bottomH;

    g.setColour (juce::Colour (ApexColours::panel));
    g.fillRect (W - 150, headerH, 150, contentH);

    g.setColour (juce::Colour (ApexColours::panelBorder));
    g.drawVerticalLine (W - 150, static_cast<float> (headerH),
                         static_cast<float> (headerH + contentH));

    // Knob labels
    g.setColour (juce::Colour (ApexColours::textMuted));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f)));

    const auto ceilBounds = ceilingKnob.getBounds();
    const auto outBounds  = outputKnob.getBounds();

    g.drawText ("CEILING",
                ceilBounds.getX() - 10, ceilBounds.getBottom() + 4, 88, 12,
                juce::Justification::centred);
    g.drawText ("OUTPUT",
                outBounds.getX() - 10, outBounds.getBottom() + 4, 88, 12,
                juce::Justification::centred);

    // Values
    g.setColour (juce::Colour (ApexColours::champagne));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.5f)));

    const float ceilDb = processorRef.apvts.getRawParameterValue ("ceiling")->load();
    const float outDb  = processorRef.apvts.getRawParameterValue ("outputGain")->load();

    g.drawText (juce::String (ceilDb, 1) + " dBFS",
                ceilBounds.getX() - 10, ceilBounds.getBottom() + 16, 88, 12,
                juce::Justification::centred);
    g.drawText (juce::String (outDb, 1) + " dB",
                outBounds.getX() - 10, outBounds.getBottom() + 16, 88, 12,
                juce::Justification::centred);
}

void NovaApexAudioProcessorEditor::paintStyleStrip (juce::Graphics& g)
{
    const int W = getWidth();
    const int H = getHeight();
    constexpr int styleH  = 36;
    constexpr int bottomH = 60;
    const int styleY = H - styleH - bottomH;

    g.setColour (juce::Colour (ApexColours::panel));
    g.fillRect (0, styleY, W, styleH);

    g.setColour (juce::Colour (ApexColours::panelBorder));
    g.drawHorizontalLine (styleY, 0.0f, static_cast<float> (W));
    g.drawHorizontalLine (styleY + styleH, 0.0f, static_cast<float> (W));

    // "Style:" label
    g.setColour (juce::Colour (ApexColours::textMuted));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.5f)));
    g.drawText ("Style:", 6, styleY, 40, styleH, juce::Justification::centredLeft);
}

void NovaApexAudioProcessorEditor::paintBottomStrip (juce::Graphics& g)
{
    const int W = getWidth();
    const int H = getHeight();
    constexpr int bottomH = 60;
    const int bottomY = H - bottomH;

    g.setColour (juce::Colour (ApexColours::panel));
    g.fillRect (0, bottomY, W, bottomH);

    g.setColour (juce::Colour (ApexColours::panelBorder));
    g.drawHorizontalLine (bottomY, 0.0f, static_cast<float> (W));

    // Knob labels
    g.setColour (juce::Colour (ApexColours::textMuted));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.0f)));

    const auto atkB = attackKnob.getBounds();
    const auto relB = releaseKnob.getBounds();
    const auto laB  = lookaheadKnob.getBounds();

    g.drawText ("ATTACK",    atkB.getX() - 4, atkB.getBottom() + 2, 52, 10, juce::Justification::centred);
    g.drawText ("RELEASE",   relB.getX() - 4, relB.getBottom() + 2, 52, 10, juce::Justification::centred);
    g.drawText ("LOOKAHEAD", laB.getX()  - 4, laB.getBottom()  + 2, 52, 10, juce::Justification::centred);
}
