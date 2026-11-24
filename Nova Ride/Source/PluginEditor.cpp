#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>
#include "NovaLicenseGate.h"

NovaRideAudioProcessorEditor::NovaRideAudioProcessorEditor (NovaRideAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    targetAttachment      = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("target"), targetRelay, nullptr);
    rangeAttachment       = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter ("range"), rangeRelay, nullptr);
    speedAttachment       = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("speed"), speedRelay, nullptr);
    sensitivityAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("sensitivity"), sensitivityRelay, nullptr);
    outputAttachment      = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("output"), outputRelay, nullptr);
    modeAttachment        = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter ("mode"), modeRelay, nullptr);
    musicAttachment       = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter ("music"), musicRelay, nullptr);
    writeAttachment        = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter ("write"), writeRelay, nullptr);
    bypassAttachment       = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter ("bypass"), bypassRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + assetVersion;
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    setResizeLimits (768, 480, 1920, 1200);
    // Default open size is below the 1280x800 design canvas (the
    // scale-to-fit mechanism shrinks the whole UI proportionally) - opening
    // at full design size made the plugin feel oversized on first load.
    setSize (1024, 640);

    telemetryTimer.startTimerHz (30);

    NovaLicenseOverlay::attachTo (*this);   // unlock screen until activated
}

NovaRideAudioProcessorEditor::~NovaRideAudioProcessorEditor()
{
    telemetryTimer.stopTimer();
}

void NovaRideAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (8, 8, 9));
}

void NovaRideAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaRideAudioProcessorEditor::timerCallbackHz30()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto script = juce::String ("if (window.updateRideTelemetry) { window.updateRideTelemetry({")
                       + "inDb:" + juce::String (processorRef.inputLevelDb.load(), 2)
                       + ",outDb:" + juce::String (processorRef.outputLevelDb.load(), 2)
                       + ",correctionDb:" + juce::String (processorRef.rideCorrectionDb.load(), 3)
                       + ",riding:" + (processorRef.gateOpen.load() ? "true" : "false")
                       + ",sidechainConnected:" + (processorRef.sidechainConnected.load() ? "true" : "false")
                       + ",sidechainPresence:" + juce::String (processorRef.sidechainPresence.load(), 3)
                       + "}); }";
    webView->evaluateJavascript (script);
}

juce::WebBrowserComponent::Options NovaRideAudioProcessorEditor::createWebOptions (NovaRideAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaRide")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.targetRelay)
                     .withOptionsFrom (editor.rangeRelay)
                     .withOptionsFrom (editor.speedRelay)
                     .withOptionsFrom (editor.sensitivityRelay)
                     .withOptionsFrom (editor.outputRelay)
                     .withOptionsFrom (editor.modeRelay)
                     .withOptionsFrom (editor.musicRelay)
                     .withOptionsFrom (editor.writeRelay)
                     .withOptionsFrom (editor.bypassRelay)
                     .withNativeFunction ("applyPreset", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleApplyPreset (args, std::move (completion));
                     })
                     .withNativeFunction ("getPresets", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleGetPresets (args, std::move (completion));
                     })
                     .withNativeFunction ("exportRideCsv", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleExportRideCsv (args, std::move (completion));
                     });

    return options;
}

void NovaRideAudioProcessorEditor::handleGetPresets (const juce::Array<juce::var>&, Completion completion)
{
    auto& manager = processorRef.getPresetManager();
    juce::Array<juce::var> names;
    for (const auto& name : manager.getPresetNames())
        names.add (name);

    auto* result = new juce::DynamicObject();
    result->setProperty ("names", names);
    result->setProperty ("currentIndex", manager.getCurrentIndex());
    completion (juce::var (result));
}

void NovaRideAudioProcessorEditor::handleApplyPreset (const juce::Array<juce::var>& args, Completion completion)
{
    auto& manager = processorRef.getPresetManager();
    auto* result = new juce::DynamicObject();

    if (args.size() < 1 || ! args[0].isInt())
    {
        result->setProperty ("success", false);
        result->setProperty ("error", "No preset index given.");
        completion (juce::var (result));
        return;
    }

    const int index = static_cast<int> (args[0]);
    if (index < 0 || index >= manager.getNumPresets())
    {
        result->setProperty ("success", false);
        result->setProperty ("error", "Preset index out of range.");
        completion (juce::var (result));
        return;
    }

    manager.setCurrentIndex (index);
    manager.applyTo (processorRef.apvts);

    result->setProperty ("success", true);
    result->setProperty ("index", index);
    result->setProperty ("name", manager.getPreset (index).name);
    completion (juce::var (result));
}

void NovaRideAudioProcessorEditor::handleExportRideCsv (const juce::Array<juce::var>&, Completion completion)
{
    activeFileChooser = std::make_unique<juce::FileChooser> (
        "Export Ride Automation As",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Nova Ride Automation.csv"),
        "*.csv");

    const auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting;

    activeFileChooser->launchAsync (flags, [this, completion] (const juce::FileChooser& fc) mutable
    {
        const auto file = fc.getResult();
        auto* result = new juce::DynamicObject();

        if (file == juce::File {})
        {
            result->setProperty ("success", false);
            result->setProperty ("error", "Cancelled");
            completion (juce::var (result));
            activeFileChooser.reset();
            return;
        }

        const bool ok = processorRef.exportRideAutomation (file);
        result->setProperty ("success", ok);
        if (ok)
            result->setProperty ("path", file.getFullPathName());
        else
            result->setProperty ("error", "Could not write file, or nothing has been captured yet (enable Write first).");

        completion (juce::var (result));
        activeFileChooser.reset();
    });
}

std::optional<juce::WebBrowserComponent::Resource> NovaRideAudioProcessorEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));

        return juce::WebBrowserComponent::Resource {
            std::move (bytes),
            juce::String (mime)
        };
    };

    // WKWebView persists its resource cache across app launches even for our
    // custom scheme. index.html's own top-level URL is cache-busted with
    // ?v=<assetVersion> already, but that only forces a fresh index.html -
    // every URL it (or js/index.js) references internally needs the same
    // treatment, or the browser can go on serving a stale cached copy of
    // that sub-resource from a previous run even after a rebuild.
    auto makeVersionedTextResource = [this] (const char* data, int size, const char* mime, const char* findText)
    {
        auto text = juce::String::fromUTF8 (data, size);
        text = text.replace (findText, juce::String (findText) + "?v=" + assetVersion);
        const auto utf8 = text.toUTF8();
        std::vector<std::byte> bytes (utf8.sizeInBytes() - 1); // exclude the trailing null
        std::memcpy (bytes.data(), utf8.getAddress(), bytes.size());
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    // NOTE: requests actually arrive here as scheme/host-less strings like
    // "//js/index.js" rather than an absolute URL under
    // getResourceProviderRoot() - stripping that prefix (as the JUCE example
    // code does) never matches anything here, so match on a simple
    // query-stripped suffix of the raw url instead. This is the one thing
    // to re-verify first if a resource ever silently 404s again.
    const auto pathOnly = url.upToFirstOccurrenceOf ("?", false, false).toLowerCase();
    const bool isRoot = pathOnly.isEmpty() || pathOnly == "/" || pathOnly == "//";

    // Without an explicit charset, the WebView falls back to Latin-1 for
    // text resources, which mangles any literal non-ASCII byte sequence
    // embedded in our UTF-8 source (e.g. "±" renders as "Â±").
    constexpr const char* htmlMime = "text/html; charset=utf-8";
    constexpr const char* jsMime   = "text/javascript; charset=utf-8";

    if (isRoot || pathOnly.endsWith ("/index.html"))
        return makeVersionedTextResource (nova_ride_BinaryData::index_html, nova_ride_BinaryData::index_htmlSize,
                                          htmlMime, "js/index.js");
    if (pathOnly.endsWith ("/js/index.js"))
        return makeVersionedTextResource (nova_ride_BinaryData::index_js, nova_ride_BinaryData::index_jsSize,
                                          jsMime, "./juce/index.js");
    if (pathOnly.endsWith ("/js/juce/index.js"))
        return makeResource (nova_ride_BinaryData::index_js2, nova_ride_BinaryData::index_js2Size, jsMime);
    if (pathOnly.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_ride_BinaryData::check_native_interop_js, nova_ride_BinaryData::check_native_interop_jsSize, jsMime);

    struct ImageEntry { const char* suffix; const char* data; int size; };
    static const ImageEntry images[] = {
        { "/img/n_logo.png",                 nova_ride_BinaryData::n_logo_png,                 nova_ride_BinaryData::n_logo_pngSize },
        { "/img/knob_target_large.png",      nova_ride_BinaryData::knob_target_large_png,      nova_ride_BinaryData::knob_target_large_pngSize },
        { "/img/knob_medium.png",            nova_ride_BinaryData::knob_medium_png,            nova_ride_BinaryData::knob_medium_pngSize },
        { "/img/knob_sensitivity_small.png", nova_ride_BinaryData::knob_sensitivity_small_png, nova_ride_BinaryData::knob_sensitivity_small_pngSize },
        { "/img/knob_arc_large.png",         nova_ride_BinaryData::knob_arc_large_png,         nova_ride_BinaryData::knob_arc_large_pngSize },
        { "/img/knob_arc_medium.png",        nova_ride_BinaryData::knob_arc_medium_png,        nova_ride_BinaryData::knob_arc_medium_pngSize },
        { "/img/knob_arc_small.png",         nova_ride_BinaryData::knob_arc_small_png,         nova_ride_BinaryData::knob_arc_small_pngSize },
        { "/img/fader_cap.png",              nova_ride_BinaryData::fader_cap_png,              nova_ride_BinaryData::fader_cap_pngSize },
        { "/img/fader_rail.png",             nova_ride_BinaryData::fader_rail_png,             nova_ride_BinaryData::fader_rail_pngSize },
        { "/img/meter_lens_off.png",         nova_ride_BinaryData::meter_lens_off_png,         nova_ride_BinaryData::meter_lens_off_pngSize },
        { "/img/meter_lens_gold_on.png",     nova_ride_BinaryData::meter_lens_gold_on_png,     nova_ride_BinaryData::meter_lens_gold_on_pngSize },
        { "/img/meter_lens_red_on.png",      nova_ride_BinaryData::meter_lens_red_on_png,      nova_ride_BinaryData::meter_lens_red_on_pngSize },
        { "/img/meter_column_shell.png",     nova_ride_BinaryData::meter_column_shell_png,     nova_ride_BinaryData::meter_column_shell_pngSize },
        { "/img/toggle_off.png",             nova_ride_BinaryData::toggle_off_png,             nova_ride_BinaryData::toggle_off_pngSize },
        { "/img/toggle_on.png",              nova_ride_BinaryData::toggle_on_png,              nova_ride_BinaryData::toggle_on_pngSize },
        { "/img/mode_button_inactive.png",   nova_ride_BinaryData::mode_button_inactive_png,   nova_ride_BinaryData::mode_button_inactive_pngSize },
        { "/img/mode_button_hover.png",      nova_ride_BinaryData::mode_button_hover_png,      nova_ride_BinaryData::mode_button_hover_pngSize },
        { "/img/mode_button_active.png",     nova_ride_BinaryData::mode_button_active_png,     nova_ride_BinaryData::mode_button_active_pngSize },
        { "/img/bypass_button_inactive.png", nova_ride_BinaryData::bypass_button_inactive_png, nova_ride_BinaryData::bypass_button_inactive_pngSize },
        { "/img/bypass_button_hover.png",    nova_ride_BinaryData::bypass_button_hover_png,    nova_ride_BinaryData::bypass_button_hover_pngSize },
        { "/img/bypass_button_pressed.png",  nova_ride_BinaryData::bypass_button_pressed_png,  nova_ride_BinaryData::bypass_button_pressed_pngSize },
        { "/img/preset_selector_shell.png",  nova_ride_BinaryData::preset_selector_shell_png,  nova_ride_BinaryData::preset_selector_shell_pngSize },
        { "/img/sidechain_selector_shell.png", nova_ride_BinaryData::sidechain_selector_shell_png, nova_ride_BinaryData::sidechain_selector_shell_pngSize },
        { "/img/help_icon.png",              nova_ride_BinaryData::help_icon_png,              nova_ride_BinaryData::help_icon_pngSize },
        { "/img/preset_arrow_left.png",      nova_ride_BinaryData::preset_arrow_left_png,      nova_ride_BinaryData::preset_arrow_left_pngSize },
        { "/img/preset_arrow_right.png",     nova_ride_BinaryData::preset_arrow_right_png,     nova_ride_BinaryData::preset_arrow_right_pngSize },
        { "/img/settings_icon.png",          nova_ride_BinaryData::settings_icon_png,          nova_ride_BinaryData::settings_icon_pngSize },
        { "/img/settings_button.png",        nova_ride_BinaryData::settings_button_png,        nova_ride_BinaryData::settings_button_pngSize },
        { "/img/corner_bracket.png",         nova_ride_BinaryData::corner_bracket_png,         nova_ride_BinaryData::corner_bracket_pngSize },
        { "/img/divider_gold.png",           nova_ride_BinaryData::divider_gold_png,           nova_ride_BinaryData::divider_gold_pngSize },
        { "/img/panel_inset.png",            nova_ride_BinaryData::panel_inset_png,            nova_ride_BinaryData::panel_inset_pngSize },
        { "/img/outer_frame.png",            nova_ride_BinaryData::outer_frame_png,            nova_ride_BinaryData::outer_frame_pngSize },
        { "/img/header_frame.png",           nova_ride_BinaryData::header_frame_png,           nova_ride_BinaryData::header_frame_pngSize },
        { "/img/footer_frame.png",           nova_ride_BinaryData::footer_frame_png,           nova_ride_BinaryData::footer_frame_pngSize },
        { "/img/screw_gold.png",             nova_ride_BinaryData::screw_gold_png,             nova_ride_BinaryData::screw_gold_pngSize },
        { "/img/ride_history_grid.png",      nova_ride_BinaryData::ride_history_grid_png,      nova_ride_BinaryData::ride_history_grid_pngSize },
        { "/img/texture_graphite_vector.png", nova_ride_BinaryData::texture_graphite_vector_png, nova_ride_BinaryData::texture_graphite_vector_pngSize },
    };
    for (const auto& img : images)
        if (pathOnly.endsWith (img.suffix))
            return makeResource (img.data, img.size, "image/png");

    return std::nullopt;
}
