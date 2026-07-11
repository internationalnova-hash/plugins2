#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class NovaMotionFXEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit NovaMotionFXEditor (NovaMotionFXProcessor&);
    ~NovaMotionFXEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void handleParamChange (const juce::String& json);

    NovaMotionFXProcessor& processor;
    juce::WebBrowserComponent webView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMotionFXEditor)
};
