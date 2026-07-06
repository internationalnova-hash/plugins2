#pragma once
#include "PluginProcessor.h"
#include "JuicePresets.h"
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
        bool isJuice = btn.getButtonText() == "JUICE";

        if (isJuice)
        {
            // Big green glowing pill button
            juce::ColourGradient bg (juce::Colour(0xff50ff80), bounds.getCentreX(), bounds.getY(),
                                     juce::Colour(0xff22aa44), bounds.getCentreX(), bounds.getBottom(), false);
            g.setGradientFill (bg);
            g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
            // Rim glow
            g.setColour (juce::Colour(0xff39e65a).withAlpha (down ? 1.f : 0.7f));
            g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, 2.f);
            // Inner highlight
            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.fillRoundedRectangle (bounds.withHeight (bounds.getHeight() * 0.45f), bounds.getHeight() * 0.5f);
        }
        else
        {
            bool on = btn.getToggleState();
            g.setColour (on ? juce::Colour(0xff39e65a) : juce::Colour(0xff2a0060));
            g.fillRoundedRectangle (bounds, 4.f);
            g.setColour (juce::Colour(0xff39e65a).withAlpha(0.6f));
            g.drawRoundedRectangle (bounds, 4.f, 1.5f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& btn, bool, bool) override
    {
        bool isJuice = btn.getButtonText() == "JUICE";
        if (isJuice)
        {
            g.setColour (juce::Colour(0xff003318));
            g.setFont (juce::Font (20.f, juce::Font::bold | juce::Font::italic));
            g.drawFittedText ("JUICE", btn.getLocalBounds(), juce::Justification::centred, 1);
        }
        else
        {
            bool on = btn.getToggleState();
            g.setColour (on ? juce::Colour(0xff000000) : juce::Colours::white);
            g.setFont (juce::Font (10.f, juce::Font::bold));
            g.drawFittedText (btn.getButtonText(), btn.getLocalBounds(), juce::Justification::centred, 1);
        }
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

// ─── Preset Browser Overlay ──────────────────────────────────────────────────
class PresetBrowser : public juce::Component
{
public:
    std::function<void(int)> onPresetSelected;
    std::function<void()>    onClose;

    PresetBrowser()
    {
        setSize (620, 520);
        selectedCategory = 0;
    }

    void paint (juce::Graphics& g) override
    {
        // Dark overlay background
        g.setColour (juce::Colour (0xf0080018));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.f);
        g.setColour (juce::Colour (0xff39e65a).withAlpha (0.4f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.f), 10.f, 1.5f);

        // Header
        g.setColour (juce::Colour (0xff1a0044));
        g.fillRoundedRectangle (0.f, 0.f, (float)getWidth(), 48.f, 10.f);
        g.fillRect (0.f, 30.f, (float)getWidth(), 18.f);
        g.setColour (juce::Colour (0xff39e65a));
        g.setFont (juce::Font (18.f, juce::Font::bold | juce::Font::italic));
        g.drawText ("🧃 JUICE GANG — Flavor Browser", 16, 10, getWidth() - 64, 28, juce::Justification::centredLeft);
        // Close button
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (juce::Font (16.f, juce::Font::bold));
        g.drawText ("x", getWidth() - 40, 8, 30, 28, juce::Justification::centred);
        g.setColour (juce::Colour (0xff39e65a).withAlpha (0.4f));
        g.drawLine (0.f, 48.f, (float)getWidth(), 48.f, 1.f);

        // Category tab labels
        const float catX = 8.f, catY = 54.f, catH = 36.f, catW = (getWidth() - 16.f) / kNumCategories;
        for (int i = 0; i < kNumCategories; ++i)
        {
            juce::Rectangle<float> tab (catX + i * catW, catY, catW - 3.f, catH);
            bool sel = (i == selectedCategory);
            g.setColour (sel ? juce::Colour (0xff39e65a) : juce::Colour (0xff1a0040));
            g.fillRoundedRectangle (tab, 5.f);
            if (sel) {
                g.setColour (juce::Colour (0xff002210));
                g.setFont (juce::Font (8.5f, juce::Font::bold));
            } else {
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.setFont (juce::Font (8.f));
            }
            g.drawFittedText (kCategoryOrder[i], tab.toNearestInt(), juce::Justification::centred, 2);
        }

        // Preset list area divider
        g.setColour (juce::Colour (0xff39e65a).withAlpha (0.2f));
        g.drawLine (0.f, catY + catH + 4.f, (float)getWidth(), catY + catH + 4.f, 1.f);

        // Preset items
        const juce::String cat = kCategoryOrder[selectedCategory];
        const float listY = catY + catH + 10.f;
        const float itemH = 42.f;
        const int   cols  = 2;
        const float colW  = (getWidth() - 20.f) / cols;
        int row = 0, col = 0;

        for (int i = 0; i < kNumPresets; ++i)
        {
            if (juce::String (kJuicePresets[i].category) != cat) continue;

            float ix = 10.f + col * colW;
            float iy = listY + row * itemH;
            juce::Rectangle<float> item (ix, iy, colW - 8.f, itemH - 5.f);

            bool hov = (i == hoveredPreset);
            bool sel2 = (i == loadedPreset);

            juce::Colour bg = sel2 ? juce::Colour (0xff1a4a2a)
                            : hov  ? juce::Colour (0xff1a0050)
                                   : juce::Colour (0xff0e0028);
            g.setColour (bg);
            g.fillRoundedRectangle (item, 5.f);
            g.setColour (sel2 ? juce::Colour (0xff39e65a)
                              : juce::Colour (0xff39e65a).withAlpha (0.2f));
            g.drawRoundedRectangle (item, 5.f, 1.f);

            // Preset name
            g.setColour (sel2 ? juce::Colour (0xff39e65a) : juce::Colours::white);
            g.setFont (juce::Font (11.f, juce::Font::bold));
            g.drawText (kJuicePresets[i].name, (int)item.getX() + 10, (int)item.getY() + 5, (int)item.getWidth() - 20, 16, juce::Justification::centredLeft);

            // Indicator dots for FX active
            float dotX = item.getRight() - 52.f;
            float dotY = item.getY() + item.getHeight() - 12.f;
            auto dot = [&](bool on, juce::Colour c) {
                g.setColour (on ? c : c.withAlpha (0.15f));
                g.fillEllipse (dotX, dotY, 8.f, 8.f);
                dotX += 12.f;
            };
            dot (kJuicePresets[i].filterOn, juce::Colour (0xffffaa22));
            dot (kJuicePresets[i].delayOn,  juce::Colour (0xff22aaff));
            dot (kJuicePresets[i].reverbOn, juce::Colour (0xffaa44ff));
            dot (kJuicePresets[i].sip > 5.f,juce::Colour (0xff39e65a));

            // SIP bar
            if (kJuicePresets[i].sip > 0.f) {
                float barW = (colW - 28.f) * kJuicePresets[i].sip / 100.f;
                g.setColour (juce::Colour (0xff39e65a).withAlpha (0.18f));
                g.fillRoundedRectangle (item.getX() + 6.f, item.getBottom() - 7.f, colW - 28.f, 4.f, 2.f);
                g.setColour (juce::Colour (0xff39e65a).withAlpha (0.5f));
                g.fillRoundedRectangle (item.getX() + 6.f, item.getBottom() - 7.f, barW, 4.f, 2.f);
            }

            col++;
            if (col >= cols) { col = 0; row++; }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Check close (top-right X)
        if (e.getPosition().getX() > getWidth() - 44 && e.getPosition().getY() < 44)
        {
            if (onClose) onClose();
            return;
        }

        // Category tabs
        const float catX = 8.f, catY = 54.f, catH = 36.f;
        const float catW = (getWidth() - 16.f) / kNumCategories;
        if (e.getPosition().getY() >= (int)catY && e.getPosition().getY() <= (int)(catY + catH))
        {
            int ci = (int)((e.getPosition().getX() - catX) / catW);
            if (ci >= 0 && ci < kNumCategories) { selectedCategory = ci; hoveredPreset = -1; repaint(); }
            return;
        }

        // Preset items
        const juce::String cat = kCategoryOrder[selectedCategory];
        const float listY = catY + catH + 10.f;
        const float itemH = 42.f;
        const int   cols  = 2;
        const float colW  = (getWidth() - 20.f) / cols;
        int row = 0, col = 0;

        for (int i = 0; i < kNumPresets; ++i)
        {
            if (juce::String (kJuicePresets[i].category) != cat) continue;
            float ix = 10.f + col * colW;
            float iy = listY + row * itemH;
            juce::Rectangle<float> item (ix, iy, colW - 8.f, itemH - 5.f);
            if (item.contains (e.getPosition().toFloat()))
            {
                loadedPreset = i;
                if (onPresetSelected) onPresetSelected (i);
                repaint();
                return;
            }
            col++;
            if (col >= cols) { col = 0; row++; }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const juce::String cat = kCategoryOrder[selectedCategory];
        const float catY = 54.f, catH = 36.f, listY = catY + catH + 10.f;
        const float itemH = 42.f;
        const int   cols  = 2;
        const float colW  = (getWidth() - 20.f) / cols;
        int row = 0, col = 0, prev = hoveredPreset;
        hoveredPreset = -1;

        for (int i = 0; i < kNumPresets; ++i)
        {
            if (juce::String (kJuicePresets[i].category) != cat) continue;
            float ix = 10.f + col * colW;
            float iy = listY + row * itemH;
            juce::Rectangle<float> item (ix, iy, colW - 8.f, itemH - 5.f);
            if (item.contains (e.getPosition().toFloat())) hoveredPreset = i;
            col++;
            if (col >= cols) { col = 0; row++; }
        }
        if (hoveredPreset != prev) repaint();
    }

    int  getLoadedPreset()   const { return loadedPreset; }
    void setLoadedPreset (int i)   { loadedPreset = i; repaint(); }

private:
    int selectedCategory = 0;
    int hoveredPreset    = -1;
    int loadedPreset     = -1;
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
    void mouseDown (const juce::MouseEvent& e) override;

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

    // ── SIP macro knob ──
    juce::Slider sipKnob;
    juce::Label  sipLbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sipAtt;

    // ── Preset browser ──
    PresetBrowser presetBrowser;
    int  currentPresetIdx = -1;

    // ── JUICE Button ──
    juce::TextButton juiceBtn { "JUICE" };
    float juicePhase   = 0.f;   // 0..1 animation progress
    bool  juiceActive  = false; // true while transition is running
    float juiceTimer   = 0.f;   // counts up in timer ticks

    // ── Preset bar ──
    juce::TextButton prevPresetBtn { "<" }, nextPresetBtn { ">" }, savePresetBtn { "SAVE" }, deletePresetBtn { "DEL" }, presetsBtn { "PRESETS" };
    juce::Label      presetNameLabel;

    float strawDroop = 0.f;  // 0 = upright (active), 1 = drooped (bypassed)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuiceGangEditor)
};
