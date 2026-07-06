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

        // ── Drop shadow (multi-pass) ──────────────────────────────────────────
        for (int i = 4; i >= 1; --i)
        {
            float sr = r + i * 1.8f;
            g.setColour (juce::Colour (0x18000000));
            g.fillEllipse (cx - sr + i * 0.5f, cy - sr + i * 0.9f, sr * 2.f, sr * 2.f);
        }

        // ── LED arc groove (dark channel behind arc) ──────────────────────────
        {
            juce::Path groove;
            float gr = r * 0.88f;
            groove.addArc (cx - gr, cy - gr, gr * 2.f, gr * 2.f, startAngle, endAngle, true);
            juce::PathStrokeType gst (r * 0.28f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            g.setColour (juce::Colour (0xff0a0018));
            g.strokePath (groove, gst);
        }

        // ── LED arc glow (outer glow → core → bright tip) ────────────────────
        float arcEnd = startAngle + sliderPos * (endAngle - startAngle);
        {
            float gr = r * 0.88f;
            // Outer glow
            juce::Path arc;
            arc.addArc (cx - gr, cy - gr, gr * 2.f, gr * 2.f, startAngle, arcEnd, true);
            juce::PathStrokeType pst (r * 0.38f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            g.setColour (juce::Colour (0xff39e65a).withAlpha (0.22f));
            g.strokePath (arc, pst);
            // Core
            pst = juce::PathStrokeType (r * 0.18f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            g.setColour (juce::Colour (0xff39e65a));
            g.strokePath (arc, pst);
            // Bright highlight
            pst = juce::PathStrokeType (r * 0.06f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            g.setColour (juce::Colour (0xffa0ffb8));
            g.strokePath (arc, pst);
        }

        // ── Anodized aluminum body gradient ──────────────────────────────────
        {
            juce::ColourGradient body (juce::Colour (0xff7a14b8), cx - r * 0.3f, cy - r * 0.6f,
                                       juce::Colour (0xff280050), cx + r * 0.2f, cy + r * 0.7f, false);
            body.addColour (0.45, juce::Colour (0xff5a0a96));
            body.addColour (0.7f, juce::Colour (0xff3a0070));
            g.setGradientFill (body);
            g.fillEllipse (cx - r * 0.78f, cy - r * 0.78f, r * 1.56f, r * 1.56f);
        }

        // ── Anodized texture rings ────────────────────────────────────────────
        for (int i = 1; i <= 3; ++i)
        {
            float tr = r * 0.78f * (0.55f + i * 0.15f);
            g.setColour (juce::Colour (0x0bffffff));
            g.drawEllipse (cx - tr, cy - tr, tr * 2.f, tr * 2.f, 0.8f);
        }

        // ── Top-left catch-light (metallic sheen) ─────────────────────────────
        {
            juce::ColourGradient catchLight (juce::Colours::white.withAlpha (0.45f),
                                             cx - r * 0.35f, cy - r * 0.52f,
                                             juce::Colours::transparentWhite,
                                             cx + r * 0.1f, cy - r * 0.05f, false);
            g.setGradientFill (catchLight);
            g.fillEllipse (cx - r * 0.62f, cy - r * 0.72f, r * 0.62f, r * 0.38f);
        }

        // ── Indicator line with glow backing ─────────────────────────────────
        {
            float angle = startAngle + sliderPos * (endAngle - startAngle);
            float li0x = cx + (r * 0.18f) * std::sin (angle);
            float li0y = cy - (r * 0.18f) * std::cos (angle);
            float li1x = cx + (r * 0.72f) * std::sin (angle);
            float li1y = cy - (r * 0.72f) * std::cos (angle);
            // Soft glow
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawLine (li0x, li0y, li1x, li1y, 4.5f);
            // Sharp white line
            g.setColour (juce::Colours::white);
            g.drawLine (li0x, li0y, li1x, li1y, 1.8f);
        }

        // ── Silver / chrome center cap ────────────────────────────────────────
        {
            float cr = r * 0.22f;
            juce::ColourGradient cap (juce::Colour (0xffe8e8f0), cx - cr * 0.4f, cy - cr * 0.5f,
                                      juce::Colour (0xff8888a0), cx + cr * 0.3f, cy + cr * 0.4f, false);
            g.setGradientFill (cap);
            g.fillEllipse (cx - cr, cy - cr, cr * 2.f, cr * 2.f);
            // Cap rim
            g.setColour (juce::Colour (0xffa0a0c0));
            g.drawEllipse (cx - cr, cy - cr, cr * 2.f, cr * 2.f, 1.f);
            // Cap highlight
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.fillEllipse (cx - cr * 0.55f, cy - cr * 0.65f, cr * 0.45f, cr * 0.28f);
        }
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

    float strawDroop = 0.f;  // 0 = upright (active), 1 = drooped (bypassed)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuiceGangEditor)
};
