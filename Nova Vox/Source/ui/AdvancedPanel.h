#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "NovaVoxLookAndFeel.h"

// ---------------------------------------------------------------------------
// AdvancedPanel — collapsible panel shown below the module cards when the
// user clicks ▼ ADV on any ModuleCard.  Only one panel is shown at a time.
//
// Layout: horizontal row of labelled knobs / toggle buttons.
// ---------------------------------------------------------------------------

struct AdvancedPanelConfig
{
    juce::String moduleName;     // e.g. "CONSOLE"
    juce::Colour accentColour;
};

class AdvancedPanel : public juce::Component
{
public:
    AdvancedPanel (juce::AudioProcessorValueTreeState& apvts,
                   NovaVoxLookAndFeel& laf);

    // Switch to showing controls for a specific module (identified by name).
    // Pass "" to hide all controls.
    void showModule (const juce::String& moduleName);

    void paint   (juce::Graphics& g) override;
    void resized () override;

private:
    void buildConsoleControls();
    void buildCurveControls();
    void buildLevelControls();
    void buildMotionControls();
    void buildSpaceControls();
    void clearControls();
    void addKnobControl   (const juce::String& paramId, const juce::String& labelText);
    void addToggleControl (const juce::String& paramId, const juce::String& labelText);

    juce::AudioProcessorValueTreeState& apvts;
    NovaVoxLookAndFeel& laf;

    juce::String activeModule;
    juce::Colour accentColour { juce::Colour (0xFF8B5CF6) };

    // ── Controls ────────────────────────────────────────────────────────────
    // Each control set is created/destroyed on showModule().
    // We keep them as raw Component children added/removed dynamically.

    struct LabelledKnob
    {
        std::unique_ptr<juce::Slider>           slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
        std::unique_ptr<juce::Label>            label;
    };

    struct LabelledToggle
    {
        std::unique_ptr<juce::ToggleButton>      button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  attach;
        std::unique_ptr<juce::Label>             label;
    };

    std::vector<LabelledKnob>   knobs;
    std::vector<LabelledToggle> toggles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdvancedPanel)
};
