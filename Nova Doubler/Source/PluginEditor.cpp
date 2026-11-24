#include "PluginEditor.h"
#include "BinaryData.h"
#include "ParameterIDs.hpp"

#include <cstring>
#include <iostream>
#include "NovaLicenseGate.h"

NovaDoublerEditor::NovaDoublerEditor (NovaDoublerProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    setResizeLimits (640, 420, 1200, 800);
    setSize (820, 540);
    startTimerHz (30);

    NovaLicenseOverlay::attachTo (*this);   // unlock screen until activated
}

NovaDoublerEditor::~NovaDoublerEditor()
{
    stopTimer();
}

void NovaDoublerEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF08070A));
}

void NovaDoublerEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaDoublerEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    // Real signal, measured on the audio thread each block by
    // NovaStereoFieldAnalyzer inside the engine - never faked/simulated
    // (spec requirement for the single live stereo visualization).
    const auto leftRms = processorRef.telemetryLeftRms.load();
    const auto rightRms = processorRef.telemetryRightRms.load();
    const auto midRms = processorRef.telemetryMidRms.load();
    const auto sideRms = processorRef.telemetrySideRms.load();
    const auto correlation = processorRef.telemetryCorrelation.load();
    const auto spread = processorRef.telemetrySpread.load();

    const auto telemetryJs = "if (window.updateStereoField) { window.updateStereoField("
                            + juce::String (leftRms, 4) + ", "
                            + juce::String (rightRms, 4) + ", "
                            + juce::String (midRms, 4) + ", "
                            + juce::String (sideRms, 4) + ", "
                            + juce::String (correlation, 4) + ", "
                            + juce::String (spread, 2)
                            + "); }";

    webView->evaluateJavascript (telemetryJs);
}

juce::WebBrowserComponent::Options NovaDoublerEditor::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaDoubler")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
                     .withNativeFunction ("setParam", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (args.size() >= 2)
                         {
                             const auto paramId = args[0].toString();
                             if (auto* param = processorRef.apvts.getParameter (paramId))
                             {
                                 if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (param))
                                     param->setValueNotifyingHost (pf->convertTo0to1 ((float) (double) args[1]));
                                 else if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
                                     param->setValueNotifyingHost ((bool) args[1] ? 1.0f : 0.0f);
                             }
                         }
                         completion (juce::var());
                     })
                     .withNativeFunction ("jsError", [] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             std::cerr << "[NovaDoubler][JS ERROR] " << args[0].toString() << std::endl;
                         completion (juce::var());
                     });

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaDoublerEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const void* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    // Match by substring on the raw url - proven pattern (Nova Chord, Nova
    // Sub, Nova Keys). Deliberately NOT using getResourceProviderRoot()-
    // stripping, which silently breaks all JS loading.
    const auto lowerUrl = url.toLowerCase();

    if (lowerUrl.contains ("juce/index.js"))
        return makeResource (nova_doubler_BinaryData::index_js2, nova_doubler_BinaryData::index_js2Size, "text/javascript");

    if (lowerUrl.contains ("juce/check_native_interop.js"))
        return makeResource (nova_doubler_BinaryData::check_native_interop_js, nova_doubler_BinaryData::check_native_interop_jsSize, "text/javascript");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_doubler_BinaryData::index_js, nova_doubler_BinaryData::index_jsSize, "text/javascript");

    if (lowerUrl.contains ("index.html") || lowerUrl.endsWith ("/") || lowerUrl.isEmpty())
        return makeResource (nova_doubler_BinaryData::index_html, nova_doubler_BinaryData::index_htmlSize, "text/html");

    return std::nullopt;
}
