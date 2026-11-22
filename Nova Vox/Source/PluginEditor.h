#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "ui/NovaVoxLookAndFeel.h"

class NovaVoxAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NovaVoxAudioProcessorEditor (NovaVoxAudioProcessor& p);
    ~NovaVoxAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    class Content;

    NovaVoxLookAndFeel     laf;
    std::unique_ptr<Content> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVoxAudioProcessorEditor)
};
