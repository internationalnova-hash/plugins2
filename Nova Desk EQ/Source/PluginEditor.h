#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

// Pixel-matched WebView UI (build spec's locked mockup): British console-
// style EQ, procedural CSS-built knobs with arc tick labels (proven
// technique from Nova 7even - knobs are CSS/SVG, not photo-based), a real
// analytic EQ curve (getEqCurve, not a stylized approximation), and live
// automation reflection on the 30Hz timer (a gap the Nova 7even precedent
// didn't cover - see timerCallback).
class NovaDeskEQEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit NovaDeskEQEditor (NovaDeskEQAudioProcessor&);
    ~NovaDeskEQEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

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

    NovaDeskEQAudioProcessor& processorRef;
    std::unique_ptr<SinglePageBrowser> webView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaDeskEQEditor)
};
