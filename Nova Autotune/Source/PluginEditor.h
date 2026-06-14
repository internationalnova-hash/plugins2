#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

class NovaAutotuneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit NovaAutotuneAudioProcessorEditor (NovaAutotuneAudioProcessor&);
    ~NovaAutotuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaAutotuneAudioProcessor& processorRef;

    juce::WebSliderRelay keyRelay         { "key" };
    juce::WebSliderRelay scaleRelay       { "scale" };
    juce::WebSliderRelay retuneSpeedRelay { "retuneSpeed" };
    juce::WebSliderRelay toleranceRelay   { "tolerance" };
    juce::WebSliderRelay amountRelay      { "amount" };
    juce::WebSliderRelay formantRelay     { "formant" };

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
    std::unique_ptr<juce::WebSliderParameterAttachment> retuneSpeedAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> toleranceAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> amountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> formantAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaAutotuneAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaAutotuneAudioProcessorEditor)
};
