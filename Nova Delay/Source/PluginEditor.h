#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class NovaDelayAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit NovaDelayAudioProcessorEditor (NovaDelayAudioProcessor&);
    ~NovaDelayAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaDelayAudioProcessor& processorRef;

    // CRITICAL: order is Relays -> WebView -> Attachments.
    juce::WebSliderRelay presetRelay { ParameterIDs::preset };
    juce::WebSliderRelay delayTimeSyncRelay { ParameterIDs::delayTimeSync };
    juce::WebSliderRelay delayTimeFreeMsRelay { ParameterIDs::delayTimeFreeMs };
    juce::WebSliderRelay syncEnabledRelay { ParameterIDs::syncEnabled };
    juce::WebSliderRelay feedbackRelay { ParameterIDs::feedback };
    juce::WebSliderRelay mixRelay { ParameterIDs::mix };
    juce::WebSliderRelay toneRelay { ParameterIDs::tone };
    juce::WebSliderRelay wowFlutterRelay { ParameterIDs::wowFlutter };
    juce::WebSliderRelay saturationRelay { ParameterIDs::saturation };
    juce::WebSliderRelay modeRelay { ParameterIDs::mode };
    juce::WebSliderRelay pingPongRelay { ParameterIDs::pingPong };
    juce::WebSliderRelay stereoRelay { ParameterIDs::stereo };
    juce::WebSliderRelay lofiRelay { ParameterIDs::lofi };
    juce::WebSliderRelay freezeRelay { ParameterIDs::freeze };
    juce::WebSliderRelay hpFilterHzRelay { ParameterIDs::hpFilterHz };
    juce::WebSliderRelay lpFilterHzRelay { ParameterIDs::lpFilterHz };
    juce::WebSliderRelay duckingRelay { ParameterIDs::ducking };
    juce::WebSliderRelay delayModelRelay { ParameterIDs::delayModel };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> presetAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> delayTimeSyncAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> delayTimeFreeMsAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> syncEnabledAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> feedbackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> toneAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> wowFlutterAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> saturationAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> modeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> pingPongAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> stereoAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lofiAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> freezeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> hpFilterHzAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lpFilterHzAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> duckingAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> delayModelAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaDelayAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaDelayAudioProcessorEditor)
};
