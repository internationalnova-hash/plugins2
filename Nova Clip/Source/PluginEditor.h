#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

class NovaClipAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit NovaClipAudioProcessorEditor (NovaClipAudioProcessor&);
    ~NovaClipAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaClipAudioProcessor& processorRef;

    juce::WebSliderRelay driveRelay { "drive" };
    juce::WebSliderRelay clipShapeRelay { "clip_shape" };
    juce::WebSliderRelay toneRelay { "tone" };
    juce::WebSliderRelay punchRelay { "punch" };
    juce::WebSliderRelay ceilingRelay { "ceiling" };
    juce::WebSliderRelay mixRelay { "mix" };

    juce::WebSliderRelay modeRelay { "mode" };
    juce::WebSliderRelay oversamplingRelay { "oversampling" };
    juce::WebSliderRelay lowLatencyRelay { "low_latency" };
    juce::WebSliderRelay safeModeRelay { "safe_mode" };
    juce::WebSliderRelay linkLRRelay { "link_lr" };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> driveAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> clipShapeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> toneAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> punchAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> ceilingAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;

    std::unique_ptr<juce::WebSliderParameterAttachment> modeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> oversamplingAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lowLatencyAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> safeModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> linkLRAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaClipAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaClipAudioProcessorEditor)
};
