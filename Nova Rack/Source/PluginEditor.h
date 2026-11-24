#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

#if NOVA_RACK_REQUIRE_LICENSE
 #include "NovaLicense.h"
#endif

class NovaRackAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit NovaRackAudioProcessorEditor (NovaRackAudioProcessor&);
    ~NovaRackAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::var buildState() const;
    juce::String buildLiveJson();
    juce::var buildMatchState();
    std::unique_ptr<juce::FileChooser> chooser;

    NovaRackAudioProcessor& processorRef;
    int seenRackGeneration = -1, seenLibraryGeneration = -1;

   #if NOVA_RACK_REQUIRE_LICENSE
    NovaLicense license { "nova-rack" };
   #endif

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
    juce::WebBrowserComponent::Options createWebOptions();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaRackAudioProcessorEditor)
};
