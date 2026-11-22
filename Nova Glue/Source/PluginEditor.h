#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class NovaGlueAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NovaGlueAudioProcessorEditor (NovaGlueAudioProcessor&);
    ~NovaGlueAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallbackHz30();

    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;
    void handleGetPresets (const juce::Array<juce::var>& args, Completion completion);
    void handleApplyPreset (const juce::Array<juce::var>& args, Completion completion);
    void handleSavePreset (const juce::Array<juce::var>& args, Completion completion);

    NovaGlueAudioProcessor& processorRef;

    // CRITICAL: order is Relays -> WebView -> Attachments.
    juce::WebSliderRelay thresholdRelay { "threshold" };
    juce::WebComboBoxRelay attackRelay { "attack" };
    juce::WebComboBoxRelay releaseRelay { "release" };
    juce::WebSliderRelay makeupRelay { "makeup" };
    juce::WebComboBoxRelay ratioRelay { "ratio" };
    juce::WebSliderRelay scHpfRelay { "scHpf" };
    juce::WebSliderRelay mixRelay { "mix" };
    juce::WebSliderRelay glueRelay { "glue" };
    juce::WebSliderRelay driveRelay { "drive" };
    juce::WebToggleButtonRelay autoGainRelay { "autoGain" };
    juce::WebToggleButtonRelay stereoLinkRelay { "stereoLink" };
    juce::WebComboBoxRelay oversamplingRelay { "oversampling" };
    juce::WebToggleButtonRelay bypassRelay { "bypass" };

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
    std::unique_ptr<juce::WebComboBoxParameterAttachment> attackAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> releaseAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> makeupAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> ratioAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> scHpfAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> glueAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> driveAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> autoGainAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> stereoLinkAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> oversamplingAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    class TelemetryTimer : public juce::Timer
    {
    public:
        explicit TelemetryTimer (NovaGlueAudioProcessorEditor& ownerIn) : owner (ownerIn) {}
        void timerCallback() override { owner.timerCallbackHz30(); }
    private:
        NovaGlueAudioProcessorEditor& owner;
    };
    TelemetryTimer telemetryTimer { *this };

    // WKWebView persists its resource cache across app launches even for
    // our custom scheme - index.html's own top-level URL and every
    // internal reference it (or js/index.js) makes need the same
    // ?v=<assetVersion> cache-busting, or a rebuilt file can keep being
    // served stale from a previous run. See getResource().
    const juce::String assetVersion { juce::String (juce::Time::getCurrentTime().toMilliseconds()) };

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaGlueAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaGlueAudioProcessorEditor)
};
