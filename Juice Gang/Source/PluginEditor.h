#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// ─── Juice Gang knob look and feel ──────────────────────────────────────────
class JGLookAndFeel : public juce::LookAndFeel_V4
{
public:
    JGLookAndFeel()
    {
        setColour (juce::Slider::thumbColourId,           juce::Colour (0xff6a0dad));
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff39e65a));
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2a0060));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        float cx = x + w * 0.5f, cy = y + h * 0.5f;
        float r  = juce::jmin (w, h) * 0.42f;

        // Body
        juce::ColourGradient body (juce::Colour(0xff5a0a96), cx, cy - r,
                                   juce::Colour(0xff3a0060), cx, cy + r, false);
        g.setGradientFill (body);
        g.fillEllipse (cx - r, cy - r, r*2, r*2);

        // Green ring
        g.setColour (juce::Colour (0xff39e65a));
        g.drawEllipse (cx - r, cy - r, r*2, r*2, 2.5f);

        // Arc fill
        juce::Path arc;
        arc.addArc (cx - r*0.8f, cy - r*0.8f, r*1.6f, r*1.6f, startAngle, startAngle + (endAngle - startAngle) * sliderPos, true);
        juce::PathStrokeType pst (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (juce::Colour (0xff39e65a));
        g.strokePath (arc, pst);

        // Pointer line
        float angle = startAngle + sliderPos * (endAngle - startAngle);
        float lx = cx + (r * 0.55f) * std::sin (angle);
        float ly = cy - (r * 0.55f) * std::cos (angle);
        g.setColour (juce::Colours::white);
        g.drawLine (cx, cy, lx, ly, 2.5f);

        // Inner highlight
        g.setColour (juce::Colour (0x3fffffff));
        g.fillEllipse (cx - r*0.25f, cy - r*0.45f, r*0.25f, r*0.18f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& btn, const juce::Colour&,
                                bool highlighted, bool down) override
    {
        auto bounds = btn.getLocalBounds().toFloat().reduced (1.f);
        bool on = btn.getToggleState();
        g.setColour (on ? juce::Colour(0xff39e65a) : juce::Colour(0xff2a0060));
        g.fillRoundedRectangle (bounds, 4.f);
        g.setColour (juce::Colour(0xff39e65a).withAlpha(0.6f));
        g.drawRoundedRectangle (bounds, 4.f, 1.5f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& btn, bool, bool) override
    {
        bool on = btn.getToggleState();
        g.setColour (on ? juce::Colour(0xff000000) : juce::Colours::white);
        g.setFont (juce::Font (10.f, juce::Font::bold));
        g.drawFittedText (btn.getButtonText(), btn.getLocalBounds(), juce::Justification::centred, 1);
    }
};

// ─── Filter curve component ──────────────────────────────────────────────────
class FilterCurveDisplay : public juce::Component, public juce::Timer
{
public:
    FilterCurveDisplay (juce::AudioProcessorValueTreeState& a) : apvts (a) { startTimerHz (30); }
    void timerCallback() override { repaint(); }
    void paint (juce::Graphics& g) override;
private:
    juce::AudioProcessorValueTreeState& apvts;
};

// ─── Reverb EQ curve ─────────────────────────────────────────────────────────
class ReverbEQDisplay : public juce::Component, public juce::Timer
{
public:
    ReverbEQDisplay (juce::AudioProcessorValueTreeState& a) : apvts (a) { startTimerHz (30); }
    void timerCallback() override { repaint(); }
    void paint (juce::Graphics& g) override;
private:
    juce::AudioProcessorValueTreeState& apvts;
};

// ─── VU Meter ────────────────────────────────────────────────────────────────
class VUMeter : public juce::Component, public juce::Timer
{
public:
    VUMeter() { startTimerHz (30); }
    void setLevel (float l) { level = l; }
    void timerCallback() override { repaint(); }
    void paint (juce::Graphics& g) override;
private:
    float level = 0.f;
};

// ─── Waveform display (delay) ─────────────────────────────────────────────────
class DelayVisualizer : public juce::Component, public juce::Timer
{
public:
    DelayVisualizer() { startTimerHz (20); }
    void timerCallback() override { phase += 0.08f; if (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi; repaint(); }
    void paint (juce::Graphics& g) override;
private:
    float phase = 0.f;
};

// ─── Main Editor ─────────────────────────────────────────────────────────────
class JuiceGangEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit JuiceGangEditor (JuiceGangProcessor&);
    ~JuiceGangEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    JuiceGangProcessor& proc;
    JGLookAndFeel laf;

    // Helper: make a labelled knob group
    void addKnob (juce::Slider& s, juce::Label& lbl, const juce::String& text,
                  juce::AudioProcessorValueTreeState::SliderAttachment*& att,
                  const juce::String& paramId);

    // Global bypass
    juce::TextButton bypassBtn { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;

    // ── Filter ──
    juce::TextButton filterOnBtn  { "ON" };
    juce::ComboBox   filterModeBox;
    juce::Slider     cutoffKnob, resonanceKnob, driveKnob, filterMixKnob, filterOutKnob;
    juce::Label      cutoffLbl, resonanceLbl, driveLbl, filterMixLbl, filterOutLbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>       filterOnAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>     filterModeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>       cutoffAtt, resonanceAtt, driveAtt, filterMixAtt, filterOutAtt;
    FilterCurveDisplay filterCurve;

    // ── Delay ──
    juce::TextButton delayOnBtn { "ON" }, delaySyncBtn { "SYNC" }, pingPongBtn { "PING\nPONG" };
    juce::ComboBox   delaySyncDivBox;
    juce::Slider     delayTimeKnob, delayFbKnob, delayLCKnob, delayHCKnob, delayMixKnob;
    juce::Label      delayTimeLbl, delayFbLbl, delayLCLbl, delayHCLbl, delayMixLbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>       delayOnAtt, delaySyncAtt, pingPongAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>     delaySyncDivAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>       delayTimeAtt, delayFbAtt, delayLCAtt, delayHCAtt, delayMixAtt;
    DelayVisualizer delayVis;

    // ── Reverb ──
    juce::TextButton reverbOnBtn { "ON" }, freezeBtn { "❄" }, reverbEqOnBtn { "ON" };
    juce::Slider     revSizeKnob, revDecayKnob, preDelayKnob, revDampKnob, revMixKnob;
    juce::Label      revSizeLbl, revDecayLbl, preDelayLbl, revDampLbl, revMixLbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>    reverbOnAtt, freezeAtt, reverbEqOnAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>    revSizeAtt, revDecayAtt, preDelayAtt, revDampAtt, revMixAtt;

    // Reverb EQ
    juce::Slider     revLCKnob, revLSKnob, revMidKnob, revHSKnob, revHCKnob;
    juce::Label      revLCLbl, revLSLbl, revMidLbl, revHSLbl, revHCLbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>    revLCAtt, revLSAtt, revMidAtt, revHSAtt, revHCAtt;
    ReverbEQDisplay  reverbEQCurve;

    // ── LFO ──
    juce::ComboBox   lfoTargetBox, lfoShapeBox;
    juce::TextButton lfoSyncBtn { "SYNC" };
    juce::ComboBox   lfoSyncDivBox;
    juce::Slider     lfoRateKnob, lfoDepthKnob;
    juce::Label      lfoRateLbl, lfoDepthLbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>  lfoTargetAtt, lfoShapeAtt, lfoSyncDivAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>    lfoSyncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>    lfoRateAtt, lfoDepthAtt;

    // ── Master ──
    juce::Slider     masterInKnob, masterMixKnob, masterOutKnob;
    juce::Label      masterInLbl, masterMixLbl, masterOutLbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>    masterInAtt, masterMixAtt, masterOutAtt;
    VUMeter          vuMeter;

    // ── Preset bar ──
    juce::TextButton prevPresetBtn { "<" }, nextPresetBtn { ">" }, savePresetBtn { "💾" }, deletePresetBtn { "🗑" }, presetsBtn { "PRESETS" };
    juce::Label      presetNameLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuiceGangEditor)
};
