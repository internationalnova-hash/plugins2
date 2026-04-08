#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class NovaMasterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit NovaMasterAudioProcessorEditor (NovaMasterAudioProcessor&);
    ~NovaMasterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaMasterAudioProcessor& processorRef;

    // CRITICAL: order is Relays → WebView → Attachments.
    juce::WebSliderRelay toneRelay { ParameterIDs::tone };
    juce::WebSliderRelay glueRelay { ParameterIDs::glue };
    juce::WebSliderRelay weightRelay { ParameterIDs::weight };
    juce::WebSliderRelay airRelay { ParameterIDs::air };
    juce::WebSliderRelay widthRelay { ParameterIDs::width };
    juce::WebSliderRelay mixRelay { ParameterIDs::mix };
    juce::WebSliderRelay outputGainRelay { ParameterIDs::outputGain };
    juce::WebSliderRelay finishModeRelay { ParameterIDs::finishMode };
    juce::WebSliderRelay modePresetRelay { ParameterIDs::modePreset };
    juce::WebSliderRelay meterViewRelay { ParameterIDs::meterView };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> toneAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> glueAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> weightAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> airAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> widthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputGainAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> finishModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> modePresetAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> meterViewAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaMasterAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMasterAudioProcessorEditor)
};
