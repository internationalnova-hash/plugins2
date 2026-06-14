#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class NovaChordAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit NovaChordAudioProcessorEditor (NovaChordAudioProcessor&);
    ~NovaChordAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaChordAudioProcessor& processorRef;

    juce::WebSliderRelay keyRelay   { "key" };
    juce::WebSliderRelay scaleRelay { "scale" };
    juce::WebSliderRelay styleRelay { "style" };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> keyAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> scaleAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> styleAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaChordAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaChordAudioProcessorEditor)
};
