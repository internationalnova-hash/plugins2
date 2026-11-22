#include "MainWindow.h"
#include "BinaryData.h"

#include <cstring>

using namespace nova::bass;
using namespace nova::basslab;

namespace
{
    juce::String curveToString (NovaMacroCurve c)
    {
        switch (c)
        {
            case NovaMacroCurve::Exponential: return "exponential";
            case NovaMacroCurve::Logarithmic: return "logarithmic";
            case NovaMacroCurve::SCurve:       return "sCurve";
            default:                          return "linear";
        }
    }
    NovaMacroCurve curveFromString (const juce::String& s)
    {
        if (s == "exponential") return NovaMacroCurve::Exponential;
        if (s == "logarithmic") return NovaMacroCurve::Logarithmic;
        if (s == "sCurve")      return NovaMacroCurve::SCurve;
        return NovaMacroCurve::Linear;
    }

    juce::var macroTableToVar (const NovaMacroTable& table)
    {
        auto* root = new juce::DynamicObject();
        for (const auto& [macroName, mappings] : table)
        {
            juce::Array<juce::var> arr;
            for (const auto& m : mappings)
            {
                auto* o = new juce::DynamicObject();
                o->setProperty ("targetParam", m.targetParam);
                o->setProperty ("min", (double) m.min);
                o->setProperty ("max", (double) m.max);
                o->setProperty ("curve", curveToString (m.curve));
                o->setProperty ("polarity", m.polarity == NovaMacroPolarity::Inverted ? "inverted" : "normal");
                arr.add (juce::var (o));
            }
            root->setProperty (macroName, juce::var (arr));
        }
        return juce::var (root);
    }

    NovaMacroTable macroTableFromVar (const juce::var& v)
    {
        NovaMacroTable table;
        if (auto* obj = v.getDynamicObject())
        {
            for (const auto& prop : obj->getProperties())
            {
                std::vector<NovaMacroMapping> mappings;
                if (auto* arr = prop.value.getArray())
                {
                    for (auto& entry : *arr)
                    {
                        NovaMacroMapping m;
                        m.targetParam = entry.getProperty ("targetParam", "").toString();
                        m.min = (float) (double) entry.getProperty ("min", 0.0);
                        m.max = (float) (double) entry.getProperty ("max", 1.0);
                        m.curve = curveFromString (entry.getProperty ("curve", "linear").toString());
                        m.polarity = entry.getProperty ("polarity", "normal").toString() == "inverted" ? NovaMacroPolarity::Inverted : NovaMacroPolarity::Normal;
                        mappings.push_back (m);
                    }
                }
                table[prop.name.toString()] = std::move (mappings);
            }
        }
        return table;
    }

    juce::var presetMetaToVar (const NovaBassPresetMeta& m)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", m.name);
        o->setProperty ("category", m.category);
        o->setProperty ("description", m.description);
        juce::Array<juce::var> tags;
        for (auto& t : m.tags) tags.add (t);
        o->setProperty ("tags", juce::var (tags));
        return juce::var (o);
    }

    NovaBassPresetMeta presetMetaFromVar (const juce::var& v)
    {
        NovaBassPresetMeta m;
        m.name = v.getProperty ("name", "Untitled").toString();
        m.author = "Nova Audio";
        m.category = v.getProperty ("category", "").toString();
        m.description = v.getProperty ("description", "").toString();
        if (auto* arr = v.getProperty ("tags", juce::var()).getArray())
            for (auto& t : *arr) m.tags.add (t.toString());
        return m;
    }

    juce::var candidateEntryToVar (const NovaCandidateEntry& e)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", e.id);
        o->setProperty ("status", NovaBassCandidateIndex::statusToString (e.status));
        o->setProperty ("generatorId", e.generatorId);
        o->setProperty ("createdDate", e.createdDate);
        o->setProperty ("mutationParentId", e.mutationParentId);
        o->setProperty ("renderedAssetPath", e.renderedAssetPath);
        return juce::var (o);
    }
}

namespace
{
    juce::var voiceParamsToVar (const NovaBassVoiceParams& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("sineTriBlend", (double) p.sineTriBlend);
        o->setProperty ("fineTuneCents", (double) p.fineTuneCents);
        o->setProperty ("pitchEnvAmountSemitones", (double) p.pitchEnvAmountSemitones);
        o->setProperty ("pitchEnvDecaySeconds", (double) p.pitchEnvDecaySeconds);
        o->setProperty ("pitchEnvCurve", (double) p.pitchEnvCurve);
        o->setProperty ("ampAttackSeconds", (double) p.ampAttackSeconds);
        o->setProperty ("ampDecaySeconds", (double) p.ampDecaySeconds);
        o->setProperty ("ampSustain", (double) p.ampSustain);
        o->setProperty ("ampReleaseSeconds", (double) p.ampReleaseSeconds);
        o->setProperty ("transientAmount", (double) p.transientAmount);
        o->setProperty ("transientToneHz", (double) p.transientToneHz);
        o->setProperty ("transientDecaySeconds", (double) p.transientDecaySeconds);
        o->setProperty ("filterHighPass", p.filterHighPass);
        o->setProperty ("filterCutoffHz", (double) p.filterCutoffHz);
        o->setProperty ("filterResonance", (double) p.filterResonance);
        o->setProperty ("retrigger", p.retrigger == NovaVoiceRetrigger::Legato ? "legato" : "retrigger");
        o->setProperty ("glideMode", p.glideMode == NovaGlideMode::Always ? "always" : "legatoOnly");
        o->setProperty ("glideTimeSeconds", (double) p.glideTimeSeconds);
        return juce::var (o);
    }

    NovaBassVoiceParams voiceParamsFromVar (const juce::var& v, const NovaBassVoiceParams& fallback)
    {
        NovaBassVoiceParams p = fallback;
        p.sineTriBlend = (float) (double) v.getProperty ("sineTriBlend", (double) p.sineTriBlend);
        p.fineTuneCents = (float) (double) v.getProperty ("fineTuneCents", (double) p.fineTuneCents);
        p.pitchEnvAmountSemitones = (float) (double) v.getProperty ("pitchEnvAmountSemitones", (double) p.pitchEnvAmountSemitones);
        p.pitchEnvDecaySeconds = (float) (double) v.getProperty ("pitchEnvDecaySeconds", (double) p.pitchEnvDecaySeconds);
        p.pitchEnvCurve = (float) (double) v.getProperty ("pitchEnvCurve", (double) p.pitchEnvCurve);
        p.ampAttackSeconds = (float) (double) v.getProperty ("ampAttackSeconds", (double) p.ampAttackSeconds);
        p.ampDecaySeconds = (float) (double) v.getProperty ("ampDecaySeconds", (double) p.ampDecaySeconds);
        p.ampSustain = (float) (double) v.getProperty ("ampSustain", (double) p.ampSustain);
        p.ampReleaseSeconds = (float) (double) v.getProperty ("ampReleaseSeconds", (double) p.ampReleaseSeconds);
        p.transientAmount = (float) (double) v.getProperty ("transientAmount", (double) p.transientAmount);
        p.transientToneHz = (float) (double) v.getProperty ("transientToneHz", (double) p.transientToneHz);
        p.transientDecaySeconds = (float) (double) v.getProperty ("transientDecaySeconds", (double) p.transientDecaySeconds);
        p.filterHighPass = (bool) v.getProperty ("filterHighPass", p.filterHighPass);
        p.filterCutoffHz = (float) (double) v.getProperty ("filterCutoffHz", (double) p.filterCutoffHz);
        p.filterResonance = (float) (double) v.getProperty ("filterResonance", (double) p.filterResonance);
        p.retrigger = v.getProperty ("retrigger", "retrigger").toString() == "legato" ? NovaVoiceRetrigger::Legato : NovaVoiceRetrigger::Retrigger;
        p.glideMode = v.getProperty ("glideMode", "legatoOnly").toString() == "always" ? NovaGlideMode::Always : NovaGlideMode::LegatoOnly;
        p.glideTimeSeconds = (float) (double) v.getProperty ("glideTimeSeconds", (double) p.glideTimeSeconds);
        return p;
    }
}

// ── NovaBassLabMainComponent ─────────────────────────────────────────────

NovaBassLabMainComponent::NovaBassLabMainComponent()
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setSize (1280, 860);
    startTimerHz (15);
}

NovaBassLabMainComponent::~NovaBassLabMainComponent()
{
    stopTimer();
}

void NovaBassLabMainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF08080A));
}

void NovaBassLabMainComponent::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

juce::var NovaBassLabMainComponent::buildStatusVar()
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("ready", true);
    return juce::var (obj);
}

void NovaBassLabMainComponent::timerCallback()
{
    // Deliberately NOT diff-suppressed against lastPushedStatusJson: the
    // first tick fires (at startTimerHz(15)'s cadence) well before the
    // WebView has finished loading window.updateStatus, so that push is a
    // silent no-op - if the content never subsequently changes (as with a
    // constant {ready:true} payload), a diff-gated push would never retry
    // and the UI would show "Loading..." forever. Push every tick instead;
    // it's a tiny JSON blob at 15Hz, cheap enough not to matter, and once
    // this grows to carry live meter data (task 139) it changes every
    // tick anyway.
    const auto statusVar = buildStatusVar();
    const auto json = juce::JSON::toString (statusVar, true);
    lastPushedStatusJson = json;

    if (webView != nullptr)
        webView->evaluateJavascript ("window.updateStatus && window.updateStatus(" + json + ");", nullptr);
}

juce::WebBrowserComponent::Options NovaBassLabMainComponent::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaBassLab")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
                     .withNativeFunction ("noteOn", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty()) audioEngine.injectNoteFromUI ((int) args[0], true);
                         completion (juce::var());
                     })
                     .withNativeFunction ("noteOff", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty()) audioEngine.injectNoteFromUI ((int) args[0], false);
                         completion (juce::var());
                     })
                     .withNativeFunction ("getVoiceParams", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         completion (voiceParamsToVar (audioEngine.getVoiceParams()));
                     })
                     .withNativeFunction ("setVoiceParams", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             audioEngine.setVoiceParams (voiceParamsFromVar (args[0], audioEngine.getVoiceParams()));
                         completion (juce::var());
                     })
                     .withNativeFunction ("getColorState", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         auto* o = new juce::DynamicObject();
                         const auto mode = audioEngine.getColorMode();
                         o->setProperty ("mode", mode == NovaColorMode::Clean ? "clean" : mode == NovaColorMode::Gritty ? "gritty" : "warm");
                         o->setProperty ("drive", (double) audioEngine.getColorDrive());
                         completion (juce::var (o));
                     })
                     .withNativeFunction ("setColorMode", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                         {
                             const auto s = args[0].toString();
                             audioEngine.setColorMode (s == "clean" ? NovaColorMode::Clean : s == "gritty" ? NovaColorMode::Gritty : NovaColorMode::Warm);
                         }
                         completion (juce::var());
                     })
                     .withNativeFunction ("setColorDrive", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty()) audioEngine.setColorDrive ((float) (double) args[0]);
                         completion (juce::var());
                     })
                     .withNativeFunction ("getStereoParams", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         auto p = audioEngine.getStereoParams();
                         auto* o = new juce::DynamicObject();
                         o->setProperty ("widthAmount", (double) p.widthAmount);
                         o->setProperty ("crossoverHz", (double) p.crossoverHz);
                         completion (juce::var (o));
                     })
                     .withNativeFunction ("setStereoParams", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                         {
                             NovaBassStereoParams p = audioEngine.getStereoParams();
                             p.widthAmount = (float) (double) args[0].getProperty ("widthAmount", (double) p.widthAmount);
                             p.crossoverHz = (float) (double) args[0].getProperty ("crossoverHz", (double) p.crossoverHz);
                             audioEngine.setStereoParams (p);
                         }
                         completion (juce::var());
                     })
                     .withNativeFunction ("generate", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty()) audioEngine.generateNew (args[0].toString());
                         completion (juce::var());
                     })
                     .withNativeFunction ("mutate", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         auto intensity = NovaMutateIntensity::Medium;
                         if (! args.isEmpty())
                         {
                             const auto s = args[0].toString();
                             intensity = s == "subtle" ? NovaMutateIntensity::Subtle : s == "wild" ? NovaMutateIntensity::Wild : NovaMutateIntensity::Medium;
                         }
                         audioEngine.mutateCurrent (intensity);
                         completion (juce::var());
                     })
                     .withNativeFunction ("getGeneratorProfiles", [] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         juce::Array<juce::var> arr;
                         for (auto& p : NovaBassGenerator::getProfiles())
                         {
                             auto* o = new juce::DynamicObject();
                             o->setProperty ("id", p.id);
                             o->setProperty ("category", p.category);
                             arr.add (juce::var (o));
                         }
                         completion (juce::var (arr));
                     })
                     .withNativeFunction ("setLifecycleStatus", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                         {
                             const auto s = args[0].toString();
                             audioEngine.setCurrentLifecycleStatus (NovaBassCandidateIndex::statusFromString (s));
                         }
                         completion (juce::var());
                     })
                     .withNativeFunction ("getCandidateInfo", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         const auto id = audioEngine.getCurrentCandidateId();
                         if (id.isEmpty()) { completion (juce::var()); return; }
                         if (auto* entry = audioEngine.getCandidateIndex().find (id))
                             completion (candidateEntryToVar (*entry));
                         else
                             completion (juce::var());
                     })
                     .withNativeFunction ("render", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         NovaBassRenderOptions options;
                         if (! args.isEmpty())
                         {
                             options.rootNote = (int) args[0].getProperty ("rootNote", 36);
                             options.durationSeconds = (float) (double) args[0].getProperty ("durationSeconds", 1.0);
                             options.tailSeconds = (float) (double) args[0].getProperty ("tailSeconds", 1.0);
                         }
                         const auto fileName = audioEngine.renderCurrentToVault (options);
                         completion (juce::var (fileName));
                     })
                     .withNativeFunction ("getMacroTable", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         completion (macroTableToVar (audioEngine.getCurrentSound().macros));
                     })
                     .withNativeFunction ("setMacroTable", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty()) audioEngine.setMacroTable (macroTableFromVar (args[0]));
                         completion (juce::var());
                     })
                     .withNativeFunction ("saveAsPreset", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         bool ok = false;
                         if (! args.isEmpty())
                             ok = audioEngine.saveCurrentAsPreset (presetMetaFromVar (args[0]));
                         completion (juce::var (ok));
                     })
                     .withNativeFunction ("getPresetLibrary", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         auto& pm = audioEngine.getPresetManager();
                         juce::Array<juce::var> arr;
                         for (int i = 0; i < pm.getNumPresets(); ++i)
                         {
                             auto* preset = pm.getPreset (i);
                             auto* o = new juce::DynamicObject();
                             o->setProperty ("index", i);
                             o->setProperty ("name", preset->meta.name);
                             o->setProperty ("category", preset->meta.category);
                             arr.add (juce::var (o));
                         }
                         completion (juce::var (arr));
                     })
                     .withNativeFunction ("loadPresetIntoLab", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                         {
                             auto& pm = audioEngine.getPresetManager();
                             if (auto* preset = pm.getPreset ((int) args[0]))
                                 audioEngine.loadSound (*preset);
                         }
                         completion (juce::var());
                     })
                     .withNativeFunction ("jsError", [] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             juce::Logger::writeToLog ("[NovaBassLab][JS ERROR] " + args[0].toString());
                         completion (juce::var());
                     });

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaBassLabMainComponent::getResource (const juce::String& url)
{
    auto makeResource = [] (const void* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    const auto lowerUrl = url.toLowerCase();

    if (lowerUrl.contains ("juce/index.js"))
        return makeResource (nova_bass_lab_BinaryData::index_js2, nova_bass_lab_BinaryData::index_js2Size, "text/javascript");

    if (lowerUrl.contains ("juce/check_native_interop.js"))
        return makeResource (nova_bass_lab_BinaryData::check_native_interop_js, nova_bass_lab_BinaryData::check_native_interop_jsSize, "text/javascript");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_bass_lab_BinaryData::index_js, nova_bass_lab_BinaryData::index_jsSize, "text/javascript");

    if (lowerUrl.contains ("index.html") || lowerUrl.endsWith ("/") || lowerUrl.isEmpty())
        return makeResource (nova_bass_lab_BinaryData::index_html, nova_bass_lab_BinaryData::index_htmlSize, "text/html");

    return std::nullopt;
}

// ── NovaBassLabMainWindow ────────────────────────────────────────────────

NovaBassLabMainWindow::NovaBassLabMainWindow (juce::String name)
    : juce::DocumentWindow (std::move (name), juce::Colours::black, DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setContentOwned (new NovaBassLabMainComponent(), true);
    centreWithSize (getWidth(), getHeight());
    setResizable (true, true);
    setVisible (true);
}
