#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class NovaPunchAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit NovaPunchAudioProcessorEditor (NovaPunchAudioProcessor&);
    ~NovaPunchAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaPunchAudioProcessor& processorRef;

    // Relay TYPE must match the JS side's getSliderState/getToggleState/
    // getComboBoxState call for that param - see Nova Bend's own hard-won
    // writeup of this exact bug class if this ever needs debugging.
    juce::WebSliderRelay punchRelay { ParameterIDs::punch };
    juce::WebSliderRelay bodyRelay { ParameterIDs::body };
    juce::WebComboBoxRelay focusRelay { ParameterIDs::focus };
    juce::WebSliderRelay sensitivityRelay { ParameterIDs::sensitivity };
    juce::WebSliderRelay mixRelay { ParameterIDs::mix };
    juce::WebToggleButtonRelay clipSafeRelay { ParameterIDs::clipSafe };
    juce::WebToggleButtonRelay bypassRelay { ParameterIDs::bypass };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> punchAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> bodyAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> focusAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> sensitivityAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> clipSafeAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    // WKWebView persists its resource cache across app launches even for a
    // custom scheme - every URL index.html references internally needs
    // its own ?v=<assetVersion> cache-bust. Built in from day one (see
    // Nova Bend/Nova Glue's own writeups of this bug).
    const juce::String assetVersion { juce::String (juce::Time::getCurrentTime().toMilliseconds()) };

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaPunchAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaPunchAudioProcessorEditor)
};
