#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaChordAudioProcessorEditor::NovaChordAudioProcessorEditor (NovaChordAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    styleIdxAttachment    = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("styleIdx"),    styleIdxRelay,    nullptr);
    octaveShiftAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("octaveShift"), octaveShiftRelay, nullptr);
    velocityAttachment    = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("velocity"),    velocityRelay,    nullptr);
    passThroughAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("passThrough"), passThroughRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (1100, 700);
    startTimerHz (30);
}

NovaChordAudioProcessorEditor::~NovaChordAudioProcessorEditor()
{
    stopTimer();
}

void NovaChordAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (10, 10, 15));
}

void NovaChordAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaChordAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const int  noteOn      = processorRef.getLastNoteOn();
    const int  chordRoot   = processorRef.getChordRootMidi();
    const bool chordActive = processorRef.isChordActive();

    const auto script = "if (window.receiveChordDSP) { window.receiveChordDSP({"
                      "lastNoteOn:"  + juce::String (noteOn)
                      + ",chordRoot:"  + juce::String (chordRoot)
                      + ",chordActive:" + juce::String (chordActive ? "true" : "false")
                      + "}); }";

    webView->evaluateJavascript (script);
}

juce::WebBrowserComponent::Options NovaChordAudioProcessorEditor::createWebOptions (NovaChordAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaChord")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.styleIdxRelay)
                     .withOptionsFrom (editor.octaveShiftRelay)
                     .withOptionsFrom (editor.velocityRelay)
                     .withOptionsFrom (editor.passThroughRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaChordAudioProcessorEditor::getResource (const juce::String& url)
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
        return makeResource (nova_chord_BinaryData::index_html,
                             nova_chord_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerUrl.contains ("n_logo.png"))
        return makeResource (nova_chord_BinaryData::n_logo_png,
                             nova_chord_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_chord_BinaryData::index_js,
                             nova_chord_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerUrl.contains ("js/fallback-boot.js"))
        return makeResource (nova_chord_BinaryData::fallbackboot_js,
                             nova_chord_BinaryData::fallbackboot_jsSize,
                             "text/javascript");

    if (lowerUrl.contains ("js/juce/index.js"))
        return makeResource (nova_chord_BinaryData::index_js2,
                             nova_chord_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerUrl.contains ("js/juce/check_native_interop.js"))
        return makeResource (nova_chord_BinaryData::check_native_interop_js,
                             nova_chord_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_chord_BinaryData::index_html,
                             nova_chord_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerPath == "/n_logo.png" || lowerPath.endsWith ("/n_logo.png"))
        return makeResource (nova_chord_BinaryData::n_logo_png,
                             nova_chord_BinaryData::n_logo_pngSize,
                             "image/png");

    if (lowerPath == "/js/index.js" || lowerPath.endsWith ("/js/index.js"))
        return makeResource (nova_chord_BinaryData::index_js,
                             nova_chord_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerPath == "/js/fallback-boot.js" || lowerPath.endsWith ("/js/fallback-boot.js"))
        return makeResource (nova_chord_BinaryData::fallbackboot_js,
                             nova_chord_BinaryData::fallbackboot_jsSize,
                             "text/javascript");

    if (lowerPath == "/js/juce/index.js" || lowerPath.endsWith ("/js/juce/index.js"))
        return makeResource (nova_chord_BinaryData::index_js2,
                             nova_chord_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerPath == "/js/juce/check_native_interop.js" || lowerPath.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_chord_BinaryData::check_native_interop_js,
                             nova_chord_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    return std::nullopt;
}
