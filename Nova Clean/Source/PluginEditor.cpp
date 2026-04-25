#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaCleanV2AudioProcessorEditor::NovaCleanV2AudioProcessorEditor (NovaCleanV2AudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    modeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("mode"), modeRelay, nullptr);
    cleanAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("clean"), cleanRelay, nullptr);
    preserveAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("preserve"), preserveRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("mix"), mixRelay, nullptr);
    outputGainAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("outputGain"), outputGainRelay, nullptr);
    bypassAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("bypass"), bypassRelay, nullptr);
    lowLatencyAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("lowLatency"), lowLatencyRelay, nullptr);
    listenRemovedAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("listenRemoved"), listenRemovedRelay, nullptr);
    advancedAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("advanced"), advancedRelay, nullptr);
    sensitivityAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("sensitivity"), sensitivityRelay, nullptr);
    clickSizeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("clickSize"), clickSizeRelay, nullptr);
    freqFocusAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("freqFocus"), freqFocusRelay, nullptr);
    strengthAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("strength"), strengthRelay, nullptr);
    shapeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("shape"), shapeRelay, nullptr);
    interpolationAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("interpolation"), interpolationRelay, nullptr);
    vocalProtectAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("vocalProtect"), vocalProtectRelay, nullptr);
    transientGuardAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("transientGuard"), transientGuardRelay, nullptr);
    hqModeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("hqMode"), hqModeRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (1536, 1024);

    startTimerHz (30);
}

NovaCleanV2AudioProcessorEditor::~NovaCleanV2AudioProcessorEditor()
{
    stopTimer();
}

void NovaCleanV2AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (8, 10, 16));
}

void NovaCleanV2AudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaCleanV2AudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto script = "if (window.receiveDSP) { window.receiveDSP({"
                        "inputL:" + juce::String (processorRef.getInputMeterL(), 4)
                        + ",inputR:" + juce::String (processorRef.getInputMeterR(), 4)
                        + ",outputL:" + juce::String (processorRef.getOutputMeterL(), 4)
                        + ",outputR:" + juce::String (processorRef.getOutputMeterR(), 4)
                        + ",removedNorm:" + juce::String (processorRef.getRemovedAmountNorm(), 4)
                        + ",removedCount:" + juce::String (processorRef.getClicksRemovedCount())
                        + "}); }";

    webView->evaluateJavascript (script);
}

juce::WebBrowserComponent::Options NovaCleanV2AudioProcessorEditor::createWebOptions (NovaCleanV2AudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaCleanV2")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.modeRelay)
                     .withOptionsFrom (editor.cleanRelay)
                     .withOptionsFrom (editor.preserveRelay)
                     .withOptionsFrom (editor.mixRelay)
                     .withOptionsFrom (editor.outputGainRelay)
                     .withOptionsFrom (editor.bypassRelay)
                     .withOptionsFrom (editor.lowLatencyRelay)
                     .withOptionsFrom (editor.listenRemovedRelay)
                     .withOptionsFrom (editor.advancedRelay)
                     .withOptionsFrom (editor.sensitivityRelay)
                     .withOptionsFrom (editor.clickSizeRelay)
                     .withOptionsFrom (editor.freqFocusRelay)
                     .withOptionsFrom (editor.strengthRelay)
                     .withOptionsFrom (editor.shapeRelay)
                     .withOptionsFrom (editor.interpolationRelay)
                     .withOptionsFrom (editor.vocalProtectRelay)
                     .withOptionsFrom (editor.transientGuardRelay)
                     .withOptionsFrom (editor.hqModeRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaCleanV2AudioProcessorEditor::getResource (const juce::String& url)
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
        return makeResource (nova_clean_v2_BinaryData::index_html,
                             nova_clean_v2_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerUrl.contains ("n_logo.png"))
        return makeResource (nova_clean_v2_BinaryData::n_logo_png,
                             nova_clean_v2_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_clean_v2_BinaryData::index_js,
                             nova_clean_v2_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerUrl.contains ("js/fallback-boot.js"))
        return makeResource (nova_clean_v2_BinaryData::fallbackboot_js,
                             nova_clean_v2_BinaryData::fallbackboot_jsSize,
                             "text/javascript");

    if (lowerUrl.contains ("js/juce/index.js"))
        return makeResource (nova_clean_v2_BinaryData::index_js2,
                             nova_clean_v2_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerUrl.contains ("js/juce/check_native_interop.js"))
        return makeResource (nova_clean_v2_BinaryData::check_native_interop_js,
                             nova_clean_v2_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_clean_v2_BinaryData::index_html,
                             nova_clean_v2_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerPath == "/n_logo.png" || lowerPath.endsWith ("/n_logo.png"))
        return makeResource (nova_clean_v2_BinaryData::n_logo_png,
                             nova_clean_v2_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerPath == "/js/index.js" || lowerPath.endsWith ("/js/index.js"))
        return makeResource (nova_clean_v2_BinaryData::index_js,
                             nova_clean_v2_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerPath == "/js/fallback-boot.js" || lowerPath.endsWith ("/js/fallback-boot.js"))
        return makeResource (nova_clean_v2_BinaryData::fallbackboot_js,
                             nova_clean_v2_BinaryData::fallbackboot_jsSize,
                             "text/javascript");

    if (lowerPath == "/js/juce/index.js" || lowerPath.endsWith ("/js/juce/index.js"))
        return makeResource (nova_clean_v2_BinaryData::index_js2,
                             nova_clean_v2_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerPath == "/js/juce/check_native_interop.js" || lowerPath.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_clean_v2_BinaryData::check_native_interop_js,
                             nova_clean_v2_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    return std::nullopt;
}
