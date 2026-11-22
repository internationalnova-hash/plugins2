#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

class NovaRoyal670Editor : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit NovaRoyal670Editor (NovaRoyal670Processor&);
    ~NovaRoyal670Editor() override;

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
    juce::var buildParamsMeta();

    NovaRoyal670Processor& processorRef;
    std::unique_ptr<SinglePageBrowser> webView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaRoyal670Editor)
};
