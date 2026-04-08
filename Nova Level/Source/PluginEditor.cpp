#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaLevelAudioProcessorEditor::NovaLevelAudioProcessorEditor (NovaLevelAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    compressionAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("compression"), compressionRelay, nullptr);
    toneAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("tone"), toneRelay, nullptr);
    outputAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("output"), outputRelay, nullptr);
    modeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("mode"), modeRelay, nullptr);
    magicAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("magic"), magicRelay, nullptr);
    meterAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("meter"), meterRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (980, 620);
    startTimerHz (30);
}

NovaLevelAudioProcessorEditor::~NovaLevelAudioProcessorEditor()
{
    stopTimer();
}

void NovaLevelAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (236, 226, 213));
}

void NovaLevelAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaLevelAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto output = processorRef.outputPeakLevel.load();
    const auto gainReduction = processorRef.gainReductionLevel.load();
    const auto isHot = processorRef.outputIsHot.load();

    const auto meterJs = "if (window.updateMeters) { window.updateMeters(" + juce::String (gainReduction, 3)
        + ", " + juce::String (output, 3)
        + ", " + juce::String (isHot ? "true" : "false") + "); }";

    webView->evaluateJavascript (meterJs);
}

juce::WebBrowserComponent::Options NovaLevelAudioProcessorEditor::createWebOptions (NovaLevelAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaLevel")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.compressionRelay)
                     .withOptionsFrom (editor.toneRelay)
                     .withOptionsFrom (editor.outputRelay)
                     .withOptionsFrom (editor.modeRelay)
                     .withOptionsFrom (editor.magicRelay)
                     .withOptionsFrom (editor.meterRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaLevelAudioProcessorEditor::getResource (const juce::String& url)
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

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_level_BinaryData::index_html,
                             nova_level_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerPath.endsWith ("/n_logo.png") || lowerPath == "/n_logo.png")
        return makeResource (nova_level_BinaryData::n_logo_png,
                             nova_level_BinaryData::n_logo_pngSize,
                             "image/png");

    return std::nullopt;
}
