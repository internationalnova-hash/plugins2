#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>

#include "PluginProcessor.h"

class NovaPitchAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                        public juce::Slider::Listener,
                                        public juce::ComboBox::Listener,
                                        public juce::Timer
{
public:
    explicit NovaPitchAudioProcessorEditor (NovaPitchAudioProcessor&);
    ~NovaPitchAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void sliderValueChanged (juce::Slider* slider) override;
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;
    void timerCallback() override;

private:
    NovaPitchAudioProcessor& audioProcessor;

    juce::ComboBox keyBox;
    juce::ComboBox scaleBox;
    juce::Slider toleranceSlider;
    juce::Slider amountSlider;
    juce::Slider confidenceSlider;

    juce::Label keyLabel { "key", "Key:" };
    juce::Label scaleLabel { "scale", "Scale:" };
    juce::Label toleranceLabel { "tolerance", "Tolerance:" };
    juce::Label amountLabel { "amount", "Amount:" };
    juce::Label confidenceLabel { "confidence", "Confidence:" };

    juce::Label detectedPitchDisplay;
    juce::Label correctedPitchDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaPitchAudioProcessorEditor)
};
