#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"
#include "NovaLicense.h"

class NovaSubAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit NovaSubAudioProcessorEditor (NovaSubAudioProcessor&);
    ~NovaSubAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaSubAudioProcessor& processorRef;

    // CRITICAL: order is Relays → WebView → Attachments.
    juce::WebSliderRelay colorRelay { ParameterIDs::color };
    juce::WebSliderRelay filterKnobRelay { ParameterIDs::filterKnob };
    juce::WebSliderRelay mixRelay { ParameterIDs::mix };
    juce::WebSliderRelay circuitModeRelay { ParameterIDs::circuitMode };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> colorAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> filterKnobAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> circuitModeAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaSubAudioProcessorEditor& editor);

    // ── License gate ──────────────────────────────────────────────────
    // Pilot implementation: a plain native overlay (not routed through the
    // WebView bridge) sits on top of everything while locked. Simpler and
    // lower-risk to get right than threading activation through the JS
    // bridge for a first pass - see the Phase 2 plan for context.
    NovaLicense license { "nova-sub" };
    bool unlocked = false;

    juce::Label unlockTitle;
    juce::Label unlockStatus;
    juce::TextEditor licenseKeyInput;
    juce::TextButton activateButton { "Activate" };

    struct UnlockOverlay : public juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour::fromRGBA (5, 4, 8, 235));
        }
    };
    UnlockOverlay unlockOverlay;

    void buildUnlockOverlay();
    void handleActivateClicked();
    void setUnlockedState (bool isUnlocked);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaSubAudioProcessorEditor)
};
