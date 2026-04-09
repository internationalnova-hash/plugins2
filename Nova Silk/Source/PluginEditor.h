#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

class NovaSilkAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit NovaSilkAudioProcessorEditor (NovaSilkAudioProcessor&);
    ~NovaSilkAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaSilkAudioProcessor& processorRef;

    juce::WebSliderRelay smoothRelay { "smooth" };
    juce::WebSliderRelay focusRelay { "focus" };
    juce::WebSliderRelay airRelay { "air_preserve" };
    juce::WebSliderRelay bodyRelay { "body" };
    juce::WebSliderRelay outputRelay { "output" };
    juce::WebSliderRelay magicRelay { "magic" };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> smoothAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> focusAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> airAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> bodyAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> magicAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaSilkAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaSilkAudioProcessorEditor)
};