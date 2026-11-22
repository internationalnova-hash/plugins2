#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class NovaGlowAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NovaGlowAudioProcessorEditor (NovaGlowAudioProcessor&);
    ~NovaGlowAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallbackHz30();

    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;
    void handleGetPresets (const juce::Array<juce::var>& args, Completion completion);
    void handleApplyPreset (const juce::Array<juce::var>& args, Completion completion);
    void handleSavePreset (const juce::Array<juce::var>& args, Completion completion);

    NovaGlowAudioProcessor& processorRef;

    // CRITICAL: order is Relays -> WebView -> Attachments.
    juce::WebSliderRelay peakReductionRelay { "peakReduction" };
    juce::WebSliderRelay gainRelay { "gain" };
    juce::WebComboBoxRelay compressLimitRelay { "compressLimit" };
    juce::WebSliderRelay scHpfRelay { "scHpf" };
    juce::WebSliderRelay emphasisRelay { "emphasis" };
    juce::WebSliderRelay glowRelay { "glow" };
    juce::WebComboBoxRelay analogModeRelay { "analogMode" };
    juce::WebSliderRelay mixRelay { "mix" };
    juce::WebSliderRelay stereoLinkRelay { "stereoLink" };
    juce::WebSliderRelay noiseRelay { "noise" };
    juce::WebComboBoxRelay oversamplingRelay { "oversampling" };
    juce::WebComboBoxRelay meterModeRelay { "meterMode" };
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

    std::unique_ptr<juce::WebSliderParameterAttachment> peakReductionAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> gainAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> compressLimitAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> scHpfAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> emphasisAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> glowAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> analogModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> stereoLinkAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> noiseAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> oversamplingAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> meterModeAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    class TelemetryTimer : public juce::Timer
    {
    public:
        explicit TelemetryTimer (NovaGlowAudioProcessorEditor& ownerIn) : owner (ownerIn) {}
        void timerCallback() override { owner.timerCallbackHz30(); }
    private:
        NovaGlowAudioProcessorEditor& owner;
    };
    TelemetryTimer telemetryTimer { *this };

    // WKWebView persists its resource cache across app launches even for
    // our custom scheme - index.html's own top-level URL and every
    // internal reference it (or js/index.js) makes need the same
    // ?v=<assetVersion> cache-busting treatment, or a rebuilt file can
    // keep being served stale from a previous run. See getResource().
    const juce::String assetVersion { juce::String (juce::Time::getCurrentTime().toMilliseconds()) };

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaGlowAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaGlowAudioProcessorEditor)
};
