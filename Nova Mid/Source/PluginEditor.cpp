#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>
#include "NovaLicenseGate.h"

using NativeArgs = const juce::Array<juce::var>&;
using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;

static constexpr int kW = 980, kH = 600;

NovaMidAudioProcessorEditor::NovaMidAudioProcessorEditor (NovaMidAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);
    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot()
                      + "index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds()));

    setResizable (true, true);
    setResizeLimits (kW * 6 / 10, kH * 6 / 10, kW * 3 / 2, kH * 3 / 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kW / kH);
    setSize (kW, kH);
    startTimerHz (30);

    NovaLicenseOverlay::attachTo (*this);   // unlock screen until activated
}

NovaMidAudioProcessorEditor::~NovaMidAudioProcessorEditor()
{
    stopTimer();
}

void NovaMidAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (3, 8, 15));
}

void NovaMidAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

static juce::String dbOrNull (float gain)
{
    return gain > 1.0e-6f ? juce::String (juce::Decibels::gainToDecibels (gain), 2) : juce::String ("null");
}

void NovaMidAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    auto& ap = processorRef.apvts;
    const auto val = [&ap] (const char* id) { return juce::String (ap.getRawParameterValue (id)->load(), 2); };

    juce::String js;
    js << "if (window.novaMidUpdate) window.novaMidUpdate({"
       << "\"in\":"      << dbOrNull (processorRef.takeInPeak())
       << ",\"out\":"    << dbOrNull (processorRef.takeOutPeak())
       << ",\"over\":"   << juce::String (processorRef.takeOverDb(), 2)
       << ",\"push\":"   << val ("push")
       << ",\"ceiling\":" << val ("ceiling")
       << ",\"output\":" << val ("output")
       << ",\"bypass\":" << (ap.getRawParameterValue ("bypass")->load() > 0.5f ? "true" : "false")
       << ",\"program\":" << processorRef.currentProgram.load()
       << "});";
    webView->evaluateJavascript (js);
}

juce::var NovaMidAudioProcessorEditor::buildState() const
{
    auto* o = new juce::DynamicObject();

    juce::Array<juce::var> presets;
    for (const auto& p : midPresets())
    {
        auto* po = new juce::DynamicObject();
        po->setProperty ("name", juce::String (p.name));
        po->setProperty ("push", p.push);
        po->setProperty ("ceiling", p.ceiling);
        po->setProperty ("output", p.output);
        presets.add (juce::var (po));
    }
    o->setProperty ("presets", presets);
    o->setProperty ("program", processorRef.currentProgram.load());

    auto& ap = processorRef.apvts;
    o->setProperty ("push", ap.getRawParameterValue ("push")->load());
    o->setProperty ("ceiling", ap.getRawParameterValue ("ceiling")->load());
    o->setProperty ("output", ap.getRawParameterValue ("output")->load());
    o->setProperty ("bypass", ap.getRawParameterValue ("bypass")->load() > 0.5f);

   #if NOVA_MID_REQUIRE_LICENSE
    o->setProperty ("licenseRequired", true);
    o->setProperty ("unlocked", license.isUnlocked());
   #else
    o->setProperty ("licenseRequired", false);
    o->setProperty ("unlocked", true);
   #endif
    return juce::var (o);
}

juce::WebBrowserComponent::Options NovaMidAudioProcessorEditor::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                         .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("NovaMid")));
   #endif

    auto& proc = processorRef;

    options = options.withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })

        .withNativeFunction ("getState", [this] (NativeArgs, Completion done) { done (buildState()); })

        // setParam(id, value, phase): phase is begin | move | end | set (a single click/edit).
        .withNativeFunction ("setParam", [&proc] (NativeArgs a, Completion done)
        {
            if (a.size() >= 2)
                if (auto* prm = proc.apvts.getParameter (a[0].toString()))
                {
                    const auto phase = a.size() > 2 ? a[2].toString() : juce::String ("set");
                    const float norm = prm->convertTo0to1 ((float) (double) a[1]);
                    if (phase == "begin" || phase == "set") prm->beginChangeGesture();
                    if (phase != "end")                     prm->setValueNotifyingHost (norm);
                    if (phase == "end" || phase == "set")   prm->endChangeGesture();
                }
            done ({});
        })

        .withNativeFunction ("setProgram", [this, &proc] (NativeArgs a, Completion done)
        {
            if (a.size() > 0)
                proc.setCurrentProgram ((int) a[0]);
            done (buildState());
        })

        .withNativeFunction ("activateLicense", [this] (NativeArgs a, Completion done)
        {
           #if NOVA_MID_REQUIRE_LICENSE
            const auto key = a.size() > 0 ? a[0].toString().trim() : juce::String();
            if (key.isEmpty()) { done ("empty"); return; }
            license.activate (key, [done] (NovaLicense::ActivationResult r)
            {
                switch (r)
                {
                    case NovaLicense::ActivationResult::Success:          done ("ok"); break;
                    case NovaLicense::ActivationResult::InvalidKey:       done ("invalid"); break;
                    case NovaLicense::ActivationResult::Revoked:          done ("revoked"); break;
                    case NovaLicense::ActivationResult::SeatLimitReached: done ("seats"); break;
                    case NovaLicense::ActivationResult::NetworkError:     done ("network"); break;
                    default:                                              done ("error"); break;
                }
            });
           #else
            juce::ignoreUnused (this, a);
            done ("ok");
           #endif
        });

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaMidAudioProcessorEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes ((size_t) size);
        std::memcpy (bytes.data(), data, (size_t) size);
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    const auto root = juce::WebBrowserComponent::getResourceProviderRoot();
    auto path = url.startsWith (root) ? url.substring (root.length()) : url;
    path = path.upToFirstOccurrenceOf ("?", false, false).toLowerCase();
    while (path.startsWith ("//"))
        path = path.substring (1);
    if (path.isEmpty() || path == "/")
        path = "/index.html";
    if (! path.startsWithChar ('/'))
        path = "/" + path;

    // Order matters: the bridge files also end in index.js.
    if (path.endsWith ("/js/juce/index.js"))
        return makeResource (nova_mid_BinaryData::index_js2, nova_mid_BinaryData::index_js2Size, "text/javascript");
    if (path.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_mid_BinaryData::check_native_interop_js, nova_mid_BinaryData::check_native_interop_jsSize, "text/javascript");
    if (path.endsWith ("/js/index.js"))
        return makeResource (nova_mid_BinaryData::index_js, nova_mid_BinaryData::index_jsSize, "text/javascript");
    if (path.endsWith ("/n_logo.png"))
        return makeResource (nova_mid_BinaryData::n_logo_png, nova_mid_BinaryData::n_logo_pngSize, "image/png");
    if (path.endsWith ("/index.html"))
        return makeResource (nova_mid_BinaryData::index_html, nova_mid_BinaryData::index_htmlSize, "text/html");

    return std::nullopt;
}
