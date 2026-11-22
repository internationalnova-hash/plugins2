#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class NovaAIMasterEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit NovaAIMasterEditor (NovaAIMasterProcessor&);
    ~NovaAIMasterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    static NovaMasterStyle styleFromName (const juce::String& name) noexcept;
    static NovaLoudnessMode loudnessFromName (const juce::String& name) noexcept;

    NovaAIMasterProcessor& processorRef;
    juce::String lastPushedButtonState;

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaAIMasterEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaAIMasterEditor)
};
