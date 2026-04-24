#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

class NovaCleanV2AudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit NovaCleanV2AudioProcessorEditor (NovaCleanV2AudioProcessor&);
    ~NovaCleanV2AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaCleanV2AudioProcessor& processorRef;

    juce::WebSliderRelay modeRelay { "mode" };
    juce::WebSliderRelay cleanRelay { "clean" };
    juce::WebSliderRelay preserveRelay { "preserve" };
    juce::WebSliderRelay mixRelay { "mix" };
    juce::WebSliderRelay outputGainRelay { "outputGain" };
    juce::WebSliderRelay bypassRelay { "bypass" };
    juce::WebSliderRelay lowLatencyRelay { "lowLatency" };
    juce::WebSliderRelay listenRemovedRelay { "listenRemoved" };
    juce::WebSliderRelay advancedRelay { "advanced" };
    juce::WebSliderRelay sensitivityRelay { "sensitivity" };
    juce::WebSliderRelay clickSizeRelay { "clickSize" };
    juce::WebSliderRelay freqFocusRelay { "freqFocus" };
    juce::WebSliderRelay strengthRelay { "strength" };
    juce::WebSliderRelay shapeRelay { "shape" };
    juce::WebSliderRelay interpolationRelay { "interpolation" };
    juce::WebSliderRelay vocalProtectRelay { "vocalProtect" };
    juce::WebSliderRelay transientGuardRelay { "transientGuard" };
    juce::WebSliderRelay hqModeRelay { "hqMode" };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> modeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> cleanAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> preserveAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputGainAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> bypassAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lowLatencyAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> listenRemovedAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> advancedAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> sensitivityAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> clickSizeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> freqFocusAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> strengthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> shapeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> interpolationAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> vocalProtectAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> transientGuardAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> hqModeAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaCleanV2AudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaCleanV2AudioProcessorEditor)
};
