#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>
#include "NovaLicenseGate.h"

NovaAIMasterEditor::NovaAIMasterEditor (NovaAIMasterProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));
    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    setResizeLimits (252, 336, 756, 1008);
    setSize (420, 560);
    startTimerHz (10);

    NovaLicenseOverlay::attachTo (*this);   // unlock screen until activated
}

NovaAIMasterEditor::~NovaAIMasterEditor()
{
    stopTimer();
}

void NovaAIMasterEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (8, 8, 10));
}

void NovaAIMasterEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaAIMasterEditor::timerCallback()
{
    juce::String current;
    switch (processorRef.getButtonState())
    {
        case NovaAIMasterButtonState::Idle:       current = "Idle"; break;
        case NovaAIMasterButtonState::Analyzing:  current = "Analyzing"; break;
        case NovaAIMasterButtonState::Ready:      current = "Ready"; break;
        case NovaAIMasterButtonState::Processing: current = "Processing"; break;
        case NovaAIMasterButtonState::Complete:   current = "Complete"; break;
    }

    if (current != lastPushedButtonState)
    {
        lastPushedButtonState = current;
        if (webView != nullptr)
            webView->evaluateJavascript ("window.updateStatus && window.updateStatus('" + current + "');", nullptr);
    }
}

NovaMasterStyle NovaAIMasterEditor::styleFromName (const juce::String& name) noexcept
{
    if (name == "WARM")   return NovaMasterStyle::Warm;
    if (name == "PUNCHY") return NovaMasterStyle::Punchy;
    if (name == "LOUD")   return NovaMasterStyle::Loud;
    if (name == "OPEN")   return NovaMasterStyle::Open;
    return NovaMasterStyle::Clean;
}

NovaLoudnessMode NovaAIMasterEditor::loudnessFromName (const juce::String& name) noexcept
{
    if (name == "BALANCED") return NovaLoudnessMode::Balanced;
    if (name == "LOUD")     return NovaLoudnessMode::Loud;
    return NovaLoudnessMode::Streaming;
}

juce::WebBrowserComponent::Options NovaAIMasterEditor::createWebOptions (NovaAIMasterEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaAIMaster")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withNativeFunction ("setStyle", [&editor] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             editor.processorRef.apvts.getParameter ("style")->setValueNotifyingHost (
                                 editor.processorRef.apvts.getParameter ("style")->convertTo0to1 ((float) (int) args[0]));
                         completion (juce::var());
                     })
                     .withNativeFunction ("setIntensity", [&editor] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             editor.processorRef.apvts.getParameter ("intensity")->setValueNotifyingHost ((float) (double) args[0]);
                         completion (juce::var());
                     })
                     .withNativeFunction ("setLoudness", [&editor] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             editor.processorRef.apvts.getParameter ("loudness")->setValueNotifyingHost (
                                 editor.processorRef.apvts.getParameter ("loudness")->convertTo0to1 ((float) (int) args[0]));
                         completion (juce::var());
                     })
                     .withNativeFunction ("beginAnalyze", [&editor] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         editor.processorRef.beginAnalyze();
                         completion (juce::var());
                     })
                     .withNativeFunction ("beginMaster", [&editor] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         NovaMasterUserPrefs prefs;
                         if (args.size() >= 3)
                         {
                             prefs.style     = styleFromName (args[0].toString());
                             prefs.intensity = (float) (double) args[1];
                             prefs.loudness  = loudnessFromName (args[2].toString());
                         }
                         editor.processorRef.beginMaster (prefs);
                         completion (juce::var());
                     })
                     .withNativeFunction ("togglePreview", [&editor] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         auto* bypassParam = editor.processorRef.apvts.getParameter ("bypass");
                         const bool newState = bypassParam->getValue() < 0.5f;
                         bypassParam->setValueNotifyingHost (newState ? 1.0f : 0.0f);
                         completion (juce::var (newState));
                     });

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaAIMasterEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const void* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));

        return juce::WebBrowserComponent::Resource {
            std::move (bytes),
            juce::String (mime)
        };
    };

    // Match by substring on the raw url (proven pattern used by every other
    // Nova WebView plugin, e.g. Nova Chord/Nova Sub's getResource()) rather
    // than stripping getResourceProviderRoot() - that prefix never actually
    // matches what WKWebView hands back here, so the stripped-path approach
    // silently fell through to index.html for every request, including JS
    // files, which meant no script ever actually loaded.
    const auto lowerUrl = url.toLowerCase();

    if (lowerUrl.contains ("juce/index.js"))
        return makeResource (nova_ai_master_BinaryData::index_js2, nova_ai_master_BinaryData::index_js2Size, "text/javascript");

    if (lowerUrl.contains ("juce/check_native_interop.js"))
        return makeResource (nova_ai_master_BinaryData::check_native_interop_js, nova_ai_master_BinaryData::check_native_interop_jsSize, "text/javascript");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_ai_master_BinaryData::index_js, nova_ai_master_BinaryData::index_jsSize, "text/javascript");

    if (lowerUrl.contains ("index.html") || lowerUrl.endsWith ("/") || lowerUrl.isEmpty())
        return makeResource (nova_ai_master_BinaryData::index_html, nova_ai_master_BinaryData::index_htmlSize, "text/html");

    return std::nullopt;
}
