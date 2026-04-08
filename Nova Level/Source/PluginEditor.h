#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class NovaLevelAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit NovaLevelAudioProcessorEditor(NovaLevelAudioProcessor&);
    ~NovaLevelAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    NovaLevelAudioProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NovaLevelAudioProcessorEditor)
};
