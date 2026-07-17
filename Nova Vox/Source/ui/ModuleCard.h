#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "NovaVoxLookAndFeel.h"

// ---------------------------------------------------------------------------
// ModuleCardConfig — describes one processing module card
// ---------------------------------------------------------------------------
struct ModuleCardConfig
{
    juce::String      name;           // e.g. "TONE", "EQ", "COMP", "SPACE", "MOTION"
    juce::String      dspName;        // subtitle e.g. "NOVA CONSOLE"
    juce::Colour      accentColour;   // module accent (gold, blue, purple, cyan, magenta)
    juce::String      amountParamId;  // APVTS parameter ID for the hero knob
    juce::String      onParamId;      // APVTS parameter ID for the bypass toggle
    juce::String      modeParamId;    // APVTS parameter ID for the mode choice
    juce::StringArray modeNames;      // list of mode name strings
};

// ---------------------------------------------------------------------------
// ModuleCard
//
// Layout (top to bottom):
//   - Power button (top-right, with coloured LED dot when active)
//   - Module name (large, accent colour) + DSP name (small, gray)
//   - Hero knob (~95px, centered)
//   - "AMOUNT" label + percentage value
//   - Mode selector: [◄] [MODE NAME] [►] + dot indicators
//   - "▼ ADVANCED" full-width button
// ---------------------------------------------------------------------------
class ModuleCard : public juce::Component
{
public:
    ModuleCard(juce::AudioProcessorValueTreeState& apvts,
               const ModuleCardConfig& config,
               NovaVoxLookAndFeel& laf);

    void paint(juce::Graphics& g) override;
    void resized() override;

    bool isAdvancedOpen() const noexcept { return advancedOpen; }
    void setAdvancedOpen(bool open);

    // Called when the ADV button is toggled; provides pointer to this card
    std::function<void(ModuleCard*)> onAdvancedToggled;

    // Returns current mode index (0-based)
    int getCurrentModeIndex() const noexcept { return currentModeIndex; }

private:
    juce::AudioProcessorValueTreeState& apvts;
    ModuleCardConfig  config;
    NovaVoxLookAndFeel& laf;

    // --- Controls ---
    juce::Slider     heroKnob;
    juce::TextButton bypassButton  { "PWR" };
    juce::TextButton prevModeButton{ "<" };
    juce::TextButton nextModeButton{ ">" };
    juce::TextButton advButton     { "\xe2\x96\xbc ADV" };   // ▼ ADV (UTF-8)

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  knobAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;

    int  currentModeIndex = 0;
    bool advancedOpen     = false;

    void updateModeLabel();
    void drawModuleIcon(juce::Graphics& g, const juce::Rectangle<float>& area) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModuleCard)
};
