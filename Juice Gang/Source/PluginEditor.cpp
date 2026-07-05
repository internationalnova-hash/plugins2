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
    setSize (960, 700);
    setLookAndFeel (&laf);

    auto& a = p.apvts;

    // ── Bypass ──
    addAndMakeVisible (bypassBtn);
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

void JuiceGangEditor::timerCallback()
{
    vuMeter.setLevel (proc.getOutputLevel());
    vuMeter.repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
void JuiceGangEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();

    // ── Carton body background ──
    juce::ColourGradient bg (kPurpleDark, 0, 0, kPurpleMid, 0, (float)b.getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // ── Top header bar ──
    g.setColour (kPurpleDark);
    g.fillRect (0, 0, b.getWidth(), 72);

    // Title text
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font ("Arial", 36.f, juce::Font::bold));
    g.drawText ("JUICE", 20, 12, 180, 44, juce::Justification::centredLeft);
    g.drawText ("GANG",  540, 12, 200, 44, juce::Justification::centredLeft);

    g.setFont (juce::Font ("Arial", 13.f, juce::Font::bold));
    g.drawText ("NOVA MOTION FX", 20, 52, 180, 18, juce::Justification::centredLeft);
    g.drawText ("LOYALTY IS FAMILY", 540, 52, 220, 18, juce::Justification::centredLeft);

    // Dollar sign badge
    g.setColour (juce::Colours::white);
    g.drawEllipse (420.f, 8.f, 80.f, 56.f, 2.f);
    g.setFont (juce::Font (28.f, juce::Font::bold));
    g.setColour (kGreen);
    g.drawText ("$", 430, 16, 60, 40, juce::Justification::centred);

    // ── Side decoration ──
    g.setColour (kPurpleDark.brighter(0.1f));
    g.fillRect (0, 72, 12, b.getHeight() - 100);
    g.fillRect (b.getWidth() - 12, 72, 12, b.getHeight() - 100);

    // Straw
    g.setColour (juce::Colours::white);
    g.fillRoundedRectangle (420.f, 0.f, 12.f, 40.f, 3.f);

    // ── Preset bar bg ──
    g.setColour (kPurpleDark);
    g.fillRect (0, b.getHeight() - 34, b.getWidth(), 34);

    // ── Panel borders ──
    auto drawPanel = [&](juce::Rectangle<int> r, const juce::String& title, bool hasOnBtn = false) {
        g.setColour (kPanelBg.withAlpha (0.92f));
        g.fillRoundedRectangle (r.toFloat(), 6.f);
        g.setColour (kPanelBorder);
        g.drawRoundedRectangle (r.toFloat(), 6.f, 1.5f);
        g.setColour (kTextDark);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText (title, r.getX() + 8, r.getY() + 5, 120, 16, juce::Justification::centredLeft);
    };

    // Filter panel
    drawPanel ({8, 74, 308, 270}, "FILTER");
    // Delay panel
    drawPanel ({322, 74, 308, 270}, "DELAY");
    // Reverb panel
    drawPanel ({636, 74, 316, 270}, "REVERB");
    // LFO panel
    drawPanel ({8, 348, 310, 120}, "LFO");
    // Master panel
    drawPanel ({636, 348, 316, 120}, "MASTER");

    // OPEN tab on carton
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (juce::Font (8.f));
    g.drawText ("OPEN ▶", 8, 4, 60, 12, juce::Justification::centredLeft);

    // Side text
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, 6.f, 300.f));
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.setFont (juce::Font (9.f, juce::Font::bold));
    g.drawText ("JUICE GANG", -80, 296, 160, 14, juce::Justification::centred);
    g.restoreState();

    // Made for creators tagline
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.setFont (juce::Font (7.f));
    g.drawText ("MADE FOR\nCREATORS\nBY NOVA", 0, 400, 14, 50, juce::Justification::centred);
}

void JuiceGangEditor::resized()
{
    const int W = getWidth();

    // ── Bypass ──
    bypassBtn.setBounds (W - 70, 12, 62, 24);

    // ── Filter panel (x=8, y=74, w=308, h=270) ──
    filterOnBtn.setBounds (274, 80, 36, 16);

    filterModeBox.setBounds (14, 100, 90, 16);
    filterModeBox.setColour (juce::ComboBox::backgroundColourId, kPurpleDark);
    filterModeBox.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
    filterModeBox.setColour (juce::ComboBox::outlineColourId,    kGreen);

    cutoffKnob.setBounds    (110, 92, 68, 68);  cutoffLbl.setBounds    (110, 158, 68, 14);
    resonanceKnob.setBounds (188, 108, 52, 52); resonanceLbl.setBounds (188, 158, 52, 14);
    driveKnob.setBounds     (14,  192, 48, 48); driveLbl.setBounds     (14,  238, 48, 14);
    filterMixKnob.setBounds (86,  192, 48, 48); filterMixLbl.setBounds (86,  238, 48, 14);
    filterOutKnob.setBounds (158, 192, 48, 48); filterOutLbl.setBounds (158, 238, 48, 14);

    filterCurve.setBounds   (14, 254, 296, 82);

    // ── Delay panel (x=322, y=74, w=308, h=270) ──
    delayOnBtn.setBounds  (594, 80, 30, 16);

    delaySyncDivBox.setBounds (330, 100, 68, 100);
    delaySyncDivBox.setColour (juce::ComboBox::backgroundColourId, kPurpleDark);
    delaySyncDivBox.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
    delaySyncDivBox.setColour (juce::ComboBox::outlineColourId,    kGreen);

    delayTimeKnob.setBounds (404, 92, 68, 68); delayTimeLbl.setBounds (404, 158, 68, 14);
    delayFbKnob.setBounds   (500, 108, 52, 52); delayFbLbl.setBounds   (500, 158, 52, 14);

    delaySyncBtn.setBounds (420, 178, 36, 14);
    pingPongBtn.setBounds  (330, 208, 52, 22);

    delayLCKnob.setBounds  (394, 196, 44, 44); delayLCLbl.setBounds  (394, 238, 44, 14);
    delayHCKnob.setBounds  (452, 196, 44, 44); delayHCLbl.setBounds  (452, 238, 44, 14);
    delayMixKnob.setBounds (510, 196, 44, 44); delayMixLbl.setBounds (510, 238, 44, 14);

    delayVis.setBounds (330, 254, 292, 82);

    // ── Reverb panel (x=636, y=74, w=316, h=270) ──
    reverbOnBtn.setBounds (910, 80, 30, 16);

    revSizeKnob.setBounds  (644, 100, 52, 52); revSizeLbl.setBounds  (644, 150, 52, 14);
    revDecayKnob.setBounds (716, 92,  68, 68); revDecayLbl.setBounds (716, 158, 68, 14);
    preDelayKnob.setBounds (808, 108, 52, 52); preDelayLbl.setBounds (808, 158, 52, 14);

    revDampKnob.setBounds  (644, 180, 52, 52); revDampLbl.setBounds  (644, 230, 52, 14);
    revMixKnob.setBounds   (720, 180, 52, 52); revMixLbl.setBounds   (720, 230, 52, 14);
    freezeBtn.setBounds    (810, 188, 44, 36);

    // Reverb EQ sub-panel within reverb section
    reverbEqOnBtn.setBounds (898, 348, 48, 14);
    revLCKnob.setBounds     (642, 370, 40, 40); revLCLbl.setBounds  (642, 408, 40, 14);
    revLSKnob.setBounds     (692, 370, 40, 40); revLSLbl.setBounds  (692, 408, 40, 14);
    revMidKnob.setBounds    (742, 370, 40, 40); revMidLbl.setBounds (742, 408, 40, 14);
    revHSKnob.setBounds     (792, 370, 40, 40); revHSLbl.setBounds  (792, 408, 40, 14);
    revHCKnob.setBounds     (842, 370, 40, 40); revHCLbl.setBounds  (842, 408, 40, 14);
    reverbEQCurve.setBounds (642, 420, 300, 54);

    // ── LFO panel (x=8, y=348, w=310, h=120) ──
    lfoTargetBox.setBounds (16, 370, 72, 16);
    lfoTargetBox.setColour (juce::ComboBox::backgroundColourId, kPurpleDark);
    lfoTargetBox.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
    lfoTargetBox.setColour (juce::ComboBox::outlineColourId,    kGreen);

    lfoShapeBox.setBounds  (16, 394, 72, 16);
    lfoShapeBox.setColour  (juce::ComboBox::backgroundColourId, kPurpleDark);
    lfoShapeBox.setColour  (juce::ComboBox::textColourId,       juce::Colours::white);
    lfoShapeBox.setColour  (juce::ComboBox::outlineColourId,    kGreen);

    lfoRateKnob.setBounds  (118, 356, 56, 56); lfoRateLbl.setBounds  (118, 410, 56, 14);
    lfoDepthKnob.setBounds (186, 356, 56, 56); lfoDepthLbl.setBounds (186, 410, 56, 14);

    lfoSyncBtn.setBounds    (130, 420, 36, 14);
    lfoSyncDivBox.setBounds (172, 420, 56, 14);
    lfoSyncDivBox.setColour (juce::ComboBox::backgroundColourId, kPurpleDark);
    lfoSyncDivBox.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
    lfoSyncDivBox.setColour (juce::ComboBox::outlineColourId,    kGreen);

    // ── Logo area (centre bottom) ──
    // (painted only, no controls)

    // ── Master panel (x=636, y=348, w=316, h=120) ──
    masterInKnob.setBounds  (648, 356, 52, 52); masterInLbl.setBounds  (648, 406, 52, 14);
    vuMeter.setBounds       (710, 356, 18, 104);
    masterMixKnob.setBounds (740, 356, 52, 52); masterMixLbl.setBounds (740, 406, 52, 14);
    masterOutKnob.setBounds (808, 356, 52, 52); masterOutLbl.setBounds (808, 406, 52, 14);

    // ── Preset bar ──
    const int PY = getHeight() - 30;
    prevPresetBtn.setBounds    (8,   PY, 28, 22);
    nextPresetBtn.setBounds    (40,  PY, 28, 22);
    presetNameLabel.setBounds  (72,  PY, 200, 22);
    savePresetBtn.setBounds    (278, PY, 28, 22);
    deletePresetBtn.setBounds  (310, PY, 28, 22);
    presetsBtn.setBounds       (W - 80, PY, 72, 22);
}
