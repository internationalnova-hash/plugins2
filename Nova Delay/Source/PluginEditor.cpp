#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaDelayAudioProcessorEditor::NovaDelayAudioProcessorEditor (NovaDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    presetAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::preset), presetRelay, nullptr);
    delayTimeSyncAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::delayTimeSync), delayTimeSyncRelay, nullptr);
    delayTimeFreeMsAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::delayTimeFreeMs), delayTimeFreeMsRelay, nullptr);
    syncEnabledAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::syncEnabled), syncEnabledRelay, nullptr);
    feedbackAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::feedback), feedbackRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::mix), mixRelay, nullptr);
    toneAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::tone), toneRelay, nullptr);
    wowFlutterAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::wowFlutter), wowFlutterRelay, nullptr);
    saturationAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::saturation), saturationRelay, nullptr);
    modeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::mode), modeRelay, nullptr);
    pingPongAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::pingPong), pingPongRelay, nullptr);
    stereoAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::stereo), stereoRelay, nullptr);
    lofiAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::lofi), lofiRelay, nullptr);
    freezeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::freeze), freezeRelay, nullptr);
    hpFilterHzAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::hpFilterHz), hpFilterHzRelay, nullptr);
    lpFilterHzAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::lpFilterHz), lpFilterHzRelay, nullptr);
    duckingAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::ducking), duckingRelay, nullptr);
    delayModelAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::delayModel), delayModelRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (1080, 680);
    startTimerHz (30);
}

NovaDelayAudioProcessorEditor::~NovaDelayAudioProcessorEditor()
{
    stopTimer();
}

void NovaDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (8, 6, 4));
}

void NovaDelayAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaDelayAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto inLevelL = processorRef.inputPeakLevelL.load();
    const auto inLevelR = processorRef.inputPeakLevelR.load();
    const auto outLevelL = processorRef.outputPeakLevelL.load();
    const auto outLevelR = processorRef.outputPeakLevelR.load();
    const auto isHot = processorRef.outputIsHot.load();

    const auto meterJs = "if (window.updateMeters) { window.updateMeters(" + juce::String (inLevelL, 3)
                       + ", " + juce::String (inLevelR, 3)
                       + ", " + juce::String (outLevelL, 3)
                       + ", " + juce::String (outLevelR, 3)
                       + ", " + juce::String (isHot ? "true" : "false")
                       + "); }";

    webView->evaluateJavascript (meterJs);
}

juce::WebBrowserComponent::Options NovaDelayAudioProcessorEditor::createWebOptions (NovaDelayAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaDelay")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.presetRelay)
                     .withOptionsFrom (editor.delayTimeSyncRelay)
                     .withOptionsFrom (editor.delayTimeFreeMsRelay)
                     .withOptionsFrom (editor.syncEnabledRelay)
                     .withOptionsFrom (editor.feedbackRelay)
                     .withOptionsFrom (editor.mixRelay)
                     .withOptionsFrom (editor.toneRelay)
                     .withOptionsFrom (editor.wowFlutterRelay)
                     .withOptionsFrom (editor.saturationRelay)
                     .withOptionsFrom (editor.modeRelay)
                     .withOptionsFrom (editor.pingPongRelay)
                     .withOptionsFrom (editor.stereoRelay)
                     .withOptionsFrom (editor.lofiRelay)
                     .withOptionsFrom (editor.freezeRelay)
                     .withOptionsFrom (editor.hpFilterHzRelay)
                     .withOptionsFrom (editor.lpFilterHzRelay)
                     .withOptionsFrom (editor.duckingRelay)
                     .withOptionsFrom (editor.delayModelRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaDelayAudioProcessorEditor::getResource (const juce::String& url)
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
        return makeResource (nova_delay_BinaryData::index_html,
                             nova_delay_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerUrl.contains ("n_logo.png"))
        return makeResource (nova_delay_BinaryData::n_logo_png,
                             nova_delay_BinaryData::n_logo_pngSize,
                             "image/png");

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_delay_BinaryData::index_html,
                             nova_delay_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerPath == "/n_logo.png" || lowerPath.endsWith ("/n_logo.png"))
        return makeResource (nova_delay_BinaryData::n_logo_png,
                             nova_delay_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerPath == "/js/index.js" || lowerPath.endsWith ("/js/index.js"))
        return makeResource (nova_delay_BinaryData::index_js,
                             nova_delay_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerPath == "/js/juce/index.js" || lowerPath.endsWith ("/js/juce/index.js"))
        return makeResource (nova_delay_BinaryData::index_js2,
                             nova_delay_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerPath == "/js/juce/check_native_interop.js" || lowerPath.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_delay_BinaryData::check_native_interop_js,
                             nova_delay_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    return std::nullopt;
}
