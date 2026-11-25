#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class NovaMirrorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NovaMirrorEditor (NovaMirrorProcessor&);
    ~NovaMirrorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::WebBrowserComponent::Options createWebOptions();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const juce::String& newURL) override { return newURL.startsWith (getResourceProviderRoot()); }
    };

    NovaMirrorProcessor& proc;
    std::unique_ptr<SinglePageBrowser> webView;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::MemoryBlock dropData;
    juce::String dropName;
    int sentInfo = -1;
    juce::String sentParams;
    bool pageReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMirrorEditor)
};
