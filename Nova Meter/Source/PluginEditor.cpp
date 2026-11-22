#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

using NativeArgs = const juce::Array<juce::var>&;
using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;

NovaMeterAudioProcessorEditor::NovaMeterAudioProcessorEditor (NovaMeterAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    seenStateGeneration = processorRef.stateGeneration.load();
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    setResizeLimits (588, 372, 1470, 930);
    setSize (980, 620);
    startTimerHz (30);
}

NovaMeterAudioProcessorEditor::~NovaMeterAudioProcessorEditor()
{
    stopTimer();
    abortUpload();
}

void NovaMeterAudioProcessorEditor::abortUpload()
{
    uploadStream.reset();
    if (uploadFile != juce::File())
        uploadFile.deleteFile();
    uploadFile = juce::File();
    uploadBytes = 0;
}

void NovaMeterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (6, 11, 18));
}

void NovaMeterAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

//==============================================================================
static juce::String num (float v)
{
    return tz::hasValue (v) ? juce::String (v, 2) : juce::String ("null");
}

juce::String NovaMeterAudioProcessorEditor::buildMeterJson()
{
    auto& eng = processorRef.engine();
    juce::String cur, hmax, hmin;

    for (int i = 0; i < tz::NumMeters; ++i)
    {
        const auto id = (tz::MeterId) i;
        const float v = (id == tz::Peak || id == tz::TruePeak) ? eng.takePeakSinceLastRead (id) : eng.current (id);
        const juce::String sep = i ? "," : "";
        cur  << sep << num (v);
        hmax << sep << num (eng.heldMax (id));
        hmin << sep << num (eng.heldMin (id));
    }

    return "{\"cur\":[" + cur + "],\"max\":[" + hmax + "],\"min\":[" + hmin + "]}";
}

void NovaMeterAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    juce::String js = "if (window.novaMeterUpdate) { window.novaMeterUpdate(" + buildMeterJson() + "); }";

    // Reference analysis status, and a full state refresh when a result lands.
    auto& ra = processorRef.referenceAnalyzer;
    const bool landed = processorRef.collectReference();
    juce::String refJs;
    const auto status = ra.getStatus();
    if (status == tz::ReferenceAnalyzer::Status::Analyzing)
        refJs = "{\"state\":\"analyzing\",\"progress\":" + juce::String (ra.getProgress(), 3) + "}";
    else if (status == tz::ReferenceAnalyzer::Status::Failed)
        refJs = "{\"state\":\"error\",\"error\":" + juce::JSON::toString (ra.getError()) + "}";
    else
        refJs = "{\"state\":\"idle\"}";
    if (refJs != lastRefStatus || ra.getStatus() == tz::ReferenceAnalyzer::Status::Analyzing)
    {
        lastRefStatus = refJs;
        js << "if (window.novaMeterRefStatus) { window.novaMeterRefStatus(" << refJs << "); }";
    }

    const int gen = processorRef.stateGeneration.load();
    if (landed)
        ++processorRef.stateGeneration;
    if (gen != seenStateGeneration || landed)
    {
        seenStateGeneration = processorRef.stateGeneration.load();
        js << "if (window.novaMeterStateChanged) { window.novaMeterStateChanged(); }";
    }

    webView->evaluateJavascript (js);
}

//==============================================================================
juce::var NovaMeterAudioProcessorEditor::buildState() const
{
    auto* o = new juce::DynamicObject();

    juce::Array<juce::var> specs;
    for (const auto& s : tz::meterSpecs())
    {
        auto* so = new juce::DynamicObject();
        so->setProperty ("key", s.key);
        so->setProperty ("name", s.name);
        so->setProperty ("unit", s.unit);
        so->setProperty ("scaleMin", s.scaleMin);
        so->setProperty ("scaleMax", s.scaleMax);
        so->setProperty ("defMin", s.defMin);
        so->setProperty ("defMax", s.defMax);
        so->setProperty ("help", s.help);
        specs.add (juce::var (so));
    }
    o->setProperty ("specs", specs);

    juce::Array<juce::var> profiles;
    for (const auto& p : processorRef.profiles.all())
    {
        auto* po = new juce::DynamicObject();
        po->setProperty ("name", p.name);
        po->setProperty ("factory", p.factory);
        profiles.add (juce::var (po));
    }
    o->setProperty ("profiles", profiles);
    o->setProperty ("active", processorRef.activeProfile);
    o->setProperty ("modified", processorRef.isModified());
    o->setProperty ("view", processorRef.viewName);
    o->setProperty ("working", processorRef.working.toVar());
    o->setProperty ("reference", processorRef.reference.valid() ? processorRef.reference.toVar() : juce::var());

   #if NOVA_METER_REQUIRE_LICENSE
    o->setProperty ("licenseRequired", true);
    o->setProperty ("unlocked", license.isUnlocked());
   #else
    o->setProperty ("licenseRequired", false);
    o->setProperty ("unlocked", true);
   #endif

    return juce::var (o);
}

juce::WebBrowserComponent::Options NovaMeterAudioProcessorEditor::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaMeter")));
   #endif

    auto& proc = processorRef;
    auto validIndex = [] (const juce::var& v) { const int i = (int) v; return i >= 0 && i < tz::NumMeters ? i : -1; };

    options = options.withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })

        .withNativeFunction ("getState", [this] (NativeArgs, Completion done) { done (buildState()); })

        .withNativeFunction ("setTarget", [&proc, validIndex] (NativeArgs a, Completion done)
        {
            if (a.size() >= 3)
                if (const int i = validIndex (a[0]); i >= 0)
                {
                    float lo = (float) (double) a[1], hi = (float) (double) a[2];
                    if (lo > hi) std::swap (lo, hi);
                    proc.working.meters[(size_t) i].min = lo;
                    proc.working.meters[(size_t) i].max = hi;
                }
            done (proc.isModified());
        })

        .withNativeFunction ("setMeterOption", [&proc, validIndex] (NativeArgs a, Completion done)
        {
            if (a.size() >= 3)
                if (const int i = validIndex (a[0]); i >= 0)
                {
                    auto& m = proc.working.meters[(size_t) i];
                    const auto key = a[1].toString();
                    if (key == "visible") m.visible = (bool) a[2];
                    if (key == "hold")    m.hold = a[2].toString() == "lowest" ? tz::HoldMode::Lowest : tz::HoldMode::Highest;
                }
            done (proc.isModified());
        })

        .withNativeFunction ("setView", [&proc] (NativeArgs a, Completion done)
        {
            const auto v = a.size() > 0 ? a[0].toString() : juce::String();
            proc.viewName = (v == "targets" || v == "reference") ? v : juce::String ("live");
            done ({});
        })

        .withNativeFunction ("loadProfile", [this, &proc] (NativeArgs a, Completion done)
        {
            if (a.size() > 0)
            {
                proc.profiles.reload();
                proc.loadProfile (a[0].toString());
            }
            done (buildState());
        })

        .withNativeFunction ("saveProfile", [this, &proc] (NativeArgs a, Completion done)
        {
            auto* r = new juce::DynamicObject();
            const auto name = a.size() > 0 ? a[0].toString().trim() : juce::String();
            const auto* existing = proc.profiles.find (name);

            if (name.isEmpty())
                r->setProperty ("error", "Give the profile a name.");
            else if (existing != nullptr && existing->factory)
                r->setProperty ("error", "\"" + name + "\" is a built-in profile. Save under a new name.");
            else
            {
                auto p = proc.working;
                p.name = name;
                if (proc.profiles.saveUser (p))
                {
                    proc.working.name = name;
                    proc.activeProfile = name;
                }
                else
                    r->setProperty ("error", "Couldn't write the profiles file.");
            }

            r->setProperty ("state", buildState());
            done (juce::var (r));
        })

        .withNativeFunction ("deleteProfile", [this, &proc] (NativeArgs a, Completion done)
        {
            if (a.size() > 0 && proc.profiles.removeUser (a[0].toString()) && proc.activeProfile == a[0].toString())
                proc.activeProfile = proc.working.name + " (unsaved)";
            done (buildState());
        })

        .withNativeFunction ("resetMeters", [&proc] (NativeArgs, Completion done)
        {
            proc.engine().requestReset();
            done ({});
        })

        .withNativeFunction ("refBrowse", [this] (NativeArgs, Completion done)
        {
            fileChooser = std::make_unique<juce::FileChooser> ("Choose a reference track", juce::File(),
                                                               "*.wav;*.wave;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.aac;*.ogg");
            fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                      [this] (const juce::FileChooser& fc)
                                      {
                                          const auto f = fc.getResult();
                                          if (f.existsAsFile())
                                              processorRef.referenceAnalyzer.start (f, f.getFileNameWithoutExtension(), false);
                                      });
            done ({});
        })

        .withNativeFunction ("refUploadBegin", [this] (NativeArgs a, Completion done)
        {
            abortUpload();
            const auto name = a.size() > 0 ? a[0].toString() : juce::String ("reference.wav");
            const auto size = a.size() > 1 ? (juce::int64) (double) a[1] : 0;
            if (size > (juce::int64) 600 * 1024 * 1024) { done ("That file is over 600 MB."); return; }

            uploadName = juce::File::createLegalFileName (name);
            uploadFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getNonexistentChildFile ("NovaMeterRef", juce::File (uploadName).getFileExtension(), false);
            uploadStream = std::make_unique<juce::FileOutputStream> (uploadFile);
            if (uploadStream->failedToOpen()) { abortUpload(); done ("Couldn't create a temporary file."); return; }
            done ("ok");
        })

        .withNativeFunction ("refUploadChunk", [this] (NativeArgs a, Completion done)
        {
            juce::MemoryBlock block;
            if (uploadStream == nullptr || a.size() < 1 || ! block.fromBase64Encoding (a[0].toString()))
            {
                abortUpload();
                done ("Upload failed.");
                return;
            }
            uploadStream->write (block.getData(), block.getSize());
            uploadBytes += (juce::int64) block.getSize();
            done ("ok");
        })

        .withNativeFunction ("refUploadEnd", [this] (NativeArgs, Completion done)
        {
            if (uploadStream == nullptr) { done ("Upload failed."); return; }
            uploadStream->flush();
            uploadStream.reset();
            // The analyzer owns the temp file from here and deletes it when finished.
            processorRef.referenceAnalyzer.start (uploadFile, juce::File (uploadName).getFileNameWithoutExtension(), true);
            uploadFile = juce::File();
            uploadBytes = 0;
            done ("ok");
        })

        .withNativeFunction ("refClear", [this, &proc] (NativeArgs, Completion done)
        {
            proc.referenceAnalyzer.cancel();
            proc.collectReference();
            proc.reference = {};
            lastRefStatus.clear();
            done (buildState());
        })

        .withNativeFunction ("refMakeTargets", [this, &proc] (NativeArgs, Completion done)
        {
            if (proc.reference.valid())
            {
                proc.working = tz::targetsFromReference (proc.reference, proc.working);
                proc.activeProfile = proc.working.name; // not saved yet: shows as modified
            }
            done (buildState());
        })

        .withNativeFunction ("activateLicense", [this] (NativeArgs a, Completion done)
        {
           #if NOVA_METER_REQUIRE_LICENSE
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

//==============================================================================
std::optional<juce::WebBrowserComponent::Resource> NovaMeterAudioProcessorEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    // JUCE normally passes just the path ("/js/index.js"); some backends pass the full URL.
    const auto root = juce::WebBrowserComponent::getResourceProviderRoot();
    auto path = url.startsWith (root) ? url.substring (root.length()) : url;
    path = path.upToFirstOccurrenceOf ("?", false, false).toLowerCase();
    if (path.isEmpty() || path == "/")
        path = "/index.html";
    if (! path.startsWithChar ('/'))  // the provider root ends in '/', so relative URLs arrive without one
        path = "/" + path;

    // Order matters: the bridge files also end in index.js.
    if (path.endsWith ("/js/juce/index.js"))
        return makeResource (nova_meter_BinaryData::index_js2, nova_meter_BinaryData::index_js2Size, "text/javascript");
    if (path.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_meter_BinaryData::check_native_interop_js, nova_meter_BinaryData::check_native_interop_jsSize, "text/javascript");
    if (path.endsWith ("/js/index.js"))
        return makeResource (nova_meter_BinaryData::index_js, nova_meter_BinaryData::index_jsSize, "text/javascript");
    if (path.endsWith ("/n_logo.png"))
        return makeResource (nova_meter_BinaryData::n_logo_png, nova_meter_BinaryData::n_logo_pngSize, "image/png");
    if (path.endsWith ("/index.html"))
        return makeResource (nova_meter_BinaryData::index_html, nova_meter_BinaryData::index_htmlSize, "text/html");

    return std::nullopt;
}
