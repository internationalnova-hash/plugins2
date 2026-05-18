#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

class NovaConsoleAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit NovaConsoleAudioProcessorEditor (NovaConsoleAudioProcessor&);
    ~NovaConsoleAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    NovaConsoleAudioProcessor& processorRef;

    // WebView-based editor members
    struct SinglePageBrowser : juce::WebBrowserComponent {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const juce::String& newURL) override {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };
    std::unique_ptr<SinglePageBrowser> webView;


    // Parameter relays for web UI (use JUCE WebSliderRelay)
    juce::WebSliderRelay modeRelay { "console_mode" };
    juce::WebSliderRelay qualityRelay { "quality" };
    juce::WebSliderRelay oversamplingRelay { "oversampling" };
    juce::WebSliderRelay inputRelay { "input" };
    juce::WebSliderRelay outputRelay { "output" };
    juce::WebSliderRelay preampOnRelay { "preamp_on" };
    juce::WebSliderRelay driveRelay { "drive" };
    juce::WebSliderRelay colorRelay { "color" };
    juce::WebSliderRelay trimRelay { "trim" };
    juce::WebSliderRelay filterOnRelay { "filter_on" };
    juce::WebSliderRelay hpfRelay { "hpf" };
    juce::WebSliderRelay lpfRelay { "lpf" };
    juce::WebSliderRelay filterSlopeRelay { "filter_slope" };
    juce::WebSliderRelay lowModeRelay { "eq_low_mode" };
    juce::WebSliderRelay highModeRelay { "eq_high_mode" };
    juce::WebSliderRelay airModeRelay { "eq_air_mode" };
    juce::WebSliderRelay compOnRelay { "comp_on" };
    juce::WebSliderRelay compThresholdRelay { "comp_threshold" };
    juce::WebSliderRelay compRatioRelay { "comp_ratio" };
    juce::WebSliderRelay compAttackRelay { "comp_attack" };
    juce::WebSliderRelay compReleaseRelay { "comp_release" };
    juce::WebSliderRelay compMixRelay { "comp_mix" };
    juce::WebSliderRelay compMakeupRelay { "comp_makeup" };
    juce::WebSliderRelay compPunchRelay { "comp_punch" };
    juce::WebSliderRelay gateOnRelay { "gate_on" };
    juce::WebSliderRelay gateThresholdRelay { "gate_threshold" };
    juce::WebSliderRelay gateAttackRelay { "gate_attack" };
    juce::WebSliderRelay gateHoldRelay { "gate_hold" };
    juce::WebSliderRelay gateReleaseRelay { "gate_release" };
    juce::WebSliderRelay gateRangeRelay { "gate_range" };
    juce::WebSliderRelay gateSmoothRelay { "gate_smooth" };
    juce::WebSliderRelay analogOnRelay { "analog_on" };
    juce::WebSliderRelay analogHeatRelay { "analog_heat" };
    juce::WebSliderRelay analogDepthRelay { "analog_depth" };
    juce::WebSliderRelay analogWidthRelay { "analog_width" };
    juce::WebSliderRelay analogDriftRelay { "analog_drift" };
    juce::WebSliderRelay analogCrosstalkRelay { "analog_crosstalk" };
    juce::WebSliderRelay analogNoiseRelay { "analog_noise" };
    juce::WebSliderRelay smartGainRelay { "smart_gain" };
    juce::WebSliderRelay focusModeRelay { "focus_mode" };
    juce::WebSliderRelay mixAssistRelay { "mix_assist" };
    juce::WebSliderRelay sidechainModeRelay { "sidechain_mode" };

    // Attachments for relays (use JUCE WebSliderParameterAttachment)
    std::unique_ptr<juce::WebSliderParameterAttachment> modeAttachment, qualityAttachment, oversamplingAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> inputAttachment, outputAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> preampOnAttachment, driveAttachment, colorAttachment, trimAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> filterOnAttachment, hpfAttachment, lpfAttachment, filterSlopeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lowModeAttachment, highModeAttachment, airModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> compOnAttachment, compThresholdAttachment, compRatioAttachment, compAttackAttachment, compReleaseAttachment, compMixAttachment, compMakeupAttachment, compPunchAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> gateOnAttachment, gateThresholdAttachment, gateAttackAttachment, gateHoldAttachment, gateReleaseAttachment, gateRangeAttachment, gateSmoothAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> analogOnAttachment, analogHeatAttachment, analogDepthAttachment, analogWidthAttachment, analogDriftAttachment, analogCrosstalkAttachment, analogNoiseAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> smartGainAttachment, focusModeAttachment, mixAssistAttachment, sidechainModeAttachment;

    static juce::WebBrowserComponent::Options createWebOptions (NovaConsoleAudioProcessorEditor& editor);
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaConsoleAudioProcessorEditor)
};
