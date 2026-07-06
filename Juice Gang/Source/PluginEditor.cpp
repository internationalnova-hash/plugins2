#include "PluginEditor.h"

// Juice carton purple palette
static const juce::Colour kPurpleDark  (0xff2a0060);
static const juce::Colour kPurpleMid   (0xff4a0096);
static const juce::Colour kPurpleLight (0xff6a0dad);
static const juce::Colour kGreen       (0xff39e65a);
static const juce::Colour kPanelBg     (0xffd8c8f0);
static const juce::Colour kPanelBorder (0xffaa88dd);
static const juce::Colour kTextDark    (0xff1a0040);

// ─── FilterCurveDisplay ───────────────────────────────────────────────────────
void FilterCurveDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0a0020));
    g.fillRoundedRectangle (b, 4.f);

    float cutoff = *apvts.getRawParameterValue ("cutoff");
    float res    = *apvts.getRawParameterValue ("resonance") / 100.f;
    int   mode   = (int)*apvts.getRawParameterValue ("filterMode");

    juce::Path curve;
    const int W = (int)b.getWidth();
    bool started = false;

    for (int px = 0; px < W; px++)
    {
        float freq = 20.f * std::pow (1000.f, px / (float)W);
        float fc = cutoff / 20000.f;
        float f  = freq  / 20000.f;
        float mag = 1.f;

        switch (mode) {
            case 0: mag = 1.f / (1.f + std::pow (f / fc, 4.f * (1.f + res)));  break; // LP
            case 1: mag = 1.f / (1.f + std::pow (fc / f, 4.f * (1.f + res)));  break; // HP
            case 2: { float bw = 0.3f * (1.f - res * 0.9f); float d = (f-fc); mag = bw*bw/(d*d+bw*bw); break; } // BP
            case 3: { float bw = 0.3f * (1.f - res * 0.9f); float d = (f-fc); mag = d*d/(d*d+bw*bw); break; }   // Notch
        }

        float dB = juce::Decibels::gainToDecibels (juce::jmax (0.0001f, mag));
        float py = b.getY() + b.getHeight() * (1.f - juce::jmap (dB, -24.f, 12.f, 0.f, 1.f));
        py = juce::jlimit (b.getY(), b.getBottom(), py);

        if (!started) { curve.startNewSubPath (b.getX() + px, py); started = true; }
        else curve.lineTo (b.getX() + px, py);
    }

    // Fill under curve
    juce::Path fill = curve;
    fill.lineTo (b.getRight(), b.getBottom());
    fill.lineTo (b.getX(), b.getBottom());
    fill.closeSubPath();
    g.setColour (kPurpleMid.withAlpha (0.35f));
    g.fillPath (fill);

    g.setColour (kGreen);
    g.strokePath (curve, juce::PathStrokeType (2.f));

    // Resonance dot
    float fc = cutoff;
    float px = (float)getWidth() * std::log (fc / 20.f) / std::log (1000.f);
    px = juce::jlimit (0.f, (float)getWidth(), px);
    g.setColour (kGreen);
    g.fillEllipse (px - 4.f, b.getCentreY() - 4.f, 8.f, 8.f);
}

// ─── ReverbEQDisplay ─────────────────────────────────────────────────────────
void ReverbEQDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0a0020));
    g.fillRoundedRectangle (b, 4.f);

    float lc  = *apvts.getRawParameterValue ("revLowCut");
    float ls  = *apvts.getRawParameterValue ("revLowShelf");
    float mid = *apvts.getRawParameterValue ("revMid");
    float hs  = *apvts.getRawParameterValue ("revHighShelf");
    float hc  = *apvts.getRawParameterValue ("revHighCut");

    const int W = getWidth();
    juce::Path curve;
    bool started = false;

    for (int px = 0; px < W; px++)
    {
        float freq = 20.f * std::pow (1000.f, px / (float)W);
        float dB = 0.f;

        // Low cut
        if (freq < lc) dB -= 12.f * (1.f - freq / lc);
        // Low shelf
        dB += ls * juce::jlimit (0.f, 1.f, 1.f - std::log (freq / 200.f) / std::log (20000.f / 200.f));
        // Mid
        { float d = (std::log(freq) - std::log(1000.f)) / std::log(2.f); dB += mid / (1.f + d*d * 4.f); }
        // High shelf
        dB += hs * juce::jlimit (0.f, 1.f, std::log (freq / 200.f) / std::log (20000.f / 200.f));
        // High cut
        if (freq > hc) dB -= 12.f * (freq - hc) / (20000.f - hc);

        float py = b.getY() + b.getHeight() * (1.f - juce::jmap (dB, -12.f, 12.f, 0.f, 1.f));
        py = juce::jlimit (b.getY(), b.getBottom(), py);

        if (!started) { curve.startNewSubPath (b.getX() + px, py); started = true; }
        else curve.lineTo (b.getX() + px, py);
    }

    juce::Path fill = curve;
    fill.lineTo (b.getRight(), b.getBottom());
    fill.lineTo (b.getX(), b.getBottom());
    fill.closeSubPath();
    g.setColour (kPurpleMid.withAlpha (0.35f));
    g.fillPath (fill);

    g.setColour (kGreen);
    g.strokePath (curve, juce::PathStrokeType (2.f));

    // Control dots
    juce::Colour dotColours[] = { juce::Colour(0xffff6666), juce::Colour(0xff66aaff),
                                   kGreen, juce::Colour(0xffffc000), juce::Colour(0xffff66ff) };
    float freqs[] = { lc, 200.f, 1000.f, 4000.f, hc };
    float gains[] = { -12.f, ls, mid, hs, -12.f };
    for (int i = 0; i < 5; i++) {
        float px = (float)W * std::log (freqs[i] / 20.f) / std::log (1000.f);
        float py = b.getY() + b.getHeight() * (1.f - juce::jmap (gains[i], -12.f, 12.f, 0.f, 1.f));
        px = juce::jlimit (0.f, (float)W - 6.f, px);
        py = juce::jlimit (b.getY(), b.getBottom() - 6.f, py);
        g.setColour (dotColours[i]);
        g.fillEllipse (px, py, 6.f, 6.f);
    }
}

// ─── VUMeter ─────────────────────────────────────────────────────────────────
void VUMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0a0020));
    g.fillRoundedRectangle (b, 3.f);

    static const float dBMarks[] = { 0.f, -6.f, -12.f, -18.f, -24.f, -36.f, -48.f };
    float clampedLvl = juce::jlimit (0.f, 1.f, level);
    float dBLvl = clampedLvl > 0.f ? juce::Decibels::gainToDecibels (clampedLvl) : -60.f;

    for (int i = 0; i < (int)b.getHeight(); i++)
    {
        float frac = 1.f - i / b.getHeight();
        float segDB = juce::jmap (frac, 0.f, 1.f, -48.f, 0.f);
        if (segDB > dBLvl) continue;
        juce::Colour c = segDB > -6.f ? juce::Colour(0xffff3333)
                        : segDB > -18.f ? kGreen
                        : juce::Colour(0xff226622);
        g.setColour (c);
        g.fillRect (b.getX() + 2, b.getY() + i, b.getWidth() - 4, 1.f);
    }

    // dB marks
    g.setFont (7.f);
    for (float mark : dBMarks) {
        float py = b.getY() + b.getHeight() * (1.f - juce::jmap (mark, -48.f, 0.f, 0.f, 1.f));
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.drawHorizontalLine ((int)py, b.getX(), b.getRight());
    }
}

// ─── DelayVisualizer ─────────────────────────────────────────────────────────
void DelayVisualizer::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0a0020));
    g.fillRoundedRectangle (b, 4.f);

    juce::Path wave;
    const int W = getWidth();
    const float cy = b.getCentreY();
    const float amp = b.getHeight() * 0.35f;
    bool started = false;

    for (int px = 0; px < W; px++)
    {
        float x = px / (float)W * juce::MathConstants<float>::twoPi * 4.f + phase;
        float decay = std::exp (-px / (float)W * 3.f);
        float y = cy + std::sin (x) * amp * decay;
        if (!started) { wave.startNewSubPath ((float)px, y); started = true; }
        else wave.lineTo ((float)px, y);
    }

    g.setColour (kPurpleMid.withAlpha (0.4f));
    juce::Path fill = wave;
    fill.lineTo ((float)W, cy); fill.lineTo (0.f, cy); fill.closeSubPath();
    g.fillPath (fill);
    g.setColour (kGreen.withAlpha (0.8f));
    g.strokePath (wave, juce::PathStrokeType (1.5f));
}

// ─── Editor ──────────────────────────────────────────────────────────────────
JuiceGangEditor::JuiceGangEditor (JuiceGangProcessor& p)
    : AudioProcessorEditor (&p), proc (p), filterCurve (p.apvts), reverbEQCurve (p.apvts)
{
    setSize (980, 800);
    setLookAndFeel (&laf);

    auto& a = p.apvts;

    // ── Bypass — wired to straw click, button intentionally hidden ──
    bypassBtn.setClickingTogglesState (true);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, "bypass", bypassBtn);

    // ── Filter ──
    addAndMakeVisible (filterOnBtn);
    filterOnBtn.setClickingTogglesState (true);
    filterOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, "filterOn", filterOnBtn);

    addAndMakeVisible (filterModeBox);
    filterModeBox.addItem ("LOW PASS",  1);
    filterModeBox.addItem ("HIGH PASS", 2);
    filterModeBox.addItem ("BAND PASS", 3);
    filterModeBox.addItem ("NOTCH",     4);
    filterModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (a, "filterMode", filterModeBox);

    auto setupKnob = [&](juce::Slider& s) {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
        s.setColour (juce::Slider::textBoxTextColourId, kTextDark);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (s);
    };
    auto setupLabel = [&](juce::Label& l, const juce::String& t) {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::Font (9.5f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, kTextDark);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    setupKnob (cutoffKnob);     setupLabel (cutoffLbl,    "CUTOFF");
    setupKnob (resonanceKnob);  setupLabel (resonanceLbl, "RESONANCE");
    setupKnob (driveKnob);      setupLabel (driveLbl,     "DRIVE");
    setupKnob (filterMixKnob);  setupLabel (filterMixLbl, "MIX");
    setupKnob (filterOutKnob);  setupLabel (filterOutLbl, "OUTPUT");

    cutoffAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "cutoff",    cutoffKnob);
    resonanceAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "resonance", resonanceKnob);
    driveAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "drive",     driveKnob);
    filterMixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "filterMix", filterMixKnob);
    filterOutAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "filterOut", filterOutKnob);

    addAndMakeVisible (filterCurve);

    // ── Delay ──
    addAndMakeVisible (delayOnBtn);  delayOnBtn.setClickingTogglesState (true);
    addAndMakeVisible (delaySyncBtn); delaySyncBtn.setClickingTogglesState (true);
    addAndMakeVisible (pingPongBtn);  pingPongBtn.setClickingTogglesState (true);
    delayOnAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, "delayOn",  delayOnBtn);
    delaySyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, "delaySync",delaySyncBtn);
    pingPongAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, "pingPong", pingPongBtn);

    addAndMakeVisible (delaySyncDivBox);
    for (auto& s : { "1/4","1/8","1/16","1/32","DOTTED","TRIPLET" })
        delaySyncDivBox.addItem (s, delaySyncDivBox.getNumItems() + 1);
    delaySyncDivAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (a, "delaySyncDiv", delaySyncDivBox);

    setupKnob (delayTimeKnob); setupLabel (delayTimeLbl, "TIME");
    setupKnob (delayFbKnob);   setupLabel (delayFbLbl,   "FEEDBACK");
    setupKnob (delayLCKnob);   setupLabel (delayLCLbl,   "LOW CUT");
    setupKnob (delayHCKnob);   setupLabel (delayHCLbl,   "HIGH CUT");
    setupKnob (delayMixKnob);  setupLabel (delayMixLbl,  "MIX");

    delayTimeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "delayTime",     delayTimeKnob);
    delayFbAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "delayFeedback", delayFbKnob);
    delayLCAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "delayLowCut",   delayLCKnob);
    delayHCAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "delayHighCut",  delayHCKnob);
    delayMixAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "delayMix",      delayMixKnob);
    addAndMakeVisible (delayVis);

    // ── Reverb ──
    addAndMakeVisible (reverbOnBtn);  reverbOnBtn.setClickingTogglesState (true);
    addAndMakeVisible (freezeBtn);    freezeBtn.setClickingTogglesState (true);
    addAndMakeVisible (reverbEqOnBtn);reverbEqOnBtn.setClickingTogglesState (true);
    reverbOnAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, "reverbOn",   reverbOnBtn);
    freezeAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, "freeze",     freezeBtn);
    reverbEqOnAtt= std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, "reverbEqOn", reverbEqOnBtn);

    setupKnob (revSizeKnob);   setupLabel (revSizeLbl,   "SIZE");
    setupKnob (revDecayKnob);  setupLabel (revDecayLbl,  "DECAY");
    setupKnob (preDelayKnob);  setupLabel (preDelayLbl,  "PRE-DELAY");
    setupKnob (revDampKnob);   setupLabel (revDampLbl,   "DAMPING");
    setupKnob (revMixKnob);    setupLabel (revMixLbl,    "MIX");

    revSizeAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "reverbSize",  revSizeKnob);
    revDecayAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "reverbDecay", revDecayKnob);
    preDelayAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "preDelay",    preDelayKnob);
    revDampAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "reverbDamp",  revDampKnob);
    revMixAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "reverbMix",   revMixKnob);

    // Reverb EQ
    setupKnob (revLCKnob);  setupLabel (revLCLbl,  "LOW CUT");
    setupKnob (revLSKnob);  setupLabel (revLSLbl,  "LOW SHELF");
    setupKnob (revMidKnob); setupLabel (revMidLbl, "MID");
    setupKnob (revHSKnob);  setupLabel (revHSLbl,  "HIGH SHELF");
    setupKnob (revHCKnob);  setupLabel (revHCLbl,  "HIGH CUT");

    revLCAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "revLowCut",    revLCKnob);
    revLSAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "revLowShelf",  revLSKnob);
    revMidAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "revMid",       revMidKnob);
    revHSAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "revHighShelf", revHSKnob);
    revHCAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "revHighCut",   revHCKnob);
    addAndMakeVisible (reverbEQCurve);

    // ── LFO ──
    addAndMakeVisible (lfoTargetBox);
    lfoTargetBox.addItem ("CUTOFF",     1);
    lfoTargetBox.addItem ("DELAY TIME", 2);
    lfoTargetBox.addItem ("REVERB MIX", 3);
    lfoTargetAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (a, "lfoTarget", lfoTargetBox);

    addAndMakeVisible (lfoShapeBox);
    for (auto& s : { "~","∧","▭","/" }) lfoShapeBox.addItem (s, lfoShapeBox.getNumItems() + 1);
    lfoShapeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (a, "lfoShape", lfoShapeBox);

    addAndMakeVisible (lfoSyncBtn); lfoSyncBtn.setClickingTogglesState (true);
    lfoSyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, "lfoSync", lfoSyncBtn);

    addAndMakeVisible (lfoSyncDivBox);
    for (auto& s : { "1/4","1/8","1/16","1/32","DOTTED","TRIPLET" })
        lfoSyncDivBox.addItem (s, lfoSyncDivBox.getNumItems() + 1);
    lfoSyncDivAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (a, "lfoSyncDiv", lfoSyncDivBox);

    setupKnob (lfoRateKnob);  setupLabel (lfoRateLbl,  "RATE");
    setupKnob (lfoDepthKnob); setupLabel (lfoDepthLbl, "DEPTH");
    lfoRateAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "lfoRate",  lfoRateKnob);
    lfoDepthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "lfoDepth", lfoDepthKnob);

    // ── Master ──
    setupKnob (masterInKnob);  setupLabel (masterInLbl,  "INPUT");
    setupKnob (masterMixKnob); setupLabel (masterMixLbl, "MIX");
    setupKnob (masterOutKnob); setupLabel (masterOutLbl, "OUTPUT");
    masterInAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "masterIn",  masterInKnob);
    masterMixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "masterMix", masterMixKnob);
    masterOutAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "masterOut", masterOutKnob);
    addAndMakeVisible (vuMeter);

    // ── Preset bar ──
    for (auto* b2 : { &prevPresetBtn, &nextPresetBtn, &savePresetBtn, &deletePresetBtn, &presetsBtn })
        addAndMakeVisible (b2);
    addAndMakeVisible (presetNameLabel);
    presetNameLabel.setText ("Default Preset", juce::dontSendNotification);
    presetNameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    presetNameLabel.setFont (juce::Font (12.f, juce::Font::bold));
    presetNameLabel.setJustificationType (juce::Justification::centred);

    startTimerHz (30);
}

JuiceGangEditor::~JuiceGangEditor() { setLookAndFeel (nullptr); }

void JuiceGangEditor::mouseDown (const juce::MouseEvent& e)
{
    // Straw hit area — roughly the gable-top-left zone where the straw lives
    juce::Rectangle<int> strawHit (40, 0, 160, 115);
    if (strawHit.contains (e.getPosition()))
    {
        bypassBtn.setToggleState (!bypassBtn.getToggleState(), juce::sendNotification);
    }
}

void JuiceGangEditor::timerCallback()
{
    vuMeter.setLevel (proc.getOutputLevel());
    vuMeter.repaint();

    // Straw bypass animation
    float bypass = *proc.apvts.getRawParameterValue ("bypass");
    float target = bypass > 0.5f ? 1.f : 0.f;
    float prev = strawDroop;
    strawDroop += (target - strawDroop) * 0.08f;
    if (std::abs (strawDroop - prev) > 0.001f)
        repaint (0, 0, 200, 115);   // only repaint gable area for performance
}

// ─────────────────────────────────────────────────────────────────────────────
void JuiceGangEditor::paint (juce::Graphics& g)
{
    const float W = (float)getWidth(), H = (float)getHeight();
    // Layout constants
    const float sideW  = 36.f;   // side strip width each side
    const float gableH = 100.f;  // carton top gable height
    const float hdrH   = 70.f;   // branding header height below gable
    const float bodyTop = gableH; // where front face begins

    // ═══ CARTON BODY ══════════════════════════════════════════════════════════
    juce::ColourGradient body (juce::Colour(0xff5c0ab0), W * 0.5f, 0.f,
                                juce::Colour(0xff380070), W * 0.5f, H, true);
    g.setGradientFill (body);
    g.fillAll();

    // Subtle vertical shine
    juce::ColourGradient shine (juce::Colours::white.withAlpha (0.07f), W * 0.25f, 0.f,
                                 juce::Colours::transparentBlack, W * 0.58f, 0.f, false);
    g.setGradientFill (shine);
    g.fillAll();

    // ═══ GABLED CARTON TOP ═════════════════════════════════════════════════════
    // Very dark background for the "top face" region
    g.setColour (juce::Colour (0xff110028));
    g.fillRect (0.f, 0.f, W, gableH);

    // Left fold panel – slightly lighter than background, angled
    juce::Path leftPanel;
    leftPanel.startNewSubPath (0.f, gableH);
    leftPanel.lineTo (W * 0.19f, 0.f);
    leftPanel.lineTo (W * 0.48f, 0.f);
    leftPanel.lineTo (W * 0.42f, gableH);
    leftPanel.closeSubPath();
    g.setColour (juce::Colour (0xff3a0878));
    g.fillPath (leftPanel);

    // Right fold panel
    juce::Path rightPanel;
    rightPanel.startNewSubPath (W, gableH);
    rightPanel.lineTo (W * 0.81f, 0.f);
    rightPanel.lineTo (W * 0.52f, 0.f);
    rightPanel.lineTo (W * 0.58f, gableH);
    rightPanel.closeSubPath();
    g.setColour (juce::Colour (0xff3a0878));
    g.fillPath (rightPanel);

    // Bright crease lines — these are the key visual element of the gable
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawLine (W * 0.19f, 0.f, W * 0.42f, gableH, 2.5f);
    g.drawLine (W * 0.81f, 0.f, W * 0.58f, gableH, 2.5f);
    // Bottom crease
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawLine (0.f, gableH, W, gableH, 1.5f);
    // Top edge of fold panels
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawLine (W * 0.19f, 0.f, W * 0.81f, 0.f, 1.f);

    // BYPASS hint label near straw
    {
        bool bypassed = *proc.apvts.getRawParameterValue ("bypass") > 0.5f;
        g.setColour (bypassed ? juce::Colour(0xffff4422).withAlpha(0.9f) : juce::Colours::white.withAlpha(0.55f));
        g.setFont (juce::Font (8.f, juce::Font::bold));
        g.drawText (bypassed ? "BYPASSED" : "TAP STRAW", 155, 8, 90, 14, juce::Justification::centredLeft);
    }

    // ═══ STRAW (animated — droops when bypassed) ══════════════════════════════
    {
        // strawDroop: 0 = upright, 1 = drooped (angled ~45°)
        const float sX = 120.f, sY = -4.f;
        const float strawW = 18.f;
        const float shaft = gableH + 8.f;

        // Rotation pivot at the top of the vertical shaft, droop up to 45 degrees
        float droop = strawDroop * juce::MathConstants<float>::pi * 0.25f;

        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (droop, sX + strawW * 0.5f, sY + strawW * 0.5f));

        // Main white shaft
        g.setColour (juce::Colour (0xfff2f2f2));
        g.fillRoundedRectangle (sX, sY, strawW, shaft, 6.f);
        // Stripe highlight
        g.setColour (juce::Colour (0xffdddddd).withAlpha (0.6f));
        g.drawLine (sX + 6.f, sY + 4.f, sX + 6.f, sY + shaft - 4.f, 1.5f);
        // Bent horizontal portion
        g.setColour (juce::Colour (0xfff2f2f2));
        g.fillRoundedRectangle (sX - 56.f, sY + 2.f, 70.f, strawW, 6.f);
        g.fillEllipse (sX - 2.f, sY, strawW + 4.f, strawW + 4.f);
        // End cap
        g.setColour (juce::Colour (0xffcccccc));
        g.fillEllipse (sX - 56.f, sY + 2.f, strawW, strawW);

        // Droop → tint straw red/orange when bypassed
        if (strawDroop > 0.05f)
        {
            g.setColour (juce::Colour (0xffff4422).withAlpha (strawDroop * 0.45f));
            g.fillRoundedRectangle (sX, sY, strawW, shaft, 6.f);
        }
        g.restoreState();
    }

    // ═══ HEADER / BRANDING ════════════════════════════════════════════════════
    g.setColour (juce::Colour (0xff1a0044));
    g.fillRect (0.f, gableH, W, hdrH);
    // Bottom edge of header
    g.setColour (kGreen.withAlpha (0.4f));
    g.drawLine (0.f, gableH + hdrH, W, gableH + hdrH, 1.5f);

    // JUICE title
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (50.f, juce::Font::bold | juce::Font::italic));
    g.drawText ("JUICE", (int)sideW, (int)gableH + 6, 280, 56, juce::Justification::centredLeft);
    g.setFont (juce::Font (11.f, juce::Font::bold));
    g.setColour (kGreen);
    g.drawText ("NOVA MOTION FX", (int)sideW + 2, (int)gableH + 58, 200, 14, juce::Justification::centredLeft);

    // GANG title
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (50.f, juce::Font::bold | juce::Font::italic));
    g.drawText ("GANG", (int)(W - sideW - 280.f), (int)gableH + 6, 276, 56, juce::Justification::centredRight);
    g.setFont (juce::Font (11.f, juce::Font::bold));
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawText ("LOYALTY IS FAMILY", (int)(W - sideW - 246.f), (int)gableH + 58, 242, 14, juce::Justification::centredRight);

    // $$ badge (circular crest, centred in header)
    const float bcx = W * 0.5f, bcy = gableH + hdrH * 0.5f, bR = 36.f;
    // Glow
    juce::ColourGradient glow (kGreen.withAlpha (0.28f), bcx, bcy,
                                juce::Colours::transparentBlack, bcx + bR + 14.f, bcy, true);
    g.setGradientFill (glow);
    g.fillEllipse (bcx - bR - 12.f, bcy - bR - 12.f, (bR + 12.f) * 2.f, (bR + 12.f) * 2.f);
    // White fill
    g.setColour (juce::Colours::white);
    g.fillEllipse (bcx - bR, bcy - bR, bR * 2.f, bR * 2.f);
    // Green border
    g.setColour (kGreen);
    g.drawEllipse (bcx - bR, bcy - bR, bR * 2.f, bR * 2.f, 3.f);
    // Inner ring
    g.setColour (kGreen.withAlpha (0.3f));
    g.drawEllipse (bcx - bR + 5.f, bcy - bR + 5.f, (bR - 5.f) * 2.f, (bR - 5.f) * 2.f, 1.f);
    // $$ text
    g.setFont (juce::Font (30.f, juce::Font::bold));
    g.setColour (kGreen);
    g.drawText ("$$", (int)(bcx - bR), (int)(bcy - bR), (int)(bR * 2.f), (int)(bR * 2.f),
                juce::Justification::centred);

    // ═══ LEFT SIDE STRIP ══════════════════════════════════════════════════════
    g.setColour (juce::Colour (0xff100030));
    g.fillRect (0.f, bodyTop + hdrH, sideW, H - bodyTop - hdrH - 32.f);

    // Side text "JUICE GANG"
    g.saveState();
    float midSideY = bodyTop + hdrH + (H - bodyTop - hdrH - 32.f) * 0.5f;
    g.addTransform (juce::AffineTransform::rotation (
        -juce::MathConstants<float>::halfPi, sideW * 0.5f, midSideY));
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (juce::Font (10.f, juce::Font::bold));
    g.drawText ("JUICE GANG", (int)(sideW * 0.5f - 60.f), (int)midSideY - 7, 120, 14,
                juce::Justification::centred);
    g.restoreState();

    // Side dollar sign
    g.setFont (juce::Font (22.f, juce::Font::bold));
    g.setColour (kGreen.withAlpha (0.45f));
    g.drawText ("$", 0, (int)(midSideY + 40.f), (int)sideW, 28, juce::Justification::centred);

    // Barcode (bottom of side strip)
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    for (int i = 0; i < 22; i++)
        g.fillRect (3 + i * 2, (int)(H - 110.f), (i % 3 == 0 ? 2 : 1), 18);

    // "MADE FOR CREATORS BY NOVA" rotated
    g.saveState();
    float mfcY = H - 80.f;
    g.addTransform (juce::AffineTransform::rotation (
        -juce::MathConstants<float>::halfPi, sideW * 0.5f, mfcY));
    g.setColour (juce::Colours::white.withAlpha (0.38f));
    g.setFont (juce::Font (7.5f, juce::Font::bold));
    g.drawText ("MADE FOR CREATORS BY NOVA",
                (int)(sideW * 0.5f - 90.f), (int)mfcY - 7, 180, 14, juce::Justification::centred);
    g.restoreState();

    // ═══ RIGHT SIDE STRIP ════════════════════════════════════════════════════
    g.setColour (juce::Colour (0xff100030));
    g.fillRect (W - sideW, bodyTop + hdrH, sideW, H - bodyTop - hdrH - 32.f);

    // ═══ PRESET BAR ═══════════════════════════════════════════════════════════
    g.setColour (juce::Colour (0xff100030));
    g.fillRect (0.f, H - 32.f, W, 32.f);
    g.setColour (kGreen.withAlpha (0.35f));
    g.drawLine (0.f, H - 32.f, W, H - 32.f, 1.5f);

    // ═══ CONDENSATION DROPLETS (Easter egg) ══════════════════════════════════
    // Subtle water drops scattered on the lower body area
    struct Drop { float x, y, rx, ry; };
    static const Drop drops[] = {
        { 54.f,  530.f, 5.f, 8.f }, { 62.f, 548.f, 3.5f, 5.5f },
        { 920.f, 510.f, 4.f, 7.f }, { 928.f, 528.f, 3.f, 4.5f },
        { 48.f,  640.f, 4.5f, 7.f }, { 58.f, 660.f, 2.5f, 4.f },
        { 915.f, 620.f, 5.f, 8.f }, { 924.f, 642.f, 3.f, 5.f },
    };
    for (auto& d : drops)
    {
        juce::ColourGradient dropGrad (juce::Colours::white.withAlpha (0.62f), d.x - d.rx * 0.3f, d.y - d.ry * 0.4f,
                                        juce::Colour (0xffb8a8ff).withAlpha (0.28f), d.x + d.rx, d.y + d.ry, false);
        g.setGradientFill (dropGrad);
        g.fillEllipse (d.x - d.rx, d.y - d.ry, d.rx * 2.f, d.ry * 2.f);
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.drawEllipse (d.x - d.rx, d.y - d.ry, d.rx * 2.f, d.ry * 2.f, 0.6f);
    }

    // ═══ NUTRITION FACTS (Easter egg — right side strip) ════════════════════
    {
        const float nx = W - sideW + 2.f, ny = bodyTop + hdrH + 20.f;
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.fillRect (nx, ny, sideW - 4.f, 80.f);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (juce::Font (4.5f, juce::Font::bold));
        g.drawText ("NUTRITION FACTS", (int)nx, (int)ny + 2, (int)(sideW - 4), 8, juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.setFont (juce::Font (3.8f));
        const char* facts[] = { "Bass 200%", "Mids 0%", "Treble 100%", "Reverb 40g", "Delay 20ms", "Drive 6dB" };
        for (int i = 0; i < 6; ++i)
            g.drawText (facts[i], (int)nx, (int)ny + 12 + i * 11, (int)(sideW - 4), 10, juce::Justification::centred);
    }

    // ═══ SHAKE WELL & EXPIRY (Easter egg — left side strip) ═════════════════
    {
        g.saveState();
        float midLeft = bodyTop + hdrH + (H - bodyTop - hdrH - 80.f) * 0.72f;
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, sideW * 0.5f, midLeft));
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.setFont (juce::Font (6.f, juce::Font::bold | juce::Font::italic));
        g.drawText ("SHAKE WELL BEFORE USE", (int)(sideW * 0.5f - 76.f), (int)midLeft - 7, 152, 14, juce::Justification::centred);
        g.restoreState();

        g.saveState();
        float expiryY = bodyTop + hdrH + (H - bodyTop - hdrH - 80.f) * 0.55f;
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, sideW * 0.5f, expiryY));
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.setFont (juce::Font (5.5f));
        g.drawText ("BEST BEFORE: DEC 2027  |  100% PURE AUDIO", (int)(sideW * 0.5f - 100.f), (int)expiryY - 6, 200, 12, juce::Justification::centred);
        g.restoreState();
    }

    // ═══ PANELS ═══════════════════════════════════════════════════════════════
    auto drawPanel = [&](juce::Rectangle<int> r, const juce::String& title)
    {
        // Frosted acrylic body — layered gradient
        juce::ColourGradient frost (juce::Colour (0xffede6ff), (float)r.getX(), (float)r.getY(),
                                    juce::Colour (0xffd8cef5), (float)r.getRight(), (float)r.getBottom(), false);
        frost.addColour (0.5, juce::Colour (0xffe8e0ff));
        g.setGradientFill (frost);
        g.fillRoundedRectangle (r.toFloat(), 6.f);

        // Very subtle vertical texture lines on frosted surface
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        for (int lx = r.getX() + 8; lx < r.getRight() - 4; lx += 10)
            g.drawLine ((float)lx, (float)(r.getY() + 24), (float)lx, (float)r.getBottom() - 4, 1.f);

        // Gloss highlight on top edge
        juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.38f), (float)r.getX(), (float)(r.getY() + 24),
                                     juce::Colours::transparentWhite, (float)r.getX(), (float)(r.getY() + 48), false);
        g.setGradientFill (gloss);
        g.fillRoundedRectangle (r.toFloat().withHeight (24.f).translated (0, 24), 3.f);

        // Purple header strip
        juce::Rectangle<float> hdrR ((float)r.getX(), (float)r.getY(), (float)r.getWidth(), 24.f);
        juce::ColourGradient hdrGrad (juce::Colour (0xff3a0080), (float)r.getX(), (float)r.getY(),
                                       juce::Colour (0xff1a0040), (float)r.getRight(), (float)r.getY(), false);
        g.setGradientFill (hdrGrad);
        g.fillRoundedRectangle (hdrR, 6.f);
        g.fillRect ((float)r.getX(), hdrR.getY() + 14.f, (float)r.getWidth(), 10.f);
        // Green accent line on header bottom
        g.setColour (juce::Colour (0xff39e65a).withAlpha (0.55f));
        g.drawLine ((float)r.getX(), (float)(r.getY() + 24), (float)r.getRight(), (float)(r.getY() + 24), 1.2f);

        // Border with subtle inner glow
        g.setColour (kPanelBorder.withAlpha (0.45f));
        g.drawRoundedRectangle (r.toFloat(), 6.f, 1.f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (r.toFloat().reduced (1.f), 5.f, 1.f);

        // Title
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (10.f, juce::Font::bold));
        g.drawText (title, r.getX() + 10, r.getY() + 5, 130, 16, juce::Justification::centredLeft);
    };

    // Row 1 (y=174, h=286)
    drawPanel ({36,  174, 296, 286}, "FILTER");
    drawPanel ({338, 174, 296, 286}, "DELAY");
    drawPanel ({640, 174, 304, 286}, "REVERB");

    // Row 2 (y=470)
    drawPanel ({36,  470, 296, 148}, "LFO");
    drawPanel ({640, 470, 304, 148}, "REVERB EQ");

    // Centre logo panel
    {
        juce::Rectangle<float> logo (338.f, 470.f, 296.f, 148.f);
        g.setColour (kPurpleDark.withAlpha (0.85f));
        g.fillRoundedRectangle (logo, 6.f);
        g.setColour (kPanelBorder.withAlpha (0.35f));
        g.drawRoundedRectangle (logo, 6.f, 1.f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (20.f, juce::Font::bold | juce::Font::italic));
        g.drawText ("JUICE", 348, 490, 100, 28, juce::Justification::centredLeft);
        g.drawText ("GANG",  526, 490, 98,  28, juce::Justification::centredRight);

        g.setFont (juce::Font (32.f, juce::Font::bold));
        g.setColour (kGreen);
        g.drawText ("$", 452, 480, 48, 44, juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.drawEllipse (450.f, 478.f, 52.f, 48.f, 1.5f);

        g.setFont (juce::Font (9.f, juce::Font::bold));
        g.setColour (kGreen.withAlpha (0.9f));
        g.drawText ("NOVA AUDIO", 338, 546, 296, 16, juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.setFont (juce::Font (7.5f));
        g.drawText ("MADE FOR CREATORS BY NOVA", 338, 566, 296, 12, juce::Justification::centred);
    }

    // Master label (floats above the master knobs area)
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.setFont (juce::Font (9.f, juce::Font::bold));
    g.drawText ("MASTER", 648, 618, 80, 14, juce::Justification::centredLeft);
}

void JuiceGangEditor::resized()
{
    const int W = getWidth();
    // All panels shifted down to sit below the 170px carton top+header section
    // Left/right margins 36px each for side strips
    // Row 1 panels: y=174, h=286   Row 2 panels: y=470, h=148

    // Bypass button is hidden — bypass toggled by clicking the straw

    auto setCombo = [&](juce::ComboBox& c) {
        c.setColour (juce::ComboBox::backgroundColourId, kPurpleDark);
        c.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
        c.setColour (juce::ComboBox::outlineColourId,    kGreen);
    };

    // ── Filter panel  x=36 y=174 w=296 h=286 ──
    filterOnBtn.setBounds (298, 180, 28, 16);
    filterModeBox.setBounds (42, 200, 90, 18);  setCombo (filterModeBox);

    cutoffKnob.setBounds    (146, 196, 72, 72); cutoffLbl.setBounds    (146, 266, 72, 14);
    resonanceKnob.setBounds (230, 210, 56, 56); resonanceLbl.setBounds (230, 264, 56, 14);
    driveKnob.setBounds     (42,  292, 52, 52); driveLbl.setBounds     (42,  342, 52, 14);
    filterMixKnob.setBounds (112, 292, 52, 52); filterMixLbl.setBounds (112, 342, 52, 14);
    filterOutKnob.setBounds (182, 292, 52, 52); filterOutLbl.setBounds (182, 342, 52, 14);
    filterCurve.setBounds   (42,  356, 282, 96);

    // ── Delay panel  x=338 y=174 w=296 h=286 ──
    delayOnBtn.setBounds (608, 180, 28, 16);
    delaySyncDivBox.setBounds (344, 200, 72, 110);  setCombo (delaySyncDivBox);

    delayTimeKnob.setBounds (432, 196, 72, 72); delayTimeLbl.setBounds (432, 266, 72, 14);
    delayFbKnob.setBounds   (520, 210, 56, 56); delayFbLbl.setBounds   (520, 264, 56, 14);

    delaySyncBtn.setBounds (448, 278, 40, 16);
    pingPongBtn.setBounds  (344, 318, 56, 24);

    delayLCKnob.setBounds  (418, 300, 48, 48); delayLCLbl.setBounds  (418, 346, 48, 14);
    delayHCKnob.setBounds  (476, 300, 48, 48); delayHCLbl.setBounds  (476, 346, 48, 14);
    delayMixKnob.setBounds (534, 300, 48, 48); delayMixLbl.setBounds (534, 346, 48, 14);
    delayVis.setBounds     (344, 358, 282, 94);

    // ── Reverb panel  x=640 y=174 w=304 h=286 ──
    reverbOnBtn.setBounds (916, 180, 28, 16);

    revSizeKnob.setBounds  (648, 200, 56, 56); revSizeLbl.setBounds  (648, 254, 56, 14);
    revDecayKnob.setBounds (722, 192, 72, 72); revDecayLbl.setBounds (722, 262, 72, 14);
    preDelayKnob.setBounds (814, 210, 56, 56); preDelayLbl.setBounds (814, 262, 56, 14);

    revDampKnob.setBounds  (648, 282, 56, 56); revDampLbl.setBounds  (648, 336, 56, 14);
    revMixKnob.setBounds   (724, 282, 56, 56); revMixLbl.setBounds   (724, 336, 56, 14);
    freezeBtn.setBounds    (816, 292, 52, 40);

    // ── Reverb EQ  x=640 y=470 w=304 h=148 ──
    reverbEqOnBtn.setBounds (912, 474, 26, 14);
    revLCKnob.setBounds     (646, 494, 42, 42); revLCLbl.setBounds  (646, 534, 42, 14);
    revLSKnob.setBounds     (696, 494, 42, 42); revLSLbl.setBounds  (696, 534, 42, 14);
    revMidKnob.setBounds    (746, 494, 42, 42); revMidLbl.setBounds (746, 534, 42, 14);
    revHSKnob.setBounds     (796, 494, 42, 42); revHSLbl.setBounds  (796, 534, 42, 14);
    revHCKnob.setBounds     (846, 494, 42, 42); revHCLbl.setBounds  (846, 534, 42, 14);
    reverbEQCurve.setBounds (646, 548, 290, 62);

    // ── LFO panel  x=36 y=470 w=296 h=148 ──
    lfoTargetBox.setBounds (44, 496, 80, 18);  setCombo (lfoTargetBox);
    lfoShapeBox.setBounds  (44, 522, 80, 18);  setCombo (lfoShapeBox);

    lfoRateKnob.setBounds  (148, 480, 60, 60); lfoRateLbl.setBounds  (148, 538, 60, 14);
    lfoDepthKnob.setBounds (222, 480, 60, 60); lfoDepthLbl.setBounds (222, 538, 60, 14);

    lfoSyncBtn.setBounds    (152, 554, 40, 16);
    lfoSyncDivBox.setBounds (198, 554, 60, 16);  setCombo (lfoSyncDivBox);

    // ── Master panel  x=640 y=470 w=304 h=148 — shares row with Rev EQ ──
    // (Rev EQ and Master are in the same area; Master is painted below Rev EQ)
    // Repositioned: Master sits below Reverb EQ display (y≈622)
    masterInKnob.setBounds  (648, 630, 56, 56); masterInLbl.setBounds  (648, 684, 56, 14);
    vuMeter.setBounds       (714, 626, 20, 112);
    masterMixKnob.setBounds (746, 630, 56, 56); masterMixLbl.setBounds (746, 684, 56, 14);
    masterOutKnob.setBounds (824, 630, 56, 56); masterOutLbl.setBounds (824, 684, 56, 14);

    // ── Preset bar ──
    const int PY = getHeight() - 32;
    prevPresetBtn.setBounds    (36,  PY, 30, 24);
    nextPresetBtn.setBounds    (70,  PY, 30, 24);
    presetNameLabel.setBounds  (106, PY, 220, 24);
    savePresetBtn.setBounds    (332, PY, 30, 24);
    deletePresetBtn.setBounds  (366, PY, 30, 24);
    presetsBtn.setBounds       (W - 96, PY, 80, 24);
}
