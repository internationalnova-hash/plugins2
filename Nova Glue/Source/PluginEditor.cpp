#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>
#include "NovaLicenseGate.h"

NovaGlueAudioProcessorEditor::NovaGlueAudioProcessorEditor (NovaGlueAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    thresholdAttachment    = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("threshold"), thresholdRelay, nullptr);
    attackAttachment       = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter ("attack"), attackRelay, nullptr);
    releaseAttachment      = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter ("release"), releaseRelay, nullptr);
    makeupAttachment       = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("makeup"), makeupRelay, nullptr);
    ratioAttachment        = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter ("ratio"), ratioRelay, nullptr);
    scHpfAttachment        = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("scHpf"), scHpfRelay, nullptr);
    mixAttachment          = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("mix"), mixRelay, nullptr);
    glueAttachment         = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("glue"), glueRelay, nullptr);
    driveAttachment        = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("drive"), driveRelay, nullptr);
    autoGainAttachment     = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter ("autoGain"), autoGainRelay, nullptr);
    stereoLinkAttachment   = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter ("stereoLink"), stereoLinkRelay, nullptr);
    oversamplingAttachment = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter ("oversampling"), oversamplingRelay, nullptr);
    bypassAttachment       = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter ("bypass"), bypassRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + assetVersion;
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    setResizeLimits (540, 564, 1620, 1689);
    // Default open size is below the 1080x1126 design canvas (the
    // scale-to-fit mechanism shrinks the whole UI proportionally) - never
    // open at full design size (the lesson from Nova Ride).
    setSize (864, 901);

    telemetryTimer.startTimerHz (30);

    NovaLicenseOverlay::attachTo (*this);   // unlock screen until activated
}

NovaGlueAudioProcessorEditor::~NovaGlueAudioProcessorEditor()
{
    telemetryTimer.stopTimer();
}

void NovaGlueAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (11, 12, 13));
}

void NovaGlueAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaGlueAudioProcessorEditor::timerCallbackHz30()
{
    if (webView == nullptr)
        return;

    const auto script = juce::String ("if (window.updateGlueTelemetry) { window.updateGlueTelemetry({")
                       + "grDb:" + juce::String (processorRef.telemetryGrDb.load(), 3)
                       + "}); }";
    webView->evaluateJavascript (script);
}

juce::WebBrowserComponent::Options NovaGlueAudioProcessorEditor::createWebOptions (NovaGlueAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaGlue")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.thresholdRelay)
                     .withOptionsFrom (editor.attackRelay)
                     .withOptionsFrom (editor.releaseRelay)
                     .withOptionsFrom (editor.makeupRelay)
                     .withOptionsFrom (editor.ratioRelay)
                     .withOptionsFrom (editor.scHpfRelay)
                     .withOptionsFrom (editor.mixRelay)
                     .withOptionsFrom (editor.glueRelay)
                     .withOptionsFrom (editor.driveRelay)
                     .withOptionsFrom (editor.autoGainRelay)
                     .withOptionsFrom (editor.stereoLinkRelay)
                     .withOptionsFrom (editor.oversamplingRelay)
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

void NovaGlueAudioProcessorEditor::handleGetPresets (const juce::Array<juce::var>&, Completion completion)
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

void NovaGlueAudioProcessorEditor::handleApplyPreset (const juce::Array<juce::var>& args, Completion completion)
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

void NovaGlueAudioProcessorEditor::handleSavePreset (const juce::Array<juce::var>& args, Completion completion)
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

    const auto values = NovaGluePresetManager::captureFrom (processorRef.apvts);
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

std::optional<juce::WebBrowserComponent::Resource> NovaGlueAudioProcessorEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    // WKWebView persists its resource cache across app launches even for
    // our custom scheme. See Nova Ride/Nova Glow's PluginEditor.cpp for
    // the fuller writeup of this bug. index.html's own top-level URL is
    // cache-busted already, but every URL it references internally - the
    // JS chain AND every img/ reference - needs the same treatment, or
    // WKWebView can go on serving a stale cached copy of that sub-resource
    // (e.g. a PNG whose on-disk content changed but whose URL didn't)
    // across launches even after a full rebuild.
    auto makeVersionedTextResource = [this] (const char* data, int size, const char* mime, const char* findText)
    {
        auto text = juce::String::fromUTF8 (data, size);
        text = text.replace (findText, juce::String (findText) + "?v=" + assetVersion);
        const auto utf8 = text.toUTF8();
        std::vector<std::byte> bytes (utf8.sizeInBytes() - 1); // exclude trailing null
        std::memcpy (bytes.data(), utf8.getAddress(), bytes.size());
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    struct ImageEntry { const char* suffix; const char* data; int size; const char* mime; };
    static const ImageEntry images[] = {
        { "/img/n_logo.png",                    nova_glue_BinaryData::n_logo_png,                    nova_glue_BinaryData::n_logo_pngSize, "image/png" },
        { "/img/main_faceplate_graphite.png",   nova_glue_BinaryData::main_faceplate_graphite_png,   nova_glue_BinaryData::main_faceplate_graphite_pngSize, "image/png" },
        { "/img/lower_panel_graphite.png",      nova_glue_BinaryData::lower_panel_graphite_png,      nova_glue_BinaryData::lower_panel_graphite_pngSize, "image/png" },
        { "/img/knob_large_champagne.png",      nova_glue_BinaryData::knob_large_champagne_png,      nova_glue_BinaryData::knob_large_champagne_pngSize, "image/png" },
        { "/img/knob_small_champagne.png",      nova_glue_BinaryData::knob_small_champagne_png,      nova_glue_BinaryData::knob_small_champagne_pngSize, "image/png" },
        { "/img/knob_glue_black.png",           nova_glue_BinaryData::knob_glue_black_png,           nova_glue_BinaryData::knob_glue_black_pngSize, "image/png" },
        { "/img/glue_ring_active.png",          nova_glue_BinaryData::glue_ring_active_png,          nova_glue_BinaryData::glue_ring_active_pngSize, "image/png" },
        { "/img/glue_ring_inactive.png",        nova_glue_BinaryData::glue_ring_inactive_png,        nova_glue_BinaryData::glue_ring_inactive_pngSize, "image/png" },
        { "/img/gr_meter_housing.png",          nova_glue_BinaryData::gr_meter_housing_png,          nova_glue_BinaryData::gr_meter_housing_pngSize, "image/png" },
        { "/img/button_standard_inactive.png",  nova_glue_BinaryData::button_standard_inactive_png,  nova_glue_BinaryData::button_standard_inactive_pngSize, "image/png" },
        { "/img/button_standard_hover.png",     nova_glue_BinaryData::button_standard_hover_png,     nova_glue_BinaryData::button_standard_hover_pngSize, "image/png" },
        { "/img/button_standard_active.png",    nova_glue_BinaryData::button_standard_active_png,    nova_glue_BinaryData::button_standard_active_pngSize, "image/png" },
        { "/img/preset_shell_inactive.png",     nova_glue_BinaryData::preset_shell_inactive_png,     nova_glue_BinaryData::preset_shell_inactive_pngSize, "image/png" },
        { "/img/icon_button_inactive.png",      nova_glue_BinaryData::icon_button_inactive_png,      nova_glue_BinaryData::icon_button_inactive_pngSize, "image/png" },
        { "/img/icon_button_hover.png",         nova_glue_BinaryData::icon_button_hover_png,         nova_glue_BinaryData::icon_button_hover_pngSize, "image/png" },
        { "/img/gr_needle.svg",                 nova_glue_BinaryData::gr_needle_svg,                 nova_glue_BinaryData::gr_needle_svgSize, "image/svg+xml" },
        { "/img/arrow_next.svg",                nova_glue_BinaryData::arrow_next_svg,                nova_glue_BinaryData::arrow_next_svgSize, "image/svg+xml" },
        { "/img/arrow_previous.svg",            nova_glue_BinaryData::arrow_previous_svg,            nova_glue_BinaryData::arrow_previous_svgSize, "image/svg+xml" },
        { "/img/save_icon.svg",                 nova_glue_BinaryData::save_icon_svg,                 nova_glue_BinaryData::save_icon_svgSize, "image/svg+xml" },
        { "/img/settings_icon.svg",             nova_glue_BinaryData::settings_icon_svg,             nova_glue_BinaryData::settings_icon_svgSize, "image/svg+xml" },
        { "/img/screw_brass.svg",               nova_glue_BinaryData::screw_brass_svg,               nova_glue_BinaryData::screw_brass_svgSize, "image/svg+xml" },
    };

    auto makeVersionedHtmlResource = [this] (const char* data, int size, const char* mime)
    {
        auto text = juce::String::fromUTF8 (data, size);
        text = text.replace ("js/index.js", "js/index.js?v=" + assetVersion);
        for (const auto& img : images)
        {
            // img.suffix is "/img/xxx.ext"; the HTML/CSS reference omits
            // the leading slash ("img/xxx.ext").
            const juce::String ref (img.suffix + 1);
            text = text.replace (ref, ref + "?v=" + assetVersion);
        }
        const auto utf8 = text.toUTF8();
        std::vector<std::byte> bytes (utf8.sizeInBytes() - 1); // exclude trailing null
        std::memcpy (bytes.data(), utf8.getAddress(), bytes.size());
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    // NOTE: requests arrive here as scheme/host-less strings like
    // "//js/index.js" rather than an absolute URL under
    // getResourceProviderRoot() - stripping that prefix never matches
    // anything, so match on a simple query-stripped suffix of the raw url
    // instead.
    const auto pathOnly = url.upToFirstOccurrenceOf ("?", false, false).toLowerCase();
    const bool isRoot = pathOnly.isEmpty() || pathOnly == "/" || pathOnly == "//";

    constexpr const char* htmlMime = "text/html; charset=utf-8";
    constexpr const char* jsMime   = "text/javascript; charset=utf-8";

    if (isRoot || pathOnly.endsWith ("/index.html"))
        return makeVersionedHtmlResource (nova_glue_BinaryData::index_html, nova_glue_BinaryData::index_htmlSize, htmlMime);
    if (pathOnly.endsWith ("/js/index.js"))
        return makeVersionedTextResource (nova_glue_BinaryData::index_js, nova_glue_BinaryData::index_jsSize,
                                          jsMime, "./juce/index.js");
    if (pathOnly.endsWith ("/js/juce/index.js"))
        return makeResource (nova_glue_BinaryData::index_js2, nova_glue_BinaryData::index_js2Size, jsMime);
    if (pathOnly.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_glue_BinaryData::check_native_interop_js, nova_glue_BinaryData::check_native_interop_jsSize, jsMime);

    for (const auto& img : images)
        if (pathOnly.endsWith (img.suffix))
            return makeResource (img.data, img.size, img.mime);

    return std::nullopt;
}
