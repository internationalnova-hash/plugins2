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

    // Juice fill — purple liquid filling from the bottom up to the curve
    juce::Path fill = curve;
    fill.lineTo (b.getRight(), b.getBottom());
    fill.lineTo (b.getX(), b.getBottom());
    fill.closeSubPath();

    juce::ColourGradient juice (juce::Colour (0xff6a0dad).withAlpha (0.85f), b.getX(), b.getCentreY(),
                                 juce::Colour (0xff3a0060).withAlpha (0.95f), b.getX(), b.getBottom(), false);
    juice.addColour (0.0, juce::Colour (0xff9a20e0).withAlpha (0.6f));
    g.setGradientFill (juice);
    g.fillPath (fill);

    // Juice surface shimmer
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillPath (fill);

    // Green surface line (rim of the juice)
    g.setColour (kGreen);
    g.strokePath (curve, juce::PathStrokeType (2.2f));

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
    juce::ColourGradient revJuice (juce::Colour (0xff5a0a96).withAlpha (0.7f), b.getX(), b.getY(),
                                    juce::Colour (0xff2a0050).withAlpha (0.9f), b.getX(), b.getBottom(), false);
    g.setGradientFill (revJuice);
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

    // Juice fill below wave
    juce::Path fill = wave;
    fill.lineTo ((float)W, b.getBottom()); fill.lineTo (0.f, b.getBottom()); fill.closeSubPath();
    juce::ColourGradient delayJuice (juce::Colour (0xff7a10b0).withAlpha (0.75f), 0.f, cy,
                                      juce::Colour (0xff3a0060).withAlpha (0.95f), 0.f, b.getBottom(), false);
    g.setGradientFill (delayJuice);
    g.fillPath (fill);

    // Ripple circles at wave peaks
    for (int px = 20; px < W; px += 60)
    {
        float x2 = px / (float)W * juce::MathConstants<float>::twoPi * 4.f + phase;
        float decay = std::exp (-px / (float)W * 3.f);
        float ry = cy + std::sin (x2) * amp * decay;
        float rr = 6.f * decay;
        g.setColour (kGreen.withAlpha (0.3f * decay));
        g.drawEllipse ((float)px - rr, ry - rr * 0.4f, rr * 2.f, rr * 0.8f, 1.f);
    }

    g.setColour (kGreen.withAlpha (0.9f));
    g.strokePath (wave, juce::PathStrokeType (1.8f));
}

// ─── Editor ──────────────────────────────────────────────────────────────────
JuiceGangEditor::JuiceGangEditor (JuiceGangProcessor& p)
    : AudioProcessorEditor (&p), proc (p), filterCurve (p.apvts), reverbEQCurve (p.apvts)
{
    setSize (720, 580);
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

    // ── SIP macro ──
    sipKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    sipKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    sipKnob.setColour (juce::Slider::textBoxTextColourId, kGreen);
    sipKnob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (sipKnob);
    sipLbl.setText ("SIP", juce::dontSendNotification);
    sipLbl.setFont (juce::Font (13.f, juce::Font::bold));
    sipLbl.setColour (juce::Label::textColourId, kGreen);
    sipLbl.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (sipLbl);
    sipAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "sip", sipKnob);

    // ── JUICE Button ──
    addAndMakeVisible (juiceBtn);
    juiceBtn.setClickingTogglesState (false);
    juiceBtn.onClick = [this]()
    {
        if (!juiceActive)
        {
            juiceActive = true;
            juiceTimer  = 0.f;
        }
    };

    // ── Preset bar ──
    for (auto* b2 : { &prevPresetBtn, &nextPresetBtn, &savePresetBtn, &deletePresetBtn, &presetsBtn })
        addAndMakeVisible (b2);
    addAndMakeVisible (presetNameLabel);
    presetNameLabel.setText ("-- Select a Flavor --", juce::dontSendNotification);
    presetNameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    presetNameLabel.setFont (juce::Font (12.f, juce::Font::bold));
    presetNameLabel.setJustificationType (juce::Justification::centred);

    // Preset browser overlay (hidden initially)
    addChildComponent (presetBrowser);
    presetBrowser.onPresetSelected = [this](int idx)
    {
        currentPresetIdx = idx;
        applyPreset (kJuicePresets[idx], proc.apvts);
        presetNameLabel.setText (kJuicePresets[idx].name, juce::dontSendNotification);
    };
    presetBrowser.onClose = [this]()
    {
        presetBrowser.setVisible (false);
    };

    // Prev/next preset navigation
    prevPresetBtn.onClick = [this]()
    {
        int next = (currentPresetIdx <= 0) ? kNumPresets - 1 : currentPresetIdx - 1;
        currentPresetIdx = next;
        applyPreset (kJuicePresets[next], proc.apvts);
        presetNameLabel.setText (kJuicePresets[next].name, juce::dontSendNotification);
        presetBrowser.setLoadedPreset (next);
    };
    nextPresetBtn.onClick = [this]()
    {
        int next = (currentPresetIdx + 1) % kNumPresets;
        currentPresetIdx = next;
        applyPreset (kJuicePresets[next], proc.apvts);
        presetNameLabel.setText (kJuicePresets[next].name, juce::dontSendNotification);
        presetBrowser.setLoadedPreset (next);
    };
    presetsBtn.onClick = [this]()
    {
        presetBrowser.setVisible (!presetBrowser.isVisible());
        if (presetBrowser.isVisible()) presetBrowser.toFront (false);
    };

    startTimerHz (30);
}

JuiceGangEditor::~JuiceGangEditor() { setLookAndFeel (nullptr); }

void JuiceGangEditor::mouseDown (const juce::MouseEvent& e)
{
    // Straw hit area — gable-top-left zone
    juce::Rectangle<int> strawHit (30, 0, 120, 78);
    if (strawHit.contains (e.getPosition()))
    {
        bypassBtn.setToggleState (!bypassBtn.getToggleState(), juce::sendNotification);
    }
}

void JuiceGangEditor::timerCallback()
{
    vuMeter.setLevel (proc.getOutputLevel());
    vuMeter.repaint();

    // ── JUICE button transition automation ──
    if (juiceActive)
    {
        juiceTimer += 1.f / 30.f;   // seconds elapsed (30fps timer)
        juicePhase = juce::jmin (1.f, juiceTimer / 4.f);  // 4-second sweep

        auto setParam = [&](const juce::String& id, float val)
        {
            if (auto* p = proc.apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (val));
        };

        if (juiceTimer < 1.0f)
        {
            // Phase 1: filter opens up + resonance rises
            float t = juiceTimer;
            setParam ("filterOn",  1.f);
            setParam ("cutoff",    juce::jmap (t, 0.f, 1.f, 200.f,  8000.f));
            setParam ("resonance", juce::jmap (t, 0.f, 1.f, 20.f,   85.f));
        }
        else if (juiceTimer < 2.0f)
        {
            // Phase 2: delay and reverb fade in
            float t = juiceTimer - 1.f;
            setParam ("delayOn",  1.f);
            setParam ("delayMix", juce::jmap (t, 0.f, 1.f, 0.f, 60.f));
            setParam ("reverbOn", 1.f);
            setParam ("reverbMix",juce::jmap (t, 0.f, 1.f, 0.f, 55.f));
        }
        else if (juiceTimer < 3.0f)
        {
            // Phase 3: drive kicks in, filter sweeps back down
            float t = juiceTimer - 2.f;
            setParam ("drive",  juce::jmap (t, 0.f, 1.f, 0.f, 8.f));
            setParam ("cutoff", juce::jmap (t, 0.f, 1.f, 8000.f, 800.f));
        }
        else if (juiceTimer < 4.0f)
        {
            // Phase 4: everything fades back to rest
            float t = juiceTimer - 3.f;
            setParam ("resonance", juce::jmap (t, 0.f, 1.f, 85.f, 20.f));
            setParam ("delayMix",  juce::jmap (t, 0.f, 1.f, 60.f, 30.f));
            setParam ("reverbMix", juce::jmap (t, 0.f, 1.f, 55.f, 30.f));
            setParam ("drive",     juce::jmap (t, 0.f, 1.f, 8.f,  0.f));
        }
        else
        {
            juiceActive = false;
            juicePhase  = 0.f;
        }

        repaint (338, 470, 296, 148);  // centre panel only
    }

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
    // Layout constants — compact 720×580
    const float sideW  = 26.f;
    const float gableH = 72.f;
    const float hdrH   = 50.f;
    const float bodyTop = gableH;

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
        g.setFont (juce::Font (7.f, juce::Font::bold));
        g.drawText (bypassed ? "BYPASSED" : "TAP STRAW", 110, 6, 80, 12, juce::Justification::centredLeft);
    }

    // ═══ STRAW (animated — droops when bypassed) ══════════════════════════════
    {
        const float sX = 86.f, sY = -3.f;
        const float strawW = 13.f;
        const float shaft = gableH + 6.f;

        // Rotation pivot at the top of the vertical shaft, droop up to 45 degrees
        float droop = strawDroop * juce::MathConstants<float>::pi * 0.25f;

        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (droop, sX + strawW * 0.5f, sY + strawW * 0.5f));

        g.setColour (juce::Colour (0xfff2f2f2));
        g.fillRoundedRectangle (sX, sY, strawW, shaft, 5.f);
        g.setColour (juce::Colour (0xffdddddd).withAlpha (0.6f));
        g.drawLine (sX + 4.f, sY + 3.f, sX + 4.f, sY + shaft - 3.f, 1.f);
        g.setColour (juce::Colour (0xfff2f2f2));
        g.fillRoundedRectangle (sX - 40.f, sY + 1.f, 52.f, strawW, 4.f);
        g.fillEllipse (sX - 2.f, sY, strawW + 2.f, strawW + 2.f);
        g.setColour (juce::Colour (0xffcccccc));
        g.fillEllipse (sX - 40.f, sY + 1.f, strawW, strawW);

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
    g.setFont (juce::Font (36.f, juce::Font::bold | juce::Font::italic));
    g.drawText ("JUICE", (int)sideW, (int)gableH + 4, 200, 40, juce::Justification::centredLeft);
    g.setFont (juce::Font (8.5f, juce::Font::bold));
    g.setColour (kGreen);
    g.drawText ("JUICE MOTION FX", (int)sideW + 2, (int)gableH + 42, 160, 12, juce::Justification::centredLeft);

    // GANG title
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (36.f, juce::Font::bold | juce::Font::italic));
    g.drawText ("GANG", (int)(W - sideW - 200.f), (int)gableH + 4, 196, 40, juce::Justification::centredRight);
    g.setFont (juce::Font (8.5f, juce::Font::bold));
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawText ("LOYALTY IS FAMILY", (int)(W - sideW - 178.f), (int)gableH + 42, 174, 12, juce::Justification::centredRight);

    // $$ badge (circular crest, centred in header)
    const float bcx = W * 0.5f, bcy = gableH + hdrH * 0.5f, bR = 26.f;
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
    g.setFont (juce::Font (22.f, juce::Font::bold));
    g.setColour (kGreen);
    g.drawText ("$$", (int)(bcx - bR), (int)(bcy - bR), (int)(bR * 2.f), (int)(bR * 2.f),
                juce::Justification::centred);

    // ═══ LEFT SIDE STRIP ══════════════════════════════════════════════════════
    g.setColour (juce::Colour (0xff100030));
    g.fillRect (0.f, bodyTop + hdrH, sideW, H - bodyTop - hdrH - 26.f);

    g.saveState();
    float midSideY = bodyTop + hdrH + (H - bodyTop - hdrH - 26.f) * 0.5f;
    g.addTransform (juce::AffineTransform::rotation (
        -juce::MathConstants<float>::halfPi, sideW * 0.5f, midSideY));
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (juce::Font (8.f, juce::Font::bold));
    g.drawText ("JUICE GANG", (int)(sideW * 0.5f - 42.f), (int)midSideY - 5, 84, 12,
                juce::Justification::centred);
    g.restoreState();

    // Barcode
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    for (int i = 0; i < 16; i++)
        g.fillRect (3 + i * 1, (int)(H - 80.f), (i % 3 == 0 ? 2 : 1), 14);

    // ═══ RIGHT SIDE STRIP ════════════════════════════════════════════════════
    g.setColour (juce::Colour (0xff100030));
    g.fillRect (W - sideW, bodyTop + hdrH, sideW, H - bodyTop - hdrH - 26.f);

    // ═══ PRESET BAR ═══════════════════════════════════════════════════════════
    g.setColour (juce::Colour (0xff100030));
    g.fillRect (0.f, H - 26.f, W, 26.f);
    g.setColour (kGreen.withAlpha (0.35f));
    g.drawLine (0.f, H - 26.f, W, H - 26.f, 1.f);

    // ═══ CONDENSATION DROPLETS (Easter egg) ══════════════════════════════════
    // Subtle water drops scattered on the lower body area
    struct Drop { float x, y, rx, ry; };
    static const Drop drops[] = {
        { 8.f,  380.f, 3.5f, 5.5f }, { 14.f, 396.f, 2.5f, 4.f },
        { 704.f,370.f, 3.f,  5.f  }, { 710.f,388.f, 2.f,  3.5f},
        { 6.f,  470.f, 4.f,  6.f  }, { 13.f, 488.f, 2.5f, 4.f },
        { 706.f,460.f, 3.5f, 5.5f }, { 712.f,478.f, 2.f,  3.5f},
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

    // Easter egg: rotated text on left strip
    {
        g.saveState();
        float midLeft = bodyTop + hdrH + (H - bodyTop - hdrH - 50.f) * 0.62f;
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, sideW * 0.5f, midLeft));
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.setFont (juce::Font (5.5f, juce::Font::bold | juce::Font::italic));
        g.drawText ("SHAKE WELL BEFORE USE", (int)(sideW * 0.5f - 60.f), (int)midLeft - 5, 120, 11, juce::Justification::centred);
        g.restoreState();
    }

    // ═══ PANELS ═══════════════════════════════════════════════════════════════
    auto drawPanel = [&](juce::Rectangle<int> r, const juce::String& title)
    {
        // ── Drop shadow (nutrition label stuck on carton) ──────────────────
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillRoundedRectangle (r.toFloat().translated (3.f, 4.f), 7.f);

        // ── Paper/label body — cream-white, very slight warm tint ─────────
        juce::ColourGradient paper (juce::Colour (0xfffff9f0), (float)r.getX(), (float)r.getY(),
                                     juce::Colour (0xfffaf3e8), (float)r.getRight(), (float)r.getBottom(), false);
        g.setGradientFill (paper);
        g.fillRoundedRectangle (r.toFloat(), 7.f);

        // Paper texture — very faint horizontal grain lines
        g.setColour (juce::Colour (0xff000000).withAlpha (0.025f));
        for (int ly = r.getY() + 28; ly < r.getBottom() - 4; ly += 5)
            g.drawHorizontalLine (ly, (float)(r.getX() + 6), (float)(r.getRight() - 6));

        // ── Purple header strip (printed title bar) ────────────────────────
        juce::Rectangle<float> hdrR ((float)r.getX(), (float)r.getY(), (float)r.getWidth(), 26.f);
        juce::ColourGradient hdrGrad (juce::Colour (0xff3a0080), (float)r.getX(), (float)r.getY(),
                                       juce::Colour (0xff1a0040), (float)r.getRight(), (float)r.getY(), false);
        g.setGradientFill (hdrGrad);
        g.fillRoundedRectangle (hdrR, 7.f);
        g.fillRect ((float)r.getX(), hdrR.getY() + 14.f, (float)r.getWidth(), 12.f);

        // Green accent line at header bottom
        g.setColour (kGreen.withAlpha (0.6f));
        g.drawLine ((float)(r.getX() + 6), (float)(r.getY() + 26), (float)(r.getRight() - 6), (float)(r.getY() + 26), 1.5f);

        // ── Outer border — dark, like a label edge ─────────────────────────
        g.setColour (juce::Colour (0xff220044).withAlpha (0.55f));
        g.drawRoundedRectangle (r.toFloat(), 7.f, 1.5f);

        // Title
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (10.f, juce::Font::bold));
        g.drawText (title, r.getX() + 10, r.getY() + 5, 160, 18, juce::Justification::centredLeft);
    };

    // Row 1 (y=122, h=200)
    drawPanel ({26,  122, 210, 200}, "FILTER");
    drawPanel ({240, 122, 210, 200}, "DELAY");
    drawPanel ({454, 122, 240, 200}, "REVERB");

    // Row 2 (y=326, h=108)
    drawPanel ({26,  326, 210, 108}, "LFO");
    drawPanel ({454, 326, 240, 108}, "REVERB EQ");

    // ── Centre panel: SIP macro ───────────────────────────────────────────────
    {
        juce::Rectangle<float> logo (240.f, 326.f, 210.f, 108.f);

        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillRoundedRectangle (logo.translated (3.f, 4.f), 7.f);

        juce::ColourGradient panelGrad (juce::Colour(0xff1a0040), 240.f, 326.f,
                                         juce::Colour(0xff080018), 450.f, 434.f, false);
        g.setGradientFill (panelGrad);
        g.fillRoundedRectangle (logo, 7.f);
        g.setColour (kGreen.withAlpha (0.3f));
        g.drawRoundedRectangle (logo, 7.f, 1.5f);

        g.setColour (kGreen);
        g.setFont (juce::Font (9.f, juce::Font::bold));
        g.drawText ("SIP MACRO", 240, 330, 210, 12, juce::Justification::centred);

        float sipVal = *proc.apvts.getRawParameterValue ("sip") / 100.f;
        if (sipVal > 0.01f)
        {
            const float scx = 305.f, scy = 380.f;
            for (int gi = 3; gi >= 1; --gi)
            {
                float gr = 38.f + gi * 5.f;
                g.setColour (kGreen.withAlpha (sipVal * 0.18f * (4 - gi)));
                g.fillEllipse (scx - gr, scy - gr, gr * 2.f, gr * 2.f);
            }
        }

        if (juiceActive)
        {
            float pulse = 0.5f + 0.5f * std::sin (juiceTimer * juce::MathConstants<float>::twoPi * 2.f);
            const float jcx = 345.f, jcy = 424.f;
            for (int gi = 2; gi >= 1; --gi)
            {
                float gr = 38.f + gi * 8.f;
                g.setColour (kGreen.withAlpha (pulse * 0.15f * gi));
                g.fillEllipse (jcx - gr, jcy - gr * 0.45f, gr * 2.f, gr * 0.9f);
            }
        }
    }

    // Master label
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.setFont (juce::Font (7.5f, juce::Font::bold));
    g.drawText ("MASTER", 456, 437, 60, 11, juce::Justification::centredLeft);

    // ── LFO waveform preview (drawn inside LFO panel, below the knobs) ────
    {
        juce::Rectangle<float> waveArea (32.f, 394.f, 196.f, 32.f);
        g.setColour (juce::Colour (0xff0a0020));
        g.fillRoundedRectangle (waveArea, 4.f);
        g.setColour (kGreen.withAlpha (0.2f));
        g.drawRoundedRectangle (waveArea, 4.f, 1.f);

        int lfoShape = (int)*proc.apvts.getRawParameterValue ("lfoShape");
        float depth  = *proc.apvts.getRawParameterValue ("lfoDepth") / 100.f;
        const int PW = (int)waveArea.getWidth();
        const float pcy = waveArea.getCentreY();
        const float pamp = waveArea.getHeight() * 0.38f * juce::jmax (depth, 0.1f);

        juce::Path lfoWave;
        for (int px = 0; px < PW; px++)
        {
            float t = px / (float)PW * juce::MathConstants<float>::twoPi * 2.f;
            float y = 0.f;
            switch (lfoShape) {
                case 0: y = std::sin (t); break;                            // Sine
                case 1: y = (t / juce::MathConstants<float>::pi) - std::floor (t / juce::MathConstants<float>::pi + 0.5f) * 2.f; break; // Triangle
                case 2: y = std::sin (t) >= 0.f ? 1.f : -1.f; break;      // Square
                case 3: y = 1.f - std::fmod (t / juce::MathConstants<float>::pi, 2.f); break; // Saw
            }
            float wy = pcy - y * pamp;
            if (px == 0) lfoWave.startNewSubPath (waveArea.getX() + px, wy);
            else          lfoWave.lineTo (waveArea.getX() + px, wy);
        }

        // Fill
        juce::Path lfoFill = lfoWave;
        lfoFill.lineTo (waveArea.getRight(), pcy);
        lfoFill.lineTo (waveArea.getX(), pcy);
        lfoFill.closeSubPath();
        g.setColour (kPurpleMid.withAlpha (0.35f));
        g.fillPath (lfoFill);

        g.setColour (kGreen.withAlpha (0.85f));
        g.strokePath (lfoWave, juce::PathStrokeType (1.5f));

    }
}

void JuiceGangEditor::resized()
{
    const int W = getWidth(), H = getHeight();

    // Bypass button hidden — toggled by clicking the straw

    auto setCombo = [&](juce::ComboBox& c) {
        c.setColour (juce::ComboBox::backgroundColourId, kPurpleDark);
        c.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
        c.setColour (juce::ComboBox::outlineColourId,    kGreen);
    };

    // ── Filter panel  x=26 y=122 w=210 h=200 ──
    filterOnBtn.setBounds (214, 128, 22, 14);
    filterModeBox.setBounds (32, 142, 84, 16);  setCombo (filterModeBox);

    cutoffKnob.setBounds    (102, 138, 70, 70); cutoffLbl.setBounds    (102, 206, 70, 12);
    resonanceKnob.setBounds (176, 146, 56, 56); resonanceLbl.setBounds (176, 201, 56, 12);
    driveKnob.setBounds     (32,  218, 48, 48); driveLbl.setBounds     (32,  265, 48, 12);
    filterMixKnob.setBounds (86,  218, 48, 48); filterMixLbl.setBounds (86,  265, 48, 12);
    filterOutKnob.setBounds (140, 218, 48, 48); filterOutLbl.setBounds (140, 265, 48, 12);
    filterCurve.setBounds   (32,  278, 196, 38);

    // ── Delay panel  x=240 y=122 w=210 h=200 ──
    delayOnBtn.setBounds (428, 128, 22, 14);
    delaySyncDivBox.setBounds (246, 140, 58, 60);  setCombo (delaySyncDivBox);

    delayTimeKnob.setBounds (316, 138, 70, 70); delayTimeLbl.setBounds (316, 206, 70, 12);
    delayFbKnob.setBounds   (390, 146, 56, 56); delayFbLbl.setBounds   (390, 201, 56, 12);

    delaySyncBtn.setBounds (248, 208, 50, 14);
    pingPongBtn.setBounds  (248, 225, 50, 18);

    delayLCKnob.setBounds  (308, 218, 44, 44); delayLCLbl.setBounds  (308, 261, 44, 12);
    delayHCKnob.setBounds  (356, 218, 44, 44); delayHCLbl.setBounds  (356, 261, 44, 12);
    delayMixKnob.setBounds (404, 218, 44, 44); delayMixLbl.setBounds (404, 261, 44, 12);
    delayVis.setBounds     (246, 278, 196, 38);

    // ── Reverb panel  x=454 y=122 w=240 h=200 ──
    reverbOnBtn.setBounds (670, 128, 22, 14);

    revSizeKnob.setBounds  (460, 138, 56, 56); revSizeLbl.setBounds  (460, 193, 56, 12);
    revDecayKnob.setBounds (522, 130, 70, 70); revDecayLbl.setBounds (522, 199, 70, 12);
    preDelayKnob.setBounds (598, 138, 56, 56); preDelayLbl.setBounds (598, 193, 56, 12);

    revDampKnob.setBounds  (460, 212, 52, 52); revDampLbl.setBounds  (460, 263, 52, 12);
    revMixKnob.setBounds   (520, 212, 52, 52); revMixLbl.setBounds   (520, 263, 52, 12);
    freezeBtn.setBounds    (584, 218, 50, 34);

    // ── LFO panel  x=26 y=326 w=210 h=108 ──
    lfoTargetBox.setBounds (32, 342, 76, 16);  setCombo (lfoTargetBox);
    lfoShapeBox.setBounds  (32, 362, 76, 16);  setCombo (lfoShapeBox);

    lfoRateKnob.setBounds  (118, 330, 56, 56); lfoRateLbl.setBounds  (118, 385, 56, 12);
    lfoDepthKnob.setBounds (178, 330, 56, 56); lfoDepthLbl.setBounds (178, 385, 56, 12);

    lfoSyncBtn.setBounds    (32, 382, 40, 14);
    lfoSyncDivBox.setBounds (76, 382, 48, 14);  setCombo (lfoSyncDivBox);

    // ── SIP macro — centre panel x=240 y=326 w=210 h=108 ──
    sipKnob.setBounds (272, 332, 76, 76);
    sipLbl.setBounds  (272, 406, 76, 14);
    juiceBtn.setBounds (356, 348, 72, 28);

    // ── Reverb EQ  x=454 y=326 w=240 h=108 ──
    reverbEqOnBtn.setBounds (668, 330, 22, 14);
    revLCKnob.setBounds     (458, 344, 36, 36); revLCLbl.setBounds  (458, 379, 36, 12);
    revLSKnob.setBounds     (498, 344, 36, 36); revLSLbl.setBounds  (498, 379, 36, 12);
    revMidKnob.setBounds    (538, 344, 36, 36); revMidLbl.setBounds (538, 379, 36, 12);
    revHSKnob.setBounds     (578, 344, 36, 36); revHSLbl.setBounds  (578, 379, 36, 12);
    revHCKnob.setBounds     (618, 344, 36, 36); revHCLbl.setBounds  (618, 379, 36, 12);
    reverbEQCurve.setBounds (458, 394, 226, 34);

    // ── Master — below row 2, right-aligned under RevEQ ──
    masterInKnob.setBounds  (456, 440, 52, 52); masterInLbl.setBounds  (456, 490, 52, 12);
    vuMeter.setBounds       (514, 438, 14, 90);
    masterMixKnob.setBounds (534, 440, 52, 52); masterMixLbl.setBounds (534, 490, 52, 12);
    masterOutKnob.setBounds (600, 440, 52, 52); masterOutLbl.setBounds (600, 490, 52, 12);

    // ── Preset browser overlay ──
    presetBrowser.setBounds ((W - 620) / 2, (H - 520) / 2, 620, 520);

    // ── Preset bar ──
    const int PY = H - 24;
    prevPresetBtn.setBounds    (26,  PY, 22, 18);
    nextPresetBtn.setBounds    (50,  PY, 22, 18);
    presetNameLabel.setBounds  (76,  PY, 240, 18);
    savePresetBtn.setBounds    (320, PY, 36, 18);
    deletePresetBtn.setBounds  (360, PY, 34, 18);
    presetsBtn.setBounds       (W - 100, PY, 88, 18);
}
