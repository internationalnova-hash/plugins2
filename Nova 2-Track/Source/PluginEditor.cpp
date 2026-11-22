#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

using NativeArgs = const juce::Array<juce::var>&;
using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;

static constexpr int kW = 1040, kH = 640;

NovaTwoTrackAudioProcessorEditor::NovaTwoTrackAudioProcessorEditor (NovaTwoTrackAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    smoothM.fill (-100.0f);
    smoothS.fill (-100.0f);
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
}

NovaTwoTrackAudioProcessorEditor::~NovaTwoTrackAudioProcessorEditor()
{
    stopTimer();
}

void NovaTwoTrackAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (3, 8, 15));
}

void NovaTwoTrackAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

static juce::String dbOrNull (float gain)
{
    return gain > 1.0e-6f ? juce::String (juce::Decibels::gainToDecibels (gain), 2) : juce::String ("null");
}

juce::String NovaTwoTrackAudioProcessorEditor::spectrumJson()
{
    auto& p = processorRef;
    const int w = p.fftWrite.load (std::memory_order_acquire);
    for (int i = 0; i < kFftSize; ++i)
    {
        const int idx = (w - kFftSize + i + NovaTwoTrackAudioProcessor::kFftRing) % NovaTwoTrackAudioProcessor::kFftRing;
        const float l = p.fftL[(size_t) idx].load (std::memory_order_relaxed);
        const float r = p.fftR[(size_t) idx].load (std::memory_order_relaxed);
        fftM[(size_t) i] = 0.5f * (l + r);
        fftS[(size_t) i] = 0.5f * (l - r);
    }
    window.multiplyWithWindowingTable (fftM.data(), (size_t) kFftSize);
    window.multiplyWithWindowingTable (fftS.data(), (size_t) kFftSize);
    fft.performFrequencyOnlyForwardTransform (fftM.data(), true);
    fft.performFrequencyOnlyForwardTransform (fftS.data(), true);

    const double sr = p.getSampleRate() > 0 ? p.getSampleRate() : 48000.0;
    const float norm = 4.0f / (float) kFftSize; // hann gain + one-sided
    auto band = [&] (const std::vector<float>& mag, double f0, double f1)
    {
        const int b0 = juce::jlimit (1, kFftSize / 2 - 1, (int) std::floor (f0 / sr * kFftSize));
        const int b1 = juce::jlimit (b0, kFftSize / 2 - 1, (int) std::ceil (f1 / sr * kFftSize));
        float m = 0.0f;
        for (int b = b0; b <= b1; ++b) m = juce::jmax (m, mag[(size_t) b]);
        return juce::Decibels::gainToDecibels (m * norm, -100.0f);
    };

    juce::String jm, js;
    const double lo = std::log10 (20.0), hi = std::log10 (20000.0);
    for (int i = 0; i < kBins; ++i)
    {
        const double f0 = std::pow (10.0, lo + (hi - lo) * i / kBins), f1 = std::pow (10.0, lo + (hi - lo) * (i + 1) / kBins);
        const float dm = band (fftM, f0, f1), ds = band (fftS, f0, f1);
        // fast rise, ~20 dB/s fall at 30 Hz
        smoothM[(size_t) i] = dm > smoothM[(size_t) i] ? dm : juce::jmax (dm, smoothM[(size_t) i] - 0.7f);
        smoothS[(size_t) i] = ds > smoothS[(size_t) i] ? ds : juce::jmax (ds, smoothS[(size_t) i] - 0.7f);
        jm << (i ? "," : "") << juce::String (smoothM[(size_t) i], 1);
        js << (i ? "," : "") << juce::String (smoothS[(size_t) i], 1);
    }
    return ",\"specM\":[" + jm + "],\"specS\":[" + js + "]";
}

void NovaTwoTrackAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    auto& p = processorRef;
    auto& ap = p.apvts;

    juce::String params;
    for (const auto& id : twoTrackParamIds())
        params << (params.isEmpty() ? "" : ",") << "\"" << id << "\":" << juce::String (ap.getRawParameterValue (id)->load(), 2);

    // Last 256 scope points (L, R interleaved, 3 decimals).
    juce::String scope;
    const int w = p.scopeWrite.load (std::memory_order_acquire);
    for (int i = 0; i < 256; ++i)
    {
        const int idx = (w - 256 + i + NovaTwoTrackAudioProcessor::kScopeSize) % NovaTwoTrackAudioProcessor::kScopeSize;
        scope << (i ? "," : "") << juce::String (p.scopeL[(size_t) idx].load (std::memory_order_relaxed), 3)
              << "," << juce::String (p.scopeR[(size_t) idx].load (std::memory_order_relaxed), 3);
    }

    juce::String js;
    js << "if (window.novaTwoTrackUpdate) window.novaTwoTrackUpdate({"
       << "\"inL\":"  << dbOrNull (NovaTwoTrackAudioProcessor::take (p.inL))
       << ",\"inR\":" << dbOrNull (NovaTwoTrackAudioProcessor::take (p.inR))
       << ",\"outL\":" << dbOrNull (NovaTwoTrackAudioProcessor::take (p.outL))
       << ",\"outR\":" << dbOrNull (NovaTwoTrackAudioProcessor::take (p.outR))
       << ",\"over\":" << juce::String (NovaTwoTrackAudioProcessor::take (p.overDb), 2)
       << ",\"corr\":" << juce::String (p.correlation.load(), 3)
       << ",\"punchDb\":" << juce::String (NovaTwoTrackAudioProcessor::take (p.punchDb), 2)
       << ",\"energyDb\":" << juce::String (NovaTwoTrackAudioProcessor::take (p.energyDb), 2)
       << spectrumJson()
       << ",\"bypass\":" << (ap.getRawParameterValue ("bypass")->load() > 0.5f ? "true" : "false")
       << ",\"program\":" << p.currentProgram.load()
       << ",\"params\":{" << params << "}"
       << ",\"scope\":[" << scope << "]"
       << "});";
    webView->evaluateJavascript (js);
}

juce::var NovaTwoTrackAudioProcessorEditor::buildState() const
{
    auto* o = new juce::DynamicObject();
    auto& ap = processorRef.apvts;

    auto paramsOf = [] (const tt::Settings& s)
    {
        auto* po = new juce::DynamicObject();
        po->setProperty ("input", s.inputDb); po->setProperty ("width", s.width); po->setProperty ("vox_carve", s.voxCarve);
        po->setProperty ("punch", s.punch); po->setProperty ("low_end", s.lowEnd); po->setProperty ("instruments", s.instruments);
        po->setProperty ("energy", s.energy); po->setProperty ("clip", s.clipOn ? 1.0f : 0.0f);
        po->setProperty ("ceiling", s.ceilingDb); po->setProperty ("output", s.outputDb);
        return juce::var (po);
    };

    juce::Array<juce::var> presets;
    for (const auto& p : twoTrackPresets())
    {
        auto* po = new juce::DynamicObject();
        po->setProperty ("name", juce::String (p.name));
        po->setProperty ("params", paramsOf (p.s));
        presets.add (juce::var (po));
    }
    o->setProperty ("presets", presets);
    o->setProperty ("program", processorRef.currentProgram.load());

    auto* cur = new juce::DynamicObject();
    for (const auto& id : twoTrackParamIds())
        cur->setProperty (id, ap.getRawParameterValue (id)->load());
    o->setProperty ("params", juce::var (cur));
    o->setProperty ("bypass", ap.getRawParameterValue ("bypass")->load() > 0.5f);

   #if NOVA_2TRACK_REQUIRE_LICENSE
    o->setProperty ("licenseRequired", true);
    o->setProperty ("unlocked", license.isUnlocked());
   #else
    o->setProperty ("licenseRequired", false);
    o->setProperty ("unlocked", true);
   #endif
    return juce::var (o);
}

juce::WebBrowserComponent::Options NovaTwoTrackAudioProcessorEditor::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                         .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Nova2Track")));
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
           #if NOVA_2TRACK_REQUIRE_LICENSE
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

std::optional<juce::WebBrowserComponent::Resource> NovaTwoTrackAudioProcessorEditor::getResource (const juce::String& url)
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
        return makeResource (nova_2track_BinaryData::index_js2, nova_2track_BinaryData::index_js2Size, "text/javascript");
    if (path.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_2track_BinaryData::check_native_interop_js, nova_2track_BinaryData::check_native_interop_jsSize, "text/javascript");
    if (path.endsWith ("/js/index.js"))
        return makeResource (nova_2track_BinaryData::index_js, nova_2track_BinaryData::index_jsSize, "text/javascript");
    if (path.endsWith ("/n_logo.png"))
        return makeResource (nova_2track_BinaryData::n_logo_png, nova_2track_BinaryData::n_logo_pngSize, "image/png");
    if (path.endsWith ("/index.html"))
        return makeResource (nova_2track_BinaryData::index_html, nova_2track_BinaryData::index_htmlSize, "text/html");

    return std::nullopt;
}
