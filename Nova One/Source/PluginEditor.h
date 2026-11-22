#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class NovaOneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit NovaOneAudioProcessorEditor (NovaOneAudioProcessor&);
    ~NovaOneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaOneAudioProcessor& processorRef;

    // CRITICAL: order is Relays → WebView → Attachments.
    juce::WebSliderRelay toneRelay { ParameterIDs::tone };
    juce::WebSliderRelay glueRelay { ParameterIDs::glue };
    juce::WebSliderRelay weightRelay { ParameterIDs::weight };
    juce::WebSliderRelay airRelay { ParameterIDs::air };
    juce::WebSliderRelay widthRelay { ParameterIDs::width };
    juce::WebSliderRelay mixRelay { ParameterIDs::mix };
    juce::WebSliderRelay outputGainRelay { ParameterIDs::outputGain };
    juce::WebSliderRelay finishAmountRelay { ParameterIDs::finishAmount };
    juce::WebSliderRelay modePresetRelay { ParameterIDs::modePreset };
    juce::WebSliderRelay meterViewRelay { ParameterIDs::meterView };
    juce::WebSliderRelay autoLevelRelay { ParameterIDs::autoLevel };
    juce::WebSliderRelay cleanAmountRelay { ParameterIDs::cleanAmount };
    juce::WebSliderRelay warmAmountRelay { ParameterIDs::warmAmount };
    juce::WebSliderRelay wideAmountRelay { ParameterIDs::wideAmount };
    juce::WebSliderRelay loudAmountRelay { ParameterIDs::loudAmount };

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
    std::unique_ptr<juce::WebSliderParameterAttachment> finishAmountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> modePresetAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> meterViewAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> autoLevelAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> cleanAmountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> warmAmountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> wideAmountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> loudAmountAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaOneAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaOneAudioProcessorEditor)
};
