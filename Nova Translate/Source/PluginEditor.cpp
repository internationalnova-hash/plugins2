#include "PluginEditor.h"
#include "BinaryData.h"
#include "EnvironmentProfiles.hpp"

#include <cstring>
#include "NovaLicenseGate.h"

NovaTranslateAudioProcessorEditor::NovaTranslateAudioProcessorEditor (NovaTranslateAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));
    addAndMakeVisible (*webView);

    environmentIdAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::environmentId), environmentIdRelay, nullptr);
    environmentAmountAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::environmentAmount), environmentAmountRelay, nullptr);
    positionAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::position), positionRelay, nullptr);
    roomAmountAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::roomAmount), roomAmountRelay, nullptr);
    characterAmountAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::characterAmount), characterAmountRelay, nullptr);
    noiseAmountAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::noiseAmount), noiseAmountRelay, nullptr);
    noiseEnabledAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::noiseEnabled), noiseEnabledRelay, nullptr);
    levelMatchEnabledAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::levelMatchEnabled), levelMatchEnabledRelay, nullptr);
    outputTrimAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::outputTrim), outputTrimRelay, nullptr);
    monoEnabledAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::monoEnabled), monoEnabledRelay, nullptr);
    leftOnlyAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::leftOnly), leftOnlyRelay, nullptr);
    rightOnlyAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::rightOnly), rightOnlyRelay, nullptr);
    midOnlyAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::midOnly), midOnlyRelay, nullptr);
    sideOnlyAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::sideOnly), sideOnlyRelay, nullptr);
    dimEnabledAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::dimEnabled), dimEnabledRelay, nullptr);
    phaseInvertAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::phaseInvert), phaseInvertRelay, nullptr);
    bypassAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::bypass), bypassRelay, nullptr);
    safeRenderEnabledAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::safeRenderEnabled), safeRenderEnabledRelay, nullptr);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    setResizeLimits (691, 461, 1728, 1152);
    setSize (1152, 768);
    startTimerHz (30);

    NovaLicenseOverlay::attachTo (*this);   // unlock screen until activated
}

NovaTranslateAudioProcessorEditor::~NovaTranslateAudioProcessorEditor()
{
    stopTimer();
}

void NovaTranslateAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (8, 8, 10));
}

void NovaTranslateAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

// Pushes the CURRENT environment's real position map (labels + whether
// Position applies at all) to the UI, straight from EnvironmentProfiles.hpp
// - the UI never hardcodes per-environment position names, so a brand new
// environment's positions show up correctly just by adding it to that file.
// Only sent when the environment actually changed, since re-parsing/
// re-rendering the pill row on every 30Hz tick would be wasteful.
void NovaTranslateAudioProcessorEditor::sendPositionOptionsIfChanged()
{
    const auto environmentIndex = static_cast<int> (
        processorRef.apvts.getRawParameterValue (ParameterIDs::environmentId)->load());

    if (environmentIndex == lastSentEnvironmentIndex)
        return;
    lastSentEnvironmentIndex = environmentIndex;

    const auto& profile = EnvironmentLibrary::getProfile (environmentIndex);

    juce::String labelsJs = "[";
    for (int i = 0; i < profile.numPositions; ++i)
    {
        if (i > 0) labelsJs += ", ";
        labelsJs += "\"" + juce::String (profile.positions[static_cast<size_t> (i)].label)
                              .replace ("\"", "\\\"") + "\"";
    }
    labelsJs += "]";

    const auto js = "if (window.updatePositionOptions) { window.updatePositionOptions("
                   + juce::String (profile.supportsPosition ? "true" : "false") + ", "
                   + labelsJs + ", "
                   + juce::String (profile.defaultPositionIndex) + "); }";

    webView->evaluateJavascript (js);
}

void NovaTranslateAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    sendPositionOptionsIfChanged();

    const auto inputPeak = processorRef.engine.inputPeakLevel.load();
    const auto inputRms = processorRef.engine.inputRmsLevel.load();
    const auto outputPeak = processorRef.engine.outputPeakLevel.load();
    const auto outputRms = processorRef.engine.outputRmsLevel.load();
    const auto correlation = processorRef.engine.stereoCorrelation.load();
    const auto levelMatchDb = processorRef.engine.levelMatchCompensationDb.load();

    const auto meterJs = "if (window.updateMeters) { window.updateMeters("
                       + juce::String (inputPeak, 4) + ", "
                       + juce::String (inputRms, 4) + ", "
                       + juce::String (outputPeak, 4) + ", "
                       + juce::String (outputRms, 4) + ", "
                       + juce::String (correlation, 3) + ", "
                       + juce::String (levelMatchDb, 2) + "); }";

    webView->evaluateJavascript (meterJs);
}

juce::WebBrowserComponent::Options NovaTranslateAudioProcessorEditor::createWebOptions (NovaTranslateAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaTranslate")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.environmentIdRelay)
                     .withOptionsFrom (editor.environmentAmountRelay)
                     .withOptionsFrom (editor.positionRelay)
                     .withOptionsFrom (editor.roomAmountRelay)
                     .withOptionsFrom (editor.characterAmountRelay)
                     .withOptionsFrom (editor.noiseAmountRelay)
                     .withOptionsFrom (editor.noiseEnabledRelay)
                     .withOptionsFrom (editor.levelMatchEnabledRelay)
                     .withOptionsFrom (editor.outputTrimRelay)
                     .withOptionsFrom (editor.monoEnabledRelay)
                     .withOptionsFrom (editor.leftOnlyRelay)
                     .withOptionsFrom (editor.rightOnlyRelay)
                     .withOptionsFrom (editor.midOnlyRelay)
                     .withOptionsFrom (editor.sideOnlyRelay)
                     .withOptionsFrom (editor.dimEnabledRelay)
                     .withOptionsFrom (editor.phaseInvertRelay)
                     .withOptionsFrom (editor.bypassRelay)
                     .withOptionsFrom (editor.safeRenderEnabledRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaTranslateAudioProcessorEditor::getResource (const juce::String& url)
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
        return makeResource (nova_translate_BinaryData::index_html,
                             nova_translate_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerUrl.contains ("n_logo.png"))
        return makeResource (nova_translate_BinaryData::n_logo_png,
                             nova_translate_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerUrl.contains ("hero-studio.jpg"))
        return makeResource (nova_translate_BinaryData::herostudio_jpg,
                             nova_translate_BinaryData::herostudio_jpgSize,
                             "image/jpeg");
    if (lowerUrl.contains ("hero-nearfield.jpg"))
        return makeResource (nova_translate_BinaryData::heronearfield_jpg,
                             nova_translate_BinaryData::heronearfield_jpgSize,
                             "image/jpeg");
    if (lowerUrl.contains ("hero-mastering.jpg"))
        return makeResource (nova_translate_BinaryData::heromastering_jpg,
                             nova_translate_BinaryData::heromastering_jpgSize,
                             "image/jpeg");
    if (lowerUrl.contains ("hero-bedroom.jpg"))
        return makeResource (nova_translate_BinaryData::herobedroom_jpg,
                             nova_translate_BinaryData::herobedroom_jpgSize,
                             "image/jpeg");
    if (lowerUrl.contains ("hero-car.jpg"))
        return makeResource (nova_translate_BinaryData::herocar_jpg,
                             nova_translate_BinaryData::herocar_jpgSize,
                             "image/jpeg");
    if (lowerUrl.contains ("hero-suv.jpg"))
        return makeResource (nova_translate_BinaryData::herosuv_jpg,
                             nova_translate_BinaryData::herosuv_jpgSize,
                             "image/jpeg");
    if (lowerUrl.contains ("hero-phone.jpg"))
        return makeResource (nova_translate_BinaryData::herophone_jpg,
                             nova_translate_BinaryData::herophone_jpgSize,
                             "image/jpeg");
    if (lowerUrl.contains ("hero-laptop.jpg"))
        return makeResource (nova_translate_BinaryData::herolaptop_jpg,
                             nova_translate_BinaryData::herolaptop_jpgSize,
                             "image/jpeg");
    if (lowerUrl.contains ("hero-bluetooth.jpg"))
        return makeResource (nova_translate_BinaryData::herobluetooth_jpg,
                             nova_translate_BinaryData::herobluetooth_jpgSize,
                             "image/jpeg");
    if (lowerUrl.contains ("hero-boombox.jpg"))
        return makeResource (nova_translate_BinaryData::heroboombox_jpg,
                             nova_translate_BinaryData::heroboombox_jpgSize,
                             "image/jpeg");
    if (lowerUrl.contains ("hero-club.jpg"))
        return makeResource (nova_translate_BinaryData::heroclub_jpg,
                             nova_translate_BinaryData::heroclub_jpgSize,
                             "image/jpeg");
    if (lowerUrl.contains ("hero-cube.jpg"))
        return makeResource (nova_translate_BinaryData::herocube_jpg,
                             nova_translate_BinaryData::herocube_jpgSize,
                             "image/jpeg");

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_translate_BinaryData::index_html,
                             nova_translate_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerPath == "/n_logo.png" || lowerPath.endsWith ("/n_logo.png"))
        return makeResource (nova_translate_BinaryData::n_logo_png,
                             nova_translate_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerPath.endsWith ("/hero-studio.jpg"))
        return makeResource (nova_translate_BinaryData::herostudio_jpg,
                             nova_translate_BinaryData::herostudio_jpgSize,
                             "image/jpeg");
    if (lowerPath.endsWith ("/hero-nearfield.jpg"))
        return makeResource (nova_translate_BinaryData::heronearfield_jpg,
                             nova_translate_BinaryData::heronearfield_jpgSize,
                             "image/jpeg");
    if (lowerPath.endsWith ("/hero-mastering.jpg"))
        return makeResource (nova_translate_BinaryData::heromastering_jpg,
                             nova_translate_BinaryData::heromastering_jpgSize,
                             "image/jpeg");
    if (lowerPath.endsWith ("/hero-bedroom.jpg"))
        return makeResource (nova_translate_BinaryData::herobedroom_jpg,
                             nova_translate_BinaryData::herobedroom_jpgSize,
                             "image/jpeg");
    if (lowerPath.endsWith ("/hero-car.jpg"))
        return makeResource (nova_translate_BinaryData::herocar_jpg,
                             nova_translate_BinaryData::herocar_jpgSize,
                             "image/jpeg");
    if (lowerPath.endsWith ("/hero-suv.jpg"))
        return makeResource (nova_translate_BinaryData::herosuv_jpg,
                             nova_translate_BinaryData::herosuv_jpgSize,
                             "image/jpeg");
    if (lowerPath.endsWith ("/hero-phone.jpg"))
        return makeResource (nova_translate_BinaryData::herophone_jpg,
                             nova_translate_BinaryData::herophone_jpgSize,
                             "image/jpeg");
    if (lowerPath.endsWith ("/hero-laptop.jpg"))
        return makeResource (nova_translate_BinaryData::herolaptop_jpg,
                             nova_translate_BinaryData::herolaptop_jpgSize,
                             "image/jpeg");
    if (lowerPath.endsWith ("/hero-bluetooth.jpg"))
        return makeResource (nova_translate_BinaryData::herobluetooth_jpg,
                             nova_translate_BinaryData::herobluetooth_jpgSize,
                             "image/jpeg");
    if (lowerPath.endsWith ("/hero-boombox.jpg"))
        return makeResource (nova_translate_BinaryData::heroboombox_jpg,
                             nova_translate_BinaryData::heroboombox_jpgSize,
                             "image/jpeg");
    if (lowerPath.endsWith ("/hero-club.jpg"))
        return makeResource (nova_translate_BinaryData::heroclub_jpg,
                             nova_translate_BinaryData::heroclub_jpgSize,
                             "image/jpeg");
    if (lowerPath.endsWith ("/hero-cube.jpg"))
        return makeResource (nova_translate_BinaryData::herocube_jpg,
                             nova_translate_BinaryData::herocube_jpgSize,
                             "image/jpeg");

    return std::nullopt;
}
