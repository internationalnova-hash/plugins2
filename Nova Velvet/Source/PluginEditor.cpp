#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>
#include "NovaLicenseGate.h"

NovaVelvetAudioProcessorEditor::NovaVelvetAudioProcessorEditor (NovaVelvetAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    thresholdAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::threshold), thresholdRelay, nullptr);
    ratioAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::ratio), ratioRelay, nullptr);
    attackAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::attack), attackRelay, nullptr);
    releaseAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::release), releaseRelay, nullptr);
    gainAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::gain), gainRelay, nullptr);
    timeSelectAttachment = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::timeSelect), timeSelectRelay, nullptr);
    scHpfAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::scHpf), scHpfRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::mix), mixRelay, nullptr);
    outputAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::output), outputRelay, nullptr);
    stereoLinkAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::stereoLink), stereoLinkRelay, nullptr);
    analogModeAttachment = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::analogMode), analogModeRelay, nullptr);
    tubeDriveAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::tubeDrive), tubeDriveRelay, nullptr);
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::bypass), bypassRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + assetVersion;
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    // Design canvas is 1086x362 @1x (half the approved 2172x724 mockup,
    // per the asset pack's own README) - open below 100% of it, matching
    // the catalog's "never open at full design-canvas size" lesson.
    setResizeLimits (700, 234, 1630, 543);
    setSize (1086, 362);
    // 20Hz - this and the WKWebView JS evaluation it triggers run on the
    // message thread, in the same process as the real-time audio thread
    // (a lesson learned directly on other plugins this session: keep this
    // cheap and not too fast).
    startTimerHz (20);

    NovaLicenseOverlay::attachTo (*this);   // unlock screen until activated
}

NovaVelvetAudioProcessorEditor::~NovaVelvetAudioProcessorEditor()
{
    stopTimer();
}

void NovaVelvetAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (5, 8, 14));
}

void NovaVelvetAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaVelvetAudioProcessorEditor::timerCallback()
{
    // isShowing() (not isVisible()) - a host can minimize/obscure a window
    // without JUCE's isVisible() flag going false; isShowing() walks the
    // whole ancestor chain including native peer visibility.
    if (webView == nullptr || ! webView->isShowing())
        return;

    // Real (not decorative/fake) telemetry throughout - spec explicitly
    // forbids fabricated meter/analysis visuals.
    juce::String json;
    json.preallocateBytes (256);
    json << "{"
         << "\"grDb\":" << juce::String (processorRef.getGrDisplayDb(), 2) << ","
         << "\"gainReductionDb\":" << juce::String (processorRef.getGainReductionDb(), 2) << ","
         << "\"inputVu\":" << juce::String (processorRef.getInputDisplayVu(), 2) << ","
         << "\"outputVu\":" << juce::String (processorRef.getOutputDisplayVu(), 2)
         << "}";

    const auto js = "if (window.updateTelemetry) { window.updateTelemetry(" + json + "); }";
    webView->evaluateJavascript (js);
}

juce::WebBrowserComponent::Options NovaVelvetAudioProcessorEditor::createWebOptions (NovaVelvetAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaVelvet")));
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
                     .withOptionsFrom (editor.thresholdRelay)
                     .withOptionsFrom (editor.ratioRelay)
                     .withOptionsFrom (editor.attackRelay)
                     .withOptionsFrom (editor.releaseRelay)
                     .withOptionsFrom (editor.gainRelay)
                     .withOptionsFrom (editor.timeSelectRelay)
                     .withOptionsFrom (editor.scHpfRelay)
                     .withOptionsFrom (editor.mixRelay)
                     .withOptionsFrom (editor.outputRelay)
                     .withOptionsFrom (editor.stereoLinkRelay)
                     .withOptionsFrom (editor.analogModeRelay)
                     .withOptionsFrom (editor.tubeDriveRelay)
                     .withOptionsFrom (editor.bypassRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaVelvetAudioProcessorEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    struct ImageEntry { const char* suffix; const char* data; int size; const char* mime; };
    static const ImageEntry images[] = {
        { "/img/amber_lamp_idle@2x.png",              nova_velvet_BinaryData::amber_lamp_idle2x_png,              nova_velvet_BinaryData::amber_lamp_idle2x_pngSize, "image/png" },
        { "/img/amber_lamp_off@2x.png",               nova_velvet_BinaryData::amber_lamp_off2x_png,               nova_velvet_BinaryData::amber_lamp_off2x_pngSize, "image/png" },
        { "/img/amber_lamp_on@2x.png",                nova_velvet_BinaryData::amber_lamp_on2x_png,                nova_velvet_BinaryData::amber_lamp_on2x_pngSize, "image/png" },
        { "/img/amber_lamp_pressed@2x.png",           nova_velvet_BinaryData::amber_lamp_pressed2x_png,           nova_velvet_BinaryData::amber_lamp_pressed2x_pngSize, "image/png" },
        { "/img/analog_mode_neutral_center@2x.png",   nova_velvet_BinaryData::analog_mode_neutral_center2x_png,   nova_velvet_BinaryData::analog_mode_neutral_center2x_pngSize, "image/png" },
        { "/img/analog_mode_position_bottom@2x.png",  nova_velvet_BinaryData::analog_mode_position_bottom2x_png,  nova_velvet_BinaryData::analog_mode_position_bottom2x_pngSize, "image/png" },
        { "/img/analog_mode_position_center@2x.png",  nova_velvet_BinaryData::analog_mode_position_center2x_png,  nova_velvet_BinaryData::analog_mode_position_center2x_pngSize, "image/png" },
        { "/img/analog_mode_position_top@2x.png",     nova_velvet_BinaryData::analog_mode_position_top2x_png,     nova_velvet_BinaryData::analog_mode_position_top2x_pngSize, "image/png" },
        { "/img/bottom_frame_charcoal@2x.png",        nova_velvet_BinaryData::bottom_frame_charcoal2x_png,        nova_velvet_BinaryData::bottom_frame_charcoal2x_pngSize, "image/png" },
        { "/img/button_menu_shell@2x.png",            nova_velvet_BinaryData::button_menu_shell2x_png,            nova_velvet_BinaryData::button_menu_shell2x_pngSize, "image/png" },
        { "/img/button_next_shell@2x.png",            nova_velvet_BinaryData::button_next_shell2x_png,            nova_velvet_BinaryData::button_next_shell2x_pngSize, "image/png" },
        { "/img/button_prev_shell@2x.png",            nova_velvet_BinaryData::button_prev_shell2x_png,            nova_velvet_BinaryData::button_prev_shell2x_pngSize, "image/png" },
        { "/img/button_save_shell@2x.png",            nova_velvet_BinaryData::button_save_shell2x_png,            nova_velvet_BinaryData::button_save_shell2x_pngSize, "image/png" },
        { "/img/button_settings_shell@2x.png",        nova_velvet_BinaryData::button_settings_shell2x_png,        nova_velvet_BinaryData::button_settings_shell2x_pngSize, "image/png" },
        { "/img/divider_metal@2x.png",                nova_velvet_BinaryData::divider_metal2x_png,                nova_velvet_BinaryData::divider_metal2x_pngSize, "image/png" },
        { "/img/edge_highlight_cyan@2x.png",          nova_velvet_BinaryData::edge_highlight_cyan2x_png,          nova_velvet_BinaryData::edge_highlight_cyan2x_pngSize, "image/png" },
        { "/img/knob_large_pointer_up@2x.png",        nova_velvet_BinaryData::knob_large_pointer_up2x_png,        nova_velvet_BinaryData::knob_large_pointer_up2x_pngSize, "image/png" },
        { "/img/knob_small_pointer_up@2x.png",        nova_velvet_BinaryData::knob_small_pointer_up2x_png,        nova_velvet_BinaryData::knob_small_pointer_up2x_pngSize, "image/png" },
        { "/img/lower_inset_bevel@2x.png",            nova_velvet_BinaryData::lower_inset_bevel2x_png,            nova_velvet_BinaryData::lower_inset_bevel2x_pngSize, "image/png" },
        { "/img/lower_inset_blue@2x.png",             nova_velvet_BinaryData::lower_inset_blue2x_png,             nova_velvet_BinaryData::lower_inset_blue2x_pngSize, "image/png" },
        { "/img/main_faceplate_blue@2x.png",          nova_velvet_BinaryData::main_faceplate_blue2x_png,          nova_velvet_BinaryData::main_faceplate_blue2x_pngSize, "image/png" },
        { "/img/preset_selector_shell@2x.png",        nova_velvet_BinaryData::preset_selector_shell2x_png,        nova_velvet_BinaryData::preset_selector_shell2x_pngSize, "image/png" },
        { "/img/rack_ear@2x.png",                     nova_velvet_BinaryData::rack_ear2x_png,                     nova_velvet_BinaryData::rack_ear2x_pngSize, "image/png" },
        { "/img/rack_screw@2x.png",                   nova_velvet_BinaryData::rack_screw2x_png,                   nova_velvet_BinaryData::rack_screw2x_pngSize, "image/png" },
        { "/img/stereo_link_off@2x.png",              nova_velvet_BinaryData::stereo_link_off2x_png,              nova_velvet_BinaryData::stereo_link_off2x_pngSize, "image/png" },
        { "/img/stereo_link_on@2x.png",               nova_velvet_BinaryData::stereo_link_on2x_png,               nova_velvet_BinaryData::stereo_link_on2x_pngSize, "image/png" },
        { "/img/time_select_neutral_center@2x.png",   nova_velvet_BinaryData::time_select_neutral_center2x_png,   nova_velvet_BinaryData::time_select_neutral_center2x_pngSize, "image/png" },
        { "/img/time_select_position_bottom@2x.png",  nova_velvet_BinaryData::time_select_position_bottom2x_png,  nova_velvet_BinaryData::time_select_position_bottom2x_pngSize, "image/png" },
        { "/img/time_select_position_center@2x.png",  nova_velvet_BinaryData::time_select_position_center2x_png,  nova_velvet_BinaryData::time_select_position_center2x_pngSize, "image/png" },
        { "/img/time_select_position_top@2x.png",     nova_velvet_BinaryData::time_select_position_top2x_png,     nova_velvet_BinaryData::time_select_position_top2x_pngSize, "image/png" },
        { "/img/top_header_graphite@2x.png",          nova_velvet_BinaryData::top_header_graphite2x_png,          nova_velvet_BinaryData::top_header_graphite2x_pngSize, "image/png" },
        { "/img/top_header_shell@2x.png",             nova_velvet_BinaryData::top_header_shell2x_png,             nova_velvet_BinaryData::top_header_shell2x_pngSize, "image/png" },
        { "/img/tube_active@2x.png",                  nova_velvet_BinaryData::tube_active2x_png,                  nova_velvet_BinaryData::tube_active2x_pngSize, "image/png" },
        { "/img/tube_idle@2x.png",                    nova_velvet_BinaryData::tube_idle2x_png,                    nova_velvet_BinaryData::tube_idle2x_pngSize, "image/png" },
        { "/img/tube_off@2x.png",                     nova_velvet_BinaryData::tube_off2x_png,                     nova_velvet_BinaryData::tube_off2x_pngSize, "image/png" },
        { "/img/tube_strong@2x.png",                  nova_velvet_BinaryData::tube_strong2x_png,                  nova_velvet_BinaryData::tube_strong2x_pngSize, "image/png" },
        { "/img/vu_amber_illumination@2x.png",        nova_velvet_BinaryData::vu_amber_illumination2x_png,        nova_velvet_BinaryData::vu_amber_illumination2x_pngSize, "image/png" },
        { "/img/vu_face_blank@2x.png",                nova_velvet_BinaryData::vu_face_blank2x_png,                nova_velvet_BinaryData::vu_face_blank2x_pngSize, "image/png" },
        { "/img/vu_glass_overlay@2x.png",             nova_velvet_BinaryData::vu_glass_overlay2x_png,             nova_velvet_BinaryData::vu_glass_overlay2x_pngSize, "image/png" },
        { "/img/vu_housing_empty@2x.png",             nova_velvet_BinaryData::vu_housing_empty2x_png,             nova_velvet_BinaryData::vu_housing_empty2x_pngSize, "image/png" },
        { "/img/vu_needle_vertical@2x.png",           nova_velvet_BinaryData::vu_needle_vertical2x_png,           nova_velvet_BinaryData::vu_needle_vertical2x_pngSize, "image/png" },
        { "/img/divider_line.svg",                    nova_velvet_BinaryData::divider_line_svg,                   nova_velvet_BinaryData::divider_line_svgSize, "image/svg+xml" },
        { "/img/menu_ellipsis.svg",                   nova_velvet_BinaryData::menu_ellipsis_svg,                  nova_velvet_BinaryData::menu_ellipsis_svgSize, "image/svg+xml" },
        { "/img/next_arrow.svg",                      nova_velvet_BinaryData::next_arrow_svg,                     nova_velvet_BinaryData::next_arrow_svgSize, "image/svg+xml" },
        { "/img/nova_audio_signature@2x.png",          nova_velvet_BinaryData::nova_audio_signature2x_png,         nova_velvet_BinaryData::nova_audio_signature2x_pngSize, "image/png" },
        { "/img/nova_n_mark.svg",                     nova_velvet_BinaryData::nova_n_mark_svg,                    nova_velvet_BinaryData::nova_n_mark_svgSize, "image/svg+xml" },
        { "/img/previous_arrow.svg",                  nova_velvet_BinaryData::previous_arrow_svg,                 nova_velvet_BinaryData::previous_arrow_svgSize, "image/svg+xml" },
        { "/img/save_icon.svg",                       nova_velvet_BinaryData::save_icon_svg,                      nova_velvet_BinaryData::save_icon_svgSize, "image/svg+xml" },
        { "/img/settings_icon.svg",                   nova_velvet_BinaryData::settings_icon_svg,                  nova_velvet_BinaryData::settings_icon_svgSize, "image/svg+xml" },
    };

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

    // Requests arrive as scheme/host-less strings like "//js/index.js"
    // rather than an absolute URL under getResourceProviderRoot() -
    // match on a simple query-stripped suffix of the raw url instead.
    const auto pathOnly = url.upToFirstOccurrenceOf ("?", false, false).toLowerCase();
    const bool isRoot = pathOnly.isEmpty() || pathOnly == "/" || pathOnly == "//";

    constexpr const char* htmlMime = "text/html; charset=utf-8";
    constexpr const char* jsMime   = "text/javascript; charset=utf-8";

    if (isRoot || pathOnly.endsWith ("/index.html"))
        return makeVersionedHtmlResource (nova_velvet_BinaryData::index_html, nova_velvet_BinaryData::index_htmlSize, htmlMime);
    if (pathOnly.endsWith ("/js/index.js"))
        return makeVersionedTextResource (nova_velvet_BinaryData::index_js, nova_velvet_BinaryData::index_jsSize,
                                          jsMime, "./juce/index.js");
    if (pathOnly.endsWith ("/js/juce/index.js"))
        return makeResource (nova_velvet_BinaryData::index_js2, nova_velvet_BinaryData::index_js2Size, jsMime);
    if (pathOnly.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_velvet_BinaryData::check_native_interop_js, nova_velvet_BinaryData::check_native_interop_jsSize, jsMime);

    for (const auto& img : images)
        if (pathOnly.endsWith (img.suffix))
            return makeResource (img.data, img.size, img.mime);

    return std::nullopt;
}
