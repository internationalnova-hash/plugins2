#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class NovaMotionFXEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit NovaMotionFXEditor (NovaMotionFXProcessor&);
    ~NovaMotionFXEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaMotionFXProcessor& processor;

    // CRITICAL: order is Relays → WebView → Attachments
    juce::WebSliderRelay motionRelay    { "motion" };
    juce::WebSliderRelay cutoffRelay    { "cutoff" };
    juce::WebSliderRelay resonanceRelay { "resonance" };
    juce::WebSliderRelay driveRelay     { "drive" };
    juce::WebSliderRelay feedbackRelay  { "feedback" };
    juce::WebSliderRelay delayMixRelay  { "delay_mix" };
    juce::WebSliderRelay sizeRelay      { "size" };
    juce::WebSliderRelay decayRelay     { "decay" };
    juce::WebSliderRelay reverbMixRelay { "reverb_mix" };
    juce::WebSliderRelay lfoRateRelay   { "lfo_rate" };
    juce::WebSliderRelay lfoDepthRelay  { "lfo_depth" };
    juce::WebSliderRelay inputRelay     { "input" };
    juce::WebSliderRelay outputRelay    { "output" };
    juce::WebSliderRelay mixRelay       { "mix" };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> motionAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> cutoffAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> resonanceAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> driveAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> feedbackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> delayMixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> sizeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> decayAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reverbMixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lfoRateAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lfoDepthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> inputAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaMotionFXEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMotionFXEditor)
};
