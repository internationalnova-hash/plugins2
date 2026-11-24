#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>
#include "NovaLicenseGate.h"

NovaGlowAudioProcessorEditor::NovaGlowAudioProcessorEditor (NovaGlowAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    peakReductionAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("peakReduction"), peakReductionRelay, nullptr);
    gainAttachment           = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("gain"), gainRelay, nullptr);
    compressLimitAttachment  = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter ("compressLimit"), compressLimitRelay, nullptr);
    scHpfAttachment          = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("scHpf"), scHpfRelay, nullptr);
    emphasisAttachment       = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("emphasis"), emphasisRelay, nullptr);
    glowAttachment           = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("glow"), glowRelay, nullptr);
    analogModeAttachment     = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter ("analogMode"), analogModeRelay, nullptr);
    mixAttachment            = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("mix"), mixRelay, nullptr);
    stereoLinkAttachment     = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("stereoLink"), stereoLinkRelay, nullptr);
    noiseAttachment          = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("noise"), noiseRelay, nullptr);
    oversamplingAttachment   = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter ("oversampling"), oversamplingRelay, nullptr);
    meterModeAttachment      = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter ("meterMode"), meterModeRelay, nullptr);
    bypassAttachment         = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter ("bypass"), bypassRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + assetVersion;
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    setResizeLimits (800, 320, 2000, 800);
    // Default open size is below the 1600x640 design canvas (the
    // scale-to-fit mechanism shrinks the whole UI proportionally).
    setSize (1400, 560);

    telemetryTimer.startTimerHz (30);

    NovaLicenseOverlay::attachTo (*this);   // unlock screen until activated
}

NovaGlowAudioProcessorEditor::~NovaGlowAudioProcessorEditor()
{
    telemetryTimer.stopTimer();
}

void NovaGlowAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (16, 17, 15));
}

void NovaGlowAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaGlowAudioProcessorEditor::timerCallbackHz30()
{
    if (webView == nullptr)
        return;

    // Real signal, measured on the audio thread each block - never faked.
    const auto script = juce::String ("if (window.updateGlowTelemetry) { window.updateGlowTelemetry({")
                       + "grDb:" + juce::String (processorRef.telemetryGrDb.load(), 3)
                       + ",outputVu:" + juce::String (processorRef.telemetryOutputVu.load(), 3)
                       + ",inputVu:" + juce::String (processorRef.telemetryInputVu.load(), 3)
                       + "}); }";
    webView->evaluateJavascript (script);
}

juce::WebBrowserComponent::Options NovaGlowAudioProcessorEditor::createWebOptions (NovaGlowAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaGlow")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.peakReductionRelay)
                     .withOptionsFrom (editor.gainRelay)
                     .withOptionsFrom (editor.compressLimitRelay)
                     .withOptionsFrom (editor.scHpfRelay)
                     .withOptionsFrom (editor.emphasisRelay)
                     .withOptionsFrom (editor.glowRelay)
                     .withOptionsFrom (editor.analogModeRelay)
                     .withOptionsFrom (editor.mixRelay)
                     .withOptionsFrom (editor.stereoLinkRelay)
                     .withOptionsFrom (editor.noiseRelay)
                     .withOptionsFrom (editor.oversamplingRelay)
                     .withOptionsFrom (editor.meterModeRelay)
                     .withOptionsFrom (editor.bypassRelay)
                     .withNativeFunction ("getPresets", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleGetPresets (args, std::move (completion));
                     })
                     .withNativeFunction ("applyPreset", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleApplyPreset (args, std::move (completion));
                     })
                     .withNativeFunction ("savePreset", [&editor] (const juce::Array<juce::var>& args, Completion completion)
                     {
                         editor.handleSavePreset (args, std::move (completion));
                     });

    return options;
}

void NovaGlowAudioProcessorEditor::handleGetPresets (const juce::Array<juce::var>&, Completion completion)
{
    auto& manager = processorRef.getPresetManager();
    juce::Array<juce::var> names;
    juce::Array<juce::var> isFactory;
    for (const auto& name : manager.getPresetNames())
        names.add (name);
    for (bool f : manager.getFactoryFlags())
        isFactory.add (f);

    auto* result = new juce::DynamicObject();
    result->setProperty ("names", names);
    result->setProperty ("isFactory", isFactory);
    result->setProperty ("currentIndex", manager.getCurrentIndex());
    completion (juce::var (result));
}

void NovaGlowAudioProcessorEditor::handleApplyPreset (const juce::Array<juce::var>& args, Completion completion)
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

    manager.setCurrentIndex (static_cast<int> (args[0]));
    manager.applyTo (processorRef.apvts);

    result->setProperty ("success", true);
    result->setProperty ("currentIndex", manager.getCurrentIndex());
    completion (juce::var (result));
}

void NovaGlowAudioProcessorEditor::handleSavePreset (const juce::Array<juce::var>& args, Completion completion)
{
    auto& manager = processorRef.getPresetManager();
    auto* result = new juce::DynamicObject();

    const juce::String name = args.size() >= 1 ? args[0].toString().trim() : juce::String();
    if (name.isEmpty())
    {
        result->setProperty ("success", false);
        result->setProperty ("error", "Preset name is empty.");
        completion (juce::var (result));
        return;
    }

    const auto values = NovaGlowPresetManager::captureFrom (processorRef.apvts);
    const bool ok = manager.saveUserPreset (name, values);

    juce::Array<juce::var> names;
    for (const auto& n : manager.getPresetNames())
        names.add (n);

    result->setProperty ("success", ok);
    result->setProperty ("names", names);
    result->setProperty ("currentIndex", manager.getCurrentIndex());
    if (! ok)
        result->setProperty ("error", "Could not write preset file.");
    completion (juce::var (result));
}

std::optional<juce::WebBrowserComponent::Resource> NovaGlowAudioProcessorEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    // WKWebView persists its resource cache across app launches even for
    // our custom scheme. index.html's own top-level URL is cache-busted
    // with ?v=<assetVersion> already, but that only forces a fresh
    // index.html - every URL it (or js/index.js) references internally
    // needs the same treatment, or the browser can go on serving a stale
    // cached copy of that sub-resource from a previous run even after a
    // rebuild. See Nova Ride's PluginEditor.cpp for the fuller writeup of
    // this bug.
    auto makeVersionedTextResource = [this] (const char* data, int size, const char* mime, const char* findText)
    {
        auto text = juce::String::fromUTF8 (data, size);
        text = text.replace (findText, juce::String (findText) + "?v=" + assetVersion);
        const auto utf8 = text.toUTF8();
        std::vector<std::byte> bytes (utf8.sizeInBytes() - 1); // exclude trailing null
        std::memcpy (bytes.data(), utf8.getAddress(), bytes.size());
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    // NOTE: requests arrive here as scheme/host-less strings like
    // "//js/index.js" rather than an absolute URL under
    // getResourceProviderRoot() - stripping that prefix never matches
    // anything, so match on a simple query-stripped suffix of the raw url
    // instead. This is the one thing to re-verify first if a resource
    // ever silently 404s (confirmed the hard way building Nova Ride).
    const auto pathOnly = url.upToFirstOccurrenceOf ("?", false, false).toLowerCase();
    const bool isRoot = pathOnly.isEmpty() || pathOnly == "/" || pathOnly == "//";

    constexpr const char* htmlMime = "text/html; charset=utf-8";
    constexpr const char* jsMime   = "text/javascript; charset=utf-8";

    if (isRoot || pathOnly.endsWith ("/index.html"))
        return makeVersionedTextResource (nova_glow_BinaryData::index_html, nova_glow_BinaryData::index_htmlSize,
                                          htmlMime, "js/index.js");
    if (pathOnly.endsWith ("/js/index.js"))
        return makeVersionedTextResource (nova_glow_BinaryData::index_js, nova_glow_BinaryData::index_jsSize,
                                          jsMime, "./juce/index.js");
    if (pathOnly.endsWith ("/js/juce/index.js"))
        return makeResource (nova_glow_BinaryData::index_js2, nova_glow_BinaryData::index_js2Size, jsMime);
    if (pathOnly.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_glow_BinaryData::check_native_interop_js, nova_glow_BinaryData::check_native_interop_jsSize, jsMime);

    struct ImageEntry { const char* suffix; const char* data; int size; };
    static const ImageEntry images[] = {
        { "/img/n_logo.png",                  nova_glow_BinaryData::n_logo_png,                  nova_glow_BinaryData::n_logo_pngSize },
        { "/img/knob_large_bakelite.png",     nova_glow_BinaryData::knob_large_bakelite_png,     nova_glow_BinaryData::knob_large_bakelite_pngSize },
        { "/img/knob_small_bakelite.png",     nova_glow_BinaryData::knob_small_bakelite_png,     nova_glow_BinaryData::knob_small_bakelite_pngSize },
        { "/img/vu_meter_blank.png",          nova_glow_BinaryData::vu_meter_blank_png,          nova_glow_BinaryData::vu_meter_blank_pngSize },
        { "/img/vu_needle.png",               nova_glow_BinaryData::vu_needle_png,               nova_glow_BinaryData::vu_needle_pngSize },
        { "/img/vu_pivot.png",                nova_glow_BinaryData::vu_pivot_png,                nova_glow_BinaryData::vu_pivot_pngSize },
        { "/img/vu_glass_highlight.png",      nova_glow_BinaryData::vu_glass_highlight_png,      nova_glow_BinaryData::vu_glass_highlight_pngSize },
        { "/img/top_bar_frame.png",           nova_glow_BinaryData::top_bar_frame_png,           nova_glow_BinaryData::top_bar_frame_pngSize },
        { "/img/wood_cheek_left.png",         nova_glow_BinaryData::wood_cheek_left_png,         nova_glow_BinaryData::wood_cheek_left_pngSize },
        { "/img/wood_cheek_right.png",        nova_glow_BinaryData::wood_cheek_right_png,        nova_glow_BinaryData::wood_cheek_right_pngSize },
        { "/img/cream_metal_texture.png",     nova_glow_BinaryData::cream_metal_texture_png,     nova_glow_BinaryData::cream_metal_texture_pngSize },
        { "/img/black_brushed_texture.png",   nova_glow_BinaryData::black_brushed_texture_png,   nova_glow_BinaryData::black_brushed_texture_pngSize },
        { "/img/power_jewel_off.png",         nova_glow_BinaryData::power_jewel_off_png,         nova_glow_BinaryData::power_jewel_off_pngSize },
        { "/img/power_jewel_on.png",          nova_glow_BinaryData::power_jewel_on_png,          nova_glow_BinaryData::power_jewel_on_pngSize },
        { "/img/compress_limit_compress.png", nova_glow_BinaryData::compress_limit_compress_png, nova_glow_BinaryData::compress_limit_compress_pngSize },
        { "/img/compress_limit_limit.png",    nova_glow_BinaryData::compress_limit_limit_png,    nova_glow_BinaryData::compress_limit_limit_pngSize },
        { "/img/analog_button_inactive.png",  nova_glow_BinaryData::analog_button_inactive_png,  nova_glow_BinaryData::analog_button_inactive_pngSize },
        { "/img/analog_button_hover.png",     nova_glow_BinaryData::analog_button_hover_png,     nova_glow_BinaryData::analog_button_hover_pngSize },
        { "/img/analog_button_active.png",    nova_glow_BinaryData::analog_button_active_png,    nova_glow_BinaryData::analog_button_active_pngSize },
        { "/img/oversample_button_inactive.png", nova_glow_BinaryData::oversample_button_inactive_png, nova_glow_BinaryData::oversample_button_inactive_pngSize },
        { "/img/oversample_button_hover.png", nova_glow_BinaryData::oversample_button_hover_png, nova_glow_BinaryData::oversample_button_hover_pngSize },
        { "/img/oversample_button_active.png", nova_glow_BinaryData::oversample_button_active_png, nova_glow_BinaryData::oversample_button_active_pngSize },
        { "/img/stereo_link_off.png",         nova_glow_BinaryData::stereo_link_off_png,         nova_glow_BinaryData::stereo_link_off_pngSize },
        { "/img/stereo_link_on.png",          nova_glow_BinaryData::stereo_link_on_png,          nova_glow_BinaryData::stereo_link_on_pngSize },
        { "/img/meter_selector.png",          nova_glow_BinaryData::meter_selector_png,          nova_glow_BinaryData::meter_selector_pngSize },
        { "/img/preset_selector_shell.png",   nova_glow_BinaryData::preset_selector_shell_png,   nova_glow_BinaryData::preset_selector_shell_pngSize },
        { "/img/preset_arrow_left.png",       nova_glow_BinaryData::preset_arrow_left_png,       nova_glow_BinaryData::preset_arrow_left_pngSize },
        { "/img/preset_arrow_right.png",      nova_glow_BinaryData::preset_arrow_right_png,      nova_glow_BinaryData::preset_arrow_right_pngSize },
        { "/img/settings_button.png",         nova_glow_BinaryData::settings_button_png,         nova_glow_BinaryData::settings_button_pngSize },
        { "/img/save_button.png",             nova_glow_BinaryData::save_button_png,             nova_glow_BinaryData::save_button_pngSize },
        { "/img/settings_icon.png",           nova_glow_BinaryData::settings_icon_png,           nova_glow_BinaryData::settings_icon_pngSize },
        { "/img/save_icon.png",               nova_glow_BinaryData::save_icon_png,               nova_glow_BinaryData::save_icon_pngSize },
        { "/img/brass_screw.png",             nova_glow_BinaryData::brass_screw_png,             nova_glow_BinaryData::brass_screw_pngSize },
        { "/img/brass_divider.png",           nova_glow_BinaryData::brass_divider_png,           nova_glow_BinaryData::brass_divider_pngSize },
    };
    for (const auto& img : images)
        if (pathOnly.endsWith (img.suffix))
            return makeResource (img.data, img.size, "image/png");

    return std::nullopt;
}
