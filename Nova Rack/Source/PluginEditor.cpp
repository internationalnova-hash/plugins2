#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

using NativeArgs = const juce::Array<juce::var>&;
using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;

NovaRackAudioProcessorEditor::NovaRackAudioProcessorEditor (NovaRackAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot()
                      + "index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds()));

    // The UI is designed at 1240 x 720 and scales to fit, so the editor keeps that shape.
    setResizable (true, true);
    setResizeLimits (868, 504, 1860, 1080);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (1240.0 / 720.0);
    setSize (1240, 720);
    startTimerHz (20);
}

NovaRackAudioProcessorEditor::~NovaRackAudioProcessorEditor()
{
    stopTimer();
    processorRef.closeAllWindows(); // closing the rack closes its plugins' full-size windows
}

void NovaRackAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (6, 11, 18));
}

void NovaRackAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

//==============================================================================
static juce::String dbJson (float linear)
{
    return linear > 1.0e-6f ? juce::String (juce::Decibels::gainToDecibels (linear), 1) : juce::String ("-120");
}

juce::String NovaRackAudioProcessorEditor::buildLiveJson()
{
    auto& p = processorRef;
    juce::String slotsJson;

    for (int i = 0; i < p.numSlots(); ++i)
    {
        auto* s = p.slot (i);
        juce::String knobs;
        if (s->instance != nullptr)
        {
            const auto& params = s->instance->getParameters();
            for (int k = 0; k < s->face.size(); ++k)
            {
                if (! juce::isPositiveAndBelow (s->face[k], params.size())) continue;
                auto* prm = params[s->face[k]];
                const float v = prm->getValue();
                auto text = prm->getText (v, 14).trim();
                const auto label = prm->getLabel().trim();
                if (label.isNotEmpty() && ! text.endsWithIgnoreCase (label))
                    text << " " << label;
                knobs << (knobs.isEmpty() ? "" : ",") << "[" << juce::String (v, 4) << "," << juce::JSON::toString (text) << "]";
            }
        }
        slotsJson << (i ? "," : "") << "{\"k\":[" << knobs << "],\"b\":" << (p.getBypass (i) ? "true" : "false")
                  << ",\"m\":" << juce::String (p.getMix (i), 3) << ",\"l\":" << s->getLatency()
                  << ",\"p\":" << dbJson (s->outPeak.exchange (0.0f)) << "}";
    }

    const auto inPk = p.inPeak.exchange (0.0f), outPk = p.outPeak.exchange (0.0f);
    juce::String lib;
    if (p.library->isScanning())
        lib << ",\"scan\":{\"progress\":" << juce::String (p.library->getProgress(), 3)
            << ",\"current\":" << juce::JSON::toString (p.library->getCurrentlyScanning()) << "}";

    return "{\"in\":" + dbJson (inPk) + ",\"out\":" + dbJson (outPk)
         + ",\"inGain\":" + juce::String (p.getGainDb (false), 1) + ",\"outGain\":" + juce::String (p.getGainDb (true), 1)
         + ",\"latency\":" + juce::String (p.getLatencySamples())
         + ",\"dirty\":" + (p.rackDirty ? "true" : "false")
         + ",\"slots\":[" + slotsJson + "]" + lib + "}";
}

void NovaRackAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    juce::String js = "if (window.novaRackUpdate) { window.novaRackUpdate(" + buildLiveJson() + "); }";

    const int rackGen = processorRef.rackGeneration.load();
    const int libGen = processorRef.library->getGeneration();
    if (rackGen != seenRackGeneration || libGen != seenLibraryGeneration)
    {
        seenRackGeneration = rackGen;
        seenLibraryGeneration = libGen;
        js << "if (window.novaRackStateChanged) { window.novaRackStateChanged(); }";
    }

    webView->evaluateJavascript (js);
}

//==============================================================================
static juce::var faceInfo (juce::AudioPluginInstance& inst, const juce::Array<int>& face)
{
    juce::Array<juce::var> out;
    const auto& params = inst.getParameters();
    for (auto idx : face)
    {
        if (! juce::isPositiveAndBelow (idx, params.size())) continue;
        auto* prm = params[idx];
        auto* o = new juce::DynamicObject();
        o->setProperty ("idx", idx);
        o->setProperty ("name", prm->getName (24));
        o->setProperty ("steps", prm->isDiscrete() ? prm->getNumSteps() : 0);
        o->setProperty ("toggle", prm->isBoolean());
        o->setProperty ("def", prm->getDefaultValue());
        out.add (juce::var (o));
    }
    return out;
}

juce::var NovaRackAudioProcessorEditor::buildState() const
{
    auto& p = processorRef;
    auto* o = new juce::DynamicObject();

    auto* rack = new juce::DynamicObject();
    rack->setProperty ("name", p.rackName);
    rack->setProperty ("dirty", p.rackDirty);
    rack->setProperty ("factory", p.presets.isFactoryRack (p.rackName));
    o->setProperty ("rack", juce::var (rack));
    o->setProperty ("maxSlots", NovaRackAudioProcessor::kMaxSlots);

    juce::Array<juce::var> slots;
    for (int i = 0; i < p.numSlots(); ++i)
    {
        auto* s = p.slot (i);
        auto* so = new juce::DynamicObject();
        so->setProperty ("plugin", s->pluginName);
        so->setProperty ("missing", s->missing);
        so->setProperty ("loading", s->missing && p.library->isScanning());
        so->setProperty ("preset", s->presetName);
        so->setProperty ("category", s->instance != nullptr ? nr::PluginLibrary::categoryFor (s->description) : juce::String());
        so->setProperty ("version", s->description.version);
        so->setProperty ("face", s->instance != nullptr ? faceInfo (*s->instance, s->face) : juce::var (juce::Array<juce::var>()));

        juce::Array<juce::var> programs;
        if (s->instance != nullptr && s->instance->getNumPrograms() > 1)
            for (int k = 0; k < s->instance->getNumPrograms(); ++k)
                programs.add (s->instance->getProgramName (k));
        so->setProperty ("programs", programs);
        slots.add (juce::var (so));
    }
    o->setProperty ("slots", slots);

    auto* lib = new juce::DynamicObject();
    lib->setProperty ("scanning", p.library->isScanning());
    juce::Array<juce::var> plugins;
    for (const auto& d : p.library->getPlugins())
    {
        auto* po = new juce::DynamicObject();
        po->setProperty ("name", d.name);
        po->setProperty ("category", nr::PluginLibrary::categoryFor (d));
        po->setProperty ("version", d.version);
        plugins.add (juce::var (po));
    }
    lib->setProperty ("plugins", plugins);
    o->setProperty ("library", juce::var (lib));

    juce::Array<juce::var> racks;
    for (const auto& r : p.presets.listRacks())
    {
        auto* ro = new juce::DynamicObject();
        ro->setProperty ("name", r.name);
        ro->setProperty ("group", r.group);
        ro->setProperty ("note", r.note);
        ro->setProperty ("factory", r.factory);
        juce::Array<juce::var> chain;
        nr::RackData rd;
        if (p.presets.loadRack (r.name, rd))
            for (const auto& sd : rd.slots)
                chain.add (sd.plugin);
        ro->setProperty ("chain", chain);
        racks.add (juce::var (ro));
    }
    o->setProperty ("racks", racks);

    juce::Array<juce::var> modules;
    for (const auto& m : p.presets.listModules())
    {
        auto* mo = new juce::DynamicObject();
        mo->setProperty ("plugin", m.plugin);
        mo->setProperty ("name", m.name);
        modules.add (juce::var (mo));
    }
    o->setProperty ("modules", modules);

   #if NOVA_RACK_REQUIRE_LICENSE
    o->setProperty ("licenseRequired", true);
    o->setProperty ("unlocked", license.isUnlocked());
   #else
    o->setProperty ("licenseRequired", false);
    o->setProperty ("unlocked", true);
   #endif
    return juce::var (o);
}

static juce::var result (const juce::String& error, const juce::var& state)
{
    auto* r = new juce::DynamicObject();
    if (error.isNotEmpty()) r->setProperty ("error", error);
    r->setProperty ("state", state);
    return juce::var (r);
}

juce::WebBrowserComponent::Options NovaRackAudioProcessorEditor::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaRack")));
   #endif

    auto& proc = processorRef;
    auto arg = [] (NativeArgs a, int i) { return i < a.size() ? a[i] : juce::var(); };

    options = options.withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })

        .withNativeFunction ("getState", [this] (NativeArgs, Completion done) { done (buildState()); })

        .withNativeFunction ("addPlugin", [this, &proc, arg] (NativeArgs a, Completion done)
        {
            const auto err = proc.addSlot (arg (a, 0).toString(), a.size() > 1 ? (int) a[1] : -1);
            done (result (err, buildState()));
        })

        .withNativeFunction ("removeSlot", [this, &proc, arg] (NativeArgs a, Completion done)
        {
            proc.removeSlot ((int) arg (a, 0));
            done (buildState());
        })

        .withNativeFunction ("moveSlot", [this, &proc, arg] (NativeArgs a, Completion done)
        {
            proc.moveSlot ((int) arg (a, 0), (int) arg (a, 1));
            done (buildState());
        })

        .withNativeFunction ("setBypass", [&proc, arg] (NativeArgs a, Completion done)
        {
            proc.setBypass ((int) arg (a, 0), (bool) arg (a, 1));
            done ({});
        })

        .withNativeFunction ("setMix", [&proc, arg] (NativeArgs a, Completion done)
        {
            proc.setMix ((int) arg (a, 0), (float) (double) arg (a, 1));
            done ({});
        })

        .withNativeFunction ("setGain", [&proc, arg] (NativeArgs a, Completion done)
        {
            proc.setGainDb (arg (a, 0).toString() == "out", (float) (double) arg (a, 1));
            done ({});
        })

        // setParam (slot, paramIndex, value0to1, phase: "begin" | "move" | "end")
        .withNativeFunction ("setParam", [&proc, arg] (NativeArgs a, Completion done)
        {
            if (auto* s = proc.slot ((int) arg (a, 0)); s != nullptr && s->instance != nullptr)
            {
                const auto& params = s->instance->getParameters();
                const int idx = (int) arg (a, 1);
                if (juce::isPositiveAndBelow (idx, params.size()))
                {
                    auto* prm = params[idx];
                    const auto phase = arg (a, 3).toString();
                    if (phase == "begin") prm->beginChangeGesture();
                    prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (float) (double) arg (a, 2)));
                    if (phase == "end") prm->endChangeGesture();
                    proc.rackDirty = true;
                }
            }
            done ({});
        })

        .withNativeFunction ("listParams", [&proc, arg] (NativeArgs a, Completion done)
        {
            juce::Array<juce::var> out;
            if (auto* s = proc.slot ((int) arg (a, 0)); s != nullptr && s->instance != nullptr)
            {
                const auto& params = s->instance->getParameters();
                for (int i = 0; i < params.size(); ++i)
                    if (params[i]->isAutomatable() && params[i]->getName (64).isNotEmpty())
                    {
                        auto* o = new juce::DynamicObject();
                        o->setProperty ("idx", i);
                        o->setProperty ("name", params[i]->getName (64));
                        out.add (juce::var (o));
                    }
            }
            done (out);
        })

        // setFace (slot, knobPosition, paramIndex)  paramIndex < 0 removes the knob
        .withNativeFunction ("setFace", [this, &proc, arg] (NativeArgs a, Completion done)
        {
            if (auto* s = proc.slot ((int) arg (a, 0)); s != nullptr && s->instance != nullptr)
            {
                const int pos = (int) arg (a, 1), idx = (int) arg (a, 2);
                if (idx < 0) { if (juce::isPositiveAndBelow (pos, s->face.size())) s->face.remove (pos); }
                else if (juce::isPositiveAndBelow (idx, s->instance->getParameters().size()))
                {
                    if (juce::isPositiveAndBelow (pos, s->face.size())) s->face.set (pos, idx);
                    else if (s->face.size() < 8) s->face.add (idx);
                }
                else if (pos == -1) s->chooseDefaultFace();
                proc.rackDirty = true;
            }
            done (buildState());
        })

        .withNativeFunction ("openPlugin", [&proc, arg] (NativeArgs a, Completion done)
        {
            proc.openPluginWindow ((int) arg (a, 0));
            done ({});
        })

        .withNativeFunction ("newRack", [this, &proc] (NativeArgs, Completion done)
        {
            proc.loadRack ({});
            done (buildState());
        })

        .withNativeFunction ("loadRack", [this, &proc, arg] (NativeArgs a, Completion done)
        {
            nr::RackData rd;
            juce::String err;
            if (! proc.presets.loadRack (arg (a, 0).toString(), rd))
                err = "Couldn't open that rack preset.";
            else
            {
                const auto missing = proc.loadRack (rd);
                if (! missing.isEmpty())
                    err = "Not installed: " + missing.joinIntoString (", ") + ". Their settings are kept in the rack.";
            }
            done (result (err, buildState()));
        })

        .withNativeFunction ("saveRack", [this, &proc, arg] (NativeArgs a, Completion done)
        {
            const auto name = arg (a, 0).toString().trim();
            juce::String err;
            if (name.isEmpty())                       err = "Give the rack a name.";
            else if (proc.presets.isFactoryRack (name)) err = "\"" + name + "\" is a built-in rack. Save under a new name.";
            else
            {
                auto rd = proc.captureRack();
                rd.name = name;
                if (proc.presets.saveRack (rd)) { proc.rackName = name; proc.rackDirty = false; }
                else                             err = "Couldn't write the rack preset.";
            }
            done (result (err, buildState()));
        })

        .withNativeFunction ("deleteRack", [this, &proc, arg] (NativeArgs a, Completion done)
        {
            const auto name = arg (a, 0).toString();
            if (proc.presets.deleteRack (name) && proc.rackName == name)
                proc.rackDirty = true;
            done (buildState());
        })

        // loadModule (slot, presetName)  "#3" loads the plugin's own factory program 3
        .withNativeFunction ("loadModule", [this, &proc, arg] (NativeArgs a, Completion done)
        {
            const int i = (int) arg (a, 0);
            const auto name = arg (a, 1).toString();
            juce::String err;
            if (auto* s = proc.slot (i); s != nullptr && s->instance != nullptr)
            {
                if (name.startsWithChar ('#'))
                {
                    const int prog = name.substring (1).getIntValue();
                    s->instance->setCurrentProgram (prog);
                    s->presetName = s->instance->getProgramName (prog);
                    proc.rackDirty = true;
                }
                else
                {
                    nr::SlotData sd;
                    if (proc.presets.loadModule (s->pluginName, name, sd))
                    {
                        sd.bypass = proc.getBypass (i);
                        proc.applySlotData (i, sd);
                    }
                    else err = "Couldn't open that preset.";
                }
            }
            done (result (err, buildState()));
        })

        .withNativeFunction ("saveModule", [this, &proc, arg] (NativeArgs a, Completion done)
        {
            const int i = (int) arg (a, 0);
            const auto name = arg (a, 1).toString().trim();
            juce::String err;
            if (auto* s = proc.slot (i); s != nullptr && s->instance != nullptr)
            {
                if (name.isEmpty()) err = "Give the preset a name.";
                else if (proc.presets.saveModule (s->pluginName, name, proc.captureSlot (i))) s->presetName = name;
                else err = "Couldn't write the preset.";
            }
            done (result (err, buildState()));
        })

        .withNativeFunction ("deleteModule", [this, &proc, arg] (NativeArgs a, Completion done)
        {
            proc.presets.deleteModule (arg (a, 0).toString(), arg (a, 1).toString());
            done (buildState());
        })

        // addModule (plugin, presetName, index): new unit already set to that preset
        .withNativeFunction ("addModule", [this, &proc, arg] (NativeArgs a, Completion done)
        {
            nr::SlotData sd;
            juce::String err;
            if (! proc.presets.loadModule (arg (a, 0).toString(), arg (a, 1).toString(), sd))
                err = "Couldn't open that preset.";
            else
                err = proc.addSlot (sd.plugin, a.size() > 2 ? (int) a[2] : -1, &sd);
            done (result (err == "missing" ? sd.plugin + " isn't installed." : err, buildState()));
        })

        .withNativeFunction ("rescan", [this, &proc] (NativeArgs, Completion done)
        {
            proc.library->rescan (false);
            done (buildState());
        })

        .withNativeFunction ("activateLicense", [this] (NativeArgs a, Completion done)
        {
           #if NOVA_RACK_REQUIRE_LICENSE
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
std::optional<juce::WebBrowserComponent::Resource> NovaRackAudioProcessorEditor::getResource (const juce::String& url)
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
    while (path.startsWith ("//"))   // the provider root ends in '/', so requests can arrive as "//skins/…"
        path = path.substring (1);
    if (! path.startsWithChar ('/'))
        path = "/" + path;

    // Order matters: the bridge files also end in index.js.
    if (path.endsWith ("/js/juce/index.js"))
        return makeResource (nova_rack_BinaryData::index_js2, nova_rack_BinaryData::index_js2Size, "text/javascript");
    if (path.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_rack_BinaryData::check_native_interop_js, nova_rack_BinaryData::check_native_interop_jsSize, "text/javascript");
    if (path.endsWith ("/js/index.js"))
        return makeResource (nova_rack_BinaryData::index_js, nova_rack_BinaryData::index_jsSize, "text/javascript");
    if (path.endsWith ("/n_logo.png"))
        return makeResource (nova_rack_BinaryData::n_logo_png, nova_rack_BinaryData::n_logo_pngSize, "image/png");
    if (path.endsWith ("/index.html"))
        return makeResource (nova_rack_BinaryData::index_html, nova_rack_BinaryData::index_htmlSize, "text/html");

    // Faceplate artwork: /skins/<file>, looked up by its original file name.
    if (path.startsWith ("/skins/"))
    {
        const auto file = path.fromLastOccurrenceOf ("/", false, false);
        for (int i = 0; i < nova_rack_BinaryData::namedResourceListSize; ++i)
            if (juce::String (nova_rack_BinaryData::originalFilenames[i]).equalsIgnoreCase (file))
            {
                int size = 0;
                if (const auto* data = nova_rack_BinaryData::getNamedResource (nova_rack_BinaryData::namedResourceList[i], size))
                    return makeResource (data, size, file.endsWith (".png") ? "image/png" : "image/jpeg");
            }
    }

    return std::nullopt;
}
