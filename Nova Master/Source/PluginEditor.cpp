#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaMasterAudioProcessorEditor::NovaMasterAudioProcessorEditor (NovaMasterAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));
    addAndMakeVisible (*webView);

    toneAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::tone), toneRelay, nullptr);
    glueAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::glue), glueRelay, nullptr);
    weightAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::weight), weightRelay, nullptr);
    airAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::air), airRelay, nullptr);
    widthAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::width), widthRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::mix), mixRelay, nullptr);
    outputGainAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::outputGain), outputGainRelay, nullptr);
    finishModeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::finishMode), finishModeRelay, nullptr);
    modePresetAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::modePreset), modePresetRelay, nullptr);
    meterViewAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::meterView), meterViewRelay, nullptr);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (980, 580);
    startTimerHz (30);
}

NovaMasterAudioProcessorEditor::~NovaMasterAudioProcessorEditor()
{
    stopTimer();
}

void NovaMasterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (239, 232, 222));
}

void NovaMasterAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaMasterAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto outputLevel = processorRef.outputPeakLevel.load();
    const auto loudnessLike = juce::jlimit (0.0f, 1.0f,
                                            processorRef.outputRmsLevel.load()
                                            + (processorRef.limiterReductionLevel.load() * 0.20f));
    const auto isHot = processorRef.outputIsHot.load();

    const auto meterJs = "if (window.updateOutputMeter) { window.updateOutputMeter("
                       + juce::String (outputLevel, 3) + ", "
                       + juce::String (loudnessLike, 3) + ", "
                       + juce::String (isHot ? "true" : "false") + "); }";

    webView->evaluateJavascript (meterJs);
}

juce::WebBrowserComponent::Options NovaMasterAudioProcessorEditor::createWebOptions (NovaMasterAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaMaster")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.toneRelay)
                     .withOptionsFrom (editor.glueRelay)
                     .withOptionsFrom (editor.weightRelay)
                     .withOptionsFrom (editor.airRelay)
                     .withOptionsFrom (editor.widthRelay)
                     .withOptionsFrom (editor.mixRelay)
                     .withOptionsFrom (editor.outputGainRelay)
                     .withOptionsFrom (editor.finishModeRelay)
                     .withOptionsFrom (editor.modePresetRelay)
                     .withOptionsFrom (editor.meterViewRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaMasterAudioProcessorEditor::getResource (const juce::String& url)
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
        return makeResource (nova_master_BinaryData::index_html,
                             nova_master_BinaryData::index_htmlSize,
                             "text/html");

    return std::nullopt;
}
