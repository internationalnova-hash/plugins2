#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaPunchAudioProcessorEditor::NovaPunchAudioProcessorEditor (NovaPunchAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    punchAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::punch), punchRelay, nullptr);
    bodyAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::body), bodyRelay, nullptr);
    focusAttachment = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::focus), focusRelay, nullptr);
    sensitivityAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::sensitivity), sensitivityRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::mix), mixRelay, nullptr);
    clipSafeAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::clipSafe), clipSafeRelay, nullptr);
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::bypass), bypassRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + assetVersion;
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    // Design canvas is 1536x1024 (3:2) - open below 100% of it, matching
    // the catalog's "never open at full design-canvas size" lesson.
    setResizeLimits (768, 512, 2304, 1536);
    // Height carries extra headroom beyond an exact 2/3-scale fit (which
    // would be 683, i.e. 1024*2/3) - the footer/underlight elements
    // deliberately extend below the nominal 1024px canvas edge (a design
    // "bleed" for the panel's own bottom lip + backlight glow), and
    // fitStage()'s width-constrained scale means any extra window height
    // becomes symmetric top/bottom letterboxing around #app rather than
    // changing the scale itself - that letterboxing is what gives the
    // bleed room to render instead of being clipped by the window's own
    // bounds. Measured directly: without this, the underlight glow's
    // computed bounding rect sat past the visible viewport (y=686 in a
    // 683-tall view) and never appeared no matter how its CSS was tuned.
    setSize (1024, 720);
    // 20Hz, not higher - see timerCallback()'s comment: this and the
    // WKWebView JS evaluation it triggers run on the message thread, in
    // the same process as the real-time audio thread (a lesson learned
    // directly on Nova Bend: the UI window being open/active measurably
    // worsened audible artifacts there when this ran at 30Hz with a
    // heavier per-tick payload).
    startTimerHz (20);
}

NovaPunchAudioProcessorEditor::~NovaPunchAudioProcessorEditor()
{
    stopTimer();
}

void NovaPunchAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (9, 10, 11));
}

void NovaPunchAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaPunchAudioProcessorEditor::timerCallback()
{
    // isShowing() (not isVisible()) - see Nova Bend's own writeup: a host
    // can minimize/obscure a window without JUCE's isVisible() flag going
    // false, but isShowing() walks the whole ancestor chain including
    // whether there's a real, currently-visible native peer.
    if (webView == nullptr || ! webView->isShowing())
        return;

    // Real (not decorative/fake) telemetry throughout - spec explicitly
    // forbids fabricated analysis visuals. Kept deliberately cheap: one
    // compact JSON object per tick, 2-decimal precision, preallocated.
    juce::String json;
    json.preallocateBytes (1024);
    json << "{"
         << "\"transientProbability\":" << juce::String (processorRef.getTransientProbability(), 2) << ","
         << "\"transientStrength\":" << juce::String (processorRef.getTransientStrength(), 2) << ","
         << "\"attackGainDb\":" << juce::String (processorRef.getAttackGainDb(), 2) << ","
         << "\"decayGainDb\":" << juce::String (processorRef.getDecayGainDb(), 2) << ","
         << "\"sustainGainDb\":" << juce::String (processorRef.getSustainGainDb(), 2) << ","
         << "\"lowActivity\":" << juce::String (processorRef.getLowActivity(), 2) << ","
         << "\"midActivity\":" << juce::String (processorRef.getMidActivity(), 2) << ","
         << "\"highActivity\":" << juce::String (processorRef.getHighActivity(), 2) << ","
         << "\"inputPeakL\":" << juce::String (processorRef.getInputPeakL(), 3) << ","
         << "\"inputPeakR\":" << juce::String (processorRef.getInputPeakR(), 3) << ","
         << "\"outputPeakL\":" << juce::String (processorRef.getOutputPeakL(), 3) << ","
         << "\"outputPeakR\":" << juce::String (processorRef.getOutputPeakR(), 3) << ","
         << "\"region\":" << processorRef.getCurrentRegion() << ","
         << "\"punchHistory\":[";
    for (int i = 0; i < NovaPunchAudioProcessor::kActivityHistorySize; ++i)
    {
        if (i > 0) json << ",";
        json << juce::String (processorRef.getPunchActivityHistoryAt (i), 2);
    }
    json << "],\"bodyHistory\":[";
    for (int i = 0; i < NovaPunchAudioProcessor::kActivityHistorySize; ++i)
    {
        if (i > 0) json << ",";
        json << juce::String (processorRef.getBodyActivityHistoryAt (i), 2);
    }
    json << "]}";

    const auto js = "if (window.updateTelemetry) { window.updateTelemetry(" + json + "); }";
    webView->evaluateJavascript (js);
}

juce::WebBrowserComponent::Options NovaPunchAudioProcessorEditor::createWebOptions (NovaPunchAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaPunch")));
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
                     .withOptionsFrom (editor.punchRelay)
                     .withOptionsFrom (editor.bodyRelay)
                     .withOptionsFrom (editor.focusRelay)
                     .withOptionsFrom (editor.sensitivityRelay)
                     .withOptionsFrom (editor.mixRelay)
                     .withOptionsFrom (editor.clipSafeRelay)
                     .withOptionsFrom (editor.bypassRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaPunchAudioProcessorEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    struct ImageEntry { const char* suffix; const char* data; int size; const char* mime; };
    static const ImageEntry images[] = {
        { "/img/button_clip_safe_off@2x.png",              nova_punch_BinaryData::button_clip_safe_off2x_png,              nova_punch_BinaryData::button_clip_safe_off2x_pngSize, "image/png" },
        { "/img/button_clip_safe_on@2x.png",               nova_punch_BinaryData::button_clip_safe_on2x_png,               nova_punch_BinaryData::button_clip_safe_on2x_pngSize, "image/png" },
        { "/img/button_focus_active@2x.png",               nova_punch_BinaryData::button_focus_active2x_png,               nova_punch_BinaryData::button_focus_active2x_pngSize, "image/png" },
        { "/img/button_focus_hover@2x.png",                nova_punch_BinaryData::button_focus_hover2x_png,                nova_punch_BinaryData::button_focus_hover2x_pngSize, "image/png" },
        { "/img/button_focus_inactive@2x.png",             nova_punch_BinaryData::button_focus_inactive2x_png,             nova_punch_BinaryData::button_focus_inactive2x_pngSize, "image/png" },
        { "/img/button_focus_pressed@2x.png",              nova_punch_BinaryData::button_focus_pressed2x_png,              nova_punch_BinaryData::button_focus_pressed2x_pngSize, "image/png" },
        { "/img/button_tab_active@2x.png",                 nova_punch_BinaryData::button_tab_active2x_png,                 nova_punch_BinaryData::button_tab_active2x_pngSize, "image/png" },
        { "/img/button_tab_hover@2x.png",                  nova_punch_BinaryData::button_tab_hover2x_png,                  nova_punch_BinaryData::button_tab_hover2x_pngSize, "image/png" },
        { "/img/button_tab_inactive@2x.png",               nova_punch_BinaryData::button_tab_inactive2x_png,               nova_punch_BinaryData::button_tab_inactive2x_pngSize, "image/png" },
        { "/img/button_tab_pressed@2x.png",                nova_punch_BinaryData::button_tab_pressed2x_png,                nova_punch_BinaryData::button_tab_pressed2x_pngSize, "image/png" },
        { "/img/display_processing_activity_frame_grid@2x.png", nova_punch_BinaryData::display_processing_activity_frame_grid2x_png, nova_punch_BinaryData::display_processing_activity_frame_grid2x_pngSize, "image/png" },
        { "/img/display_transient_intelligence_frame_grid@2x.png", nova_punch_BinaryData::display_transient_intelligence_frame_grid2x_png, nova_punch_BinaryData::display_transient_intelligence_frame_grid2x_pngSize, "image/png" },
        { "/img/footer_frame@2x.png",                      nova_punch_BinaryData::footer_frame2x_png,                      nova_punch_BinaryData::footer_frame2x_pngSize, "image/png" },
        { "/img/header_icon_button_shell@2x.png",          nova_punch_BinaryData::header_icon_button_shell2x_png,          nova_punch_BinaryData::header_icon_button_shell2x_pngSize, "image/png" },
        { "/img/indicator_lamp_off@2x.png",                nova_punch_BinaryData::indicator_lamp_off2x_png,                nova_punch_BinaryData::indicator_lamp_off2x_pngSize, "image/png" },
        { "/img/indicator_lamp_on@2x.png",                 nova_punch_BinaryData::indicator_lamp_on2x_png,                 nova_punch_BinaryData::indicator_lamp_on2x_pngSize, "image/png" },
        { "/img/knob_punch_physical@2x.png",               nova_punch_BinaryData::knob_punch_physical2x_png,               nova_punch_BinaryData::knob_punch_physical2x_pngSize, "image/png" },
        { "/img/knob_body_physical@2x.png",                nova_punch_BinaryData::knob_body_physical2x_png,                nova_punch_BinaryData::knob_body_physical2x_pngSize, "image/png" },
        { "/img/knob_mix_physical@2x.png",                 nova_punch_BinaryData::knob_mix_physical2x_png,                 nova_punch_BinaryData::knob_mix_physical2x_pngSize, "image/png" },
        { "/img/knob_sensitivity_physical@2x.png",         nova_punch_BinaryData::knob_sensitivity_physical2x_png,         nova_punch_BinaryData::knob_sensitivity_physical2x_pngSize, "image/png" },
        { "/img/meter_in_housing_background@2x.png",       nova_punch_BinaryData::meter_in_housing_background2x_png,       nova_punch_BinaryData::meter_in_housing_background2x_pngSize, "image/png" },
        { "/img/meter_out_housing_background@2x.png",      nova_punch_BinaryData::meter_out_housing_background2x_png,      nova_punch_BinaryData::meter_out_housing_background2x_pngSize, "image/png" },
        { "/img/texture_graphite_lower@2x.png",             nova_punch_BinaryData::texture_graphite_lower2x_png,            nova_punch_BinaryData::texture_graphite_lower2x_pngSize, "image/png" },
        { "/img/texture_titanium_main@2x.png",              nova_punch_BinaryData::texture_titanium_main2x_png,             nova_punch_BinaryData::texture_titanium_main2x_pngSize, "image/png" },
        { "/img/texture_titanium_mockup_swatch.png",        nova_punch_BinaryData::texture_titanium_mockup_swatch_png,      nova_punch_BinaryData::texture_titanium_mockup_swatch_pngSize, "image/png" },
        { "/img/module_separator_bevel@2x.png",             nova_punch_BinaryData::module_separator_bevel2x_png,            nova_punch_BinaryData::module_separator_bevel2x_pngSize, "image/png" },
        { "/img/texture_graphite_top@2x.png",               nova_punch_BinaryData::texture_graphite_top2x_png,              nova_punch_BinaryData::texture_graphite_top2x_pngSize, "image/png" },
        { "/img/preset_selector_shell@2x.png",              nova_punch_BinaryData::preset_selector_shell2x_png,             nova_punch_BinaryData::preset_selector_shell2x_pngSize, "image/png" },
        { "/img/rack_edge_left@2x.png",                     nova_punch_BinaryData::rack_edge_left2x_png,                    nova_punch_BinaryData::rack_edge_left2x_pngSize, "image/png" },
        { "/img/rack_edge_right@2x.png",                    nova_punch_BinaryData::rack_edge_right2x_png,                   nova_punch_BinaryData::rack_edge_right2x_pngSize, "image/png" },
        { "/img/accent_divider.svg",                        nova_punch_BinaryData::accent_divider_svg,                       nova_punch_BinaryData::accent_divider_svgSize, "image/svg+xml" },
        { "/img/accent_underlight.svg",                     nova_punch_BinaryData::accent_underlight_svg,                    nova_punch_BinaryData::accent_underlight_svgSize, "image/svg+xml" },
        { "/img/corner_screw.svg",                          nova_punch_BinaryData::corner_screw_svg,                         nova_punch_BinaryData::corner_screw_svgSize, "image/svg+xml" },
        { "/img/icon_arrow_next.svg",                       nova_punch_BinaryData::icon_arrow_next_svg,                      nova_punch_BinaryData::icon_arrow_next_svgSize, "image/svg+xml" },
        { "/img/icon_arrow_previous.svg",                   nova_punch_BinaryData::icon_arrow_previous_svg,                  nova_punch_BinaryData::icon_arrow_previous_svgSize, "image/svg+xml" },
        { "/img/icon_menu.svg",                             nova_punch_BinaryData::icon_menu_svg,                            nova_punch_BinaryData::icon_menu_svgSize, "image/svg+xml" },
        { "/img/icon_save.svg",                             nova_punch_BinaryData::icon_save_svg,                            nova_punch_BinaryData::icon_save_svgSize, "image/svg+xml" },
        { "/img/icon_settings.svg",                         nova_punch_BinaryData::icon_settings_svg,                        nova_punch_BinaryData::icon_settings_svgSize, "image/svg+xml" },
        { "/img/nova_n_mark.svg",                           nova_punch_BinaryData::nova_n_mark_svg,                          nova_punch_BinaryData::nova_n_mark_svgSize, "image/svg+xml" },
    };

    // WKWebView persists its resource cache across app launches even for
    // our custom scheme. index.html's own top-level URL is cache-busted
    // already, but every URL it references internally - the JS chain AND
    // every img/ reference - needs the same treatment, or WKWebView can go
    // on serving a stale cached copy of that sub-resource across launches
    // even after a full rebuild. See Nova Glue/Nova Bend's PluginEditor.cpp
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
        return makeVersionedHtmlResource (nova_punch_BinaryData::index_html, nova_punch_BinaryData::index_htmlSize, htmlMime);
    if (pathOnly.endsWith ("/js/index.js"))
        return makeVersionedTextResource (nova_punch_BinaryData::index_js, nova_punch_BinaryData::index_jsSize,
                                          jsMime, "./juce/index.js");
    if (pathOnly.endsWith ("/js/juce/index.js"))
        return makeResource (nova_punch_BinaryData::index_js2, nova_punch_BinaryData::index_js2Size, jsMime);
    if (pathOnly.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_punch_BinaryData::check_native_interop_js, nova_punch_BinaryData::check_native_interop_jsSize, jsMime);

    for (const auto& img : images)
        if (pathOnly.endsWith (img.suffix))
            return makeResource (img.data, img.size, img.mime);

    return std::nullopt;
}
