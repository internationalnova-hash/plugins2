#include "MainWindow.h"
#include "BinaryData.h"

#include <cstring>

// ── NovaAIMasterMainComponent ────────────────────────────────────────────

NovaAIMasterMainComponent::NovaAIMasterMainComponent()
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setSize (1080, 860);
    startTimerHz (10);
}

NovaAIMasterMainComponent::~NovaAIMasterMainComponent()
{
    stopTimer();
}

void NovaAIMasterMainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF08080A));
}

void NovaAIMasterMainComponent::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

juce::var NovaAIMasterMainComponent::waveformToVar (const std::vector<float>& peaks) const
{
    juce::Array<juce::var> arr;
    arr.ensureStorageAllocated ((int) peaks.size());
    for (float p : peaks) arr.add (juce::var (p));
    return juce::var (arr);
}

juce::var NovaAIMasterMainComponent::buildStatusVar()
{
    auto* obj = new juce::DynamicObject();

    obj->setProperty ("busy", audioEngine.isBusy());
    obj->setProperty ("hasImport", audioEngine.hasImportedAudio());
    obj->setProperty ("hasMaster", audioEngine.hasMasteredAudio());
    obj->setProperty ("isPlaying", audioEngine.isPlaying());
    obj->setProperty ("fileName", audioEngine.getImportedFileName());
    obj->setProperty ("sampleRate", audioEngine.getImportedSampleRate());
    obj->setProperty ("bitDepth", audioEngine.getImportedBitDepth());
    obj->setProperty ("stereo", audioEngine.getImportedIsStereo());
    obj->setProperty ("lengthSeconds", audioEngine.getImportedLengthSeconds());

    if (audioEngine.hasMasteredAudio())
    {
        const auto& p = audioEngine.getLastProfile();
        obj->setProperty ("integratedLufs", p.integratedLufs);
        obj->setProperty ("truePeakDb", p.truePeakDb);
    }

    return juce::var (obj);
}

void NovaAIMasterMainComponent::timerCallback()
{
    const auto statusVar = buildStatusVar();
    const auto json = juce::JSON::toString (statusVar, true);

    if (json != lastPushedStatusJson)
    {
        lastPushedStatusJson = json;
        if (webView != nullptr)
            webView->evaluateJavascript ("window.updateStatus && window.updateStatus(" + json + ");", nullptr);
    }
}

juce::WebBrowserComponent::Options NovaAIMasterMainComponent::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaAIMasterStandalone")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
                     .withNativeFunction ("importMix", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         activeFileChooser = std::make_unique<juce::FileChooser> (
                             "Select a stereo mix to import", juce::File(), "*.wav;*.aiff;*.aif");
                         const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
                         activeFileChooser->launchAsync (flags, [this, completion] (const juce::FileChooser& fc) mutable
                         {
                             const auto file = fc.getResult();
                             if (file != juce::File())
                                 audioEngine.importFile (file);
                             completion (juce::var());
                         });
                     })
                     .withNativeFunction ("beginMaster", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (args.size() >= 3)
                         {
                             const auto styleName = args[0].toString();
                             if (styleName == "WARM")        prefs.style = NovaMasterStyle::Warm;
                             else if (styleName == "PUNCHY") prefs.style = NovaMasterStyle::Punchy;
                             else if (styleName == "LOUD")   prefs.style = NovaMasterStyle::Loud;
                             else if (styleName == "OPEN")   prefs.style = NovaMasterStyle::Open;
                             else                              prefs.style = NovaMasterStyle::Clean;

                             prefs.intensity = (float) (double) args[1];

                             const auto loudName = args[2].toString();
                             if (loudName == "BALANCED")     prefs.loudness = NovaLoudnessMode::Balanced;
                             else if (loudName == "LOUD")    prefs.loudness = NovaLoudnessMode::Loud;
                             else                              prefs.loudness = NovaLoudnessMode::Streaming;
                         }
                         audioEngine.requestMaster (prefs);
                         completion (juce::var());
                     })
                     .withNativeFunction ("previewOriginal", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         audioEngine.playOriginal();
                         completion (juce::var());
                     })
                     .withNativeFunction ("previewMaster", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         audioEngine.playMastered();
                         completion (juce::var());
                     })
                     .withNativeFunction ("stopPreview", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         audioEngine.stopPlayback();
                         completion (juce::var());
                     })
                     .withNativeFunction ("exportMix", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         const juce::String formatName = args.isEmpty() ? "wav24" : args[0].toString();
                         auto format = NovaAIMasterAudioEngine::ExportFormat::Wav24;
                         juce::String ext = ".wav";
                         if (formatName == "wav16") format = NovaAIMasterAudioEngine::ExportFormat::Wav16;
                         else if (formatName == "aiff24") { format = NovaAIMasterAudioEngine::ExportFormat::Aiff24; ext = ".aiff"; }

                         activeFileChooser = std::make_unique<juce::FileChooser> (
                             "Export mastered mix",
                             juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("NovaAIMaster-Master" + ext),
                             "*" + ext);

                         activeFileChooser->launchAsync (juce::FileBrowserComponent::saveMode, [this, format, completion] (const juce::FileChooser& fc) mutable
                         {
                             const auto file = fc.getResult();
                             if (file == juce::File()) { completion (juce::var (false)); return; }

                             juce::String error;
                             const bool ok = audioEngine.exportMastered (file, format, error);
                             completion (juce::var (ok));
                         });
                     })
                     .withNativeFunction ("getWaveforms", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         const int numPoints = args.isEmpty() ? 120 : (int) args[0];
                         auto* obj = new juce::DynamicObject();
                         obj->setProperty ("original", waveformToVar (audioEngine.getWaveformPeaks (false, numPoints)));
                         obj->setProperty ("mastered", waveformToVar (audioEngine.getWaveformPeaks (true, numPoints)));
                         completion (juce::var (obj));
                     });

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaAIMasterMainComponent::getResource (const juce::String& url)
{
    auto makeResource = [] (const void* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    // Match by substring on the raw url (proven pattern used by every other
    // Nova WebView plugin, e.g. Nova Chord/Nova Sub's getResource()) rather
    // than stripping getResourceProviderRoot() - that prefix never actually
    // matches what WKWebView hands back here, so the stripped-path approach
    // silently fell through to index.html for every request, including JS
    // files, which meant no script ever actually loaded.
    const auto lowerUrl = url.toLowerCase();

    if (lowerUrl.contains ("juce/index.js"))
        return makeResource (nova_ai_master_standalone_BinaryData::index_js2, nova_ai_master_standalone_BinaryData::index_js2Size, "text/javascript");

    if (lowerUrl.contains ("juce/check_native_interop.js"))
        return makeResource (nova_ai_master_standalone_BinaryData::check_native_interop_js, nova_ai_master_standalone_BinaryData::check_native_interop_jsSize, "text/javascript");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_ai_master_standalone_BinaryData::index_js, nova_ai_master_standalone_BinaryData::index_jsSize, "text/javascript");

    if (lowerUrl.contains ("index.html") || lowerUrl.endsWith ("/") || lowerUrl.isEmpty())
        return makeResource (nova_ai_master_standalone_BinaryData::index_html, nova_ai_master_standalone_BinaryData::index_htmlSize, "text/html");

    return std::nullopt;
}

// ── NovaAIMasterMainWindow ───────────────────────────────────────────────

NovaAIMasterMainWindow::NovaAIMasterMainWindow (juce::String name)
    : juce::DocumentWindow (std::move (name), juce::Colours::black, DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setContentOwned (new NovaAIMasterMainComponent(), true);
    centreWithSize (getWidth(), getHeight());
    setResizable (true, true);
    setVisible (true);
}
