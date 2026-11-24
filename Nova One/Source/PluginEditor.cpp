#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>
#include "NovaLicenseGate.h"

NovaOneAudioProcessorEditor::NovaOneAudioProcessorEditor (NovaOneAudioProcessor& p)
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
    finishAmountAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::finishAmount), finishAmountRelay, nullptr);
    modePresetAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::modePreset), modePresetRelay, nullptr);
    meterViewAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::meterView), meterViewRelay, nullptr);
    autoLevelAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::autoLevel), autoLevelRelay, nullptr);
    cleanAmountAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::cleanAmount), cleanAmountRelay, nullptr);
    warmAmountAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::warmAmount), warmAmountRelay, nullptr);
    wideAmountAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::wideAmount), wideAmountRelay, nullptr);
    loudAmountAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::loudAmount), loudAmountRelay, nullptr);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    setResizeLimits (648, 408, 1620, 1020);
    setSize (1080, 680);
    startTimerHz (30);

    NovaLicenseOverlay::attachTo (*this);   // unlock screen until activated
}

NovaOneAudioProcessorEditor::~NovaOneAudioProcessorEditor()
{
    stopTimer();
}

void NovaOneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (239, 232, 222));
}

void NovaOneAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaOneAudioProcessorEditor::timerCallback()
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

    const auto inputPeak = processorRef.inputPeakLevel.load();
    const auto inputRms = processorRef.inputRmsLevel.load();

    const auto inputMeterJs = "if (window.updateInputMeter) { window.updateInputMeter("
                             + juce::String (inputPeak, 3) + ", "
                             + juce::String (inputRms, 3) + "); }";

    webView->evaluateJavascript (inputMeterJs);
}

juce::WebBrowserComponent::Options NovaOneAudioProcessorEditor::createWebOptions (NovaOneAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaOne")));
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
                     .withOptionsFrom (editor.finishAmountRelay)
                     .withOptionsFrom (editor.modePresetRelay)
                     .withOptionsFrom (editor.meterViewRelay)
                     .withOptionsFrom (editor.autoLevelRelay)
                     .withOptionsFrom (editor.cleanAmountRelay)
                     .withOptionsFrom (editor.warmAmountRelay)
                     .withOptionsFrom (editor.wideAmountRelay)
                     .withOptionsFrom (editor.loudAmountRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaOneAudioProcessorEditor::getResource (const juce::String& url)
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
        return makeResource (nova_one_BinaryData::index_html,
                             nova_one_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerUrl.contains ("n_logo.png"))
        return makeResource (nova_one_BinaryData::n_logo_png,
                             nova_one_BinaryData::n_logo_pngSize,
                             "image/png");

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_one_BinaryData::index_html,
                             nova_one_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerPath == "/n_logo.png" || lowerPath.endsWith ("/n_logo.png"))
        return makeResource (nova_one_BinaryData::n_logo_png,
                             nova_one_BinaryData::n_logo_pngSize,
                             "image/png");

    return std::nullopt;
}
