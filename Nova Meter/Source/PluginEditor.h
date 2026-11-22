#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

#if NOVA_METER_REQUIRE_LICENSE
 #include "NovaLicense.h"
#endif

class NovaMeterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit NovaMeterAudioProcessorEditor (NovaMeterAudioProcessor&);
    ~NovaMeterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::var buildState() const;
    juce::String buildMeterJson();

    NovaMeterAudioProcessor& processorRef;

    // Reference loading: native file picker, or chunked upload of a file dropped on the web view.
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File uploadFile;
    juce::String uploadName;
    std::unique_ptr<juce::FileOutputStream> uploadStream;
    juce::int64 uploadBytes = 0;
    void abortUpload();
    juce::String lastRefStatus;
    int seenStateGeneration = 0;

   #if NOVA_METER_REQUIRE_LICENSE
    NovaLicense license { "nova-meter" };
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMeterAudioProcessorEditor)
};
