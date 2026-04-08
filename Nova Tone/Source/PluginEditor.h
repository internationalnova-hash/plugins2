#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class NovaToneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit NovaToneAudioProcessorEditor (NovaToneAudioProcessor&);
    ~NovaToneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaToneAudioProcessor& processorRef;

    // CRITICAL: order is Relays → WebView → Attachments.
    juce::WebSliderRelay lowFreqRelay { ParameterIDs::lowFreq };
    juce::WebSliderRelay lowBoostRelay { ParameterIDs::lowBoost };
    juce::WebSliderRelay lowAttenuationRelay { ParameterIDs::lowAttenuation };
    juce::WebSliderRelay highBoostFreqRelay { ParameterIDs::highBoostFreq };
    juce::WebSliderRelay highBoostRelay { ParameterIDs::highBoost };
    juce::WebSliderRelay bandwidthRelay { ParameterIDs::bandwidth };
    juce::WebSliderRelay highAttenuationFreqRelay { ParameterIDs::highAttenuationFreq };
    juce::WebSliderRelay highAttenuationRelay { ParameterIDs::highAttenuation };
    juce::WebSliderRelay outputGainRelay { ParameterIDs::outputGain };
    juce::WebSliderRelay modePresetRelay { ParameterIDs::modePreset };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> lowFreqAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lowBoostAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lowAttenuationAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> highBoostFreqAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> highBoostAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> bandwidthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> highAttenuationFreqAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> highAttenuationAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputGainAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> modePresetAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaToneAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaToneAudioProcessorEditor)
};
