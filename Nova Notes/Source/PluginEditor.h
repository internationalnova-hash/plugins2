#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class NovaNotesEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NovaNotesEditor (NovaNotesProcessor&);
    ~NovaNotesEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::WebBrowserComponent::Options createWebOptions();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot());
        }
    };

    NovaNotesProcessor& proc;
    std::unique_ptr<SinglePageBrowser> webView;
    int sentAnalysis = -1, sentNotes = -1;
    bool pageReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaNotesEditor)
};
