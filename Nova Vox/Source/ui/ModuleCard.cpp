#include "ModuleCard.h"
#include <cmath>

// ---------------------------------------------------------------------------
ModuleCard::ModuleCard(juce::AudioProcessorValueTreeState& apvts_,
                       const ModuleCardConfig& cfg,
                       NovaVoxLookAndFeel& lafRef)
    : apvts(apvts_), config(cfg), laf(lafRef)
{
    setName (cfg.name);

    // --- Hero knob ---
    heroKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    heroKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    heroKnob.setColour(NovaVoxLookAndFeel::knobAccentColourId, config.accentColour);
    heroKnob.setLookAndFeel(&laf);
    addAndMakeVisible(heroKnob);

    if (apvts.getParameter(config.amountParamId) != nullptr)
        knobAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, config.amountParamId, heroKnob);

    // --- Bypass / power button ---
    bypassButton.setClickingTogglesState(true);
    bypassButton.setLookAndFeel(&laf);
    addAndMakeVisible(bypassButton);

    if (apvts.getParameter(config.onParamId) != nullptr)
        bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, config.onParamId, bypassButton);

    // --- Mode navigation buttons ---
    prevModeButton.setLookAndFeel(&laf);
    nextModeButton.setLookAndFeel(&laf);
    addAndMakeVisible(prevModeButton);
    addAndMakeVisible(nextModeButton);

    prevModeButton.onClick = [this]
    {
        if (config.modeNames.isEmpty()) return;
        currentModeIndex = (currentModeIndex - 1 + config.modeNames.size())
                           % config.modeNames.size();
        updateModeLabel();
        repaint();
    };

    nextModeButton.onClick = [this]
    {
        if (config.modeNames.isEmpty()) return;
        currentModeIndex = (currentModeIndex + 1) % config.modeNames.size();
        updateModeLabel();
        repaint();
    };

    // --- Advanced button ---
    advButton.setLookAndFeel(&laf);
    addAndMakeVisible(advButton);
    advButton.onClick = [this]
    {
        advancedOpen = !advancedOpen;
        if (onAdvancedToggled) onAdvancedToggled(this);
        repaint();
    };

    updateModeLabel();
}

// ---------------------------------------------------------------------------
void ModuleCard::setAdvancedOpen(bool open)
{
    advancedOpen = open;
    repaint();
}

void ModuleCard::updateModeLabel()
{
    // Mode label is painted in paint(), nothing to set on controls here
}

// ---------------------------------------------------------------------------
// Draw a simple vector icon for each module type
// ---------------------------------------------------------------------------
void ModuleCard::drawModuleIcon(juce::Graphics& g,
                                const juce::Rectangle<float>& area) const
{
    g.setColour(config.accentColour.withAlpha(0.85f));

    const float cx = area.getCentreX();
    const float cy = area.getCentreY();
    const float r  = juce::jmin(area.getWidth(), area.getHeight()) * 0.45f;

    const juce::String& name = config.name;

    if (name == "TONE")
    {
        // Mini waveform (sine-ish with 2 bumps)
        juce::Path p;
        p.startNewSubPath(area.getX(), cy);
        const int steps = 24;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float)i / (float)steps;
            const float x = area.getX() + t * area.getWidth();
            const float y = cy - std::sin(t * juce::MathConstants<float>::twoPi) * r * 0.7f;
            i == 0 ? p.startNewSubPath(x, y) : p.lineTo(x, y);
        }
        g.strokePath(p, juce::PathStrokeType(1.5f));
    }
    else if (name == "EQ")
    {
        // Frequency curve / bell shape
        juce::Path p;
        const int steps = 20;
        for (int i = 0; i <= steps; ++i)
        {
            const float t  = (float)i / (float)steps;
            const float x  = area.getX() + t * area.getWidth();
            const float dX = (t - 0.5f) * 2.f;
            const float y  = cy - r * 0.7f * std::exp(-dX * dX * 4.f);
            i == 0 ? p.startNewSubPath(x, y) : p.lineTo(x, y);
        }
        g.strokePath(p, juce::PathStrokeType(1.5f));
    }
    else if (name == "COMP")
    {
        // Compression knee curve
        juce::Path p;
        p.startNewSubPath(area.getX(), area.getBottom());
        p.lineTo(cx - r * 0.3f, cy + r * 0.3f);
        p.lineTo(cx + r * 0.5f, cy - r * 0.1f);
        p.lineTo(area.getRight(), cy - r * 0.5f);
        g.strokePath(p, juce::PathStrokeType(1.5f));
    }
    else if (name == "SPACE")
    {
        // Concentric arcs (reverb tail)
        for (int i = 1; i <= 3; ++i)
        {
            const float ri = r * 0.35f * (float)i;
            g.drawEllipse(cx - ri, cy - ri * 0.6f, ri * 2.f, ri * 1.2f, 1.2f);
        }
    }
    else if (name == "MOTION")
    {
        // LFO sine wave
        juce::Path p;
        const int steps = 20;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float)i / (float)steps;
            const float x = area.getX() + t * area.getWidth();
            const float y = cy - std::sin(t * juce::MathConstants<float>::twoPi * 1.5f) * r * 0.65f;
            i == 0 ? p.startNewSubPath(x, y) : p.lineTo(x, y);
        }
        g.strokePath(p, juce::PathStrokeType(1.5f));
    }
}

// ---------------------------------------------------------------------------
void ModuleCard::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    const float r = 12.f;

    // --- Card background ---
    g.setColour(juce::Colour(0xFF1A1A24));
    g.fillRoundedRectangle(bounds, r);

    // --- Coloured border glow (1px) ---
    g.setColour(config.accentColour.withAlpha(0.45f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), r, 1.f);

    // --- Inner top bevel highlight ---
    g.setColour(juce::Colours::white.withAlpha(0.04f));
    g.drawLine(bounds.getX() + r,     bounds.getY() + 0.5f,
               bounds.getRight() - r, bounds.getY() + 0.5f, 1.f);

    // --- Module icon (top-left) ---
    {
        const float iconSize = 18.f;
        const float iconX    = bounds.getX() + 10.f;
        const float iconY    = bounds.getY() + 10.f;
        drawModuleIcon(g, juce::Rectangle<float>(iconX, iconY, iconSize, iconSize));
    }

    // --- Module name (accent colour, large, uppercase) ---
    {
        g.setFont(juce::Font(juce::FontOptions().withHeight(15.f).withStyle("Bold")));
        g.setColour(config.accentColour);
        const float nameX = bounds.getX() + 34.f;
        const float nameY = bounds.getY() + 8.f;
        g.drawText(config.name, (int)nameX, (int)nameY,
                   (int)(w - nameX - 40.f), 18,
                   juce::Justification::centredLeft, false);
    }

    // --- DSP name (gray, small) ---
    {
        g.setFont(juce::Font(juce::FontOptions().withHeight(10.f)));
        g.setColour(juce::Colour(0xFF64748B));
        const float dspX = bounds.getX() + 34.f;
        const float dspY = bounds.getY() + 26.f;
        g.drawText(config.dspName, (int)dspX, (int)dspY,
                   (int)(w - dspX - 40.f), 14,
                   juce::Justification::centredLeft, false);
    }

    // --- Power LED dot next to bypass button ---
    {
        const bool isOn = bypassButton.getToggleState();
        const float ledR = 4.f;
        const float ledX = bounds.getRight() - 28.f;
        const float ledY = bounds.getY() + 14.f;

        if (isOn)
        {
            // Glow
            g.setColour(config.accentColour.withAlpha(0.35f));
            g.fillEllipse(ledX - ledR * 2.f, ledY - ledR * 2.f,
                          ledR * 4.f, ledR * 4.f);
            // Core
            g.setColour(config.accentColour);
            g.fillEllipse(ledX - ledR, ledY - ledR, ledR * 2.f, ledR * 2.f);
        }
        else
        {
            g.setColour(juce::Colour(0xFF2A2A38));
            g.fillEllipse(ledX - ledR, ledY - ledR, ledR * 2.f, ledR * 2.f);
            g.setColour(juce::Colour(0xFF3A3A50));
            g.drawEllipse(ledX - ledR, ledY - ledR, ledR * 2.f, ledR * 2.f, 1.f);
        }
    }

    // --- "AMOUNT" label below knob (painted; knob bounds computed in resized) ---
    {
        const float knobBottom = bounds.getY() + 50.f + 100.f;  // approx
        const float labelY     = knobBottom + 2.f;

        g.setFont(juce::Font(juce::FontOptions().withHeight(9.f).withStyle("Bold")));
        g.setColour(juce::Colour(0xFF64748B));
        g.drawText("AMOUNT", 0, (int)labelY, (int)w, 12,
                   juce::Justification::centred, false);

        // Percentage readout from knob value
        const float rawVal = (float)heroKnob.getValue();
        const float pct    = juce::jlimit(0.f, 100.f, rawVal);
        g.setFont(juce::Font(juce::FontOptions().withHeight(13.f).withStyle("Bold")));
        g.setColour(juce::Colour(0xFFE2E8F0));
        g.drawText(juce::String((int)pct) + "%",
                   0, (int)(labelY + 13.f), (int)w, 16,
                   juce::Justification::centred, false);
    }

    // --- Mode dot indicators (below mode name row) ---
    {
        const int    numModes = config.modeNames.size();
        const float  dotR     = 3.f;
        const float  dotSpan  = (float)(numModes - 1) * (dotR * 2.f + 4.f);
        const float  dotStartX = bounds.getCentreX() - dotSpan * 0.5f;
        const float  dotY     = bounds.getBottom() - advButton.getHeight() - 10.f - dotR;

        for (int i = 0; i < numModes; ++i)
        {
            const float dx = dotStartX + (float)i * (dotR * 2.f + 4.f);
            if (i == currentModeIndex)
            {
                g.setColour(config.accentColour);
                g.fillEllipse(dx - dotR, dotY - dotR, dotR * 2.f, dotR * 2.f);
            }
            else
            {
                g.setColour(juce::Colour(0xFF2A2A38));
                g.fillEllipse(dx - dotR, dotY - dotR, dotR * 2.f, dotR * 2.f);
                g.setColour(juce::Colour(0xFF3A3A50));
                g.drawEllipse(dx - dotR, dotY - dotR, dotR * 2.f, dotR * 2.f, 1.f);
            }
        }
    }

    // --- Current mode name (accent colour) ---
    {
        const float advH   = 28.f;
        const float modeH  = 20.f;
        const float dotsH  = 14.f;
        const float modeY  = bounds.getBottom() - advH - dotsH - modeH - 4.f;

        g.setFont(juce::Font(juce::FontOptions().withHeight(11.f).withStyle("Bold")));
        g.setColour(config.accentColour);

        const juce::String& modeName = config.modeNames.isEmpty()
            ? juce::String("---")
            : config.modeNames[currentModeIndex];

        // Leave room for prev/next arrows (painted by buttons)
        g.drawText(modeName.toUpperCase(),
                   (int)(bounds.getX() + 28.f), (int)modeY,
                   (int)(w - 56.f), (int)modeH,
                   juce::Justification::centred, true);
    }
}

// ---------------------------------------------------------------------------
void ModuleCard::resized()
{
    const auto bounds = getLocalBounds();
    const int  w      = bounds.getWidth();
    const int  h      = bounds.getHeight();

    // Power button — top-right
    const int pwrW = 36, pwrH = 22;
    bypassButton.setBounds(w - pwrW - 6, 6, pwrW, pwrH);

    // Hero knob — centered, ~95px, starting below the header labels
    const int knobSize  = juce::jmin(95, w - 20);
    const int knobX     = (w - knobSize) / 2;
    const int knobY     = 48;
    heroKnob.setBounds(knobX, knobY, knobSize, knobSize);

    // ADV button — full width at very bottom
    const int advH = 26;
    advButton.setBounds(6, h - advH - 4, w - 12, advH);

    // Mode navigation: [◄] ... [►] just above the dot indicators and above ADV
    const int dotsH  = 14;
    const int modeH  = 22;
    const int modeY  = h - advH - dotsH - modeH - 8;
    const int arrowW = 22;

    prevModeButton.setBounds(4,          modeY, arrowW, modeH);
    nextModeButton.setBounds(w - arrowW - 4, modeY, arrowW, modeH);
}
