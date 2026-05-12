#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaCurveAudioProcessorEditor::NovaCurveAudioProcessorEditor (NovaCurveAudioProcessor& processor)
    : AudioProcessorEditor (&processor), processorRef (processor)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));
    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (1080, 680);
    startTimerHz (4);
}

NovaCurveAudioProcessorEditor::~NovaCurveAudioProcessorEditor()
{
    stopTimer();
}

void NovaCurveAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (juce::Colour::fromRGB (2, 5, 15));
}

void NovaCurveAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaCurveAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    if (uiInteractionActive.load (std::memory_order_relaxed))
        return;

    auto serialiseSpectrum = [] (const std::array<std::atomic<float>, NovaCurveAudioProcessor::spectrumBins>& spectrum)
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

    const auto script = "if (window.updateCurveAnalyzer) { window.updateCurveAnalyzer("
                      + serialiseSpectrum (processorRef.getPreSpectrum()) + ","
                      + serialiseSpectrum (processorRef.getPostSpectrum()) + ","
                      + serialiseSpectrum (processorRef.getReductionSpectrum()) + ","
                      + juce::String (processorRef.outputPeakLevel.load(), 4) + ","
                      + juce::String (processorRef.dynamicActivity.load(), 4) + "); }";

    webView->evaluateJavascript (script);
}

juce::WebBrowserComponent::Options NovaCurveAudioProcessorEditor::createWebOptions (NovaCurveAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaCurve")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withNativeFunction ("getInitialState", [&editor] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
                     {
                         complete (editor.processorRef.getUiStateAsJson());
                     })
                     .withNativeFunction ("setUiState", [&editor] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
                     {
                         if (! args.isEmpty())
                             editor.processorRef.applyUiStateFromJson (args[0].toString());

                         complete (true);
                     })
                     .withNativeFunction ("setInteractionActive", [&editor] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
                     {
                         if (! args.isEmpty())
                             editor.uiInteractionActive.store (static_cast<bool> (args[0]), std::memory_order_relaxed);
                         else
                             editor.uiInteractionActive.store (false, std::memory_order_relaxed);

                         complete (true);
                     })
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     });

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaCurveAudioProcessorEditor::getResource (const juce::String& url)
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
        return makeResource (nova_curve_BinaryData::index_html,
                             nova_curve_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerUrl.contains ("n_logo.png"))
        return makeResource (nova_curve_BinaryData::n_logo_png,
                             nova_curve_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_curve_BinaryData::index_js,
                             nova_curve_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerUrl.contains ("js/juce/index.js"))
        return makeResource (nova_curve_BinaryData::index_js2,
                             nova_curve_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerUrl.contains ("js/juce/check_native_interop.js"))
        return makeResource (nova_curve_BinaryData::check_native_interop_js,
                             nova_curve_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_curve_BinaryData::index_html,
                             nova_curve_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerPath == "/n_logo.png" || lowerPath.endsWith ("/n_logo.png"))
        return makeResource (nova_curve_BinaryData::n_logo_png,
                             nova_curve_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerPath == "/js/index.js" || lowerPath.endsWith ("/js/index.js"))
        return makeResource (nova_curve_BinaryData::index_js,
                             nova_curve_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerPath == "/js/juce/index.js" || lowerPath.endsWith ("/js/juce/index.js"))
        return makeResource (nova_curve_BinaryData::index_js2,
                             nova_curve_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerPath == "/js/juce/check_native_interop.js" || lowerPath.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_curve_BinaryData::check_native_interop_js,
                             nova_curve_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    return std::nullopt;
}
