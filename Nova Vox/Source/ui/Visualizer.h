#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <vector>

// ---------------------------------------------------------------------------
// VisualizerData
// Written from the audio thread, read on the message thread.
// ---------------------------------------------------------------------------
struct VisualizerData
{
    std::atomic<float> level  { 0.f };   // 0-1 audio level (RMS or peak)
    std::atomic<float> tone   { 0.f };   // 0-1 module amounts
    std::atomic<float> eq     { 0.f };
    std::atomic<float> comp   { 0.f };
    std::atomic<float> space  { 0.f };
    std::atomic<float> motion { 0.f };
};

// ---------------------------------------------------------------------------
// NovaVoxVisualizer
//
// Draws a cinematic flowing-particle visualisation:
//  - Multiple overlapping sine waves rendered as thousands of tiny glowing dots
//  - Color transitions left-to-right: gold → blue → purple → cyan → magenta
//  - Wave amplitude reacts to smoothed audio level
//  - Ambient animation even with no audio (soft constant amplitude)
//  - Repaints at 60 fps via juce::Timer
// ---------------------------------------------------------------------------
class NovaVoxVisualizer : public juce::Component,
                          private juce::Timer
{
public:
    explicit NovaVoxVisualizer(VisualizerData& data);
    ~NovaVoxVisualizer() override;

    void paint(juce::Graphics& g) override;
    void timerCallback() override;

private:
    // Colour stops for the left-to-right colour sweep (normalised 0-1 positions)
    struct ColourStop { float pos; juce::Colour colour; };

    static juce::Colour lerpColour(const juce::Colour& a,
                                   const juce::Colour& b, float t) noexcept;
    juce::Colour colourAtX(float xNorm) const noexcept;  // xNorm: 0-1

    VisualizerData& vizData;

    float phase          = 0.f;
    float smoothLevel    = 0.f;
    float smoothAmounts[5] = { 0.f, 0.f, 0.f, 0.f, 0.f };

    // Five colour stops: Tone/EQ/Comp/Space/Motion
    static const ColourStop kStops[5];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NovaVoxVisualizer)
};
