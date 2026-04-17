#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

class NovaPitchAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit NovaPitchAudioProcessorEditor (NovaPitchAudioProcessor&);
    ~NovaPitchAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaPitchAudioProcessor& processorRef;

    juce::WebSliderRelay keyRelay { "key" };
    juce::WebSliderRelay scaleRelay { "scale" };
    juce::WebSliderRelay toleranceRelay { "tolerance" };
    juce::WebSliderRelay amountRelay { "amount" };
    juce::WebSliderRelay confidenceRelay { "confidenceThreshold" };
    juce::WebSliderRelay vibratoRelay { "vibrato" };
    juce::WebSliderRelay formantRelay { "formant" };
    juce::WebSliderRelay lowLatencyRelay { "lowLatency" };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> keyAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> scaleAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> toleranceAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> amountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> confidenceAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> vibratoAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> formantAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lowLatencyAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaPitchAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaPitchAudioProcessorEditor)
};
