#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

class NovaChordAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit NovaChordAudioProcessorEditor (NovaChordAudioProcessor&);
    ~NovaChordAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaChordAudioProcessor& processorRef;

    juce::WebSliderRelay styleIdxRelay    { "styleIdx" };
    juce::WebSliderRelay octaveShiftRelay { "octaveShift" };
    juce::WebSliderRelay velocityRelay    { "velocity" };
    juce::WebSliderRelay passThroughRelay { "passThrough" };
    juce::WebSliderRelay voiceTypeRelay   { "voiceType" };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> styleIdxAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> octaveShiftAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> velocityAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> passThroughAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> voiceTypeAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaChordAudioProcessorEditor& editor);

    // Native function handlers, called from the UI via getNativeFunction().
    // previewChord(rootMidi, [semitones...]) -> triggers a one-shot MIDI
    // preview of the chord (see NovaChordAudioProcessor::requestChordPreview).
    void handlePreviewChord (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion);

    // exportMidiFile([{root, semis:[...]}, ...]) -> opens a native save
    // dialog and writes the progression as a standard MIDI file, one chord
    // per beat at 120bpm. Resolves with {success: bool, path?: string}.
    void handleExportMidiFile (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion);

    std::unique_ptr<juce::FileChooser> activeFileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaChordAudioProcessorEditor)
};
