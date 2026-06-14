#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class NovaApexAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit NovaApexAudioProcessorEditor (NovaApexAudioProcessor&);
    ~NovaApexAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaApexAudioProcessor& processorRef;

    // CRITICAL: order is Relays -> WebView -> Attachments.
    juce::WebSliderRelay gainRelay              { ParameterIDs::gain };
    juce::WebSliderRelay driveRelay             { ParameterIDs::drive };
    juce::WebSliderRelay ceilingRelay           { ParameterIDs::ceiling };
    juce::WebSliderRelay outputGainRelay        { ParameterIDs::outputGain };
    juce::WebSliderRelay styleRelay             { ParameterIDs::style };
    juce::WebSliderRelay attackRelay            { ParameterIDs::attack };
    juce::WebSliderRelay releaseRelay           { ParameterIDs::release };
    juce::WebSliderRelay lookaheadRelay         { ParameterIDs::lookahead };
    juce::WebSliderRelay oversampleRelay        { ParameterIDs::oversample };
    juce::WebSliderRelay linkRelay              { ParameterIDs::link };
    juce::WebSliderRelay ditherRelay            { ParameterIDs::dither };
    juce::WebSliderRelay transientPreserveRelay { ParameterIDs::transientPreserve };
    juce::WebSliderRelay lowEndProtectRelay     { ParameterIDs::lowEndProtect };
    juce::WebSliderRelay loudnessTargetRelay    { ParameterIDs::loudnessTarget };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> gainAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> driveAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> ceilingAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputGainAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> styleAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> attackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> releaseAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lookaheadAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> oversampleAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> linkAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> ditherAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> transientPreserveAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lowEndProtectAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> loudnessTargetAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaApexAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaApexAudioProcessorEditor)
};
