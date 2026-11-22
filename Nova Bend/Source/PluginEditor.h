#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class NovaBendAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit NovaBendAudioProcessorEditor (NovaBendAudioProcessor&);
    ~NovaBendAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaBendAudioProcessor& processorRef;

    // CRITICAL: order is Relays -> WebView -> Attachments. Relay TYPE must
    // match the JS side's getSliderState/getToggleState/getComboBoxState
    // call for that param - using WebSliderRelay uniformly (as an earlier
    // draft of this file did) silently breaks bool/choice params, since
    // each JS accessor talks to a differently-identifier-prefixed native
    // relay type (confirmed the hard way: Quality's buttons didn't respond
    // to clicks at all because getComboBoxState("quality") was talking to
    // nothing, since the native side had only ever registered "quality" as
    // a WebSliderRelay, not a WebComboBoxRelay).
    juce::WebSliderRelay pitchRelay { ParameterIDs::pitch };
    juce::WebSliderRelay formantRelay { ParameterIDs::formant };
    juce::WebToggleButtonRelay flattenRelay { ParameterIDs::flatten };
    juce::WebComboBoxRelay flattenNoteRelay { ParameterIDs::flattenNote };
    juce::WebComboBoxRelay flattenOctaveRelay { ParameterIDs::flattenOctave };
    juce::WebToggleButtonRelay linkRelay { ParameterIDs::link };
    juce::WebSliderRelay mixRelay { ParameterIDs::mix };
    juce::WebComboBoxRelay qualityRelay { ParameterIDs::quality };
    juce::WebToggleButtonRelay bypassRelay { ParameterIDs::bypass };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> pitchAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> formantAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> flattenAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> flattenNoteAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> flattenOctaveAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> linkAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> qualityAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    // WKWebView persists its resource cache across app launches even for a
    // custom scheme (see Nova Glue/Nova Ride's own writeup of this bug) -
    // every URL index.html references internally (JS chain AND every img/
    // reference) needs its own ?v=<assetVersion> cache-bust, or a rebuilt
    // asset's stale bytes can keep being served indefinitely. Built in from
    // day one here rather than discovered later.
    const juce::String assetVersion { juce::String (juce::Time::getCurrentTime().toMilliseconds()) };

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaBendAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaBendAudioProcessorEditor)
};
