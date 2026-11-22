#pragma once

#include <JuceHeader.h>
#include "AudioEngine.h"

class NovaAIMasterMainComponent : public juce::Component,
                                  private juce::Timer
{
public:
    NovaAIMasterMainComponent();
    ~NovaAIMasterMainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::var buildStatusVar();
    juce::var waveformToVar (const std::vector<float>& peaks) const;

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

    NovaAIMasterAudioEngine audioEngine;
    NovaMasterUserPrefs prefs;
    std::unique_ptr<juce::FileChooser> activeFileChooser;
    juce::String lastPushedStatusJson;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaAIMasterMainComponent)
};

class NovaAIMasterMainWindow : public juce::DocumentWindow
{
public:
    explicit NovaAIMasterMainWindow (juce::String name);

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaAIMasterMainWindow)
};
