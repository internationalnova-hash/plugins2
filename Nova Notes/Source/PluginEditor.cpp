#include "PluginEditor.h"
#include "BinaryData.h"
#include "NovaLicenseGate.h"

#include <cstring>
#include <iostream>

namespace
{
    std::vector<int> idsFrom (const juce::var& v)
    {
        std::vector<int> ids;
        if (auto* a = v.getArray())
            for (auto& x : *a) ids.push_back ((int) x);
        return ids;
    }
}

NovaNotesEditor::NovaNotesEditor (NovaNotesProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);
    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot() + "/index.html?v="
                      + juce::String (juce::Time::getCurrentTime().toMilliseconds()));

    // Fixed shape: the page scales to fit, so nothing is ever cut off
    setResizable (true, true);
    setResizeLimits (896, 574, 2560, 1640);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (1280.0 / 820.0);
    setSize (1200, 769);
    startTimerHz (30);

    NovaLicenseOverlay::attachTo (*this);   // unlock screen until activated
}

NovaNotesEditor::~NovaNotesEditor() { stopTimer(); }

void NovaNotesEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xFF0A0A10)); }

void NovaNotesEditor::resized()
{
    if (webView != nullptr) webView->setBounds (getLocalBounds());
}

void NovaNotesEditor::timerCallback()
{
    if (webView == nullptr || ! pageReady) return;

    if (sentAnalysis != proc.getAnalysisVersion())
    {
        sentAnalysis = proc.getAnalysisVersion();
        sentNotes = -1;
        webView->evaluateJavascript ("window.NN && NN.onTrack(" + proc.getTrackJson() + "," + proc.getInfoJson() + ")");
    }
    if (sentNotes != proc.getNotesVersion())
    {
        sentNotes = proc.getNotesVersion();
        webView->evaluateJavascript ("window.NN && NN.onNotes(" + proc.getNotesJson() + "," + proc.getInfoJson() + ")");
    }

    juce::String s;
    s << "window.NN && NN.onStatus({cap:" << proc.getCaptureState()
      << ",capSec:" << juce::String (proc.getCapturedSeconds(), 2)
      << ",play:" << (proc.isHostPlaying() ? "true" : "false")
      << ",ph:" << juce::String (proc.getTakePlayheadSeconds(), 4)
      << ",busy:" << (proc.isBusy() ? "true" : "false")
      << ",analysing:" << (proc.isAnalysing() ? "true" : "false")
      << ",undo:" << (proc.canUndo() ? "true" : "false")
      << ",redo:" << (proc.canRedo() ? "true" : "false")
      << ",orig:" << (proc.getShowOriginal() ? "true" : "false")
      << ",msg:" << juce::JSON::toString (proc.getStatusMessage())
      << "})";
    webView->evaluateJavascript (s);
}

juce::WebBrowserComponent::Options NovaNotesEditor::createWebOptions()
{
    using Fn = juce::WebBrowserComponent::NativeFunctionCompletion;
    using Args = juce::Array<juce::var>;
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                         .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("NovaNotes")));
   #endif

    return options.withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
        .withNativeFunction ("ready", [this] (const Args&, Fn done)
        {
            std::cerr << "[NovaNotes] page ready" << std::endl;
            pageReady = true;
            sentAnalysis = sentNotes = -1;
            done ({});
        })
        .withNativeFunction ("patch", [this] (const Args& a, Fn done)
        {
            if (a.size() >= 1) proc.applyPatches (a[0], a.size() >= 2 && (bool) a[1]);
            done ({});
        })
        .withNativeFunction ("split", [this] (const Args& a, Fn done)
        {
            if (a.size() >= 2) proc.splitNote ((int) a[0], (int) a[1]);
            done ({});
        })
        .withNativeFunction ("correct", [this] (const Args& a, Fn done)
        {
            if (a.size() >= 4) proc.correctPitch (idsFrom (a[0]), (float) (double) a[1], (float) (double) a[2], (int) a[3]);
            done ({});
        })
        .withNativeFunction ("assist", [this] (const Args& a, Fn done)
        {
            done (a.size() >= 2 ? proc.runAssist ((int) a[0], idsFrom (a[1])) : juce::String());
        })
        .withNativeFunction ("quantize", [this] (const Args& a, Fn done)
        {
            if (a.size() >= 2) proc.quantizeTime (idsFrom (a[0]), (float) (double) a[1]);
            done ({});
        })
        .withNativeFunction ("reset", [this] (const Args& a, Fn done)
        {
            proc.resetNotes (a.size() >= 1 ? idsFrom (a[0]) : std::vector<int> {});
            done ({});
        })
        .withNativeFunction ("undo", [this] (const Args&, Fn done) { proc.undo(); done ({}); })
        .withNativeFunction ("redo", [this] (const Args&, Fn done) { proc.redo(); done ({}); })
        .withNativeFunction ("capture", [this] (const Args& a, Fn done)
        {
            const auto cmd = a.isEmpty() ? juce::String() : a[0].toString();
            std::cerr << "[NovaNotes] capture " << cmd << std::endl;
            if (cmd == "arm") proc.armCapture();
            else if (cmd == "stop") proc.stopCapture();
            else if (cmd == "clear") proc.clearTake();
            done ({});
        })
        .withNativeFunction ("setCompare", [this] (const Args& a, Fn done)
        {
            proc.setShowOriginal (! a.isEmpty() && (bool) a[0]);
            done ({});
        })
        .withNativeFunction ("setKey", [this] (const Args& a, Fn done)
        {
            if (a.size() >= 2) proc.setKey ((int) a[0], (bool) a[1]);
            done ({});
        })
        .withNativeFunction ("jsError", [] (const Args& a, Fn done)
        {
            if (! a.isEmpty()) std::cerr << "[NovaNotes][JS] " << a[0].toString() << std::endl;
            done ({});
        });
}

std::optional<juce::WebBrowserComponent::Resource> NovaNotesEditor::getResource (const juce::String& url)
{
    auto make = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes ((size_t) size);
        std::memcpy (bytes.data(), data, (size_t) size);
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    // JUCE may hand us the full URL or just the path
    const auto root = juce::WebBrowserComponent::getResourceProviderRoot();
    auto path = (url.startsWith (root) ? url.substring (root.length()) : url)
                    .upToFirstOccurrenceOf ("?", false, false).toLowerCase();
    if (! path.startsWithChar ('/')) path = "/" + path;   // the root may or may not end in '/'
    if (path == "/") path = "/index.html";

    if (path.endsWith ("/js/juce/index.js"))
        return make (nova_notes_BinaryData::index_js2, nova_notes_BinaryData::index_js2Size, "text/javascript");
    if (path.endsWith ("/js/juce/check_native_interop.js"))
        return make (nova_notes_BinaryData::check_native_interop_js, nova_notes_BinaryData::check_native_interop_jsSize, "text/javascript");
    if (path.endsWith ("/js/index.js"))
        return make (nova_notes_BinaryData::index_js, nova_notes_BinaryData::index_jsSize, "text/javascript");
    if (path.endsWith ("/index.html"))
        return make (nova_notes_BinaryData::index_html, nova_notes_BinaryData::index_htmlSize, "text/html");
    return std::nullopt;
}
