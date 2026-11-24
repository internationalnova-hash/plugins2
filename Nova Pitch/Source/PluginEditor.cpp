#include "PluginEditor.h"
#include "BinaryData.h"
#include "ParameterIDs.hpp"

#include <cstring>
#include <cmath>
#include <iostream>

namespace
{
    void hzToNoteInfo (float hz, juce::String& outName, int& outOctave, float& outCents)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (hz <= 0.0f) { outName = ""; outOctave = 0; outCents = 0.0f; return; }
        const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
        const int midiRounded = (int) std::round (midi);
        const int pc = ((midiRounded % 12) + 12) % 12;
        outName = names[pc];
        outOctave = midiRounded / 12 - 1;
        outCents = (midi - (float) midiRounded) * 100.0f;
    }
}

NovaPitchAudioProcessorEditor::NovaPitchAudioProcessorEditor (NovaPitchAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    applyViewSize (1100);
    startTimerHz (30);
}

void NovaPitchAudioProcessorEditor::applyViewSize (int width)
{
    // Compact is the original 1100 x 660 screen; Full adds the keyboard row (1100 x 760).
    const int designH = processorRef.compactView.load() ? 660 : 760;
    setResizeLimits (660, 660 * designH / 1100, 1980, 1980 * designH / 1100);
    setSize (width, width * designH / 1100);
}

NovaPitchAudioProcessorEditor::~NovaPitchAudioProcessorEditor()
{
    stopTimer();
}

void NovaPitchAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0A0A0F));
}

void NovaPitchAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaPitchAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const float detectedHz = processorRef.getDetectedPitch();
    const float confidence = juce::jlimit (0.0f, 1.0f, processorRef.getConfidence());
    const bool voiced = processorRef.getVoicedDebug();

    juce::String noteName;
    int octave = 0;
    float cents = 0.0f;
    hzToNoteInfo (detectedHz, noteName, octave, cents);

    juce::String levelsJson = "[";
    for (int i = 0; i < NovaPitchAudioProcessor::kLevelHistorySize; ++i)
    {
        if (i > 0) levelsJson += ",";
        levelsJson += juce::String (processorRef.getLevelHistoryAt (i), 4);
    }
    levelsJson += "]";

    const auto& harm = processorRef.getHarmony();
    juce::String partsJson = "[";
    for (int i = 0; i < NovaPitchHarmony::kParts; ++i)
        partsJson << (i > 0 ? "," : "") << juce::String (harm.getPartMidi (i), 1);
    partsJson << "]";
    juce::String voicesJson = "[";
    for (int i = 0; i < NovaPitchHarmony::kMaxVoices; ++i)
        voicesJson << (i > 0 ? "," : "") << juce::String (harm.getVoiceLevel (i), 4);
    voicesJson << "]";

    const auto script = "if (window.receiveDSP) { window.receiveDSP({"
                      "noteName:\"" + noteName + "\""
                      ",octave:" + juce::String (octave)
                      + ",cents:" + juce::String (cents, 2)
                      + ",confidence:" + juce::String (confidence, 4)
                      + ",voiced:" + juce::String (voiced ? "true" : "false")
                      + ",levels:" + levelsJson
                      + ",inMidi:" + juce::String (processorRef.getInputMidi(), 2)
                      + ",targetMidi:" + juce::String (processorRef.getTargetMidi(), 1)
                      + ",parts:" + partsJson
                      + ",voiceLevels:" + voicesJson
                      + ",activeVoices:" + juce::String (harm.getActiveVoiceCount())
                      + "}); }";

    webView->evaluateJavascript (script);
}

juce::WebBrowserComponent::Options NovaPitchAudioProcessorEditor::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaPitch")));
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
                                 if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
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
                             ParameterIDs::key, ParameterIDs::scale, ParameterIDs::tolerance, ParameterIDs::amount,
                             ParameterIDs::confidenceThreshold, ParameterIDs::vibrato, ParameterIDs::formant,
                             ParameterIDs::lowLatency, ParameterIDs::formantWarp, ParameterIDs::mix,
                             ParameterIDs::outputGain, ParameterIDs::range, ParameterIDs::vibratoRate,
                             ParameterIDs::transients, ParameterIDs::bypass, ParameterIDs::tracking,
                             ParameterIDs::harmOn, ParameterIDs::harmMode, ParameterIDs::harmStyle,
                             ParameterIDs::harmVoices, ParameterIDs::harmWidth, ParameterIDs::harmHumanize,
                             ParameterIDs::harmLead, ParameterIDs::harmMix,
                             ParameterIDs::partOn[0], ParameterIDs::partOn[1], ParameterIDs::partOn[2], ParameterIDs::partOn[3],
                             ParameterIDs::partInterval[0], ParameterIDs::partInterval[1], ParameterIDs::partInterval[2], ParameterIDs::partInterval[3],
                             ParameterIDs::partLevel[0], ParameterIDs::partLevel[1], ParameterIDs::partLevel[2], ParameterIDs::partLevel[3]
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
                     .withNativeFunction ("getCompact", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         completion (juce::var (processorRef.compactView.load()));
                     })
                     .withNativeFunction ("setCompact", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         processorRef.compactView.store (! args.isEmpty() && (bool) args[0]);
                         applyViewSize (juce::jlimit (660, 1980, getWidth()));
                         completion (juce::var());
                     })
                     .withNativeFunction ("jsError", [] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             std::cerr << "[NovaPitch][JS ERROR] " << args[0].toString() << std::endl;
                         completion (juce::var());
                     });

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaPitchAudioProcessorEditor::getResource (const juce::String& url)
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

    const auto lowerUrl = url.toLowerCase();

    if (lowerUrl.contains ("index.html"))
        return makeResource (nova_pitch_BinaryData::index_html,
                             nova_pitch_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_pitch_BinaryData::index_js,
                             nova_pitch_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerUrl.contains ("js/juce/index.js"))
        return makeResource (nova_pitch_BinaryData::index_js2,
                             nova_pitch_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerUrl.contains ("js/juce/check_native_interop.js"))
        return makeResource (nova_pitch_BinaryData::check_native_interop_js,
                             nova_pitch_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_pitch_BinaryData::index_html,
                             nova_pitch_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerPath == "/js/index.js" || lowerPath.endsWith ("/js/index.js"))
        return makeResource (nova_pitch_BinaryData::index_js,
                             nova_pitch_BinaryData::index_jsSize,
                             "text/javascript");

    if (lowerPath == "/js/juce/index.js" || lowerPath.endsWith ("/js/juce/index.js"))
        return makeResource (nova_pitch_BinaryData::index_js2,
                             nova_pitch_BinaryData::index_js2Size,
                             "text/javascript");

    if (lowerPath == "/js/juce/check_native_interop.js" || lowerPath.endsWith ("/js/juce/check_native_interop.js"))
        return makeResource (nova_pitch_BinaryData::check_native_interop_js,
                             nova_pitch_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    return std::nullopt;
}
