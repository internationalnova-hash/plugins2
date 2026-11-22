#include "PluginEditor.h"
#include "BinaryData.h"
#include "ParameterIDs.hpp"

#include <cstring>
#include <iostream>

NovaTapeAudioProcessorEditor::NovaTapeAudioProcessorEditor (NovaTapeAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    // Default window is a comfortable ~2/3 scale of the 1536x1024 design
    // canvas (same 3:2 aspect ratio, so the CSS scale-to-fit shows the
    // whole UI with no cropping) - the full 1536x1024 default was too
    // large for a typical screen, showing only a few controls at once.
    setResizeLimits (700, 467, 1800, 1200);
    setSize (1024, 683);
    startTimerHz (30);
}

NovaTapeAudioProcessorEditor::~NovaTapeAudioProcessorEditor()
{
    stopTimer();
}

void NovaTapeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0B0B0D));
}

void NovaTapeAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaTapeAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const float inLevel = processorRef.inputLevelDebug.load();
    const float outLevel = processorRef.outputLevelDebug.load();
    const float reelSpeed = processorRef.reelSpeedDebug.load();

    // reelSpeedDebug is a binary on/off gate on instantaneous input level
    // (see processBlock), so real musical material - which has gaps
    // between notes/transients - was snapping the reels to a dead stop
    // and restarting every time the signal dipped under threshold. That
    // start-stop motion read as a knob being nudged back and forth, not
    // a wheel spinning. A tape transport's reels don't do that - they
    // keep turning on momentum through brief gaps in the material, only
    // slowing once the tape actually stops rolling. Fast attack so
    // playback starts spinning immediately; slow release (~1.5s to
    // settle) so normal gaps don't visibly stop it.
    const double target = (double) reelSpeed;
    if (target > currentReelSpeed)
        currentReelSpeed += (target - currentReelSpeed) * 0.5;
    else
        currentReelSpeed += (target - currentReelSpeed) * 0.03;

    // Reel rotation angle is accumulated here (UI thread, 30Hz) - purely
    // visual, never touches processBlock. 4deg/tick at full speed =
    // 120deg/sec = one full rotation every 3s - a real reel-to-reel
    // machine's reels turn far slower than the original 36deg/tick
    // (3 rotations/sec) this was set to, which read as visibly too
    // fast/unreal.
    reelAngleDeg += 4.0 * currentReelSpeed;
    if (reelAngleDeg >= 360.0) reelAngleDeg -= 360.0;

    juce::String js;
    js << "if (window.updateTelemetry) { window.updateTelemetry("
       << juce::String (inLevel, 4) << "," << juce::String (outLevel, 4) << ","
       << juce::String (reelAngleDeg, 2) << "); }";
    webView->evaluateJavascript (js);
}

juce::WebBrowserComponent::Options NovaTapeAudioProcessorEditor::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaTape")));
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
                                 if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (param))
                                     param->setValueNotifyingHost (choiceParam->convertTo0to1 ((float) (int) args[1]));
                                 else if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                                     param->setValueNotifyingHost (ranged->convertTo0to1 ((float) (double) args[1]));
                                 else if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
                                     param->setValueNotifyingHost ((bool) args[1] ? 1.0f : 0.0f);
                             }
                         }
                         completion (juce::var());
                     })
                     .withNativeFunction ("getParamsMeta", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         static const char* const kIds[] = {
                             ParameterIDs::input, ParameterIDs::tapeAmount, ParameterIDs::tapeType, ParameterIDs::ips,
                             ParameterIDs::bias, ParameterIDs::wowFlutter, ParameterIDs::noise, ParameterIDs::lowCut,
                             ParameterIDs::highCut, ParameterIDs::mix, ParameterIDs::output, ParameterIDs::trim,
                             ParameterIDs::oversampling, ParameterIDs::bypass
                         };

                         juce::Array<juce::var> arr;
                         for (auto* id : kIds)
                         {
                             auto* param = processorRef.apvts.getParameter (id);
                             if (param == nullptr) continue;

                             auto* o = new juce::DynamicObject();
                             o->setProperty ("id", juce::String (id));
                             o->setProperty ("name", param->getName (64));
                             o->setProperty ("text", param->getCurrentValueAsText());

                             if (auto* boolParam = dynamic_cast<juce::AudioParameterBool*> (param))
                             {
                                 o->setProperty ("type", "bool");
                                 o->setProperty ("value", boolParam->get());
                             }
                             else if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (param))
                             {
                                 o->setProperty ("type", "choice");
                                 o->setProperty ("value", choiceParam->getIndex());
                                 juce::Array<juce::var> choices;
                                 for (auto& s : choiceParam->choices) choices.add (s);
                                 o->setProperty ("choices", juce::var (choices));
                             }
                             else if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                             {
                                 const auto& range = ranged->getNormalisableRange();
                                 o->setProperty ("type", "float");
                                 o->setProperty ("value", (double) ranged->convertFrom0to1 (ranged->getValue()));
                                 o->setProperty ("min", (double) range.start);
                                 o->setProperty ("max", (double) range.end);
                             }

                             arr.add (juce::var (o));
                         }
                         completion (juce::var (arr));
                     })
                     .withNativeFunction ("getPresetsMeta", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         juce::Array<juce::var> arr;
                         for (int i = 0; i < processorRef.presetManager.getNumPresets(); ++i)
                         {
                             auto* o = new juce::DynamicObject();
                             o->setProperty ("name", processorRef.presetManager.getPreset (i).name);
                             o->setProperty ("isUser", processorRef.presetManager.isUserPreset (i));
                             arr.add (juce::var (o));
                         }
                         completion (juce::var (arr));
                     })
                     .withNativeFunction ("loadPreset", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             processorRef.presetManager.applyPreset ((int) args[0], processorRef.apvts);
                         completion (juce::var());
                     })
                     .withNativeFunction ("savePreset", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         const auto name = args.isEmpty() ? juce::String ("User Preset") : args[0].toString();
                         const int newIndex = processorRef.saveCurrentAsPreset (name);
                         completion (juce::var (newIndex));
                     })
                     .withNativeFunction ("abGetState", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         auto* o = new juce::DynamicObject();
                         o->setProperty ("activeSlot", processorRef.abSnapshot.getActiveSlot() == ABSnapshot::Slot::A ? "A" : "B");
                         o->setProperty ("hasA", processorRef.abSnapshot.hasSnapshot (ABSnapshot::Slot::A));
                         o->setProperty ("hasB", processorRef.abSnapshot.hasSnapshot (ABSnapshot::Slot::B));
                         completion (juce::var (o));
                     })
                     .withNativeFunction ("abCaptureActive", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         processorRef.abSnapshot.captureActive (processorRef.apvts);
                         completion (juce::var());
                     })
                     .withNativeFunction ("abToggleAndRecall", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         processorRef.abSnapshot.toggleAndRecall (processorRef.apvts);
                         completion (juce::var());
                     })
                     .withNativeFunction ("abCopyActiveToOther", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         processorRef.abSnapshot.copyActiveToOther();
                         completion (juce::var());
                     })
                     .withNativeFunction ("jsError", [] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             std::cerr << "[NovaTape][JS ERROR] " << args[0].toString() << std::endl;
                         completion (juce::var());
                     });

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaTapeAudioProcessorEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const void* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    const auto lowerUrl = url.toLowerCase();

    if (lowerUrl.contains ("reel_left.png"))
        return makeResource (nova_tape_BinaryData::reel_left_png, nova_tape_BinaryData::reel_left_pngSize, "image/png");
    if (lowerUrl.contains ("reel_right.png"))
        return makeResource (nova_tape_BinaryData::reel_right_png, nova_tape_BinaryData::reel_right_pngSize, "image/png");
    if (lowerUrl.contains ("vu_meter_pair_housing.png"))
        return makeResource (nova_tape_BinaryData::vu_meter_pair_housing_png, nova_tape_BinaryData::vu_meter_pair_housing_pngSize, "image/png");
    if (lowerUrl.contains ("tape_head_center_housing.png"))
        return makeResource (nova_tape_BinaryData::tape_head_center_housing_png, nova_tape_BinaryData::tape_head_center_housing_pngSize, "image/png");
    if (lowerUrl.contains ("knob_large_input.png"))
        return makeResource (nova_tape_BinaryData::knob_large_input_png, nova_tape_BinaryData::knob_large_input_pngSize, "image/png");
    if (lowerUrl.contains ("knob_large_tape.png"))
        return makeResource (nova_tape_BinaryData::knob_large_tape_png, nova_tape_BinaryData::knob_large_tape_pngSize, "image/png");
    if (lowerUrl.contains ("knob_large_output.png"))
        return makeResource (nova_tape_BinaryData::knob_large_output_png, nova_tape_BinaryData::knob_large_output_pngSize, "image/png");
    if (lowerUrl.contains ("knob_small.png"))
        return makeResource (nova_tape_BinaryData::knob_small_png, nova_tape_BinaryData::knob_small_pngSize, "image/png");
    if (lowerUrl.contains ("led_meter_vertical.png"))
        return makeResource (nova_tape_BinaryData::led_meter_vertical_png, nova_tape_BinaryData::led_meter_vertical_pngSize, "image/png");
    if (lowerUrl.contains ("lamp_red.png"))
        return makeResource (nova_tape_BinaryData::lamp_red_png, nova_tape_BinaryData::lamp_red_pngSize, "image/png");
    if (lowerUrl.contains ("lamp_amber_on.png"))
        return makeResource (nova_tape_BinaryData::lamp_amber_on_png, nova_tape_BinaryData::lamp_amber_on_pngSize, "image/png");
    if (lowerUrl.contains ("lamp_amber.png"))
        return makeResource (nova_tape_BinaryData::lamp_amber_png, nova_tape_BinaryData::lamp_amber_pngSize, "image/png");
    if (lowerUrl.contains ("lamp_green.png"))
        return makeResource (nova_tape_BinaryData::lamp_green_png, nova_tape_BinaryData::lamp_green_pngSize, "image/png");
    if (lowerUrl.contains ("button_header_dark.png"))
        return makeResource (nova_tape_BinaryData::button_header_dark_png, nova_tape_BinaryData::button_header_dark_pngSize, "image/png");
    if (lowerUrl.contains ("button_header_amber.png"))
        return makeResource (nova_tape_BinaryData::button_header_amber_png, nova_tape_BinaryData::button_header_amber_pngSize, "image/png");
    if (lowerUrl.contains ("button_tape_type_blank.png"))
        return makeResource (nova_tape_BinaryData::button_tape_type_blank_png, nova_tape_BinaryData::button_tape_type_blank_pngSize, "image/png");
    if (lowerUrl.contains ("button_ips_blank.png"))
        return makeResource (nova_tape_BinaryData::button_ips_blank_png, nova_tape_BinaryData::button_ips_blank_pngSize, "image/png");
    if (lowerUrl.contains ("roller_left_large.png"))
        return makeResource (nova_tape_BinaryData::roller_left_large_png, nova_tape_BinaryData::roller_left_large_pngSize, "image/png");
    if (lowerUrl.contains ("roller_left_small.png"))
        return makeResource (nova_tape_BinaryData::roller_left_small_png, nova_tape_BinaryData::roller_left_small_pngSize, "image/png");
    if (lowerUrl.contains ("roller_right_large.png"))
        return makeResource (nova_tape_BinaryData::roller_right_large_png, nova_tape_BinaryData::roller_right_large_pngSize, "image/png");
    if (lowerUrl.contains ("roller_right_small.png"))
        return makeResource (nova_tape_BinaryData::roller_right_small_png, nova_tape_BinaryData::roller_right_small_pngSize, "image/png");
    if (lowerUrl.contains ("mix_slider_track_thumb.png"))
        return makeResource (nova_tape_BinaryData::mix_slider_track_thumb_png, nova_tape_BinaryData::mix_slider_track_thumb_pngSize, "image/png");
    if (lowerUrl.contains ("mix_thumb.png"))
        return makeResource (nova_tape_BinaryData::mix_thumb_png, nova_tape_BinaryData::mix_thumb_pngSize, "image/png");
    if (lowerUrl.contains ("footer_treatment.png"))
        return makeResource (nova_tape_BinaryData::footer_treatment_png, nova_tape_BinaryData::footer_treatment_pngSize, "image/png");
    if (lowerUrl.contains ("settings_icon.png"))
        return makeResource (nova_tape_BinaryData::settings_icon_png, nova_tape_BinaryData::settings_icon_pngSize, "image/png");
    if (lowerUrl.contains ("menu_icon_button.png"))
        return makeResource (nova_tape_BinaryData::menu_icon_button_png, nova_tape_BinaryData::menu_icon_button_pngSize, "image/png");

    if (lowerUrl.contains ("juce/index.js"))
        return makeResource (nova_tape_BinaryData::index_js2, nova_tape_BinaryData::index_js2Size, "text/javascript");

    if (lowerUrl.contains ("juce/check_native_interop.js"))
        return makeResource (nova_tape_BinaryData::check_native_interop_js, nova_tape_BinaryData::check_native_interop_jsSize, "text/javascript");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_tape_BinaryData::index_js, nova_tape_BinaryData::index_jsSize, "text/javascript");

    if (lowerUrl.contains ("index.html") || lowerUrl.endsWith ("/") || lowerUrl.isEmpty())
        return makeResource (nova_tape_BinaryData::index_html, nova_tape_BinaryData::index_htmlSize, "text/html");

    return std::nullopt;
}
