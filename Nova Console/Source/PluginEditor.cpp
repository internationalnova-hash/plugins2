
#include "PluginEditor.h"
#include "BinaryData.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cstring>

NovaConsoleAudioProcessorEditor::NovaConsoleAudioProcessorEditor (NovaConsoleAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    modeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("console_mode"), modeRelay, nullptr);
    qualityAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("quality"), qualityRelay, nullptr);
    oversamplingAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("oversampling"), oversamplingRelay, nullptr);

    inputAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("input"), inputRelay, nullptr);
    outputAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("output"), outputRelay, nullptr);

    preampOnAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("preamp_on"), preampOnRelay, nullptr);
    driveAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("drive"), driveRelay, nullptr);
    colorAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("color"), colorRelay, nullptr);
    trimAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("trim"), trimRelay, nullptr);

    filterOnAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("filter_on"), filterOnRelay, nullptr);
    hpfAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("hpf"), hpfRelay, nullptr);
    lpfAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("lpf"), lpfRelay, nullptr);
    hpfSlopeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("hpf_slope"), hpfSlopeRelay, nullptr);
    lpfSlopeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("lpf_slope"), lpfSlopeRelay, nullptr);

    lowModeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("eq_low_mode"), lowModeRelay, nullptr);
    highModeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("eq_high_mode"), highModeRelay, nullptr);
    airModeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("eq_air_mode"), airModeRelay, nullptr);

    compOnAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("comp_on"), compOnRelay, nullptr);
    compThresholdAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("comp_threshold"), compThresholdRelay, nullptr);
    compRatioAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("comp_ratio"), compRatioRelay, nullptr);
    compAttackAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("comp_attack"), compAttackRelay, nullptr);
    compReleaseAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("comp_release"), compReleaseRelay, nullptr);
    compMixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("comp_mix"), compMixRelay, nullptr);
    compMakeupAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("comp_makeup"), compMakeupRelay, nullptr);
    compPunchAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("comp_punch"), compPunchRelay, nullptr);

    gateOnAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("gate_on"), gateOnRelay, nullptr);
    gateThresholdAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("gate_threshold"), gateThresholdRelay, nullptr);
    gateAttackAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("gate_attack"), gateAttackRelay, nullptr);
    gateHoldAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("gate_hold"), gateHoldRelay, nullptr);
    gateReleaseAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("gate_release"), gateReleaseRelay, nullptr);
    gateRangeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("gate_range"), gateRangeRelay, nullptr);
    gateSmoothAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("gate_smooth"), gateSmoothRelay, nullptr);

    analogOnAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("analog_on"), analogOnRelay, nullptr);
    analogHeatAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("analog_heat"), analogHeatRelay, nullptr);
    analogDepthAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("analog_depth"), analogDepthRelay, nullptr);
    analogWidthAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("analog_width"), analogWidthRelay, nullptr);
    analogDriftAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("analog_drift"), analogDriftRelay, nullptr);
    analogCrosstalkAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("analog_crosstalk"), analogCrosstalkRelay, nullptr);
    analogNoiseAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("analog_noise"), analogNoiseRelay, nullptr);

    smartGainAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("smart_gain"), smartGainRelay, nullptr);
    focusModeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("focus_mode"), focusModeRelay, nullptr);
    mixAssistAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("mix_assist"), mixAssistRelay, nullptr);
    sidechainModeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("sidechain_mode"), sidechainModeRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (1240, 760);
    startTimerHz (30);
}

NovaConsoleAudioProcessorEditor::~NovaConsoleAudioProcessorEditor()
{
    stopTimer();
}

void NovaConsoleAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (10, 10, 12));
}

void NovaConsoleAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaConsoleAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto inLevel = processorRef.getInputMeter();
    const auto outLevel = processorRef.getOutputMeter();
    const auto grLevel = processorRef.getGainReductionMeter();

    const auto meterJs = "if (window.updateMeters) { window.updateMeters(" + juce::String (inLevel, 4)
                       + "," + juce::String (outLevel, 4)
                       + "," + juce::String (grLevel, 4)
                       + "); }";

    webView->evaluateJavascript (meterJs);
}

juce::WebBrowserComponent::Options NovaConsoleAudioProcessorEditor::createWebOptions (NovaConsoleAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaConsole")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.modeRelay)
                     .withOptionsFrom (editor.qualityRelay)
                     .withOptionsFrom (editor.oversamplingRelay)
                     .withOptionsFrom (editor.inputRelay)
                     .withOptionsFrom (editor.outputRelay)
                     .withOptionsFrom (editor.preampOnRelay)
                     .withOptionsFrom (editor.driveRelay)
                     .withOptionsFrom (editor.colorRelay)
                     .withOptionsFrom (editor.trimRelay)
                     .withOptionsFrom (editor.filterOnRelay)
                     .withOptionsFrom (editor.hpfRelay)
                     .withOptionsFrom (editor.lpfRelay)
                     .withOptionsFrom (editor.hpfSlopeRelay)
                     .withOptionsFrom (editor.lpfSlopeRelay)
                     .withOptionsFrom (editor.lowModeRelay)
                     .withOptionsFrom (editor.highModeRelay)
                     .withOptionsFrom (editor.airModeRelay)
                     .withOptionsFrom (editor.compOnRelay)
                     .withOptionsFrom (editor.compThresholdRelay)
                     .withOptionsFrom (editor.compRatioRelay)
                     .withOptionsFrom (editor.compAttackRelay)
                     .withOptionsFrom (editor.compReleaseRelay)
                     .withOptionsFrom (editor.compMixRelay)
                     .withOptionsFrom (editor.compMakeupRelay)
                     .withOptionsFrom (editor.compPunchRelay)
                     .withOptionsFrom (editor.gateOnRelay)
                     .withOptionsFrom (editor.gateThresholdRelay)
                     .withOptionsFrom (editor.gateAttackRelay)
                     .withOptionsFrom (editor.gateHoldRelay)
                     .withOptionsFrom (editor.gateReleaseRelay)
                     .withOptionsFrom (editor.gateRangeRelay)
                     .withOptionsFrom (editor.gateSmoothRelay)
                     .withOptionsFrom (editor.analogOnRelay)
                     .withOptionsFrom (editor.analogHeatRelay)
                     .withOptionsFrom (editor.analogDepthRelay)
                     .withOptionsFrom (editor.analogWidthRelay)
                     .withOptionsFrom (editor.analogDriftRelay)
                     .withOptionsFrom (editor.analogCrosstalkRelay)
                     .withOptionsFrom (editor.analogNoiseRelay)
                     .withOptionsFrom (editor.smartGainRelay)
                     .withOptionsFrom (editor.focusModeRelay)
                     .withOptionsFrom (editor.mixAssistRelay)
                     .withOptionsFrom (editor.sidechainModeRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaConsoleAudioProcessorEditor::getResource (const juce::String& url)
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
        return makeResource (nova_console::index_html,
                             nova_console::index_htmlSize,
                             "text/html");

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_console::index_html,
                             nova_console::index_htmlSize,
                             "text/html");

    return std::nullopt;
}
