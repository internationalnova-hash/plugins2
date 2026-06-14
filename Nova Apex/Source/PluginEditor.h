#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

// -----------------------------------------------------------------------------
// Nova Apex colour palette
namespace ApexColours
{
    inline constexpr juce::uint32 background   = 0xFF0D0D10;
    inline constexpr juce::uint32 panel        = 0xFF151518;
    inline constexpr juce::uint32 panelBorder  = 0xFF252530;
    inline constexpr juce::uint32 champagne    = 0xFFC8A97E;
    inline constexpr juce::uint32 champagneDim = 0xFF8A7055;
    inline constexpr juce::uint32 white        = 0xFFE8E4DC;
    inline constexpr juce::uint32 red          = 0xFFD64C3A;
    inline constexpr juce::uint32 textMuted    = 0xFF666670;
}

// -----------------------------------------------------------------------------
// Custom LookAndFeel for Nova Apex knobs & buttons
class ApexLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ApexLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                                const juce::Colour& backgroundColour,
                                bool shouldDrawButtonAsHighlighted,
                                bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawLabel (juce::Graphics&, juce::Label&) override;
};

// -----------------------------------------------------------------------------
// Gain-reduction / waveform display component
class GRDisplayComponent : public juce::Component
{
public:
    GRDisplayComponent();

    void pushGRValue (float grDb);
    void setInputLevel (float l, float r) noexcept;
    void paint (juce::Graphics&) override;

private:
    static constexpr int kHistorySize = 512;
    std::array<float, kHistorySize> grHistory {};
    int writePos { 0 };
    float inputL { 0.0f }, inputR { 0.0f };
};

// -----------------------------------------------------------------------------
// True-peak meter (vertical segmented bar)
class TruePeakMeter : public juce::Component
{
public:
    TruePeakMeter();
    void setLevel (float linearLevel) noexcept;
    void paint (juce::Graphics&) override;

private:
    float levelLinear { 0.0f };
};

// -----------------------------------------------------------------------------
// Main editor
class NovaApexAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit NovaApexAudioProcessorEditor (NovaApexAudioProcessor&);
    ~NovaApexAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildKnob (juce::Slider& s, const juce::String& paramId,
                    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att);

    void paintHeader      (juce::Graphics&);
    void paintLeftPanel   (juce::Graphics&);
    void paintRightPanel  (juce::Graphics&);
    void paintStyleStrip  (juce::Graphics&);
    void paintBottomStrip (juce::Graphics&);

    NovaApexAudioProcessor& processorRef;
    ApexLookAndFeel laf;

    GRDisplayComponent grDisplay;
    TruePeakMeter      tpMeter;

    // Large knobs — left panel
    juce::Slider gainKnob  { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };
    juce::Slider driveKnob { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };

    // Large knobs — right panel
    juce::Slider ceilingKnob { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };
    juce::Slider outputKnob  { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };

    // Small knobs — bottom strip
    juce::Slider attackKnob    { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };
    juce::Slider releaseKnob   { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };
    juce::Slider lookaheadKnob { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };

    // Style toggle buttons
    juce::TextButton styleButtons[6];

    // Bottom-strip controls
    juce::TextButton  oversampleButton { "2x" };
    juce::ToggleButton linkButton;
    juce::ToggleButton ditherButton;
    juce::TextButton  matchButton { "Match" };

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<SliderAtt> gainAtt, driveAtt, ceilingAtt, outputAtt;
    std::unique_ptr<SliderAtt> attackAtt, releaseAtt, lookaheadAtt;

    // Cached meter values (updated in timer)
    float cachedLufs     { -70.0f };
    float cachedTruePeak { -70.0f };
    float cachedGR       { 0.0f };
    int   currentOversampleIdx { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaApexAudioProcessorEditor)
};
