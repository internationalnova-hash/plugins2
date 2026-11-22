#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

#if NOVA_MID_REQUIRE_LICENSE
 #include "NovaLicense.h"
#endif

class NovaMidAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit NovaMidAudioProcessorEditor (NovaMidAudioProcessor&);
    ~NovaMidAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::var buildState() const;

    NovaMidAudioProcessor& processorRef;

   #if NOVA_MID_REQUIRE_LICENSE
    NovaLicense license { "nova-mid" };
   #endif

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot());
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    juce::WebBrowserComponent::Options createWebOptions();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMidAudioProcessorEditor)
};
