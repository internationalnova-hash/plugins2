#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

class NovaVoiceAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit NovaVoiceAudioProcessorEditor (NovaVoiceAudioProcessor&);
    ~NovaVoiceAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaVoiceAudioProcessor& processorRef;

    juce::WebSliderRelay morphRelay { "morph" };
    juce::WebSliderRelay textureRelay { "texture" };
    juce::WebSliderRelay formRelay { "form" };
    juce::WebSliderRelay airRelay { "air" };
    juce::WebSliderRelay blendRelay { "blend" };
    juce::WebSliderRelay modeRelay { "voice_mode" };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> morphAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> textureAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> formAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> airAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> blendAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> modeAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaVoiceAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVoiceAudioProcessorEditor)
};