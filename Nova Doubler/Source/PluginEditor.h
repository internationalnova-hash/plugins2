#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

class NovaDoublerEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit NovaDoublerEditor (NovaDoublerProcessor&);
    ~NovaDoublerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    juce::WebBrowserComponent::Options createWebOptions();

    NovaDoublerProcessor& processorRef;
    std::unique_ptr<SinglePageBrowser> webView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaDoublerEditor)
};
