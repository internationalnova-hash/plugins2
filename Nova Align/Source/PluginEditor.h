#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class NovaAlignAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NovaAlignAudioProcessorEditor (NovaAlignAudioProcessor&);
    ~NovaAlignAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    NovaAlignAudioProcessor& processorRef;

    // CRITICAL: order is Relays → WebView → Attachments.
    juce::WebSliderRelay alignModeRelay { "align_mode" };
    juce::WebSliderRelay alignStrengthRelay { "align_strength" };
    juce::WebSliderRelay preserveTransientsRelay { "preserve_transients" };
    juce::WebSliderRelay smoothingRelay { "smoothing" };
    juce::WebSliderRelay phraseSensitivityRelay { "phrase_sensitivity" };
    juce::WebSliderRelay maxShiftMsRelay { "max_shift_ms" };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> alignModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> alignStrengthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> preserveTransientsAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> smoothingAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> phraseSensitivityAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> maxShiftMsAttachment;

    // Must be a member (not local) so it survives past the triggering call,
    // per the FileChooser-from-WebView pattern used elsewhere in the catalog
    // (see Nova Chord's exportMidiFile / Nova Keys' locateLibrary).
    std::unique_ptr<juce::FileChooser> activeFileChooser;

    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;
    void handleLoadGuideFile   (const juce::Array<juce::var>& args, Completion completion);
    void handleLoadTargetFile  (const juce::Array<juce::var>& args, Completion completion);
    void handleRunAlign        (const juce::Array<juce::var>& args, Completion completion);
    void handleGetAlignStatus  (const juce::Array<juce::var>& args, Completion completion);
    void handleRenderAlignedAs (const juce::Array<juce::var>& args, Completion completion);
    void handlePreviewPlay     (const juce::Array<juce::var>& args, Completion completion);
    void handlePreviewStop     (const juce::Array<juce::var>& args, Completion completion);
    void handleDropGuideFile   (const juce::Array<juce::var>& args, Completion completion);
    void handleDropTargetFile  (const juce::Array<juce::var>& args, Completion completion);
    void handleArmCaptureGuide  (const juce::Array<juce::var>& args, Completion completion);
    void handleArmCaptureTarget (const juce::Array<juce::var>& args, Completion completion);
    void handleCancelCapture    (const juce::Array<juce::var>& args, Completion completion);
    void handleStopCapture      (const juce::Array<juce::var>& args, Completion completion);
    void handleGetCaptureStatus (const juce::Array<juce::var>& args, Completion completion);
    void handleGetLinkStatus    (const juce::Array<juce::var>& args, Completion completion);
    void handleSetLinkRole      (const juce::Array<juce::var>& args, Completion completion);
    void handleSetLinkGroup     (const juce::Array<juce::var>& args, Completion completion);

    void pickAudioFile (const juce::String& dialogTitle, Completion completion,
                        std::function<void (const juce::File&)> onChosen);

    // Shared by handleDropGuideFile/handleDropTargetFile - args are
    // [base64FileBytes, originalFileName]. Decodes, writes to a per-instance
    // temp file (keeping the original extension so AudioFormatManager picks
    // the right reader), and hands it to onChosen if it's a valid audio file.
    void receiveDroppedAudio (const juce::Array<juce::var>& args, Completion completion,
                              std::function<void (const juce::File&)> onChosen);

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaAlignAudioProcessorEditor& editor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaAlignAudioProcessorEditor)
};
