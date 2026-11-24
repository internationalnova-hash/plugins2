#include "PluginEditor.h"
#include "BinaryData.h"
#include "ParameterIDs.hpp"

#include <cstring>
#include <iostream>
#include "NovaLicenseGate.h"

NovaSevenEditor::NovaSevenEditor (NovaSevenAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    setResizeLimits (700, 420, 1400, 900);
    setSize (1000, 585);
    startTimerHz (30);

    NovaLicenseOverlay::attachTo (*this);   // unlock screen until activated
}

NovaSevenEditor::~NovaSevenEditor()
{
    stopTimer();
}

void NovaSevenEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF201812));
}

void NovaSevenEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaSevenEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto vuLevel = processorRef.vuDisplayLevel.load();
    webView->evaluateJavascript ("if (window.updateVu) { window.updateVu(" + juce::String (vuLevel, 4) + "); }");
}

juce::WebBrowserComponent::Options NovaSevenEditor::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaSeven")));
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
                             ParameterIDs::inputTrim, ParameterIDs::preampGain, ParameterIDs::characterMode,
                             ParameterIDs::hpfFreq, ParameterIDs::lowFreq, ParameterIDs::lowGain,
                             ParameterIDs::midFreq, ParameterIDs::midGain, ParameterIDs::highShelfGain,
                             ParameterIDs::mix, ParameterIDs::outputTrim, ParameterIDs::phaseInvert,
                             ParameterIDs::bypass, ParameterIDs::oversamplingMode
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
                             std::cerr << "[NovaSeven][JS ERROR] " << args[0].toString() << std::endl;
                         completion (juce::var());
                     });

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaSevenEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const void* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    const auto lowerUrl = url.toLowerCase();

    if (lowerUrl.contains ("wood-strip.jpg"))
        return makeResource (nova_seven_BinaryData::woodstrip_jpg, nova_seven_BinaryData::woodstrip_jpgSize, "image/jpeg");

    if (lowerUrl.contains ("faceplate.jpg"))
        return makeResource (nova_seven_BinaryData::faceplateseamless_jpg, nova_seven_BinaryData::faceplateseamless_jpgSize, "image/jpeg");

    if (lowerUrl.contains ("knob-red.jpg"))
        return makeResource (nova_seven_BinaryData::knobred_jpg, nova_seven_BinaryData::knobred_jpgSize, "image/jpeg");

    if (lowerUrl.contains ("knob-dark.jpg"))
        return makeResource (nova_seven_BinaryData::knobdark_jpg, nova_seven_BinaryData::knobdark_jpgSize, "image/jpeg");

    if (lowerUrl.contains ("knob-cream.jpg"))
        return makeResource (nova_seven_BinaryData::knobcream_jpg, nova_seven_BinaryData::knobcream_jpgSize, "image/jpeg");

    if (lowerUrl.contains ("vu-face.jpg"))
        return makeResource (nova_seven_BinaryData::vuface_jpg, nova_seven_BinaryData::vuface_jpgSize, "image/jpeg");

    if (lowerUrl.contains ("desk-knob-red.png"))
        return makeResource (nova_seven_BinaryData::deskknobred_png, nova_seven_BinaryData::deskknobred_pngSize, "image/png");

    if (lowerUrl.contains ("desk-knob-teal.png"))
        return makeResource (nova_seven_BinaryData::deskknobteal_png, nova_seven_BinaryData::deskknobteal_pngSize, "image/png");

    if (lowerUrl.contains ("desk-knob-rim-teal.png"))
        return makeResource (nova_seven_BinaryData::deskknobrimteal_png, nova_seven_BinaryData::deskknobrimteal_pngSize, "image/png");

    if (lowerUrl.contains ("desk-knob-tan.png"))
        return makeResource (nova_seven_BinaryData::deskknobtan_png, nova_seven_BinaryData::deskknobtan_pngSize, "image/png");

    if (lowerUrl.contains ("desk-knob-rim-red.png"))
        return makeResource (nova_seven_BinaryData::deskknobrimred_png, nova_seven_BinaryData::deskknobrimred_pngSize, "image/png");

    if (lowerUrl.contains ("juce/index.js"))
        return makeResource (nova_seven_BinaryData::index_js2, nova_seven_BinaryData::index_js2Size, "text/javascript");

    if (lowerUrl.contains ("juce/check_native_interop.js"))
        return makeResource (nova_seven_BinaryData::check_native_interop_js, nova_seven_BinaryData::check_native_interop_jsSize, "text/javascript");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_seven_BinaryData::index_js, nova_seven_BinaryData::index_jsSize, "text/javascript");

    if (lowerUrl.contains ("index.html") || lowerUrl.endsWith ("/") || lowerUrl.isEmpty())
        return makeResource (nova_seven_BinaryData::index_html, nova_seven_BinaryData::index_htmlSize, "text/html");

    return std::nullopt;
}
