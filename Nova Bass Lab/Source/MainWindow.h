#pragma once

#include <JuceHeader.h>
#include "AudioEngine.h"

class NovaBassLabMainComponent : public juce::Component,
                                  private juce::Timer
{
public:
    NovaBassLabMainComponent();
    ~NovaBassLabMainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::var buildStatusVar();

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

    NovaBassLabAudioEngine audioEngine;
    juce::String lastPushedStatusJson;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaBassLabMainComponent)
};

class NovaBassLabMainWindow : public juce::DocumentWindow
{
public:
    explicit NovaBassLabMainWindow (juce::String name);

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaBassLabMainWindow)
};
