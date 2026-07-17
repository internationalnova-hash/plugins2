#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "ui/NovaVoxLookAndFeel.h"
#include "ui/Visualizer.h"
#include "ui/ModuleCard.h"
#include "ui/AdvancedPanel.h"
#include "ui/VUMeter.h"

class NovaVoxAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit NovaVoxAudioProcessorEditor (NovaVoxAudioProcessor& p);
    ~NovaVoxAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    void timerCallback() override;
    void onAdvancedToggled (ModuleCard* card);

    NovaVoxAudioProcessor& processor;
    NovaVoxLookAndFeel     laf;

    // ── Visualizer ─────────────────────────────────────────────────────────
    NovaVoxVisualizer visualizer;

    // ── VU Meters ──────────────────────────────────────────────────────────
    VUMeter inputMeter;
    VUMeter outputMeter;

    // ── Five module cards ──────────────────────────────────────────────────
    std::unique_ptr<ModuleCard> consoleCard;
    std::unique_ptr<ModuleCard> curveCard;
    std::unique_ptr<ModuleCard> levelCard;
    std::unique_ptr<ModuleCard> motionCard;
    std::unique_ptr<ModuleCard> spaceCard;

    std::array<ModuleCard*, 5> cards {};

    // ── Advanced panel (shown below cards) ─────────────────────────────────
    AdvancedPanel  advancedPanel;
    ModuleCard*    activeAdvCard = nullptr;

    // ── Input / output gain labels and sliders ─────────────────────────────
    juce::Slider inputGainSlider;
    juce::Slider outputGainSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttach;

    // ── Global bypass ──────────────────────────────────────────────────────
    juce::TextButton bypassButton { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVoxAudioProcessorEditor)
};
