#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaVoiceAudioProcessorEditor::NovaVoiceAudioProcessorEditor (NovaVoiceAudioProcessor& processor)
    : AudioProcessorEditor (&processor), processorRef (processor)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    pitchAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("pitch"), pitchRelay, nullptr);
    morphAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("morph"), morphRelay, nullptr);
    textureAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("texture"), textureRelay, nullptr);
    formAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("form"), formRelay, nullptr);
    airAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("air"), airRelay, nullptr);
    blendAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("blend"), blendRelay, nullptr);
    modeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("voice_mode"), modeRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (1080, 680);
    startTimerHz (30);
}

NovaVoiceAudioProcessorEditor::~NovaVoiceAudioProcessorEditor()
{
    stopTimer();
}

void NovaVoiceAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (juce::Colour::fromRGB (234, 232, 235));
}

void NovaVoiceAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaVoiceAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    auto serialiseSpectrum = [] (const std::array<std::atomic<float>, NovaVoiceAudioProcessor::spectrumBins>& spectrum)
    {
        juce::String output;
        output << "[";

        for (size_t index = 0; index < spectrum.size(); ++index)
        {
            if (index != 0)
                output << ",";

            output << juce::String (spectrum[index].load(), 4);
        }

        output << "]";
        return output;
    };

    const auto script = "if (window.updateVoiceSpectrum) { window.updateVoiceSpectrum("
                      + serialiseSpectrum (processorRef.getInputSpectrum()) + ","
                      + serialiseSpectrum (processorRef.getProblemSpectrum()) + ","
                      + serialiseSpectrum (processorRef.getReductionSpectrum()) + "); }"
                      + "if (window.updateOutputPeak) { window.updateOutputPeak("
                      + juce::String (processorRef.outputPeakLevel.load(), 4) + "); }";

    webView->evaluateJavascript (script);
}

juce::WebBrowserComponent::Options NovaVoiceAudioProcessorEditor::createWebOptions (NovaVoiceAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaVoice")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.pitchRelay)
                     .withOptionsFrom (editor.morphRelay)
                     .withOptionsFrom (editor.textureRelay)
                     .withOptionsFrom (editor.formRelay)
                     .withOptionsFrom (editor.airRelay)
                     .withOptionsFrom (editor.blendRelay)
                     .withOptionsFrom (editor.modeRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaVoiceAudioProcessorEditor::getResource (const juce::String& url)
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
        return makeResource (nova_voice_BinaryData::index_html,
                             nova_voice_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerUrl.contains ("n_logo.png"))
        return makeResource (nova_voice_BinaryData::n_logo_png,
                             nova_voice_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_voice_BinaryData::index_js,
                             nova_voice_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerUrl.contains ("js/fallback-boot.js"))
        return makeResource (nova_voice_BinaryData::fallbackboot_js,
                             nova_voice_BinaryData::fallbackboot_jsSize,
                             "text/javascript");

    if (lowerUrl.contains ("js/juce/index.js"))
        return makeResource (nova_voice_BinaryData::index_js2,
                             nova_voice_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerUrl.contains ("js/juce/check_native_interop.js"))
        return makeResource (nova_voice_BinaryData::check_native_interop_js,
                             nova_voice_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_voice_BinaryData::index_html,
                             nova_voice_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerPath == "/n_logo.png" || lowerPath.endsWith ("/n_logo.png"))
        return makeResource (nova_voice_BinaryData::n_logo_png,
                             nova_voice_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerPath == "/js/index.js" || lowerPath.endsWith ("/js/index.js"))
        return makeResource (nova_voice_BinaryData::index_js,
                             nova_voice_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerPath == "/js/fallback-boot.js" || lowerPath.endsWith ("/js/fallback-boot.js"))
        return makeResource (nova_voice_BinaryData::fallbackboot_js,
                             nova_voice_BinaryData::fallbackboot_jsSize,
                             "text/javascript");

    if (lowerPath == "/js/juce/index.js" || lowerPath.endsWith ("/js/juce/index.js"))
        return makeResource (nova_voice_BinaryData::index_js2,
                             nova_voice_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerPath == "/js/juce/check_native_interop.js" || lowerPath.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_voice_BinaryData::check_native_interop_js,
                             nova_voice_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    return std::nullopt;
}