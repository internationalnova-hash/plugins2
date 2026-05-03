#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class NovaHarmonyAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit NovaHarmonyAudioProcessorEditor (NovaHarmonyAudioProcessor&);
    ~NovaHarmonyAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaHarmonyAudioProcessor& processorRef;

    // CRITICAL: order is Relays → WebView → Attachments.
    juce::WebSliderRelay voicesRelay { ParameterIDs::voices };
    juce::WebSliderRelay widthRelay { ParameterIDs::width };
    juce::WebSliderRelay humanizeRelay { ParameterIDs::humanize };
    juce::WebSliderRelay mixRelay { ParameterIDs::mix };
    juce::WebSliderRelay styleRelay { ParameterIDs::style };
    juce::WebSliderRelay toneRelay { ParameterIDs::tone };
    juce::WebSliderRelay keyModeRelay { ParameterIDs::keyMode };
    juce::WebSliderRelay keyNoteRelay { ParameterIDs::keyNote };
    juce::WebSliderRelay lowLatencyRelay { ParameterIDs::lowLatency };
    juce::WebSliderRelay qualityModeRelay { ParameterIDs::qualityMode };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> voicesAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> widthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> humanizeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> styleAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> toneAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> keyModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> keyNoteAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lowLatencyAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> qualityModeAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaHarmonyAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaHarmonyAudioProcessorEditor)
};
