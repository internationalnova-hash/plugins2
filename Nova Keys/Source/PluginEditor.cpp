#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>
#include <iostream>
#include "NovaLicenseGate.h"

NovaKeysEditor::NovaKeysEditor (NovaKeysProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    setResizeLimits (1150, 740, 2200, 1440);
    setSize (1360, 800);
    startTimerHz (30); // fast enough for the virtual keyboard to feel live

    NovaLicenseOverlay::attachTo (*this);   // unlock screen until activated
}

NovaKeysEditor::~NovaKeysEditor()
{
    stopTimer();
}

void NovaKeysEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF07070A));
}

void NovaKeysEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

juce::var NovaKeysEditor::buildStatusVar()
{
    auto* obj = new juce::DynamicObject();

    obj->setProperty ("master", (double) *processorRef.apvts.getRawParameterValue ("master"));
    obj->setProperty ("tune", (double) *processorRef.apvts.getRawParameterValue ("tune"));
    obj->setProperty ("macroTone", (double) *processorRef.apvts.getRawParameterValue ("macroTone"));
    obj->setProperty ("macroMotion", (double) *processorRef.apvts.getRawParameterValue ("macroMotion"));
    obj->setProperty ("macroSpace", (double) *processorRef.apvts.getRawParameterValue ("macroSpace"));
    obj->setProperty ("macroCharacter", (double) *processorRef.apvts.getRawParameterValue ("macroCharacter"));

    juce::Array<juce::var> held;
    for (int n = 0; n < 128; ++n)
        if (processorRef.isNoteHeld (n))
            held.add (n);
    obj->setProperty ("heldNotes", juce::var (held));

    obj->setProperty ("midiActivity", processorRef.midiActivity.exchange (false));

    return juce::var (obj);
}

void NovaKeysEditor::timerCallback()
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

juce::var NovaKeysEditor::buildPresetVar (const NovaPreset& preset, int index)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("index", index);
    o->setProperty ("name", preset.meta.name);
    o->setProperty ("category", preset.meta.category);
    o->setProperty ("description", preset.meta.description);
    o->setProperty ("favorite", preset.meta.favorite);
    o->setProperty ("artworkRef", preset.meta.artworkRef);

    juce::Array<juce::var> tags;
    for (auto& t : preset.meta.tags) tags.add (t);
    o->setProperty ("tags", juce::var (tags));

    static const char* kLetters[4] = { "A", "B", "C", "D" };
    juce::Array<juce::var> layers;
    for (int i = 0; i < 4; ++i)
    {
        const auto& l = preset.layers[(size_t) i];
        auto* lo = new juce::DynamicObject();
        lo->setProperty ("letter", juce::String (kLetters[i]));
        lo->setProperty ("used", l.used);
        lo->setProperty ("name", l.displayName);
        lo->setProperty ("type", l.sourceType == NovaLayerSourceType::Sample ? "sample" : "synth");
        lo->setProperty ("on", l.layerParams.on);
        lo->setProperty ("volumeDb", (double) l.layerParams.volumeDb);
        lo->setProperty ("lowKey", l.layerParams.lowKey);
        lo->setProperty ("highKey", l.layerParams.highKey);
        lo->setProperty ("lowVelocity", l.layerParams.lowVelocity);
        lo->setProperty ("highVelocity", l.layerParams.highVelocity);
        layers.add (juce::var (lo));
    }
    o->setProperty ("layers", juce::var (layers));

    auto* fx = new juce::DynamicObject();
    fx->setProperty ("eqLowGainDb", (double) preset.effects.eqLowGainDb);
    fx->setProperty ("eqMidGainDb", (double) preset.effects.eqMidGainDb);
    fx->setProperty ("eqHighGainDb", (double) preset.effects.eqHighGainDb);
    fx->setProperty ("chorusRateHz", (double) preset.effects.chorusRateHz);
    fx->setProperty ("chorusMix", (double) preset.effects.chorusMix);
    fx->setProperty ("delayTimeMs", (double) preset.effects.delayTimeMs);
    fx->setProperty ("delayMix", (double) preset.effects.delayMix);
    fx->setProperty ("reverbRoomSize", (double) preset.effects.reverbRoomSize);
    fx->setProperty ("reverbMix", (double) preset.effects.reverbMix);
    fx->setProperty ("compRatio", (double) preset.effects.compRatio);
    fx->setProperty ("compThresholdDb", (double) preset.effects.compThresholdDb);
    o->setProperty ("fx", juce::var (fx));

    auto* macros = new juce::DynamicObject();
    macros->setProperty ("tone", (double) preset.macros.tone);
    macros->setProperty ("motion", (double) preset.macros.motion);
    macros->setProperty ("space", (double) preset.macros.space);
    macros->setProperty ("character", (double) preset.macros.character);
    o->setProperty ("macros", juce::var (macros));

    return juce::var (o);
}

juce::var NovaKeysEditor::buildLibraryVar()
{
    auto* root = new juce::DynamicObject();

    auto& vault = processorRef.getVault();
    root->setProperty ("vaultFound", vault.isVaultFound());
    root->setProperty ("vaultPath", vault.getVaultPath().getFullPathName());
    root->setProperty ("currentIndex", processorRef.getCurrentPresetIndex());

    auto& pm = processorRef.getPresetManager();
    juce::Array<juce::var> presets;
    for (int i = 0; i < pm.getNumPresets(); ++i)
        presets.add (buildPresetVar (*pm.getPreset (i), i));
    root->setProperty ("presets", juce::var (presets));

    juce::Array<juce::var> cats;
    for (auto& c : pm.getCategories()) cats.add (c);
    root->setProperty ("categories", juce::var (cats));

    return juce::var (root);
}

juce::WebBrowserComponent::Options NovaKeysEditor::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaKeys")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
                     .withNativeFunction ("setParam", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (args.size() >= 2)
                         {
                             const auto paramId = args[0].toString();
                             if (auto* param = processorRef.apvts.getParameter (paramId))
                             {
                                 if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (param))
                                     param->setValueNotifyingHost (pf->convertTo0to1 ((float) (double) args[1]));
                                 else if (auto* pi = dynamic_cast<juce::AudioParameterInt*> (param))
                                     param->setValueNotifyingHost (pi->convertTo0to1 ((int) args[1]));
                                 else if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (param))
                                     param->setValueNotifyingHost (pc->convertTo0to1 ((int) args[1]));

                                 if (paramId.startsWith ("macro"))
                                     processorRef.refreshMacrosFromApvts();
                             }
                         }
                         completion (juce::var());
                     })
                     .withNativeFunction ("getLibrary", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         completion (buildLibraryVar());
                     })
                     .withNativeFunction ("selectPreset", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                         {
                             const int index = (int) args[0];
                             if (processorRef.loadPresetByIndex (index))
                             {
                                 processorRef.getPresetManager().setSelectedIndex (index);
                                 if (auto* preset = processorRef.getPresetManager().getPreset (index))
                                 {
                                     completion (buildPresetVar (*preset, index));
                                     return;
                                 }
                             }
                         }
                         completion (juce::var());
                     })
                     .withNativeFunction ("nextPreset", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         auto& pm = processorRef.getPresetManager();
                         pm.selectNext();
                         const int idx = pm.getSelectedIndex();
                         if (processorRef.loadPresetByIndex (idx))
                             if (auto* preset = pm.getPreset (idx)) { completion (buildPresetVar (*preset, idx)); return; }
                         completion (juce::var());
                     })
                     .withNativeFunction ("previousPreset", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         auto& pm = processorRef.getPresetManager();
                         pm.selectPrevious();
                         const int idx = pm.getSelectedIndex();
                         if (processorRef.loadPresetByIndex (idx))
                             if (auto* preset = pm.getPreset (idx)) { completion (buildPresetVar (*preset, idx)); return; }
                         completion (juce::var());
                     })
                     .withNativeFunction ("toggleFavorite", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                         {
                             auto& pm = processorRef.getPresetManager();
                             const int index = (int) args[0];
                             if (auto* preset = pm.getPreset (index))
                             {
                                 const bool newFavorite = ! preset->meta.favorite;
                                 pm.setFavorite (index, newFavorite);
                                 completion (juce::var (newFavorite));
                                 return;
                             }
                         }
                         completion (juce::var (false));
                     })
                     .withNativeFunction ("locateLibrary", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         vaultFileChooser = std::make_unique<juce::FileChooser> ("Locate Nova Sound Vault", juce::File(), "*");
                         vaultFileChooser->launchAsync (
                             juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                             [this, completion] (const juce::FileChooser& fc)
                             {
                                 auto dir = fc.getResult();
                                 if (dir != juce::File())
                                     processorRef.relocateVault (dir);
                                 completion (buildLibraryVar());
                             });
                     })
                     .withNativeFunction ("noteOn", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             processorRef.injectNoteFromUI ((int) args[0], true);
                         completion (juce::var());
                     })
                     .withNativeFunction ("noteOff", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             processorRef.injectNoteFromUI ((int) args[0], false);
                         completion (juce::var());
                     })
                     .withNativeFunction ("jsError", [] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             std::cerr << "[NovaKeys][JS ERROR] " << args[0].toString() << std::endl;
                         completion (juce::var());
                     });

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaKeysEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const void* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    // Match by substring on the raw url - the proven pattern used across
    // this catalog's WebView plugins (Nova Chord, Nova Sub). Deliberately
    // NOT using getResourceProviderRoot()-stripping, which was found to
    // silently break all JS loading in Nova AI Master.
    const auto lowerUrl = url.toLowerCase();

    if (lowerUrl.contains ("juce/index.js"))
        return makeResource (nova_keys_BinaryData::index_js2, nova_keys_BinaryData::index_js2Size, "text/javascript");

    if (lowerUrl.contains ("juce/check_native_interop.js"))
        return makeResource (nova_keys_BinaryData::check_native_interop_js, nova_keys_BinaryData::check_native_interop_jsSize, "text/javascript");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_keys_BinaryData::index_js, nova_keys_BinaryData::index_jsSize, "text/javascript");

    if (lowerUrl.contains ("hero-pianos.jpg"))
        return makeResource (nova_keys_BinaryData::heropianos_jpg, nova_keys_BinaryData::heropianos_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-strings.jpg"))
        return makeResource (nova_keys_BinaryData::herostrings_jpg, nova_keys_BinaryData::herostrings_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-brass.jpg"))
        return makeResource (nova_keys_BinaryData::herobrass_jpg, nova_keys_BinaryData::herobrass_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-woodwinds.jpg"))
        return makeResource (nova_keys_BinaryData::herowoodwinds_jpg, nova_keys_BinaryData::herowoodwinds_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-mallets.jpg"))
        return makeResource (nova_keys_BinaryData::heromallets_jpg, nova_keys_BinaryData::heromallets_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-percussion.jpg"))
        return makeResource (nova_keys_BinaryData::heropercussion_jpg, nova_keys_BinaryData::heropercussion_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-choirs.jpg"))
        return makeResource (nova_keys_BinaryData::herochoirs_jpg, nova_keys_BinaryData::herochoirs_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-electric_pianos.jpg"))
        return makeResource (nova_keys_BinaryData::heroelectric_pianos_jpg, nova_keys_BinaryData::heroelectric_pianos_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-organs.jpg"))
        return makeResource (nova_keys_BinaryData::heroorgans_jpg, nova_keys_BinaryData::heroorgans_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-guitars.jpg"))
        return makeResource (nova_keys_BinaryData::heroguitars_jpg, nova_keys_BinaryData::heroguitars_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-bass.jpg"))
        return makeResource (nova_keys_BinaryData::herobass_jpg, nova_keys_BinaryData::herobass_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-synth_bass.jpg"))
        return makeResource (nova_keys_BinaryData::herosynth_bass_jpg, nova_keys_BinaryData::herosynth_bass_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-leads.jpg"))
        return makeResource (nova_keys_BinaryData::heroleads_jpg, nova_keys_BinaryData::heroleads_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-pads.jpg"))
        return makeResource (nova_keys_BinaryData::heropads_jpg, nova_keys_BinaryData::heropads_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-textures.jpg"))
        return makeResource (nova_keys_BinaryData::herotextures_jpg, nova_keys_BinaryData::herotextures_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-plucks.jpg"))
        return makeResource (nova_keys_BinaryData::heroplucks_jpg, nova_keys_BinaryData::heroplucks_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-arps.jpg"))
        return makeResource (nova_keys_BinaryData::heroarps_jpg, nova_keys_BinaryData::heroarps_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-cinematic.jpg"))
        return makeResource (nova_keys_BinaryData::herocinematic_jpg, nova_keys_BinaryData::herocinematic_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-world.jpg"))
        return makeResource (nova_keys_BinaryData::heroworld_jpg, nova_keys_BinaryData::heroworld_jpgSize, "image/jpeg");
    if (lowerUrl.contains ("hero-nova_signature.jpg"))
        return makeResource (nova_keys_BinaryData::heronova_signature_jpg, nova_keys_BinaryData::heronova_signature_jpgSize, "image/jpeg");

    if (lowerUrl.contains ("index.html") || lowerUrl.endsWith ("/") || lowerUrl.isEmpty())
        return makeResource (nova_keys_BinaryData::index_html, nova_keys_BinaryData::index_htmlSize, "text/html");

    return std::nullopt;
}
