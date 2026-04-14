#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaPitchAudioProcessorEditor::NovaPitchAudioProcessorEditor (NovaPitchAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    keyAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("key"), keyRelay, nullptr);
    scaleAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("scale"), scaleRelay, nullptr);
    toleranceAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("tolerance"), toleranceRelay, nullptr);
    amountAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("amount"), amountRelay, nullptr);
    confidenceAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("confidenceThreshold"), confidenceRelay, nullptr);
    vibratoAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("vibrato"), vibratoRelay, nullptr);
    formantAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("formant"), formantRelay, nullptr);
    lowLatencyAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("lowLatency"), lowLatencyRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (1060, 640);
    startTimerHz (30);
}

NovaPitchAudioProcessorEditor::~NovaPitchAudioProcessorEditor()
{
    stopTimer();
}

void NovaPitchAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (8, 17, 29));
}

void NovaPitchAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaPitchAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto detectedHz = processorRef.getDetectedPitch();
    const auto correctedHz = processorRef.getCorrectedPitch();
    const auto confidence = juce::jlimit (0.0f, 1.0f, processorRef.getConfidence());

    const auto correctionAmount = detectedHz > 1.0f
        ? juce::jlimit (0.0f, 1.0f, std::abs (correctedHz - detectedHz) / juce::jmax (detectedHz, 1.0f))
        : 0.0f;

    const auto isRetuneActive = correctionAmount > 0.005f;

    const auto script = "if (window.receiveDSP) { window.receiveDSP({"
                      "correctionAmount:" + juce::String (correctionAmount, 4)
                      + ",trackingConfidence:" + juce::String (confidence, 4)
                      + ",retuneActive:" + juce::String (isRetuneActive ? "true" : "false")
                      + "}); }";

    webView->evaluateJavascript (script);
}

juce::WebBrowserComponent::Options NovaPitchAudioProcessorEditor::createWebOptions (NovaPitchAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaPitch")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.keyRelay)
                     .withOptionsFrom (editor.scaleRelay)
                     .withOptionsFrom (editor.toleranceRelay)
                     .withOptionsFrom (editor.amountRelay)
                     .withOptionsFrom (editor.confidenceRelay)
                     .withOptionsFrom (editor.vibratoRelay)
                     .withOptionsFrom (editor.formantRelay)
                     .withOptionsFrom (editor.lowLatencyRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaPitchAudioProcessorEditor::getResource (const juce::String& url)
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
        return makeResource (nova_pitch_BinaryData::index_html,
                             nova_pitch_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerUrl.contains ("n_logo.png"))
        return makeResource (nova_pitch_BinaryData::n_logo_png,
                             nova_pitch_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_pitch_BinaryData::index_js,
                             nova_pitch_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerUrl.contains ("js/fallback-boot.js"))
        return makeResource (nova_pitch_BinaryData::fallbackboot_js,
                             nova_pitch_BinaryData::fallbackboot_jsSize,
                             "text/javascript");

    if (lowerUrl.contains ("js/juce/index.js"))
        return makeResource (nova_pitch_BinaryData::index_js2,
                             nova_pitch_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerUrl.contains ("js/juce/check_native_interop.js"))
        return makeResource (nova_pitch_BinaryData::check_native_interop_js,
                             nova_pitch_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_pitch_BinaryData::index_html,
                             nova_pitch_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerPath == "/n_logo.png" || lowerPath.endsWith ("/n_logo.png"))
        return makeResource (nova_pitch_BinaryData::n_logo_png,
                             nova_pitch_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerPath == "/js/index.js" || lowerPath.endsWith ("/js/index.js"))
        return makeResource (nova_pitch_BinaryData::index_js,
                             nova_pitch_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerPath == "/js/fallback-boot.js" || lowerPath.endsWith ("/js/fallback-boot.js"))
        return makeResource (nova_pitch_BinaryData::fallbackboot_js,
                             nova_pitch_BinaryData::fallbackboot_jsSize,
                             "text/javascript");

    if (lowerPath == "/js/juce/index.js" || lowerPath.endsWith ("/js/juce/index.js"))
        return makeResource (nova_pitch_BinaryData::index_js2,
                             nova_pitch_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerPath == "/js/juce/check_native_interop.js" || lowerPath.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_pitch_BinaryData::check_native_interop_js,
                             nova_pitch_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    return std::nullopt;
}
