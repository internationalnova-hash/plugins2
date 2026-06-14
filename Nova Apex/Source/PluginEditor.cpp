#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaApexAudioProcessorEditor::NovaApexAudioProcessorEditor (NovaApexAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    gainAttachment              = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::gain),              gainRelay,              nullptr);
    driveAttachment             = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::drive),             driveRelay,             nullptr);
    ceilingAttachment           = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::ceiling),           ceilingRelay,           nullptr);
    outputGainAttachment        = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::outputGain),        outputGainRelay,        nullptr);
    styleAttachment             = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::style),             styleRelay,             nullptr);
    attackAttachment            = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::attack),            attackRelay,            nullptr);
    releaseAttachment           = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::release),           releaseRelay,           nullptr);
    lookaheadAttachment         = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::lookahead),         lookaheadRelay,         nullptr);
    oversampleAttachment        = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::oversample),        oversampleRelay,        nullptr);
    linkAttachment              = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::link),              linkRelay,              nullptr);
    ditherAttachment            = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::dither),            ditherRelay,            nullptr);
    transientPreserveAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::transientPreserve), transientPreserveRelay, nullptr);
    lowEndProtectAttachment     = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::lowEndProtect),     lowEndProtectRelay,     nullptr);
    loudnessTargetAttachment    = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::loudnessTarget),    loudnessTargetRelay,    nullptr);
    ispProtectAttachment        = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::ispProtect),        ispProtectRelay,        nullptr);
    smartReleaseAttachment      = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::smartRelease),      smartReleaseRelay,      nullptr);
    safeModeAttachment          = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::safeMode),          safeModeRelay,          nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (1400, 860);
    startTimerHz (30);
}

NovaApexAudioProcessorEditor::~NovaApexAudioProcessorEditor()
{
    stopTimer();
}

void NovaApexAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (13, 13, 16));
}

void NovaApexAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaApexAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto loadParam = [this] (const char* paramId) -> float
    {
        if (auto* p = processorRef.apvts.getRawParameterValue (paramId))
            return p->load();
        return 0.0f;
    };

    const auto script = juce::String (
        "if (window.updateApexTelemetry) { window.updateApexTelemetry({"
        "inputL:")       + juce::String (processorRef.inputLevelL.load(),  4)
      + ",inputR:"       + juce::String (processorRef.inputLevelR.load(),  4)
      + ",outputL:"      + juce::String (processorRef.outputLevelL.load(), 4)
      + ",outputR:"      + juce::String (processorRef.outputLevelR.load(), 4)
      + ",grDb:"         + juce::String (processorRef.gainReductionDb.load(), 3)
      + ",lufs:"         + juce::String (processorRef.getLUFS(),     2)
      + ",truePeak:"     + juce::String (processorRef.getTruePeak(), 2)
      + ",novaGuard:"    + juce::String (processorRef.novaGuardActivity.load(), 3)
      + ",smartMatch:"   + juce::String (processorRef.getSmartMatchGain(), 2)
      + ",deltaActive:"  + (processorRef.deltaMode.load() ? "true" : "false")
      + ",gain:"         + juce::String (loadParam (ParameterIDs::gain),    3)
      + ",ceiling:"      + juce::String (loadParam (ParameterIDs::ceiling), 3)
      + ",style:"        + juce::String (loadParam (ParameterIDs::style),   1)
      + "}); }";

    webView->evaluateJavascript (script);
}

juce::WebBrowserComponent::Options NovaApexAudioProcessorEditor::createWebOptions (NovaApexAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaApex")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withNativeFunction ("apexSetMode",
                         [&editor] (const juce::Array<juce::var>& args, auto complete)
                         {
                             if (args.size() >= 2)
                             {
                                 const auto mode  = args[0].toString();
                                 const bool value = static_cast<bool> (args[1]);
                                 if (mode == "delta")
                                     editor.processorRef.deltaMode.store (value);
                                 else if (mode == "smartLoudness")
                                     editor.processorRef.smartLoudnessMode.store (value);
                                 else if (mode == "ditherBits")
                                     editor.processorRef.ditherBits.store (static_cast<int> (args[1]));
                                 else if (mode == "loadPreset")
                                     editor.processorRef.setCurrentProgram (static_cast<int> (args[1]));
                             }
                             complete ({});
                         })
                     .withOptionsFrom (editor.gainRelay)
                     .withOptionsFrom (editor.driveRelay)
                     .withOptionsFrom (editor.ceilingRelay)
                     .withOptionsFrom (editor.outputGainRelay)
                     .withOptionsFrom (editor.styleRelay)
                     .withOptionsFrom (editor.attackRelay)
                     .withOptionsFrom (editor.releaseRelay)
                     .withOptionsFrom (editor.lookaheadRelay)
                     .withOptionsFrom (editor.oversampleRelay)
                     .withOptionsFrom (editor.linkRelay)
                     .withOptionsFrom (editor.ditherRelay)
                     .withOptionsFrom (editor.transientPreserveRelay)
                     .withOptionsFrom (editor.lowEndProtectRelay)
                     .withOptionsFrom (editor.loudnessTargetRelay)
                     .withOptionsFrom (editor.ispProtectRelay)
                     .withOptionsFrom (editor.smartReleaseRelay)
                     .withOptionsFrom (editor.safeModeRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaApexAudioProcessorEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    const auto lowerUrl = url.toLowerCase();

    // Direct URL matches
    if (lowerUrl.contains ("index.html"))
        return makeResource (nova_apex_BinaryData::index_html,
                             nova_apex_BinaryData::index_htmlSize, "text/html");

    if (lowerUrl.contains ("n_logo.png"))
        return makeResource (nova_apex_BinaryData::n_logo_png,
                             nova_apex_BinaryData::n_logo_pngSize, "image/png");

    if (lowerUrl.contains ("js/index.js") && ! lowerUrl.contains ("juce"))
        return makeResource (nova_apex_BinaryData::index_js,
                             nova_apex_BinaryData::index_jsSize, "text/javascript");

    if (lowerUrl.contains ("js/juce/index.js"))
        return makeResource (nova_apex_BinaryData::index_js2,
                             nova_apex_BinaryData::index_js2Size, "text/javascript");

    if (lowerUrl.contains ("check_native_interop.js"))
        return makeResource (nova_apex_BinaryData::check_native_interop_js,
                             nova_apex_BinaryData::check_native_interop_jsSize, "text/javascript");

    // Path-based fallback
    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lp = resourcePath.toLowerCase();

    if (lp == "/index.html" || lp.endsWith ("/index.html"))
        return makeResource (nova_apex_BinaryData::index_html,
                             nova_apex_BinaryData::index_htmlSize, "text/html");

    if (lp == "/n_logo.png" || lp.endsWith ("/n_logo.png"))
        return makeResource (nova_apex_BinaryData::n_logo_png,
                             nova_apex_BinaryData::n_logo_pngSize, "image/png");

    if ((lp == "/js/index.js" || lp.endsWith ("/js/index.js")) && ! lp.contains ("juce"))
        return makeResource (nova_apex_BinaryData::index_js,
                             nova_apex_BinaryData::index_jsSize, "text/javascript");

    if (lp == "/js/juce/index.js" || lp.endsWith ("/js/juce/index.js"))
        return makeResource (nova_apex_BinaryData::index_js2,
                             nova_apex_BinaryData::index_js2Size, "text/javascript");

    if (lp == "/js/juce/check_native_interop.js" || lp.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_apex_BinaryData::check_native_interop_js,
                             nova_apex_BinaryData::check_native_interop_jsSize, "text/javascript");

    return std::nullopt;
}
