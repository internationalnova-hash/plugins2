#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class NovaLinkAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::DragAndDropContainer,
                                     private juce::Timer
{
public:
    explicit NovaLinkAudioProcessorEditor(NovaLinkAudioProcessor&);
    ~NovaLinkAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::var buildState();

    NovaLinkAudioProcessor& processorRef;
    juce::String videoError;

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad(const juce::String& newURL) override { return newURL.startsWith(getResourceProviderRoot()); }
    };
    std::unique_ptr<SinglePageBrowser> webView;

    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);
    juce::WebBrowserComponent::Options createWebOptions();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NovaLinkAudioProcessorEditor)
};
