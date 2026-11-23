#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

#if NOVA_LOOPLAB_REQUIRE_LICENSE
 #include "NovaLicense.h"
#endif

class NovaLoopLabAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit NovaLoopLabAudioProcessorEditor (NovaLoopLabAudioProcessor&);
    ~NovaLoopLabAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::var buildState();

    // Writes a loop / chop to ~/Music/Nova Loop Lab/Exports and starts an OS
    // drag of it. If the host won't start a drag from here, the file is shown
    // in Finder/Explorer instead so it can be dragged from there.
    juce::var exportAndDrag (bool loop, int index);

    NovaLoopLabAudioProcessor& processorRef;
    std::unique_ptr<juce::FileChooser> chooser;
    int peaksVersion = -1;
    juce::var peaksCache;

   #if NOVA_LOOPLAB_REQUIRE_LICENSE
    NovaLicense license { "nova-loop-lab" };
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaLoopLabAudioProcessorEditor)
};
