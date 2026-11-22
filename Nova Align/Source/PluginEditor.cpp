#include "PluginEditor.h"
#include "BinaryData.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cstring>

NovaAlignAudioProcessorEditor::NovaAlignAudioProcessorEditor (NovaAlignAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    alignModeAttachment          = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("align_mode"), alignModeRelay, nullptr);
    alignStrengthAttachment      = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("align_strength"), alignStrengthRelay, nullptr);
    preserveTransientsAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("preserve_transients"), preserveTransientsRelay, nullptr);
    smoothingAttachment          = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("smoothing"), smoothingRelay, nullptr);
    phraseSensitivityAttachment  = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("phrase_sensitivity"), phraseSensitivityRelay, nullptr);
    maxShiftMsAttachment         = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("max_shift_ms"), maxShiftMsRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    setResizeLimits (690, 456, 1725, 1140);
    setSize (1150, 760);
}

NovaAlignAudioProcessorEditor::~NovaAlignAudioProcessorEditor()
{
    processorRef.getPreviewPlayer().stop();
}

void NovaAlignAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (10, 10, 12));
}

void NovaAlignAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

juce::WebBrowserComponent::Options NovaAlignAudioProcessorEditor::createWebOptions (NovaAlignAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaAlign")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.alignModeRelay)
                     .withOptionsFrom (editor.alignStrengthRelay)
                     .withOptionsFrom (editor.preserveTransientsRelay)
                     .withOptionsFrom (editor.smoothingRelay)
                     .withOptionsFrom (editor.phraseSensitivityRelay)
                     .withOptionsFrom (editor.maxShiftMsRelay)
                     .withNativeFunction ("loadGuideFile", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleLoadGuideFile (args, std::move (completion));
                     })
                     .withNativeFunction ("loadTargetFile", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleLoadTargetFile (args, std::move (completion));
                     })
                     .withNativeFunction ("runAlign", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleRunAlign (args, std::move (completion));
                     })
                     .withNativeFunction ("getAlignStatus", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleGetAlignStatus (args, std::move (completion));
                     })
                     .withNativeFunction ("renderAlignedAs", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleRenderAlignedAs (args, std::move (completion));
                     })
                     .withNativeFunction ("previewPlay", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handlePreviewPlay (args, std::move (completion));
                     })
                     .withNativeFunction ("previewStop", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handlePreviewStop (args, std::move (completion));
                     })
                     .withNativeFunction ("dropGuideFile", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleDropGuideFile (args, std::move (completion));
                     })
                     .withNativeFunction ("dropTargetFile", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleDropTargetFile (args, std::move (completion));
                     })
                     .withNativeFunction ("armCaptureGuide", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleArmCaptureGuide (args, std::move (completion));
                     })
                     .withNativeFunction ("armCaptureTarget", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleArmCaptureTarget (args, std::move (completion));
                     })
                     .withNativeFunction ("cancelCapture", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleCancelCapture (args, std::move (completion));
                     })
                     .withNativeFunction ("stopCapture", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleStopCapture (args, std::move (completion));
                     })
                     .withNativeFunction ("getCaptureStatus", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleGetCaptureStatus (args, std::move (completion));
                     })
                     .withNativeFunction ("getLinkStatus", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleGetLinkStatus (args, std::move (completion));
                     })
                     .withNativeFunction ("setLinkRole", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleSetLinkRole (args, std::move (completion));
                     })
                     .withNativeFunction ("setLinkGroup", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleSetLinkGroup (args, std::move (completion));
                     });

    return options;
}

void NovaAlignAudioProcessorEditor::pickAudioFile (const juce::String& dialogTitle, Completion completion,
                                                   std::function<void (const juce::File&)> onChosen)
{
    activeFileChooser = std::make_unique<juce::FileChooser> (
        dialogTitle,
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac");

    const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    activeFileChooser->launchAsync (flags, [this, completion, onChosen] (const juce::FileChooser& fc) mutable
    {
        const auto file = fc.getResult();
        auto* result = new juce::DynamicObject();

        if (file == juce::File {})
        {
            result->setProperty ("success", false);
            result->setProperty ("error", "Cancelled");
            completion (juce::var (result));
            activeFileChooser.reset();
            return;
        }

        onChosen (file);

        result->setProperty ("success", true);
        result->setProperty ("path", file.getFullPathName());
        result->setProperty ("fileName", file.getFileName());
        completion (juce::var (result));
        activeFileChooser.reset();
    });
}

void NovaAlignAudioProcessorEditor::handleLoadGuideFile (const juce::Array<juce::var>&, Completion completion)
{
    pickAudioFile ("Select Guide Audio (the reference performance)", std::move (completion),
        [this] (const juce::File& file) { processorRef.setGuideFile (file); });
}

void NovaAlignAudioProcessorEditor::handleLoadTargetFile (const juce::Array<juce::var>&, Completion completion)
{
    pickAudioFile ("Select Target Audio (the take to align)", std::move (completion),
        [this] (const juce::File& file) { processorRef.setTargetFile (file); });
}

void NovaAlignAudioProcessorEditor::handleRunAlign (const juce::Array<juce::var>&, Completion completion)
{
    processorRef.requestAlign();

    auto* result = new juce::DynamicObject();
    result->setProperty ("started", true);
    completion (juce::var (result));
}

void NovaAlignAudioProcessorEditor::handleGetAlignStatus (const juce::Array<juce::var>&, Completion completion)
{
    auto* result = new juce::DynamicObject();
    result->setProperty ("busy", processorRef.getAlignJobRunner().isBusy());
    result->setProperty ("guideLoaded", processorRef.getGuideFile().existsAsFile());
    result->setProperty ("targetLoaded", processorRef.getTargetFile().existsAsFile());
    result->setProperty ("guideName", processorRef.getGuideFile().getFileName());
    result->setProperty ("targetName", processorRef.getTargetFile().getFileName());
    result->setProperty ("hasAligned", processorRef.getAlignJobRunner().getAlignedBuffer() != nullptr);
    completion (juce::var (result));
}

void NovaAlignAudioProcessorEditor::handleRenderAlignedAs (const juce::Array<juce::var>&, Completion completion)
{
    if (processorRef.getAlignJobRunner().getAlignedBuffer() == nullptr)
    {
        auto* result = new juce::DynamicObject();
        result->setProperty ("success", false);
        result->setProperty ("error", "Nothing to render yet - press Align first.");
        completion (juce::var (result));
        return;
    }

    const auto targetStem = processorRef.getTargetFile().getFileNameWithoutExtension();
    const auto defaultName = (targetStem.isEmpty() ? juce::String ("Aligned") : targetStem) + "_aligned.wav";

    activeFileChooser = std::make_unique<juce::FileChooser> (
        "Render Aligned Audio As",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (defaultName),
        "*.wav");

    const auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting;

    activeFileChooser->launchAsync (flags, [this, completion] (const juce::FileChooser& fc) mutable
    {
        const auto file = fc.getResult();
        auto* result = new juce::DynamicObject();

        if (file == juce::File {})
        {
            result->setProperty ("success", false);
            result->setProperty ("error", "Cancelled");
            completion (juce::var (result));
            activeFileChooser.reset();
            return;
        }

        const bool ok = processorRef.renderAlignedTo (file);
        result->setProperty ("success", ok);
        if (ok)
            result->setProperty ("path", file.getFullPathName());
        else
            result->setProperty ("error", "Could not write file");

        completion (juce::var (result));
        activeFileChooser.reset();
    });
}

void NovaAlignAudioProcessorEditor::handlePreviewPlay (const juce::Array<juce::var>& args, Completion completion)
{
    const auto which = args.size() > 0 ? args[0].toString() : juce::String();

    juce::File fileToPlay;
    if (which == "guide")        fileToPlay = processorRef.getGuideFile();
    else if (which == "target")  fileToPlay = processorRef.getTargetFile();
    else if (which == "aligned") fileToPlay = processorRef.getAlignedTempFile();

    auto* result = new juce::DynamicObject();
    const bool ok = fileToPlay.existsAsFile() && processorRef.getPreviewPlayer().play (fileToPlay);
    result->setProperty ("success", ok);
    if (! ok)
        result->setProperty ("error", "Nothing to preview for '" + which + "' yet.");
    completion (juce::var (result));
}

void NovaAlignAudioProcessorEditor::handlePreviewStop (const juce::Array<juce::var>&, Completion completion)
{
    processorRef.getPreviewPlayer().stop();
    auto* result = new juce::DynamicObject();
    result->setProperty ("success", true);
    completion (juce::var (result));
}

void NovaAlignAudioProcessorEditor::receiveDroppedAudio (const juce::Array<juce::var>& args, Completion completion,
                                                         std::function<void (const juce::File&)> onChosen)
{
    auto* result = new juce::DynamicObject();

    if (args.size() < 2 || ! args[0].isString())
    {
        result->setProperty ("success", false);
        result->setProperty ("error", "No file data received.");
        completion (juce::var (result));
        return;
    }

    const juce::String base64Data = args[0].toString();
    const juce::String originalName = args[1].toString();

    juce::MemoryOutputStream decoded;
    if (! juce::Base64::convertFromBase64 (decoded, base64Data) || decoded.getDataSize() == 0)
    {
        result->setProperty ("success", false);
        result->setProperty ("error", "Could not read the dropped file.");
        completion (juce::var (result));
        return;
    }

    auto extension = originalName.fromLastOccurrenceOf (".", true, false);
    if (extension.isEmpty())
        extension = ".wav";

    const auto tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("NovaAlign_dropped_" + juce::Uuid().toString() + extension);
    tempFile.deleteFile();

    if (! tempFile.replaceWithData (decoded.getData(), decoded.getDataSize()))
    {
        result->setProperty ("success", false);
        result->setProperty ("error", "Could not write the dropped file to disk.");
        completion (juce::var (result));
        return;
    }

    // Sanity-check it's actually readable audio before accepting it (a
    // dropped MIDI clip, image, or anything else should fail cleanly here
    // rather than silently becoming an unusable Guide/Target).
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (tempFile));
    if (reader == nullptr)
    {
        tempFile.deleteFile();
        result->setProperty ("success", false);
        result->setProperty ("error", "That doesn't look like a supported audio file.");
        completion (juce::var (result));
        return;
    }
    reader.reset();

    onChosen (tempFile);

    result->setProperty ("success", true);
    result->setProperty ("path", tempFile.getFullPathName());
    result->setProperty ("fileName", originalName.isNotEmpty() ? originalName : tempFile.getFileName());
    completion (juce::var (result));
}

void NovaAlignAudioProcessorEditor::handleDropGuideFile (const juce::Array<juce::var>& args, Completion completion)
{
    receiveDroppedAudio (args, std::move (completion),
        [this] (const juce::File& file) { processorRef.setGuideFile (file); });
}

void NovaAlignAudioProcessorEditor::handleDropTargetFile (const juce::Array<juce::var>& args, Completion completion)
{
    receiveDroppedAudio (args, std::move (completion),
        [this] (const juce::File& file) { processorRef.setTargetFile (file); });
}

void NovaAlignAudioProcessorEditor::handleArmCaptureGuide (const juce::Array<juce::var>&, Completion completion)
{
    processorRef.armCaptureGuide();
    auto* result = new juce::DynamicObject();
    result->setProperty ("success", true);
    completion (juce::var (result));
}

void NovaAlignAudioProcessorEditor::handleArmCaptureTarget (const juce::Array<juce::var>&, Completion completion)
{
    processorRef.armCaptureTarget();
    auto* result = new juce::DynamicObject();
    result->setProperty ("success", true);
    completion (juce::var (result));
}

void NovaAlignAudioProcessorEditor::handleCancelCapture (const juce::Array<juce::var>&, Completion completion)
{
    processorRef.cancelCapture();
    auto* result = new juce::DynamicObject();
    result->setProperty ("success", true);
    completion (juce::var (result));
}

void NovaAlignAudioProcessorEditor::handleStopCapture (const juce::Array<juce::var>&, Completion completion)
{
    const auto slotBefore = processorRef.getCaptureEngine().getArmedSlot();
    const auto captured = processorRef.finishCapture();

    auto* result = new juce::DynamicObject();
    const bool ok = captured.guideCaptured || captured.targetCaptured;
    result->setProperty ("success", ok);
    result->setProperty ("guideCaptured", captured.guideCaptured);
    result->setProperty ("targetCaptured", captured.targetCaptured);
    result->setProperty ("guideFromLink", captured.guideFromLink);

    auto guideName = juce::String ("Captured Guide take");
    if (captured.guideFromLink)
    {
        const auto trackName = processorRef.getLink().getGroupInfo().guideTrackName;
        guideName = trackName.isNotEmpty() ? "Linked guide: " + trackName : juce::String ("Linked guide take");
    }
    result->setProperty ("guideName", guideName);
    result->setProperty ("targetName", "Captured Target take");

    if (! ok)
    {
        const bool wantedGuide = slotBefore == NovaAlign::CaptureEngine::Slot::guide;
        result->setProperty ("error", wantedGuide
            ? "No guide captured - set Nova Align on the guide track to GUIDE (same link), then REC and press play."
            : "Nothing was captured - arm capture, then press play in your DAW.");
    }
    completion (juce::var (result));
}

void NovaAlignAudioProcessorEditor::handleGetCaptureStatus (const juce::Array<juce::var>&, Completion completion)
{
    auto& engine = processorRef.getCaptureEngine();
    const auto slot = engine.getArmedSlot();

    juce::String slotName = "none";
    if (slot == NovaAlign::CaptureEngine::Slot::guide) slotName = "guide";
    else if (slot == NovaAlign::CaptureEngine::Slot::target) slotName = "target";

    auto* result = new juce::DynamicObject();
    result->setProperty ("armedSlot", slotName);
    result->setProperty ("capturing", engine.isCapturing());
    double seconds = engine.getCapturedSeconds();
    const auto info = processorRef.getLink().getGroupInfo();
    if (slot == NovaAlign::CaptureEngine::Slot::guide && info.guides > 0)
        seconds = juce::jmax (seconds, info.guideSecondsRecorded);
    result->setProperty ("capturedSeconds", seconds);
    completion (juce::var (result));
}

void NovaAlignAudioProcessorEditor::handleGetLinkStatus (const juce::Array<juce::var>&, Completion completion)
{
    auto& link = processorRef.getLink();
    const auto info = link.getGroupInfo();

    auto* result = new juce::DynamicObject();
    result->setProperty ("role", link.getRole() == NovaAlign::LinkEndpoint::Role::guide ? "guide" : "target");
    result->setProperty ("group", link.getGroup());
    result->setProperty ("guides", info.guides);
    result->setProperty ("targets", info.targets);
    result->setProperty ("guideTrackName", info.guideTrackName);
    result->setProperty ("guideRecording", info.guideRecording);
    result->setProperty ("guideSeconds", info.guideSecondsRecorded);
    result->setProperty ("trackName", link.getTrackName());
    completion (juce::var (result));
}

void NovaAlignAudioProcessorEditor::handleSetLinkRole (const juce::Array<juce::var>& args, Completion completion)
{
    const bool guide = args.size() > 0 && args[0].toString() == "guide";
    processorRef.setLinkRole (guide ? NovaAlign::LinkEndpoint::Role::guide : NovaAlign::LinkEndpoint::Role::target);
    handleGetLinkStatus ({}, std::move (completion));
}

void NovaAlignAudioProcessorEditor::handleSetLinkGroup (const juce::Array<juce::var>& args, Completion completion)
{
    if (args.size() > 0)
        processorRef.getLink().setGroup ((int) args[0]);
    handleGetLinkStatus ({}, std::move (completion));
}

std::optional<juce::WebBrowserComponent::Resource> NovaAlignAudioProcessorEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));

        return juce::WebBrowserComponent::Resource {
            std::move (bytes),
            juce::String (mime)
        };
    };

    const auto lowerUrl = url.toLowerCase();

    if (lowerUrl.contains ("index.html"))
        return makeResource (nova_align_BinaryData::index_html,
                             nova_align_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerUrl.contains ("n_logo.png"))
        return makeResource (nova_align_BinaryData::n_logo_png,
                             nova_align_BinaryData::n_logo_pngSize,
                             "image/png");

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_align_BinaryData::index_html,
                             nova_align_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerPath == "/n_logo.png" || lowerPath.endsWith ("/n_logo.png"))
        return makeResource (nova_align_BinaryData::n_logo_png,
                             nova_align_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerPath == "/js/index.js" || lowerPath.endsWith ("/js/index.js"))
        return makeResource (nova_align_BinaryData::index_js,
                             nova_align_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerPath == "/js/juce/index.js" || lowerPath.endsWith ("/js/juce/index.js"))
        return makeResource (nova_align_BinaryData::index_js2,
                             nova_align_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerPath == "/js/juce/check_native_interop.js" || lowerPath.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_align_BinaryData::check_native_interop_js,
                             nova_align_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    return std::nullopt;
}
