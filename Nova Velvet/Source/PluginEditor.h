#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class NovaVelvetAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit NovaVelvetAudioProcessorEditor (NovaVelvetAudioProcessor&);
    ~NovaVelvetAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaVelvetAudioProcessor& processorRef;

    // Relay TYPE must match the JS side's getSliderState/getToggleState/
    // getComboBoxState call for that param - a lesson applied proactively
    // from day one on this plugin (see Nova Bend's own hard-won writeup of
    // this exact bug class if this ever needs debugging).
    juce::WebSliderRelay thresholdRelay { ParameterIDs::threshold };
    juce::WebSliderRelay ratioRelay { ParameterIDs::ratio };
    juce::WebSliderRelay attackRelay { ParameterIDs::attack };
    juce::WebSliderRelay releaseRelay { ParameterIDs::release };
    juce::WebSliderRelay gainRelay { ParameterIDs::gain };
    juce::WebComboBoxRelay timeSelectRelay { ParameterIDs::timeSelect };
    juce::WebSliderRelay scHpfRelay { ParameterIDs::scHpf };
    juce::WebSliderRelay mixRelay { ParameterIDs::mix };
    juce::WebSliderRelay outputRelay { ParameterIDs::output };
    juce::WebToggleButtonRelay stereoLinkRelay { ParameterIDs::stereoLink };
    juce::WebComboBoxRelay analogModeRelay { ParameterIDs::analogMode };
    juce::WebSliderRelay tubeDriveRelay { ParameterIDs::tubeDrive };
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

    std::unique_ptr<juce::WebSliderParameterAttachment> thresholdAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> ratioAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> attackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> releaseAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> gainAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> timeSelectAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> scHpfAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> stereoLinkAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> analogModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> tubeDriveAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    // WKWebView persists its resource cache across app launches even for a
    // custom scheme - every URL index.html references internally needs its
    // own ?v=<assetVersion> cache-bust.
    const juce::String assetVersion { juce::String (juce::Time::getCurrentTime().toMilliseconds()) };

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaVelvetAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVelvetAudioProcessorEditor)
};
