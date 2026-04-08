#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaToneAudioProcessorEditor::NovaToneAudioProcessorEditor (NovaToneAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    lowFreqAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::lowFreq), lowFreqRelay, nullptr);
    lowBoostAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::lowBoost), lowBoostRelay, nullptr);
    lowAttenuationAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::lowAttenuation), lowAttenuationRelay, nullptr);
    highBoostFreqAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::highBoostFreq), highBoostFreqRelay, nullptr);
    highBoostAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::highBoost), highBoostRelay, nullptr);
    bandwidthAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::bandwidth), bandwidthRelay, nullptr);
    highAttenuationFreqAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::highAttenuationFreq), highAttenuationFreqRelay, nullptr);
    highAttenuationAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::highAttenuation), highAttenuationRelay, nullptr);
    outputGainAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::outputGain), outputGainRelay, nullptr);
    modePresetAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::modePreset), modePresetRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (980, 620);
    startTimerHz (30);
}

NovaToneAudioProcessorEditor::~NovaToneAudioProcessorEditor()
{
    stopTimer();
}

void NovaToneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (242, 232, 220));
}

void NovaToneAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaToneAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto level = processorRef.outputPeakLevel.load();
    const auto isHot = processorRef.outputIsHot.load();

    const auto meterJs = "if (window.updateOutputMeter) { window.updateOutputMeter(" + juce::String (level, 3)
                       + ", " + juce::String (isHot ? "true" : "false") + "); }";

    webView->evaluateJavascript (meterJs);
}

juce::WebBrowserComponent::Options NovaToneAudioProcessorEditor::createWebOptions (NovaToneAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaTone")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.lowFreqRelay)
                     .withOptionsFrom (editor.lowBoostRelay)
                     .withOptionsFrom (editor.lowAttenuationRelay)
                     .withOptionsFrom (editor.highBoostFreqRelay)
                     .withOptionsFrom (editor.highBoostRelay)
                     .withOptionsFrom (editor.bandwidthRelay)
                     .withOptionsFrom (editor.highAttenuationFreqRelay)
                     .withOptionsFrom (editor.highAttenuationRelay)
                     .withOptionsFrom (editor.outputGainRelay)
                     .withOptionsFrom (editor.modePresetRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaToneAudioProcessorEditor::getResource (const juce::String& url)
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
        return makeResource (nova_tone_BinaryData::index_html,
                             nova_tone_BinaryData::index_htmlSize,
                             "text/html");

    if (resourcePath == "/js/index.js")
        return makeResource (nova_tone_BinaryData::index_js,
                             nova_tone_BinaryData::index_jsSize,
                             "text/javascript");

    if (resourcePath == "/js/juce/index.js")
        return makeResource (nova_tone_BinaryData::index_js2,
                             nova_tone_BinaryData::index_js2Size,
                             "text/javascript");

    if (resourcePath == "/js/juce/check_native_interop.js")
        return makeResource (nova_tone_BinaryData::check_native_interop_js,
                             nova_tone_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    return std::nullopt;
}
