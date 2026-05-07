#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaAuraAudioProcessorEditor::NovaAuraAudioProcessorEditor (NovaAuraAudioProcessor& auraProcessor)
    : AudioProcessorEditor (&auraProcessor), processorRef (auraProcessor)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    midAuraAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("mid_aura"), midAuraRelay, nullptr);
    highAuraAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("high_aura"), highAuraRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("mix"), mixRelay, nullptr);
    safeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("safe"), safeRelay, nullptr);
    wideAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("wide"), wideRelay, nullptr);
    lowLatencyAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("low_latency"), lowLatencyRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (1080, 680);
    startTimerHz (30);
}

NovaAuraAudioProcessorEditor::~NovaAuraAudioProcessorEditor()
{
    stopTimer();
}

void NovaAuraAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (10, 14, 24));
}

void NovaAuraAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaAuraAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto loadParam = [this] (const char* paramId)
    {
        if (auto* p = processorRef.apvts.getRawParameterValue (paramId))
            return p->load();
        return 0.0f;
    };

    const auto script = "if (window.updateAuraTelemetry) { window.updateAuraTelemetry({"
                      "inL:" + juce::String (processorRef.inputPeakL.load(), 4)
                    + ",inR:" + juce::String (processorRef.inputPeakR.load(), 4)
                    + ",outL:" + juce::String (processorRef.outputPeakL.load(), 4)
                    + ",outR:" + juce::String (processorRef.outputPeakR.load(), 4)
                    + ",aura:" + juce::String (processorRef.auraIntensity.load(), 4)
                    + ",harsh:" + juce::String (processorRef.harshnessAmount.load(), 4)
                    + ",midAura:" + juce::String (loadParam ("mid_aura"), 3)
                    + ",highAura:" + juce::String (loadParam ("high_aura"), 3)
                    + ",mix:" + juce::String (loadParam ("mix"), 3)
                    + ",safe:" + juce::String (loadParam ("safe"), 3)
                    + ",wide:" + juce::String (loadParam ("wide"), 3)
                    + ",lowLatency:" + juce::String (loadParam ("low_latency"), 3)
                    + "}); }";

    webView->evaluateJavascript (script);
}

juce::WebBrowserComponent::Options NovaAuraAudioProcessorEditor::createWebOptions (NovaAuraAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaAura")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.midAuraRelay)
                     .withOptionsFrom (editor.highAuraRelay)
                     .withOptionsFrom (editor.mixRelay)
                     .withOptionsFrom (editor.safeRelay)
                     .withOptionsFrom (editor.wideRelay)
                     .withOptionsFrom (editor.lowLatencyRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaAuraAudioProcessorEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));

        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    const auto lowerUrl = url.toLowerCase();

    if (lowerUrl.contains ("index.html"))
        return makeResource (nova_aura_BinaryData::index_html, nova_aura_BinaryData::index_htmlSize, "text/html");

    if (lowerUrl.contains ("n_logo.png"))
        return makeResource (nova_aura_BinaryData::n_logo_png, nova_aura_BinaryData::n_logo_pngSize, "image/png");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_aura_BinaryData::index_js, nova_aura_BinaryData::index_jsSize, "text/javascript");

    if (lowerUrl.contains ("js/juce/index.js"))
        return makeResource (nova_aura_BinaryData::index_js2, nova_aura_BinaryData::index_js2Size, "text/javascript");

    if (lowerUrl.contains ("js/juce/check_native_interop.js"))
        return makeResource (nova_aura_BinaryData::check_native_interop_js, nova_aura_BinaryData::check_native_interop_jsSize, "text/javascript");

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_aura_BinaryData::index_html, nova_aura_BinaryData::index_htmlSize, "text/html");

    if (lowerPath == "/n_logo.png" || lowerPath.endsWith ("/n_logo.png"))
        return makeResource (nova_aura_BinaryData::n_logo_png, nova_aura_BinaryData::n_logo_pngSize, "image/png");

    if (lowerPath == "/js/index.js" || lowerPath.endsWith ("/js/index.js"))
        return makeResource (nova_aura_BinaryData::index_js, nova_aura_BinaryData::index_jsSize, "text/javascript");

    if (lowerPath == "/js/juce/index.js" || lowerPath.endsWith ("/js/juce/index.js"))
        return makeResource (nova_aura_BinaryData::index_js2, nova_aura_BinaryData::index_js2Size, "text/javascript");

    if (lowerPath == "/js/juce/check_native_interop.js" || lowerPath.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_aura_BinaryData::check_native_interop_js, nova_aura_BinaryData::check_native_interop_jsSize, "text/javascript");

    return std::nullopt;
}
