#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

// ---------------------------------------------------------------------------
// NovaVoxLookAndFeel
// Custom LookAndFeel for the Nova Vox plugin — dark graphite / glass theme.
// ---------------------------------------------------------------------------
class NovaVoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Custom colour ID for the LED arc accent on knobs
    static constexpr int knobAccentColourId = 0x1001001;

    NovaVoxLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId,        juce::Colour(0xFF080810));
        setColour(juce::TextButton::buttonColourId,                 juce::Colour(0xFF1A1A24));
        setColour(juce::TextButton::buttonOnColourId,               juce::Colour(0xFF252535));
        setColour(juce::TextButton::textColourOffId,                juce::Colour(0xFF64748B));
        setColour(juce::TextButton::textColourOnId,                 juce::Colour(0xFFE2E8F0));
        setColour(juce::ComboBox::backgroundColourId,               juce::Colour(0xFF1A1A24));
        setColour(juce::ComboBox::textColourId,                     juce::Colour(0xFFE2E8F0));
        setColour(juce::ComboBox::outlineColourId,                  juce::Colour(0xFF3A3A50));
        setColour(juce::ComboBox::arrowColourId,                    juce::Colour(0xFF64748B));
        setColour(juce::PopupMenu::backgroundColourId,              juce::Colour(0xFF0E0E16));
        setColour(juce::PopupMenu::textColourId,                    juce::Colour(0xFFE2E8F0));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,   juce::Colour(0xFF252535));
        setColour(juce::PopupMenu::highlightedTextColourId,         juce::Colour(0xFFE2E8F0));
        setColour(juce::Slider::rotarySliderFillColourId,           juce::Colour(0xFF4A90FF));
        setColour(juce::Slider::rotarySliderOutlineColourId,        juce::Colour(0xFF1A1A24));
    }

    // -------------------------------------------------------------------------
    // Hero knob — dark brushed-metal body, coloured LED arc, white dot indicator
    // -------------------------------------------------------------------------
    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int w, int h,
                          float sliderPos,
                          float startAngle, float endAngle,
                          juce::Slider& slider) override
    {
        const float cx = (float)x + (float)w * 0.5f;
        const float cy = (float)y + (float)h * 0.5f;
        // Leave room for the LED arc ring outside the body
        const float arcRadius  = juce::jmin((float)w, (float)h) * 0.5f - 3.f;
        const float bodyRadius = arcRadius - 7.f;

        const juce::Colour accentColour =
            slider.isColourSpecified(knobAccentColourId)
                ? slider.findColour(knobAccentColourId)
                : juce::Colour(0xFF4A90FF);

        // --- Track arc (7 o'clock to 5 o'clock), very dark background ---
        {
            juce::Path trackArc;
            trackArc.addCentredArc(cx, cy, arcRadius, arcRadius,
                                   0.f, startAngle, endAngle, true);
            g.setColour(juce::Colour(0xFF1E1E2A));
            g.strokePath(trackArc, juce::PathStrokeType(4.f,
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
        }

        // --- LED value arc ---
        {
            const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);
            juce::Path ledArc;
            ledArc.addCentredArc(cx, cy, arcRadius, arcRadius,
                                 0.f, startAngle, valueAngle, true);

            // Outer glow
            g.setColour(accentColour.withAlpha(0.20f));
            g.strokePath(ledArc, juce::PathStrokeType(9.f,
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));

            // Bright arc
            g.setColour(accentColour.withAlpha(0.95f));
            g.strokePath(ledArc, juce::PathStrokeType(3.5f,
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
        }

        // --- Drop shadow under body ---
        g.setColour(juce::Colour(0xFF000005).withAlpha(0.7f));
        g.fillEllipse(cx - bodyRadius + 2.f, cy - bodyRadius + 3.f,
                      bodyRadius * 2.f, bodyRadius * 2.f);

        // --- Metallic knob body ---
        {
            juce::ColourGradient bodyGrad(
                juce::Colour(0xFF3A3A4A),
                cx - bodyRadius * 0.5f, cy - bodyRadius * 0.5f,
                juce::Colour(0xFF111118),
                cx + bodyRadius * 0.6f, cy + bodyRadius * 0.6f,
                true);
            bodyGrad.addColour(0.45, juce::Colour(0xFF1E1E2C));
            g.setGradientFill(bodyGrad);
            g.fillEllipse(cx - bodyRadius, cy - bodyRadius,
                          bodyRadius * 2.f, bodyRadius * 2.f);
        }

        // --- Top-left rim highlight (1px bright arc) ---
        {
            juce::Path rimArc;
            rimArc.addCentredArc(cx, cy, bodyRadius - 0.5f, bodyRadius - 0.5f,
                                 0.f,
                                 -juce::MathConstants<float>::pi * 0.75f,
                                 -juce::MathConstants<float>::pi * 0.15f,
                                 true);
            g.setColour(juce::Colours::white.withAlpha(0.22f));
            g.strokePath(rimArc, juce::PathStrokeType(1.2f));
        }

        // --- Subtle centre bowl ---
        {
            const float innerR = bodyRadius * 0.58f;
            juce::ColourGradient innerGrad(
                juce::Colour(0xFF0E0E18), cx, cy + innerR * 0.2f,
                juce::Colour(0xFF1C1C28), cx, cy - innerR,
                true);
            g.setGradientFill(innerGrad);
            g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.f, innerR * 2.f);
        }

        // --- White indicator dot on knob edge ---
        {
            const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);
            const float dotDist = bodyRadius - 5.5f;
            const float dotX    = cx + std::sin(valueAngle) * dotDist;
            const float dotY    = cy - std::cos(valueAngle) * dotDist;

            // Halo
            g.setColour(juce::Colours::white.withAlpha(0.30f));
            g.fillEllipse(dotX - 5.f, dotY - 5.f, 10.f, 10.f);
            // Core dot
            g.setColour(juce::Colours::white.withAlpha(0.95f));
            g.fillEllipse(dotX - 2.5f, dotY - 2.5f, 5.f, 5.f);
        }
    }

    // -------------------------------------------------------------------------
    // Button background
    // -------------------------------------------------------------------------
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& /*bg*/,
                              bool isHighlighted,
                              bool isDown) override
    {
        const auto  b          = button.getLocalBounds().toFloat().reduced(0.5f);
        const float cornerSize = 6.f;

        juce::Colour fill(0xFF1A1A24);
        if (isDown)             fill = juce::Colour(0xFF111118);
        else if (isHighlighted) fill = juce::Colour(0xFF252535);
        if (button.getToggleState()) fill = juce::Colour(0xFF20203A);

        g.setColour(fill);
        g.fillRoundedRectangle(b, cornerSize);

        const juce::Colour border = button.getToggleState()
            ? juce::Colour(0xFF8B5CF6).withAlpha(0.75f)
            : juce::Colour(0xFF3A3A50).withAlpha(isHighlighted ? 0.9f : 0.55f);
        g.setColour(border);
        g.drawRoundedRectangle(b, cornerSize, 1.f);

        // Top bevel
        g.setColour(juce::Colours::white.withAlpha(0.04f));
        g.drawLine(b.getX() + cornerSize,     b.getY() + 0.5f,
                   b.getRight() - cornerSize, b.getY() + 0.5f, 1.f);
    }

    // -------------------------------------------------------------------------
    // Button text
    // -------------------------------------------------------------------------
    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool isHighlighted, bool /*isDown*/) override
    {
        const float h        = (float)button.getHeight();
        const float fontSize = juce::jmin(11.f, h * 0.42f);

        g.setFont(juce::Font(juce::FontOptions().withHeight(fontSize)
                                                .withStyle("Bold")));

        juce::Colour col = button.getToggleState()
            ? juce::Colour(0xFFE2E8F0)
            : juce::Colour(0xFF64748B);
        if (isHighlighted) col = col.brighter(0.25f);

        g.setColour(col);
        g.drawFittedText(button.getButtonText().toUpperCase(),
                         button.getLocalBounds(),
                         juce::Justification::centred, 1);
    }

    // -------------------------------------------------------------------------
    // ComboBox
    // -------------------------------------------------------------------------
    void drawComboBox(juce::Graphics& g,
                      int w, int h,
                      bool /*isButtonDown*/,
                      int /*bx*/, int /*by*/, int /*bw*/, int /*bh*/,
                      juce::ComboBox& box) override
    {
        const juce::Rectangle<float> b(0.f, 0.f, (float)w, (float)h);
        g.setColour(juce::Colour(0xFF131320));
        g.fillRoundedRectangle(b, 5.f);
        g.setColour(juce::Colour(0xFF3A3A50));
        g.drawRoundedRectangle(b.reduced(0.5f), 5.f, 1.f);

        // Arrow
        const float ax = (float)w - 13.f;
        const float ay = (float)h * 0.5f;
        juce::Path  arrow;
        arrow.startNewSubPath(ax - 3.5f, ay - 2.f);
        arrow.lineTo(ax,         ay + 3.f);
        arrow.lineTo(ax + 3.5f,  ay - 2.f);
        g.setColour(juce::Colour(0xFF64748B));
        g.strokePath(arrow, juce::PathStrokeType(1.5f));

        g.setFont(juce::Font(juce::FontOptions().withHeight(11.f).withStyle("Bold")));
        g.setColour(juce::Colour(0xFFE2E8F0));
        g.drawFittedText(box.getText().toUpperCase(),
                         8, 0, w - 22, h,
                         juce::Justification::centredLeft, 1);
    }

    // -------------------------------------------------------------------------
    // Popup menu background
    // -------------------------------------------------------------------------
    void drawPopupMenuBackground(juce::Graphics& g, int w, int h) override
    {
        const juce::Rectangle<float> b(0.f, 0.f, (float)w, (float)h);
        g.setColour(juce::Colour(0xFF0E0E16));
        g.fillRoundedRectangle(b, 6.f);
        g.setColour(juce::Colour(0xFF3A3A50));
        g.drawRoundedRectangle(b.reduced(0.5f), 6.f, 1.f);
    }

    // -------------------------------------------------------------------------
    // Popup menu item
    // -------------------------------------------------------------------------
    void drawPopupMenuItem(juce::Graphics& g,
                           const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive,
                           bool isHighlighted, bool /*isTicked*/,
                           bool /*hasSubMenu*/,
                           const juce::String& text,
                           const juce::String& /*shortcut*/,
                           const juce::Drawable* /*icon*/,
                           const juce::Colour* /*textColour*/) override
    {
        if (isSeparator)
        {
            const float my = (float)area.getCentreY();
            g.setColour(juce::Colour(0xFF2A2A38));
            g.drawHorizontalLine((int)my, (float)area.getX() + 8.f,
                                          (float)area.getRight() - 8.f);
            return;
        }

        if (isHighlighted && isActive)
        {
            g.setColour(juce::Colour(0xFF252535));
            g.fillRoundedRectangle(area.toFloat().reduced(2.f, 1.f), 4.f);
            g.setColour(juce::Colour(0xFF8B5CF6).withAlpha(0.45f));
            g.drawRoundedRectangle(area.toFloat().reduced(2.f, 1.f), 4.f, 1.f);
        }

        const juce::Colour textCol = isActive
            ? (isHighlighted ? juce::Colour(0xFFE2E8F0) : juce::Colour(0xFFB0B8C8))
            : juce::Colour(0xFF404060);

        g.setFont(juce::Font(juce::FontOptions().withHeight(11.f).withStyle("Bold")));
        g.setColour(textCol);
        g.drawFittedText(text.toUpperCase(), area.reduced(12, 2),
                         juce::Justification::centredLeft, 1);
    }

    // -------------------------------------------------------------------------
    // Scroll bar (dark, subtle)
    // -------------------------------------------------------------------------
    void drawScrollbar(juce::Graphics& g,
                       juce::ScrollBar& scrollbar,
                       int x, int y, int w, int h,
                       bool /*isScrollbarVertical*/,
                       int thumbStartPosition, int thumbSize,
                       bool /*isMouseOver*/, bool /*isMouseDown*/) override
    {
        g.setColour(juce::Colour(0xFF111118));
        g.fillRect(x, y, w, h);
        g.setColour(juce::Colour(0xFF3A3A50));
        g.fillRoundedRectangle((float)(x + 2), (float)(y + thumbStartPosition + 2),
                               (float)(w - 4), (float)(thumbSize - 4), 3.f);
        juce::ignoreUnused(scrollbar);
    }
};
