#include "PluginEditor.h"
#include "BinaryData.h"
#include "NovaLicenseGate.h"

#include <cstring>
#include <iostream>

namespace
{
    const char* const kParamIds[] = { "cut", "boost", "link", "speed", "from", "to", "output", "mode", "stereo",
                                      "punch", "peak", "freeze", "sections", "power", "listen" };

    juce::String bandsJson (const NovaMirror::BandArray& a)
    {
        juce::String s ("[");
        for (int b = 0; b < NovaMirror::kBands; ++b)
        {
            if (b) s << ",";
            s << juce::String (std::max (-200.f, a[(size_t) b]), 1);
        }
        return s + "]";
    }
}

NovaMirrorEditor::NovaMirrorEditor (NovaMirrorProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);
    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot() + "index.html?v="
                      + juce::String (juce::Time::getCurrentTime().toMilliseconds()));

    // Fixed shape: the page scales to fit, so nothing is ever cut off
    setResizable (true, true);
    setResizeLimits (770, 490, 2200, 1400);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (1100.0 / 700.0);
    setSize (1100, 700);
    startTimerHz (30);

    NovaLicenseOverlay::attachTo (*this);
}

NovaMirrorEditor::~NovaMirrorEditor() { stopTimer(); }

void NovaMirrorEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xFF0C0D10)); }

void NovaMirrorEditor::resized()
{
    if (webView != nullptr) webView->setBounds (getLocalBounds());
}

void NovaMirrorEditor::timerCallback()
{
    if (webView == nullptr || ! pageReady) return;

    if (sentInfo != proc.getInfoVersion())
    {
        sentInfo = proc.getInfoVersion();
        webView->evaluateJavascript ("window.NM && NM.onInfo(" + proc.getInfoJson() + ")");
    }

    juce::String params ("{");
    bool first = true;
    for (auto* id : kParamIds)
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id)))
        {
            params << (first ? "" : ",") << "\"" << id << "\":" << juce::String (prm->convertFrom0to1 (prm->getValue()), 4);
            first = false;
        }
    params << "}";
    if (params != sentParams)
    {
        sentParams = params;
        webView->evaluateJavascript ("window.NM && NM.onParams(" + params + ")");
    }

    NovaMirror::EngineMeters m;
    proc.getMeters (m);
    juce::String s;
    s << "window.NM && NM.onFrame({in:" << bandsJson (m.inDb) << ",tgt:" << bandsJson (m.targetDb)
      << ",gain:" << bandsJson (m.gainDb) << ",match:" << juce::String (m.match, 1)
      << ",section:" << juce::JSON::toString (juce::String (NovaMirror::sectionName (m.section)))
      << ",active:" << (m.active ? "true" : "false")
      << ",listenPos:" << juce::String (proc.getListenPositionSeconds(), 2)
      << ",outPk:" << juce::String (proc.getOutputPeakDb(), 1) << "})";
    webView->evaluateJavascript (s);
}

juce::WebBrowserComponent::Options NovaMirrorEditor::createWebOptions()
{
    using Fn = juce::WebBrowserComponent::NativeFunctionCompletion;
    using Args = juce::Array<juce::var>;
    auto options = juce::WebBrowserComponent::Options {};
   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                         .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("NovaMirror")));
   #endif
    return options.withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
        .withNativeFunction ("ready", [this] (const Args&, Fn done)
        {
            std::cerr << "[NovaMirror] page ready" << std::endl;
            pageReady = true; sentInfo = -1; sentParams = {};
            done ({});
        })
        .withNativeFunction ("setParam", [this] (const Args& a, Fn done)
        {
            if (a.size() >= 2)
                if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (a[0].toString())))
                    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (double) a[1]));
            done ({});
        })
        .withNativeFunction ("gesture", [this] (const Args& a, Fn done)
        {
            if (a.size() >= 2)
                if (auto* prm = proc.apvts.getParameter (a[0].toString()))
                    (bool) a[1] ? prm->beginChangeGesture() : prm->endChangeGesture();
            done ({});
        })
        .withNativeFunction ("chooseReference", [this] (const Args&, Fn done)
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose a reference track", juce::File(),
                                                           "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.aac;*.caf");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto f = fc.getResult();
                                      if (f.existsAsFile()) proc.loadReferenceFile (f);
                                  });
            done ({});
        })
        .withNativeFunction ("dropChunk", [this] (const Args& a, Fn done)
        {
            // name, base64 chunk, index, total
            if (a.size() >= 4)
            {
                if ((int) a[2] == 0) { dropData.reset(); dropName = a[0].toString(); }
                juce::MemoryOutputStream mo (dropData, true);
                juce::Base64::convertFromBase64 (mo, a[1].toString());
                mo.flush();
                if ((int) a[2] == (int) a[3] - 1) { proc.loadReferenceBytes (dropName, dropData); dropData.reset(); }
            }
            done ({});
        })
        .withNativeFunction ("clearReference", [this] (const Args&, Fn done) { proc.clearReference(); done ({}); })
        .withNativeFunction ("setTarget", [this] (const Args& a, Fn done)
        {
            if (a.size() >= 2) proc.setTarget (a[0].toString(), a[1].toString());
            done ({});
        })
        .withNativeFunction ("saveTarget", [this] (const Args& a, Fn done)
        {
            done (a.size() >= 1 && proc.saveCurrentAsUserTarget (a[0].toString()));
        })
        .withNativeFunction ("deleteTarget", [this] (const Args& a, Fn done)
        {
            if (a.size() >= 1) proc.deleteUserTarget (a[0].toString());
            done ({});
        })
        .withNativeFunction ("ab", [this] (const Args& a, Fn done)
        {
            if (a.size() >= 1) proc.selectAB ((int) a[0]);
            done ({});
        })
        .withNativeFunction ("jsError", [] (const Args& a, Fn done)
        {
            if (! a.isEmpty()) std::cerr << "[NovaMirror][JS] " << a[0].toString() << std::endl;
            done ({});
        });
}

std::optional<juce::WebBrowserComponent::Resource> NovaMirrorEditor::getResource (const juce::String& url)
{
    auto make = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes ((size_t) size);
        std::memcpy (bytes.data(), data, (size_t) size);
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };
    // JUCE may hand us the full URL or just the path
    const auto root = juce::WebBrowserComponent::getResourceProviderRoot();
    auto path = (url.startsWith (root) ? url.substring (root.length()) : url).upToFirstOccurrenceOf ("?", false, false).toLowerCase();
    if (! path.startsWithChar ('/')) path = "/" + path;
    if (path == "/") path = "/index.html";

    if (path.endsWith ("/js/juce/index.js"))
        return make (nova_mirror_BinaryData::index_js2, nova_mirror_BinaryData::index_js2Size, "text/javascript");
    if (path.endsWith ("/js/juce/check_native_interop.js"))
        return make (nova_mirror_BinaryData::check_native_interop_js, nova_mirror_BinaryData::check_native_interop_jsSize, "text/javascript");
    if (path.endsWith ("/js/index.js"))
        return make (nova_mirror_BinaryData::index_js, nova_mirror_BinaryData::index_jsSize, "text/javascript");
    if (path.endsWith ("/index.html"))
        return make (nova_mirror_BinaryData::index_html, nova_mirror_BinaryData::index_htmlSize, "text/html");
    return std::nullopt;
}
