#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaClipAudioProcessorEditor::NovaClipAudioProcessorEditor (NovaClipAudioProcessor& clipProcessor)
    : AudioProcessorEditor (&clipProcessor), processorRef (clipProcessor)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    driveAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("drive"), driveRelay, nullptr);
    clipShapeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("clip_shape"), clipShapeRelay, nullptr);
    toneAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("tone"), toneRelay, nullptr);
    punchAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("punch"), punchRelay, nullptr);
    ceilingAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("ceiling"), ceilingRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("mix"), mixRelay, nullptr);

    modeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("mode"), modeRelay, nullptr);
    oversamplingAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("oversampling"), oversamplingRelay, nullptr);
    lowLatencyAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("low_latency"), lowLatencyRelay, nullptr);
    safeModeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("safe_mode"), safeModeRelay, nullptr);
    linkLRAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("link_lr"), linkLRRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (980, 620);
    startTimerHz (30);
}

NovaClipAudioProcessorEditor::~NovaClipAudioProcessorEditor()
{
    stopTimer();
}

void NovaClipAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (juce::Colour::fromRGB (6, 11, 22));
}

void NovaClipAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaClipAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto loadParam = [this] (const char* paramId)
    {
        if (auto* param = processorRef.apvts.getRawParameterValue (paramId))
            return param->load();
        return 0.0f;
    };

    const auto script = "if (window.updateClipTelemetry) { window.updateClipTelemetry({"
                      "inL:" + juce::String (processorRef.inputPeakL.load(), 4)
                    + ",inR:" + juce::String (processorRef.inputPeakR.load(), 4)
                    + ",outL:" + juce::String (processorRef.outputPeakL.load(), 4)
                    + ",outR:" + juce::String (processorRef.outputPeakR.load(), 4)
                    + ",clipDb:" + juce::String (processorRef.clipReductionDb.load(), 3)
                    + ",heat:" + juce::String (processorRef.heatAmount.load(), 4)
                    + ",drive:" + juce::String (loadParam ("drive"), 3)
                    + ",shape:" + juce::String (loadParam ("clip_shape"), 3)
                    + ",tone:" + juce::String (loadParam ("tone"), 3)
                    + ",punch:" + juce::String (loadParam ("punch"), 3)
                    + ",ceiling:" + juce::String (loadParam ("ceiling"), 3)
                    + ",mix:" + juce::String (loadParam ("mix"), 3)
                    + ",mode:" + juce::String (loadParam ("mode"), 3)
                    + ",safeMode:" + juce::String (loadParam ("safe_mode"), 3)
                    + "}); }";

    webView->evaluateJavascript (script);
}

juce::WebBrowserComponent::Options NovaClipAudioProcessorEditor::createWebOptions (NovaClipAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaClip")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.driveRelay)
                     .withOptionsFrom (editor.clipShapeRelay)
                     .withOptionsFrom (editor.toneRelay)
                     .withOptionsFrom (editor.punchRelay)
                     .withOptionsFrom (editor.ceilingRelay)
                     .withOptionsFrom (editor.mixRelay)
                     .withOptionsFrom (editor.modeRelay)
                     .withOptionsFrom (editor.oversamplingRelay)
                     .withOptionsFrom (editor.lowLatencyRelay)
                     .withOptionsFrom (editor.safeModeRelay)
                     .withOptionsFrom (editor.linkLRRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaClipAudioProcessorEditor::getResource (const juce::String& url)
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
        return makeResource (nova_clip_BinaryData::index_html,
                             nova_clip_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerUrl.contains ("n_logo.png"))
        return makeResource (nova_clip_BinaryData::n_logo_png,
                             nova_clip_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_clip_BinaryData::index_js,
                             nova_clip_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerUrl.contains ("js/juce/index.js"))
        return makeResource (nova_clip_BinaryData::index_js2,
                             nova_clip_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerUrl.contains ("js/juce/check_native_interop.js"))
        return makeResource (nova_clip_BinaryData::check_native_interop_js,
                             nova_clip_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_clip_BinaryData::index_html,
                             nova_clip_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerPath == "/n_logo.png" || lowerPath.endsWith ("/n_logo.png"))
        return makeResource (nova_clip_BinaryData::n_logo_png,
                             nova_clip_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerPath == "/js/index.js" || lowerPath.endsWith ("/js/index.js"))
        return makeResource (nova_clip_BinaryData::index_js,
                             nova_clip_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerPath == "/js/juce/index.js" || lowerPath.endsWith ("/js/juce/index.js"))
        return makeResource (nova_clip_BinaryData::index_js2,
                             nova_clip_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerPath == "/js/juce/check_native_interop.js" || lowerPath.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_clip_BinaryData::check_native_interop_js,
                             nova_clip_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    return std::nullopt;
}
