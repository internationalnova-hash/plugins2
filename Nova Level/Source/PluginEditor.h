#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class NovaLevelAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit NovaLevelAudioProcessorEditor(NovaLevelAudioProcessor&);
    ~NovaLevelAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    NovaLevelAudioProcessor& processorRef;

    // Parameter relays for WebView
    juce::WebSliderRelay compressionRelay { "compression" };
    juce::WebSliderRelay toneRelay { "tone" };
    juce::WebSliderRelay outputRelay { "output" };
    juce::WebSliderRelay modeRelay { "mode" };
    juce::WebSliderRelay magicRelay { "magic" };
    juce::WebSliderRelay meterRelay { "meter" };

    struct SinglePageBrowser : juce::WebBrowserComponent {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad(const juce::String& newURL) override {
            return newURL.startsWith(getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };
    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> compressionAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> toneAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> modeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> magicAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> meterAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NovaLevelAudioProcessorEditor)
};
