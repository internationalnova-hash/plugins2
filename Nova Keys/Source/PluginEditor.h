#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "Preset/NovaPreset.h"

class NovaKeysEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit NovaKeysEditor (NovaKeysProcessor&);
    ~NovaKeysEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::var buildStatusVar();

    // JSON shapes pushed to the WebView for the preset browser / layer
    // strips / FX grid - built from the REAL NovaPreset/NovaPresetManager/
    // NovaSoundVault state (task #98), not placeholder data.
    static juce::var buildPresetVar (const NovaPreset& preset, int index);
    juce::var buildLibraryVar();

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

    NovaKeysProcessor& processorRef;
    std::unique_ptr<SinglePageBrowser> webView;
    juce::String lastPushedStatusJson;
    std::unique_ptr<juce::FileChooser> vaultFileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaKeysEditor)
};
