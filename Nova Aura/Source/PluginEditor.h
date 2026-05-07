#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

class NovaAuraAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit NovaAuraAudioProcessorEditor (NovaAuraAudioProcessor&);
    ~NovaAuraAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaAuraAudioProcessor& processorRef;

    juce::WebSliderRelay midAuraRelay { "mid_aura" };
    juce::WebSliderRelay highAuraRelay { "high_aura" };
    juce::WebSliderRelay mixRelay { "mix" };
    juce::WebSliderRelay safeRelay { "safe" };
    juce::WebSliderRelay wideRelay { "wide" };
    juce::WebSliderRelay lowLatencyRelay { "low_latency" };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot())
                || newURL == getResourceProviderRoot()
                || newURL.startsWithIgnoreCase ("about:blank");
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> midAuraAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> highAuraAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> safeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> wideAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lowLatencyAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaAuraAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaAuraAudioProcessorEditor)
};
