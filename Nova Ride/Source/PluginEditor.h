#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class NovaRideAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NovaRideAudioProcessorEditor (NovaRideAudioProcessor&);
    ~NovaRideAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallbackHz30();

    NovaRideAudioProcessor& processorRef;

    // CRITICAL: order is Relays → WebView → Attachments.
    juce::WebSliderRelay targetRelay { "target" };
    juce::WebComboBoxRelay rangeRelay { "range" };
    juce::WebSliderRelay speedRelay { "speed" };
    juce::WebSliderRelay sensitivityRelay { "sensitivity" };
    juce::WebSliderRelay outputRelay { "output" };
    juce::WebComboBoxRelay modeRelay { "mode" };
    juce::WebToggleButtonRelay musicRelay { "music" };
    juce::WebToggleButtonRelay writeRelay { "write" };
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

    std::unique_ptr<juce::WebSliderParameterAttachment> targetAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> rangeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> speedAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> sensitivityAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> modeAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> musicAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> writeAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    class TelemetryTimer : public juce::Timer
    {
    public:
        explicit TelemetryTimer (NovaRideAudioProcessorEditor& ownerIn) : owner (ownerIn) {}
        void timerCallback() override { owner.timerCallbackHz30(); }
    private:
        NovaRideAudioProcessorEditor& owner;
    };
    TelemetryTimer telemetryTimer { *this };

    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;
    void handleApplyPreset  (const juce::Array<juce::var>& args, Completion completion);
    void handleGetPresets   (const juce::Array<juce::var>& args, Completion completion);
    void handleExportRideCsv (const juce::Array<juce::var>& args, Completion completion);

    std::unique_ptr<juce::FileChooser> activeFileChooser;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaRideAudioProcessorEditor& editor);

    // WKWebView persists its resource cache across app launches even for our
    // custom scheme, so a rebuilt js/index.js can keep being served stale
    // from a previous run unless every internal reference to it (and to the
    // other non-HTML resources it/index.html pull in) is versioned the same
    // way index.html's own top-level URL already is.
    const juce::String assetVersion { juce::String (juce::Time::getCurrentTime().toMilliseconds()) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaRideAudioProcessorEditor)
};
