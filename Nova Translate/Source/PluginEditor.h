#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class NovaTranslateAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit NovaTranslateAudioProcessorEditor (NovaTranslateAudioProcessor&);
    ~NovaTranslateAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void sendPositionOptionsIfChanged();

    NovaTranslateAudioProcessor& processorRef;
    int lastSentEnvironmentIndex = -1;

    // CRITICAL: order is Relays → WebView → Attachments.
    juce::WebSliderRelay environmentIdRelay { ParameterIDs::environmentId };
    juce::WebSliderRelay environmentAmountRelay { ParameterIDs::environmentAmount };
    juce::WebSliderRelay positionRelay { ParameterIDs::position };
    juce::WebSliderRelay roomAmountRelay { ParameterIDs::roomAmount };
    juce::WebSliderRelay characterAmountRelay { ParameterIDs::characterAmount };
    juce::WebSliderRelay noiseAmountRelay { ParameterIDs::noiseAmount };
    juce::WebSliderRelay noiseEnabledRelay { ParameterIDs::noiseEnabled };
    juce::WebSliderRelay levelMatchEnabledRelay { ParameterIDs::levelMatchEnabled };
    juce::WebSliderRelay outputTrimRelay { ParameterIDs::outputTrim };
    juce::WebSliderRelay monoEnabledRelay { ParameterIDs::monoEnabled };
    juce::WebSliderRelay leftOnlyRelay { ParameterIDs::leftOnly };
    juce::WebSliderRelay rightOnlyRelay { ParameterIDs::rightOnly };
    juce::WebSliderRelay midOnlyRelay { ParameterIDs::midOnly };
    juce::WebSliderRelay sideOnlyRelay { ParameterIDs::sideOnly };
    juce::WebSliderRelay dimEnabledRelay { ParameterIDs::dimEnabled };
    juce::WebSliderRelay phaseInvertRelay { ParameterIDs::phaseInvert };
    juce::WebSliderRelay bypassRelay { ParameterIDs::bypass };
    juce::WebSliderRelay safeRenderEnabledRelay { ParameterIDs::safeRenderEnabled };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> environmentIdAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> environmentAmountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> positionAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> roomAmountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> characterAmountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> noiseAmountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> noiseEnabledAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> levelMatchEnabledAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputTrimAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> monoEnabledAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> leftOnlyAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> rightOnlyAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> midOnlyAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> sideOnlyAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> dimEnabledAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> phaseInvertAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> bypassAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> safeRenderEnabledAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaTranslateAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaTranslateAudioProcessorEditor)
};
