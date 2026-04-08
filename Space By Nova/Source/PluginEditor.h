#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class SpaceByNovaAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SpaceByNovaAudioProcessorEditor (SpaceByNovaAudioProcessor&);
    ~SpaceByNovaAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SpaceByNovaAudioProcessor& processorRef;

    // CRITICAL: order is Relays → WebView → Attachments.
    juce::WebSliderRelay spaceRelay { "space" };
    juce::WebSliderRelay airRelay { "air" };
    juce::WebSliderRelay depthRelay { "depth" };
    juce::WebSliderRelay mixRelay { "mix" };
    juce::WebSliderRelay widthRelay { "width" };
    juce::WebSliderRelay modeRelay { "nova_mode" };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> spaceAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> airAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> depthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> widthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> modeAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (SpaceByNovaAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceByNovaAudioProcessorEditor)
};
