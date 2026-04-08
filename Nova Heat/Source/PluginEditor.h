#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class NovaHeatAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit NovaHeatAudioProcessorEditor (NovaHeatAudioProcessor&);
    ~NovaHeatAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaHeatAudioProcessor& processorRef;

    // CRITICAL: order is Relays → WebView → Attachments.
    juce::WebSliderRelay driveRelay { ParameterIDs::drive };
    juce::WebSliderRelay toneRelay { ParameterIDs::tone };
    juce::WebSliderRelay heatRelay { ParameterIDs::heat };
    juce::WebSliderRelay mixRelay { ParameterIDs::mix };
    juce::WebSliderRelay outputGainRelay { ParameterIDs::outputGain };
    juce::WebSliderRelay characterModeRelay { ParameterIDs::characterMode };
    juce::WebSliderRelay magicModeRelay { ParameterIDs::magicMode };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> driveAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> toneAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> heatAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputGainAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> characterModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> magicModeAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaHeatAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaHeatAudioProcessorEditor)
};
