#include "PluginEditor.h"
#include "BinaryData.h"
#include "ParameterIDs.hpp"

#include <cstring>
#include <iostream>
#include <cmath>

NovaDeskEQEditor::NovaDeskEQEditor (NovaDeskEQAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    // Window proportions measured directly from the locked mockup: total
    // plugin canvas 1536x595px -> header/strip/bottom-bar heights of
    // 13.78% / 66.89% / 19.33%, replicated exactly in index.html's CSS at
    // this base width. Resize limits keep close to that aspect ratio.
    setResizable (true, true);
    setResizeLimits (900, 350, 1600, 700);
    setSize (1180, 530);
    startTimerHz (30);
}

NovaDeskEQEditor::~NovaDeskEQEditor()
{
    stopTimer();
}

void NovaDeskEQEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF1a1712));
}

void NovaDeskEQEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaDeskEQEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto inLevel = processorRef.inputMeterLevel.load();
    const auto outLevel = processorRef.outputMeterLevel.load();
    webView->evaluateJavascript ("if (window.updateMeters) { window.updateMeters(" + juce::String (inLevel, 4) + ", " + juce::String (outLevel, 4) + "); }");

    // Live automation reflection: push every param's current normalized
    // value so host automation (or an A/B recall) is visually reflected
    // in the knobs even when the user isn't the one turning them. The JS
    // side skips updating any knob the user is CURRENTLY dragging, so
    // this never fights an in-progress local drag. Runs at the same 30Hz
    // as the meters - cheap (24 floats), no separate timer needed.
    static const char* const kIds[] = {
        ParameterIDs::input, ParameterIDs::inputIn,
        ParameterIDs::output, ParameterIDs::outputIn,
        ParameterIDs::hpfFreqIndex, ParameterIDs::hpfIn,
        ParameterIDs::lpfFreqIndex, ParameterIDs::lpfIn,
        ParameterIDs::lowFreq, ParameterIDs::lowGain, ParameterIDs::lowType,
        ParameterIDs::lowMidFreq, ParameterIDs::lowMidGain, ParameterIDs::lowMidQ,
        ParameterIDs::highMidFreq, ParameterIDs::highMidGain, ParameterIDs::highMidQ,
        ParameterIDs::highFreq, ParameterIDs::highGain, ParameterIDs::highType,
        ParameterIDs::egMode, ParameterIDs::drive, ParameterIDs::mix,
        ParameterIDs::bypass
    };

    juce::String js = "if (window.onParamsPushed) { window.onParamsPushed({";
    bool first = true;
    for (auto* id : kIds)
    {
        if (auto* param = processorRef.apvts.getParameter (id))
        {
            if (! first) js << ",";
            js << "\"" << id << "\":" << juce::String (param->getValue(), 6);
            first = false;
        }
    }
    js << "}); }";
    webView->evaluateJavascript (js);
}

juce::WebBrowserComponent::Options NovaDeskEQEditor::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaDeskEQ")));
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
                                 // The JS UI always sends an already-normalised 0..1 value
                                 // (see app.js setParamLive/nf.setParam call sites) - pass it
                                 // straight through. Do NOT run it through convertTo0to1(),
                                 // which expects a raw engineering-units value and would
                                 // double-convert, collapsing every drag to a value within a
                                 // hair of the parameter's midpoint.
                                 param->setValueNotifyingHost ((float) (double) args[1]);
                             }
                         }
                         completion (juce::var());
                     })
                     .withNativeFunction ("getParam", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                         {
                             if (auto* param = processorRef.apvts.getParameter (args[0].toString()))
                             {
                                 completion (juce::var (param->getValue()));
                                 return;
                             }
                         }
                         completion (juce::var());
                     })
                     .withNativeFunction ("getParamsMeta", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         static const char* const kIds[] = {
                             ParameterIDs::input, ParameterIDs::inputIn,
                             ParameterIDs::output, ParameterIDs::outputIn,
                             ParameterIDs::hpfFreqIndex, ParameterIDs::hpfIn,
                             ParameterIDs::lpfFreqIndex, ParameterIDs::lpfIn,
                             ParameterIDs::lowFreq, ParameterIDs::lowGain, ParameterIDs::lowType,
                             ParameterIDs::lowMidFreq, ParameterIDs::lowMidGain, ParameterIDs::lowMidQ,
                             ParameterIDs::highMidFreq, ParameterIDs::highMidGain, ParameterIDs::highMidQ,
                             ParameterIDs::highFreq, ParameterIDs::highGain, ParameterIDs::highType,
                             ParameterIDs::egMode, ParameterIDs::drive, ParameterIDs::mix,
                             ParameterIDs::bypass
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
                                 // Frequency knobs use a skewed range (see
                                 // createParameterLayout()) - the JS side
                                 // needs this to replicate JUCE's own
                                 // convertFrom0to1() skew formula for its
                                 // live-drag readout, not a linear guess.
                                 o->setProperty ("skew", (double) range.skew);
                             }

                             arr.add (juce::var (o));
                         }
                         completion (juce::var (arr));
                     })
                     .withNativeFunction ("listPresets", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         juce::Array<juce::var> arr;
                         for (int i = 0; i < processorRef.presetManager.getNumPresets(); ++i)
                         {
                             const auto& preset = processorRef.presetManager.getPreset (i);
                             auto* o = new juce::DynamicObject();
                             o->setProperty ("index", i);
                             o->setProperty ("id", preset.id);
                             o->setProperty ("name", preset.displayName);
                             o->setProperty ("category", preset.category);
                             arr.add (juce::var (o));
                         }
                         completion (juce::var (arr));
                     })
                     .withNativeFunction ("getCurrentPresetIndex", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         completion (juce::var (processorRef.getCurrentPresetIndex()));
                     })
                     .withNativeFunction ("loadPreset", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             processorRef.loadPreset ((int) args[0]);
                         completion (juce::var (processorRef.getCurrentPresetIndex()));
                     })
                     .withNativeFunction ("savePreset", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         const auto name = args.isEmpty() ? juce::String() : args[0].toString();
                         const auto newIndex = processorRef.saveCurrentAsPreset (name);
                         completion (juce::var (newIndex));
                     })
                     .withNativeFunction ("loadNextPreset", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         processorRef.loadNextPreset();
                         completion (juce::var (processorRef.getCurrentPresetIndex()));
                     })
                     .withNativeFunction ("loadPreviousPreset", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         processorRef.loadPreviousPreset();
                         completion (juce::var (processorRef.getCurrentPresetIndex()));
                     })
                     .withNativeFunction ("getEqCurve", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         // Log-spaced 20Hz-20kHz sweep of the REAL analytic
                         // combined magnitude response (HPF+4 bands+LPF).
                         static constexpr int kNumPoints = 64;
                         juce::Array<juce::var> arr;
                         for (int i = 0; i < kNumPoints; ++i)
                         {
                             const double t = (double) i / (double) (kNumPoints - 1);
                             const double freq = 20.0 * std::pow (1000.0, t); // 20Hz -> 20kHz
                             arr.add (juce::var ((double) processorRef.getEqCurveMagnitudeDb (freq)));
                         }
                         completion (juce::var (arr));
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
                             std::cerr << "[NovaDeskEQ][JS ERROR] " << args[0].toString() << std::endl;
                         completion (juce::var());
                     });

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaDeskEQEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const void* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    const auto lowerUrl = url.toLowerCase();

    if (lowerUrl.contains ("juce/index.js"))
        return makeResource (nova_desk_eq_BinaryData::index_js, nova_desk_eq_BinaryData::index_jsSize, "text/javascript");

    if (lowerUrl.contains ("juce/check_native_interop.js"))
        return makeResource (nova_desk_eq_BinaryData::check_native_interop_js, nova_desk_eq_BinaryData::check_native_interop_jsSize, "text/javascript");

    if (lowerUrl.contains ("js/app.js"))
        return makeResource (nova_desk_eq_BinaryData::app_js, nova_desk_eq_BinaryData::app_jsSize, "text/javascript");

    if (lowerUrl.contains ("img/mockup-faceplate-tile.jpg"))
        return makeResource (nova_desk_eq_BinaryData::mockupfaceplatetile_jpg, nova_desk_eq_BinaryData::mockupfaceplatetile_jpgSize, "image/jpeg");

    if (lowerUrl.contains ("img/knob-red.png"))
        return makeResource (nova_desk_eq_BinaryData::knobred_png, nova_desk_eq_BinaryData::knobred_pngSize, "image/png");

    if (lowerUrl.contains ("img/knob-tan.png"))
        return makeResource (nova_desk_eq_BinaryData::knobtan_png, nova_desk_eq_BinaryData::knobtan_pngSize, "image/png");

    if (lowerUrl.contains ("img/knob-green.png"))
        return makeResource (nova_desk_eq_BinaryData::knobgreen_png, nova_desk_eq_BinaryData::knobgreen_pngSize, "image/png");

    if (lowerUrl.contains ("img/knob-blue.png"))
        return makeResource (nova_desk_eq_BinaryData::knobblue_png, nova_desk_eq_BinaryData::knobblue_pngSize, "image/png");

    if (lowerUrl.contains ("img/knob-rim-red.png"))
        return makeResource (nova_desk_eq_BinaryData::knobrimred_png, nova_desk_eq_BinaryData::knobrimred_pngSize, "image/png");

    if (lowerUrl.contains ("img/knob-rim-green.png"))
        return makeResource (nova_desk_eq_BinaryData::knobrimgreen_png, nova_desk_eq_BinaryData::knobrimgreen_pngSize, "image/png");

    if (lowerUrl.contains ("img/mockup-wood.jpg"))
        return makeResource (nova_desk_eq_BinaryData::mockupwood_jpg, nova_desk_eq_BinaryData::mockupwood_jpgSize, "image/jpeg");

    if (lowerUrl.contains ("index.html") || lowerUrl.endsWith ("/") || lowerUrl.isEmpty())
        return makeResource (nova_desk_eq_BinaryData::index_html, nova_desk_eq_BinaryData::index_htmlSize, "text/html");

    return std::nullopt;
}
