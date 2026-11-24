#include "PluginEditor.h"
#include "BinaryData.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <cstring>
#include "NovaLicenseGate.h"

using NativeArgs = const juce::Array<juce::var>&;
using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;

static constexpr int kW = 1120, kH = 700;

NovaLoopLabAudioProcessorEditor::NovaLoopLabAudioProcessorEditor (NovaLoopLabAudioProcessor& p)
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

NovaLoopLabAudioProcessorEditor::~NovaLoopLabAudioProcessorEditor()
{
    stopTimer();
    processorRef.auditionLoop.store (false);
}

void NovaLoopLabAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (3, 8, 15));
}

void NovaLoopLabAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

static juce::String dbOrNull (float gain)
{
    return gain > 1.0e-6f ? juce::String (juce::Decibels::gainToDecibels (gain), 2) : juce::String ("null");
}

void NovaLoopLabAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    auto& p = processorRef;
    juce::String params;
    for (const auto& id : loopLabParamIds())
        params << (params.isEmpty() ? "" : ",") << "\"" << id << "\":" << juce::String (p.apvts.getRawParameterValue (id)->load(), 2);

    juce::String js;
    js << "if (window.novaLoopLabUpdate) window.novaLoopLabUpdate({"
       << "\"playhead\":" << juce::String (p.playhead.load(), 4)
       << ",\"pattern\":" << p.playingPattern.load()
       << ",\"modelVersion\":" << p.core.getModelVersion()
       << ",\"busy\":" << (p.core.isBusy() ? "true" : "false")
       << ",\"hostBpm\":" << juce::String (p.hostBpm.load(), 2)
       << ",\"hostPlaying\":" << (p.hostPlaying.load() ? "true" : "false")
       << ",\"auditionLoop\":" << (p.auditionLoop.load() ? "true" : "false")
       << ",\"outL\":" << dbOrNull (NovaLoopLabAudioProcessor::take (p.outL))
       << ",\"outR\":" << dbOrNull (NovaLoopLabAudioProcessor::take (p.outR))
       << ",\"error\":" << juce::JSON::toString (p.core.getLoadError())
       << ",\"program\":" << p.currentProgram.load()
       << ",\"bypass\":" << (p.apvts.getRawParameterValue ("bypass")->load() > 0.5f ? "true" : "false")
       << ",\"params\":{" << params << "}"
       << "});";
    webView->evaluateJavascript (js);
}

juce::var NovaLoopLabAudioProcessorEditor::buildState()
{
    auto& p = processorRef;
    auto* o = new juce::DynamicObject();

    juce::Array<juce::var> presets;
    for (const auto& st : flipStyles()) presets.add (juce::String (st.name));
    o->setProperty ("presets", presets);
    o->setProperty ("program", p.currentProgram.load());

    auto* params = new juce::DynamicObject();
    for (const auto& id : loopLabParamIds())
        params->setProperty (id, p.apvts.getRawParameterValue (id)->load());
    o->setProperty ("params", juce::var (params));
    o->setProperty ("bypass", p.apvts.getRawParameterValue ("bypass")->load() > 0.5f);

    const auto spec = p.core.getSpec();
    const auto model = p.core.getModel();
    o->setProperty ("modelVersion", p.core.getModelVersion());
    o->setProperty ("busy", p.core.isBusy());
    o->setProperty ("error", p.core.getLoadError());
    o->setProperty ("tempoMult", spec.tempoMult);

    juce::Array<juce::var> locked;
    for (bool l : spec.locked) locked.add (l);
    o->setProperty ("locked", locked);

    const bool has = model != nullptr && model->hasSample();
    o->setProperty ("hasSample", has);
    if (has)
    {
        const auto& m = *model;
        o->setProperty ("sampleName", m.sampleName);
        o->setProperty ("sampleBpm", m.analysis.bpm);
        o->setProperty ("sampleBars", m.analysis.beats / 4.0);
        o->setProperty ("key", ll::keyName (m.analysis.key));
        o->setProperty ("loopBars", m.bars);
        o->setProperty ("loopBpm", m.bpm);
        o->setProperty ("effectiveGridDiv", m.effectiveGridDiv);

        if (peaksVersion != m.version || peaksCache.isVoid())
        {
            juce::Array<juce::var> peaks;
            peaks.ensureStorageAllocated ((int) m.analysis.peaks.size());
            for (float v : m.analysis.peaks) peaks.add (std::round (v * 1000.0f) / 1000.0f);
            peaksCache = peaks;
            peaksVersion = m.version;
        }
        o->setProperty ("peaks", peaksCache);

        const double len = (double) m.sample->length();
        juce::Array<juce::var> chops, fx;
        for (size_t i = 0; i < m.chops.size(); ++i)
        {
            auto* c = new juce::DynamicObject();
            c->setProperty ("s", m.chops[i].start / len);
            c->setProperty ("e", m.chops[i].end / len);
            chops.add (juce::var (c));

            const auto& f = m.fxUsed[i];
            auto* fo = new juce::DynamicObject();
            fo->setProperty ("pitch", f.pitch); fo->setProperty ("stretch", f.stretch);
            fo->setProperty ("filter", f.filter); fo->setProperty ("tone", f.tone);
            fo->setProperty ("reverse", f.reverse); fo->setProperty ("use", f.use);
            fx.add (juce::var (fo));
        }
        o->setProperty ("chops", chops);
        o->setProperty ("fx", fx);

        juce::Array<juce::var> vars;
        for (const auto& steps : m.steps)
        {
            juce::Array<juce::var> sv;
            for (const auto& st : steps)
            {
                juce::Array<juce::var> a { st.t, st.len, st.chop, st.stutter ? 1 : 0 };
                sv.add (a);
            }
            vars.add (sv);
        }
        o->setProperty ("steps", vars);
    }

   #if NOVA_LOOPLAB_REQUIRE_LICENSE
    o->setProperty ("licenseRequired", true);
    o->setProperty ("unlocked", license.isUnlocked());
   #else
    o->setProperty ("licenseRequired", false);
    o->setProperty ("unlocked", true);
   #endif
    return juce::var (o);
}

juce::var NovaLoopLabAudioProcessorEditor::exportAndDrag (bool loop, int index)
{
    auto* r = new juce::DynamicObject();
    const auto model = processorRef.core.getModel();
    if (model == nullptr || ! model->hasSample())
    {
        r->setProperty ("error", "Load a sample first.");
        return juce::var (r);
    }

    const auto& m = *model;
    const juce::AudioBuffer<float>* audio = nullptr;
    juce::String what, fingerprint;
    if (loop && juce::isPositiveAndBelow (index, ll::kNumVariations))
    {
        audio = &m.loops[(size_t) index];
        what = "flip V" + juce::String (index + 1) + " " + juce::String (juce::roundToInt (m.bpm)) + "bpm";
    }
    else if (! loop && juce::isPositiveAndBelow (index, (int) m.renderedChops.size()))
    {
        audio = &m.renderedChops[(size_t) index];
        what = "chop " + juce::String (index + 1).paddedLeft ('0', 2);
    }
    if (audio == nullptr || audio->getNumSamples() == 0)
    {
        r->setProperty ("error", "Nothing to export yet.");
        return juce::var (r);
    }

    // Same audio -> same file name, so repeated drags don't pile up copies,
    // and a changed loop never overwrites a file a DAW project already uses.
    juce::MemoryOutputStream fp;
    for (int ch = 0; ch < audio->getNumChannels(); ++ch)
        fp.write (audio->getReadPointer (ch), sizeof (float) * (size_t) std::min (audio->getNumSamples(), 48000));
    fp.writeInt (audio->getNumSamples());
    const auto hash = juce::String::toHexString (juce::String::fromUTF8 ((const char*) fp.getData(), (int) fp.getDataSize()).hashCode64()).getLastCharacters (5);

    auto stem = juce::File::createLegalFileName (juce::File (m.sampleName).getFileNameWithoutExtension()).trim();
    if (stem.isEmpty()) stem = "Loop Lab";
    const auto dir = ll::LoopLabCore::exportsFolder();
    dir.createDirectory();
    const auto file = dir.getChildFile (stem + " " + what + " " + hash + ".wav");

    if (! file.existsAsFile())
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (file.createOutputStream().release());
        if (os == nullptr) { r->setProperty ("error", "Couldn't write to " + dir.getFullPathName()); return juce::var (r); }
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), m.sampleRate, 2, 24, {}, 0));
        if (w == nullptr) { r->setProperty ("error", "Couldn't create the WAV file."); return juce::var (r); }
        os.release();
        w->writeFromAudioSampleBuffer (*audio, 0, audio->getNumSamples());
    }

    const bool dragged = juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
    if (! dragged)
        file.revealToUser();

    r->setProperty ("dragged", dragged);
    r->setProperty ("path", file.getFullPathName());
    r->setProperty ("name", file.getFileName());
    return juce::var (r);
}

juce::WebBrowserComponent::Options NovaLoopLabAudioProcessorEditor::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                         .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("NovaLoopLab")));
   #endif

    auto& proc = processorRef;

    auto fxFromVar = [] (const juce::var& v, ll::ChopFx f)
    {
        if (auto* obj = v.getDynamicObject())
        {
            if (obj->hasProperty ("pitch"))   f.pitch   = juce::jlimit (-12, 12, (int) obj->getProperty ("pitch"));
            if (obj->hasProperty ("stretch")) f.stretch = juce::jlimit (50, 200, (int) obj->getProperty ("stretch"));
            if (obj->hasProperty ("filter"))  f.filter  = juce::jlimit (-100, 100, (int) obj->getProperty ("filter"));
            if (obj->hasProperty ("tone"))    f.tone    = juce::jlimit (0, 100, (int) obj->getProperty ("tone"));
            if (obj->hasProperty ("reverse")) f.reverse = (bool) obj->getProperty ("reverse");
            if (obj->hasProperty ("use"))     f.use     = (bool) obj->getProperty ("use");
        }
        return f;
    };

    options = options.withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })

        .withNativeFunction ("getState", [this] (NativeArgs, Completion done) { done (buildState()); })

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
            if (a.size() > 0) proc.setCurrentProgram ((int) a[0]);
            done (buildState());
        })

        .withNativeFunction ("loadSample", [this, &proc] (NativeArgs, Completion done)
        {
            chooser = std::make_unique<juce::FileChooser> ("Load a sample to flip",
                juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [&proc, done] (const juce::FileChooser& fc)
                {
                    const auto f = fc.getResult();
                    if (f.existsAsFile()) proc.core.loadFile (f, true);
                    done (f.existsAsFile());
                });
        })

        // Finder drops arrive in the web view as bytes: [base64, fileName].
        .withNativeFunction ("dropSample", [&proc] (NativeArgs a, Completion done)
        {
            juce::MemoryOutputStream bytes;
            const bool ok = a.size() >= 2 && juce::Base64::convertFromBase64 (bytes, a[0].toString());
            if (! ok || bytes.getDataSize() == 0) { done ("Couldn't read the dropped file."); return; }
            const auto name = juce::File::createLegalFileName (a[1].toString());
            const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("NovaLoopLab_drops");
            dir.createDirectory();
            const auto f = dir.getChildFile (name.isNotEmpty() ? name : juce::String ("dropped.wav"));
            if (! f.replaceWithData (bytes.getData(), bytes.getDataSize())) { done ("Couldn't save the dropped file."); return; }
            proc.core.loadFile (f, true);
            done ("ok");
        })

        .withNativeFunction ("generate", [&proc] (NativeArgs, Completion done)
        {
            proc.core.updateSpec ([] (ll::Spec& s)
            {
                juce::Random r;
                for (int v = 0; v < ll::kNumVariations; ++v)
                    if (! s.locked[(size_t) v]) s.seeds[(size_t) v] = (juce::uint32) r.nextInt (1 << 30) + 1;
            });
            done ({});
        })

        // setLock(variation, locked): a locked flip keeps its exact steps.
        .withNativeFunction ("setLock", [&proc] (NativeArgs a, Completion done)
        {
            if (a.size() >= 2)
            {
                const int v = (int) a[0];
                const bool lock = (bool) a[1];
                const auto model = proc.core.getModel();
                if (juce::isPositiveAndBelow (v, ll::kNumVariations))
                    proc.core.updateSpec ([&] (ll::Spec& s)
                    {
                        s.locked[(size_t) v] = lock;
                        s.lockedSteps[(size_t) v] = lock && model != nullptr ? model->steps[(size_t) v] : std::vector<ll::Step> {};
                    });
            }
            done ({});
        })

        .withNativeFunction ("setChopFx", [&proc, fxFromVar] (NativeArgs a, Completion done)
        {
            if (a.size() >= 2)
            {
                const int i = (int) a[0];
                if (juce::isPositiveAndBelow (i, ll::kMaxChops))
                    proc.core.updateSpec ([&] (ll::Spec& s)
                    {
                        s.fx.resize ((size_t) ll::kMaxChops);
                        s.fx[(size_t) i] = fxFromVar (a[1], s.fx[(size_t) i]);
                    });
            }
            done ({});
        })

        .withNativeFunction ("setAllChopFx", [&proc, fxFromVar] (NativeArgs a, Completion done)
        {
            if (a.size() >= 1)
                proc.core.updateSpec ([&] (ll::Spec& s)
                {
                    s.fx.resize ((size_t) ll::kMaxChops);
                    for (auto& f : s.fx)
                    {
                        const bool keepUse = f.use;
                        f = fxFromVar (a[0], f);
                        f.use = keepUse;
                    }
                });
            done ({});
        })

        .withNativeFunction ("setTempoMult", [&proc] (NativeArgs a, Completion done)
        {
            if (a.size() >= 1)
            {
                const double m = (double) a[0];
                proc.core.updateSpec ([m] (ll::Spec& s) { s.tempoMult = juce::jlimit (0.25, 4.0, m); });
            }
            done ({});
        })

        .withNativeFunction ("auditionChop", [&proc] (NativeArgs a, Completion done)
        {
            if (a.size() >= 1) proc.auditionChop.store ((int) a[0]);
            done ({});
        })

        .withNativeFunction ("auditionLoop", [&proc] (NativeArgs a, Completion done)
        {
            proc.auditionLoop.store (a.size() >= 1 && (bool) a[0]);
            done ({});
        })

        .withNativeFunction ("dragLoop", [this] (NativeArgs a, Completion done) { done (exportAndDrag (true, a.size() > 0 ? (int) a[0] : 0)); })
        .withNativeFunction ("dragChop", [this] (NativeArgs a, Completion done) { done (exportAndDrag (false, a.size() > 0 ? (int) a[0] : 0)); })

        .withNativeFunction ("activateLicense", [this] (NativeArgs a, Completion done)
        {
           #if NOVA_LOOPLAB_REQUIRE_LICENSE
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

std::optional<juce::WebBrowserComponent::Resource> NovaLoopLabAudioProcessorEditor::getResource (const juce::String& url)
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
        return makeResource (nova_looplab_BinaryData::index_js2, nova_looplab_BinaryData::index_js2Size, "text/javascript");
    if (path.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_looplab_BinaryData::check_native_interop_js, nova_looplab_BinaryData::check_native_interop_jsSize, "text/javascript");
    if (path.endsWith ("/js/index.js"))
        return makeResource (nova_looplab_BinaryData::index_js, nova_looplab_BinaryData::index_jsSize, "text/javascript");
    if (path.endsWith ("/n_logo.png"))
        return makeResource (nova_looplab_BinaryData::n_logo_png, nova_looplab_BinaryData::n_logo_pngSize, "image/png");
    if (path.endsWith ("/index.html"))
        return makeResource (nova_looplab_BinaryData::index_html, nova_looplab_BinaryData::index_htmlSize, "text/html");

    return std::nullopt;
}
