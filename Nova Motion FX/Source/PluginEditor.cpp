#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaMotionFXEditor::NovaMotionFXEditor (NovaMotionFXProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    auto& apvts = processor.apvts;
    motionAttachment    = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter ("motion"),     motionRelay,    nullptr);
    cutoffAttachment    = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter ("cutoff"),     cutoffRelay,    nullptr);
    resonanceAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter ("resonance"),  resonanceRelay, nullptr);
    driveAttachment     = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter ("drive"),      driveRelay,     nullptr);
    feedbackAttachment  = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter ("feedback"),   feedbackRelay,  nullptr);
    delayMixAttachment  = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter ("delay_mix"),  delayMixRelay,  nullptr);
    sizeAttachment      = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter ("size"),       sizeRelay,      nullptr);
    decayAttachment     = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter ("decay"),      decayRelay,     nullptr);
    reverbMixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter ("reverb_mix"), reverbMixRelay, nullptr);
    lfoRateAttachment   = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter ("lfo_rate"),   lfoRateRelay,   nullptr);
    lfoDepthAttachment  = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter ("lfo_depth"),  lfoDepthRelay,  nullptr);
    inputAttachment     = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter ("input"),      inputRelay,     nullptr);
    outputAttachment    = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter ("output"),     outputRelay,    nullptr);
    mixAttachment       = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter ("mix"),        mixRelay,       nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (960, 620);
    startTimerHz (30);
}

NovaMotionFXEditor::~NovaMotionFXEditor()
{
    stopTimer();
}

void NovaMotionFXEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111113));
}

void NovaMotionFXEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaMotionFXEditor::timerCallback()
{
    if (webView == nullptr || !webView->isVisible()) return;
    const float l = processor.peakL.load();
    const float r = processor.peakR.load();
    const auto script = juce::String ("if(window.updateMeters)updateMeters(")
                      + juce::String (l) + "," + juce::String (r) + ");";
    webView->evaluateJavascript (script);
}

juce::WebBrowserComponent::Options NovaMotionFXEditor::createWebOptions (NovaMotionFXEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaMotionFX")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.motionRelay)
                     .withOptionsFrom (editor.cutoffRelay)
                     .withOptionsFrom (editor.resonanceRelay)
                     .withOptionsFrom (editor.driveRelay)
                     .withOptionsFrom (editor.feedbackRelay)
                     .withOptionsFrom (editor.delayMixRelay)
                     .withOptionsFrom (editor.sizeRelay)
                     .withOptionsFrom (editor.decayRelay)
                     .withOptionsFrom (editor.reverbMixRelay)
                     .withOptionsFrom (editor.lfoRateRelay)
                     .withOptionsFrom (editor.lfoDepthRelay)
                     .withOptionsFrom (editor.inputRelay)
                     .withOptionsFrom (editor.outputRelay)
                     .withOptionsFrom (editor.mixRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaMotionFXEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_motion_BinaryData::index_html,
                             nova_motion_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerPath == "/n_logo.png" || lowerPath.endsWith ("/n_logo.png"))
        return makeResource (nova_motion_BinaryData::n_logo_png,
                             nova_motion_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerPath == "/js/index.js" || lowerPath.endsWith ("/js/index.js"))
        return makeResource (nova_motion_BinaryData::index_js,
                             nova_motion_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerPath == "/js/juce/index.js" || lowerPath.endsWith ("/js/juce/index.js"))
        return makeResource (nova_motion_BinaryData::index_js2,
                             nova_motion_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerPath == "/js/juce/check_native_interop.js" || lowerPath.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_motion_BinaryData::check_native_interop_js,
                             nova_motion_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    return std::nullopt;
}
