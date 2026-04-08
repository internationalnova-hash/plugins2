#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaHeatAudioProcessorEditor::NovaHeatAudioProcessorEditor (NovaHeatAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    driveAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::drive), driveRelay, nullptr);
    toneAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::tone), toneRelay, nullptr);
    heatAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::heat), heatRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::mix), mixRelay, nullptr);
    outputGainAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::outputGain), outputGainRelay, nullptr);
    characterModeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::characterMode), characterModeRelay, nullptr);
    magicModeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::magicMode), magicModeRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (980, 620);
    startTimerHz (30);
}

NovaHeatAudioProcessorEditor::~NovaHeatAudioProcessorEditor()
{
    stopTimer();
}

void NovaHeatAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (238, 217, 187));
}

void NovaHeatAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaHeatAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto level = processorRef.outputPeakLevel.load();
    const auto isHot = processorRef.outputIsHot.load();

    const auto meterJs = "if (window.updateOutputMeter) { window.updateOutputMeter(" + juce::String (level, 3)
                       + ", " + juce::String (isHot ? "true" : "false") + "); }";

    webView->evaluateJavascript (meterJs);
}

juce::WebBrowserComponent::Options NovaHeatAudioProcessorEditor::createWebOptions (NovaHeatAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaHeat")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.driveRelay)
                     .withOptionsFrom (editor.toneRelay)
                     .withOptionsFrom (editor.heatRelay)
                     .withOptionsFrom (editor.mixRelay)
                     .withOptionsFrom (editor.outputGainRelay)
                     .withOptionsFrom (editor.characterModeRelay)
                     .withOptionsFrom (editor.magicModeRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaHeatAudioProcessorEditor::getResource (const juce::String& url)
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

    if (resourcePath == "/index.html")
        return makeResource (nova_heat_BinaryData::index_html,
                             nova_heat_BinaryData::index_htmlSize,
                             "text/html");

    return std::nullopt;
}
