#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

// ---------------------------------------------------------------------------
// VUMeter — vertical segmented LED meter (thin, dark style)
//
// Segments:
//   green  : level < -12 dB
//   yellow : -12 to -6 dB
//   orange : -6 to -3 dB
//   red    : above -3 dB
//
// Inactive segments are shown as very dark (not invisible).
// ---------------------------------------------------------------------------
class VUMeter : public juce::Component
{
public:
    VUMeter()
    {
        setOpaque(false);
    }

    // Call from a timer on the message thread.
    // linearLevel: 0.0 = silence, 1.0 = 0 dBFS (clipping above 1)
    void setLevel(float linearLevel)
    {
        const float target = juce::jlimit(0.f, 1.5f, linearLevel);

        const float attackCoef  = 0.5f;
        const float releaseCoef = 0.07f;
        displayLevel = (target > displayLevel)
            ? displayLevel + (target - displayLevel) * attackCoef
            : displayLevel + (target - displayLevel) * releaseCoef;

        // Peak hold
        if (showPeakHold)
        {
            if (displayLevel >= peakHold)
            {
                peakHold        = displayLevel;
                peakHoldCounter = 90;
            }
            else if (peakHoldCounter > 0)
            {
                --peakHoldCounter;
            }
            else
            {
                peakHold -= 0.005f;
                if (peakHold < 0.f) peakHold = 0.f;
            }
        }

        repaint();
    }

    void setGainReduction(float grDb)
    {
        gainReductionDb   = grDb;
        showGainReduction = (grDb < -0.1f);
        repaint();
    }

    void setPeakHold(bool enabled)
    {
        showPeakHold = enabled;
        if (!enabled) { peakHold = 0.f; peakHoldCounter = 0; }
    }

    // -------------------------------------------------------------------------
    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float w = bounds.getWidth();
        const float h = bounds.getHeight();

        // Background
        g.setColour(juce::Colour(0xFF0A0A14));
        g.fillRoundedRectangle(bounds, 3.f);

        // Segment layout
        // dB range: -60 to +3. Total 63 dB span.
        // Segments: every 1.5 dB gap with 1px gap between segments.
        static constexpr float kDbMin  = -60.f;
        static constexpr float kDbMax  =  +3.f;
        static constexpr float kDbSpan = kDbMax - kDbMin;

        const float segGap  = 1.5f;  // px gap between segments
        const float availH  = h - 4.f;
        const int   numSegs = 42;    // covers 63 dB at 1.5 dB/seg
        const float segH    = (availH - (float)(numSegs - 1) * segGap) / (float)numSegs;

        // Convert displayLevel (linear, 0-1 = 0 dBFS) to dB
        float levelDb = -120.f;
        if (displayLevel > 0.f)
            levelDb = 20.f * std::log10(displayLevel);
        levelDb = juce::jlimit(kDbMin, kDbMax, levelDb);

        float peakDb = -120.f;
        if (showPeakHold && peakHold > 0.f)
            peakDb = juce::jlimit(kDbMin, kDbMax, 20.f * std::log10(peakHold));

        int peakSegIdx = -1;
        if (showPeakHold && peakHold > 0.f)
        {
            const float peakFrac = (peakDb - kDbMin) / kDbSpan;
            peakSegIdx = (int)((float)(numSegs - 1) * peakFrac);
        }

        for (int i = 0; i < numSegs; ++i)
        {
            // Segment 0 = bottom (lowest dB), segment numSegs-1 = top (highest dB)
            const float segDbLow  = kDbMin + ((float)i / (float)(numSegs - 1)) * kDbSpan;

            const float segTop = bounds.getY() + 2.f
                + (float)(numSegs - 1 - i) * (segH + segGap);
            const juce::Rectangle<float> segRect(
                bounds.getX() + 2.f, segTop,
                w - 4.f, segH);

            // Determine base colour by dB threshold
            juce::Colour onColour;
            if      (segDbLow >= -3.f)  onColour = juce::Colour(0xFFEF4444);  // red
            else if (segDbLow >= -6.f)  onColour = juce::Colour(0xFFF97316);  // orange
            else if (segDbLow >= -12.f) onColour = juce::Colour(0xFFEAB308);  // yellow
            else                         onColour = juce::Colour(0xFF22C55E);  // green

            const bool isLit = (segDbLow <= levelDb);
            const bool isPeak = (i == peakSegIdx);

            if (isPeak && showPeakHold)
            {
                // Peak hold — bright white segment
                g.setColour(juce::Colours::white.withAlpha(0.9f));
                g.fillRoundedRectangle(segRect, 1.f);
            }
            else if (isLit)
            {
                // Active segment — full brightness with subtle inner glow
                g.setColour(onColour.withAlpha(0.95f));
                g.fillRoundedRectangle(segRect, 1.f);
                // Thin highlight on top edge
                g.setColour(onColour.brighter(0.4f).withAlpha(0.5f));
                g.fillRect(segRect.getX(), segRect.getY(),
                           segRect.getWidth(), 1.f);
            }
            else
            {
                // Inactive — very dark tint of the segment colour
                g.setColour(onColour.withAlpha(0.08f));
                g.fillRoundedRectangle(segRect, 1.f);
            }
        }

        // Gain reduction overlay (purple, from top)
        if (showGainReduction && gainReductionDb < -0.1f)
        {
            const float grFrac = juce::jlimit(0.f, 1.f,
                (-gainReductionDb) / 20.f);
            const float grH = availH * grFrac * 0.5f;
            g.setColour(juce::Colour(0xFF8B5CF6).withAlpha(0.45f));
            g.fillRoundedRectangle(bounds.getX() + 2.f, bounds.getY() + 2.f,
                                   w - 4.f, grH, 2.f);
        }
    }

private:
    float displayLevel      = 0.f;
    float peakHold          = 0.f;
    int   peakHoldCounter   = 0;
    bool  showPeakHold      = true;
    bool  showGainReduction = false;
    float gainReductionDb   = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUMeter)
};
