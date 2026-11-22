#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaBendAudioProcessorEditor::NovaBendAudioProcessorEditor (NovaBendAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    pitchAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::pitch), pitchRelay, nullptr);
    formantAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::formant), formantRelay, nullptr);
    flattenAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::flatten), flattenRelay, nullptr);
    flattenNoteAttachment = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::flattenNote), flattenNoteRelay, nullptr);
    flattenOctaveAttachment = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::flattenOctave), flattenOctaveRelay, nullptr);
    linkAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::link), linkRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::mix), mixRelay, nullptr);
    qualityAttachment = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::quality), qualityRelay, nullptr);
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::bypass), bypassRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + assetVersion;
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    // Design canvas is 1080x720 (3:2) - open below 100% of it, matching the
    // catalog's "never open at full design-canvas size" lesson.
    setResizeLimits (648, 432, 1620, 1080);
    setSize (864, 576);
    // 15Hz, not 30 - see timerCallback()'s comment: this and the WKWebView
    // JS evaluation it triggers run on the message thread, in the same
    // process as the real-time audio thread; halving the rate halves how
    // often that contention can happen. Still smooth enough for a simple
    // level-meter-style waveform.
    startTimerHz (15);
}

NovaBendAudioProcessorEditor::~NovaBendAudioProcessorEditor()
{
    stopTimer();
}

void NovaBendAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (9, 10, 11));
}

void NovaBendAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaBendAudioProcessorEditor::timerCallback()
{
    // isShowing() (not isVisible()) - isVisible() only reflects the JUCE
    // component tree's own flag, which can stay true even when a host has
    // minimized/obscured the actual window (confirmed directly: the user
    // still heard reduced-but-present clicking with the plugin window
    // "closed or out of the way" in FL Studio, not fully gone) - isShowing()
    // walks the whole ancestor chain including whether there's a real,
    // currently-visible native peer, so it correctly goes false in that
    // case too.
    if (webView == nullptr || ! webView->isShowing())
        return;

    // Real (not decorative) waveform telemetry - spec explicitly forbids
    // fake/random animation pretending to respond to audio. Kept
    // deliberately cheap: this runs on the message thread, in the same
    // process as the real-time audio thread, several times a second - the
    // user directly confirmed audibly worse clicking specifically while
    // the plugin's UI window was open/visible in FL Studio versus closed,
    // pointing at WKWebView JS evaluation (and whatever repaint/compositing
    // it triggers) contending with the audio thread's own scheduling, on
    // top of whatever the DSP itself contributes. 2 decimals is still
    // visually indistinguishable for a level-meter-style waveform and
    // roughly halves the string this builds every tick.
    juce::String samplesJs;
    samplesJs.preallocateBytes ((size_t) NovaBendAudioProcessor::kLevelHistorySize * 8);
    samplesJs << "[";
    for (int i = 0; i < NovaBendAudioProcessor::kLevelHistorySize; ++i)
    {
        if (i > 0) samplesJs << ",";
        samplesJs << juce::String (processorRef.getLevelHistoryAt (i), 2);
    }
    samplesJs << "]";

    const auto js = "if (window.updateWaveform) { window.updateWaveform(" + samplesJs + "); }";
    webView->evaluateJavascript (js);
}

juce::WebBrowserComponent::Options NovaBendAudioProcessorEditor::createWebOptions (NovaBendAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaBend")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withNativeFunction ("listPresets", [&editor] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         juce::Array<juce::var> arr;
                         for (int i = 0; i < editor.processorRef.presetManager.getNumPresets(); ++i)
                         {
                             const auto& preset = editor.processorRef.presetManager.getPreset (i);
                             auto* o = new juce::DynamicObject();
                             o->setProperty ("index", i);
                             o->setProperty ("id", preset.id);
                             o->setProperty ("name", preset.displayName);
                             o->setProperty ("category", preset.category);
                             arr.add (juce::var (o));
                         }
                         completion (juce::var (arr));
                     })
                     .withNativeFunction ("getCurrentPresetIndex", [&editor] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         completion (juce::var (editor.processorRef.getCurrentPresetIndex()));
                     })
                     .withNativeFunction ("loadPreset", [&editor] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             editor.processorRef.loadPreset ((int) args[0]);
                         completion (juce::var (editor.processorRef.getCurrentPresetIndex()));
                     })
                     .withNativeFunction ("loadNextPreset", [&editor] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         editor.processorRef.loadNextPreset();
                         completion (juce::var (editor.processorRef.getCurrentPresetIndex()));
                     })
                     .withNativeFunction ("loadPreviousPreset", [&editor] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         editor.processorRef.loadPreviousPreset();
                         completion (juce::var (editor.processorRef.getCurrentPresetIndex()));
                     })
                     .withNativeFunction ("savePreset", [&editor] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         const auto name = args.isEmpty() ? juce::String() : args[0].toString();
                         completion (juce::var (editor.processorRef.saveCurrentAsPreset (name)));
                     })
                     .withOptionsFrom (editor.pitchRelay)
                     .withOptionsFrom (editor.formantRelay)
                     .withOptionsFrom (editor.flattenRelay)
                     .withOptionsFrom (editor.flattenNoteRelay)
                     .withOptionsFrom (editor.flattenOctaveRelay)
                     .withOptionsFrom (editor.linkRelay)
                     .withOptionsFrom (editor.mixRelay)
                     .withOptionsFrom (editor.qualityRelay)
                     .withOptionsFrom (editor.bypassRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaBendAudioProcessorEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    struct ImageEntry { const char* suffix; const char* data; int size; const char* mime; };
    static const ImageEntry images[] = {
        { "/img/n_logo.png",                         nova_bend_BinaryData::n_logo_png,                         nova_bend_BinaryData::n_logo_pngSize, "image/png" },
        { "/img/knob_pitch_black_gold.png",          nova_bend_BinaryData::knob_pitch_black_gold_png,          nova_bend_BinaryData::knob_pitch_black_gold_pngSize, "image/png" },
        { "/img/knob_pitch_outer_ring.png",          nova_bend_BinaryData::knob_pitch_outer_ring_png,          nova_bend_BinaryData::knob_pitch_outer_ring_pngSize, "image/png" },
        { "/img/pitch_ring_active.png",              nova_bend_BinaryData::pitch_ring_active_png,              nova_bend_BinaryData::pitch_ring_active_pngSize, "image/png" },
        { "/img/knob_formant_graphite_champagne.png",nova_bend_BinaryData::knob_formant_graphite_champagne_png,nova_bend_BinaryData::knob_formant_graphite_champagne_pngSize, "image/png" },
        { "/img/knob_formant_outer_ring.png",        nova_bend_BinaryData::knob_formant_outer_ring_png,        nova_bend_BinaryData::knob_formant_outer_ring_pngSize, "image/png" },
        { "/img/formant_ring_active.png",            nova_bend_BinaryData::formant_ring_active_png,            nova_bend_BinaryData::formant_ring_active_pngSize, "image/png" },
        { "/img/knob_mix_small.png",                 nova_bend_BinaryData::knob_mix_small_png,                 nova_bend_BinaryData::knob_mix_small_pngSize, "image/png" },
        { "/img/utility_button_inactive.png",        nova_bend_BinaryData::utility_button_inactive_png,        nova_bend_BinaryData::utility_button_inactive_pngSize, "image/png" },
        { "/img/utility_button_hover.png",           nova_bend_BinaryData::utility_button_hover_png,           nova_bend_BinaryData::utility_button_hover_pngSize, "image/png" },
        { "/img/utility_button_active.png",          nova_bend_BinaryData::utility_button_active_png,          nova_bend_BinaryData::utility_button_active_pngSize, "image/png" },
        { "/img/utility_button_pressed.png",         nova_bend_BinaryData::utility_button_pressed_png,         nova_bend_BinaryData::utility_button_pressed_pngSize, "image/png" },
        { "/img/note_dropdown_inactive.png",         nova_bend_BinaryData::note_dropdown_inactive_png,         nova_bend_BinaryData::note_dropdown_inactive_pngSize, "image/png" },
        { "/img/note_dropdown_hover.png",            nova_bend_BinaryData::note_dropdown_hover_png,            nova_bend_BinaryData::note_dropdown_hover_pngSize, "image/png" },
        { "/img/note_dropdown_active.png",           nova_bend_BinaryData::note_dropdown_active_png,           nova_bend_BinaryData::note_dropdown_active_pngSize, "image/png" },
        { "/img/octave_dropdown_inactive.png",       nova_bend_BinaryData::octave_dropdown_inactive_png,       nova_bend_BinaryData::octave_dropdown_inactive_pngSize, "image/png" },
        { "/img/octave_dropdown_hover.png",          nova_bend_BinaryData::octave_dropdown_hover_png,          nova_bend_BinaryData::octave_dropdown_hover_pngSize, "image/png" },
        { "/img/octave_dropdown_active.png",         nova_bend_BinaryData::octave_dropdown_active_png,         nova_bend_BinaryData::octave_dropdown_active_pngSize, "image/png" },
        { "/img/preset_selector_inactive.png",       nova_bend_BinaryData::preset_selector_inactive_png,       nova_bend_BinaryData::preset_selector_inactive_pngSize, "image/png" },
        { "/img/preset_selector_hover.png",          nova_bend_BinaryData::preset_selector_hover_png,          nova_bend_BinaryData::preset_selector_hover_pngSize, "image/png" },
        { "/img/preset_selector_active.png",         nova_bend_BinaryData::preset_selector_active_png,         nova_bend_BinaryData::preset_selector_active_pngSize, "image/png" },
        { "/img/icon_button_inactive.png",           nova_bend_BinaryData::icon_button_inactive_png,           nova_bend_BinaryData::icon_button_inactive_pngSize, "image/png" },
        { "/img/icon_button_hover.png",              nova_bend_BinaryData::icon_button_hover_png,              nova_bend_BinaryData::icon_button_hover_pngSize, "image/png" },
        { "/img/icon_button_active.png",             nova_bend_BinaryData::icon_button_active_png,             nova_bend_BinaryData::icon_button_active_pngSize, "image/png" },
        { "/img/nova_n_emblem.png",                  nova_bend_BinaryData::nova_n_emblem_png,                  nova_bend_BinaryData::nova_n_emblem_pngSize, "image/png" },
        { "/img/nova_emblem_gold_glow.png",          nova_bend_BinaryData::nova_emblem_gold_glow_png,          nova_bend_BinaryData::nova_emblem_gold_glow_pngSize, "image/png" },
        { "/img/main_panel_aged_graphite.png",       nova_bend_BinaryData::main_panel_aged_graphite_png,       nova_bend_BinaryData::main_panel_aged_graphite_pngSize, "image/png" },
        { "/img/lower_utility_panel.png",            nova_bend_BinaryData::lower_utility_panel_png,            nova_bend_BinaryData::lower_utility_panel_pngSize, "image/png" },
        { "/img/screw_brass.svg",                    nova_bend_BinaryData::screw_brass_svg,                    nova_bend_BinaryData::screw_brass_svgSize, "image/svg+xml" },
        { "/img/chevron_down.svg",                   nova_bend_BinaryData::chevron_down_svg,                   nova_bend_BinaryData::chevron_down_svgSize, "image/svg+xml" },
        { "/img/arrow_next.svg",                     nova_bend_BinaryData::arrow_next_svg,                     nova_bend_BinaryData::arrow_next_svgSize, "image/svg+xml" },
        { "/img/arrow_previous.svg",                 nova_bend_BinaryData::arrow_previous_svg,                 nova_bend_BinaryData::arrow_previous_svgSize, "image/svg+xml" },
        { "/img/save_icon.svg",                      nova_bend_BinaryData::save_icon_svg,                      nova_bend_BinaryData::save_icon_svgSize, "image/svg+xml" },
        { "/img/settings_icon.svg",                  nova_bend_BinaryData::settings_icon_svg,                  nova_bend_BinaryData::settings_icon_svgSize, "image/svg+xml" },
        { "/img/corner_bracket.svg",                 nova_bend_BinaryData::corner_bracket_svg,                 nova_bend_BinaryData::corner_bracket_svgSize, "image/svg+xml" },
        { "/img/divider_gold.svg",                   nova_bend_BinaryData::divider_gold_svg,                   nova_bend_BinaryData::divider_gold_svgSize, "image/svg+xml" },
    };

    // WKWebView persists its resource cache across app launches even for
    // our custom scheme. index.html's own top-level URL is cache-busted
    // already, but every URL it references internally - the JS chain AND
    // every img/ reference - needs the same treatment, or WKWebView can go
    // on serving a stale cached copy of that sub-resource across launches
    // even after a full rebuild. See Nova Glue/Nova Ride's PluginEditor.cpp
    // for the fuller writeup of this bug - built in here from day one
    // rather than discovered later.
    auto makeVersionedHtmlResource = [this] (const char* data, int size, const char* mime)
    {
        auto text = juce::String::fromUTF8 (data, size);
        text = text.replace ("js/index.js", "js/index.js?v=" + assetVersion);
        for (const auto& img : images)
        {
            const juce::String ref (img.suffix + 1); // strip leading '/'
            text = text.replace (ref, ref + "?v=" + assetVersion);
        }
        const auto utf8 = text.toUTF8();
        std::vector<std::byte> bytes (utf8.sizeInBytes() - 1);
        std::memcpy (bytes.data(), utf8.getAddress(), bytes.size());
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    auto makeVersionedTextResource = [this] (const char* data, int size, const char* mime, const char* findText)
    {
        auto text = juce::String::fromUTF8 (data, size);
        text = text.replace (findText, juce::String (findText) + "?v=" + assetVersion);
        const auto utf8 = text.toUTF8();
        std::vector<std::byte> bytes (utf8.sizeInBytes() - 1);
        std::memcpy (bytes.data(), utf8.getAddress(), bytes.size());
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    // NOTE: requests arrive as scheme/host-less strings like "//js/index.js"
    // rather than an absolute URL under getResourceProviderRoot() -
    // stripping that prefix never matches anything, so match on a simple
    // query-stripped suffix of the raw url instead.
    const auto pathOnly = url.upToFirstOccurrenceOf ("?", false, false).toLowerCase();
    const bool isRoot = pathOnly.isEmpty() || pathOnly == "/" || pathOnly == "//";

    constexpr const char* htmlMime = "text/html; charset=utf-8";
    constexpr const char* jsMime   = "text/javascript; charset=utf-8";

    if (isRoot || pathOnly.endsWith ("/index.html"))
        return makeVersionedHtmlResource (nova_bend_BinaryData::index_html, nova_bend_BinaryData::index_htmlSize, htmlMime);
    if (pathOnly.endsWith ("/js/index.js"))
        return makeVersionedTextResource (nova_bend_BinaryData::index_js, nova_bend_BinaryData::index_jsSize,
                                          jsMime, "./juce/index.js");
    if (pathOnly.endsWith ("/js/juce/index.js"))
        return makeResource (nova_bend_BinaryData::index_js2, nova_bend_BinaryData::index_js2Size, jsMime);
    if (pathOnly.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_bend_BinaryData::check_native_interop_js, nova_bend_BinaryData::check_native_interop_jsSize, jsMime);

    for (const auto& img : images)
        if (pathOnly.endsWith (img.suffix))
            return makeResource (img.data, img.size, img.mime);

    return std::nullopt;
}
