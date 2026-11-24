#include "PluginEditor.h"
#include "BinaryData.h"
#include "ParameterIDs.hpp"

#include <cstring>
#include <iostream>
#include "NovaLicenseGate.h"

NovaRoyal670Editor::NovaRoyal670Editor (NovaRoyal670Processor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (true, true);
    setResizeLimits (1050, 650, 2100, 1300);
    // Default open size is well below the 1580x980 design canvas (the
    // scale-to-fit mechanism shrinks the whole UI proportionally) - at
    // 1:1 the window is uncomfortably large on a typical screen.
    setSize (1140, 707);
    startTimerHz (30);

    NovaLicenseOverlay::attachTo (*this);   // unlock screen until activated
}

NovaRoyal670Editor::~NovaRoyal670Editor()
{
    stopTimer();
}

void NovaRoyal670Editor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0A0806));
}

void NovaRoyal670Editor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void NovaRoyal670Editor::timerCallback()
{
    // Deliberately NOT gated on webView->isVisible(): Component::isVisible()
    // reflects this component's own flag plus its ancestor chain, and some
    // hosts (reported: FL Studio, VST3) embed/parent the editor in a way
    // that leaves this false even while the plugin window is genuinely
    // open and rendering on screen - which would silently freeze every
    // meter/tube-glow update forever with no error. The DSP -> telemetry
    // atomics path is proven correct independently (see
    // Source/TelemetryDiag.cpp's history in git log), so the fix is here:
    // always push telemetry, and let evaluateJavascript itself be the
    // only gate (it silently no-ops if the WebView truly isn't ready).
    if (webView == nullptr)
        return;

    // Real signal, measured on the audio thread each block by the
    // engine's VU ballistics + detector - never faked/simulated.
    const auto vuLeftOrMid = processorRef.telemetryVuLeftOrMid.load();
    const auto vuRightOrSide = processorRef.telemetryVuRightOrSide.load();
    const auto gainReductionDb = processorRef.telemetryGainReductionDb.load();

    const auto telemetryJs = "if (window.updateStatus) { window.updateStatus("
                            + juce::String (vuLeftOrMid, 3) + ", "
                            + juce::String (vuRightOrSide, 3) + ", "
                            + juce::String (gainReductionDb, 3)
                            + "); }";

    webView->evaluateJavascript (telemetryJs);
}

juce::var NovaRoyal670Editor::buildParamsMeta()
{
    juce::Array<juce::var> arr;

    for (auto* param : processorRef.getParameters())
    {
        auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param);
        if (withId == nullptr)
            continue;

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", withId->paramID);
        obj->setProperty ("value", (double) param->getValue()); // normalized 0-1

        if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (param))
        {
            obj->setProperty ("type", "float");
            obj->setProperty ("min", (double) pf->getNormalisableRange().start);
            obj->setProperty ("max", (double) pf->getNormalisableRange().end);
            // getDefaultValue() on the base AudioProcessorParameter returns
            // the NORMALIZED (0-1) default - AudioParameterFloat hides its
            // own differently-typed overload as private.
            const float normalizedDefault = param->getDefaultValue();
            obj->setProperty ("default", (double) (normalizedDefault * (pf->getNormalisableRange().end - pf->getNormalisableRange().start) + pf->getNormalisableRange().start));
            obj->setProperty ("realValue", (double) pf->get());
        }
        else if (auto* pb = dynamic_cast<juce::AudioParameterBool*> (param))
        {
            obj->setProperty ("type", "bool");
            obj->setProperty ("realValue", pb->get());
        }
        else if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (param))
        {
            obj->setProperty ("type", "choice");
            juce::Array<juce::var> choices;
            for (auto& c : pc->choices)
                choices.add (c);
            obj->setProperty ("choices", choices);
            obj->setProperty ("realValue", pc->getIndex());
        }

        arr.add (juce::var (obj));
    }

    return juce::var (arr);
}

juce::WebBrowserComponent::Options NovaRoyal670Editor::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaRoyal670")));
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
                                 if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (param))
                                     param->setValueNotifyingHost (pc->convertTo0to1 ((float) (int) args[1]));
                                 else if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (param))
                                     param->setValueNotifyingHost (pf->convertTo0to1 ((float) (double) args[1]));
                                 else if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
                                     param->setValueNotifyingHost ((bool) args[1] ? 1.0f : 0.0f);
                             }
                         }
                         completion (juce::var());
                     })
                     .withNativeFunction ("getParamsMeta", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         completion (buildParamsMeta());
                     })
                     .withNativeFunction ("jsError", [] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             std::cerr << "[NovaRoyal670][JS ERROR] " << args[0].toString() << std::endl;
                         completion (juce::var());
                     });

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaRoyal670Editor::getResource (const juce::String& url)
{
    auto makeResource = [] (const void* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    // Match by substring on the raw url - proven pattern across this
    // catalog. Deliberately NOT using getResourceProviderRoot()-stripping,
    // which silently breaks all JS loading.
    const auto lowerUrl = url.toLowerCase();

    if (lowerUrl.contains ("desk-knob-red.png"))
        return makeResource (nova_royal670_BinaryData::deskknobred_png, nova_royal670_BinaryData::deskknobred_pngSize, "image/png");
    if (lowerUrl.contains ("black-knob.png"))
        return makeResource (nova_royal670_BinaryData::blackknob_png, nova_royal670_BinaryData::blackknob_pngSize, "image/png");
    if (lowerUrl.contains ("tube-strip.png"))
        return makeResource (nova_royal670_BinaryData::tubestrip_png, nova_royal670_BinaryData::tubestrip_pngSize, "image/png");
    if (lowerUrl.contains ("nameplate-dark.png"))
        return makeResource (nova_royal670_BinaryData::nameplatedark_png, nova_royal670_BinaryData::nameplatedark_pngSize, "image/png");
    if (lowerUrl.contains ("m-medallion.png"))
        return makeResource (nova_royal670_BinaryData::mmedallion_png, nova_royal670_BinaryData::mmedallion_pngSize, "image/png");
    if (lowerUrl.contains ("n_logo.png"))
        return makeResource (nova_royal670_BinaryData::n_logo_png, nova_royal670_BinaryData::n_logo_pngSize, "image/png");
    if (lowerUrl.contains ("panel-texture.png"))
        return makeResource (nova_royal670_BinaryData::paneltexture_png, nova_royal670_BinaryData::paneltexture_pngSize, "image/png");
    if (lowerUrl.contains ("screw-slotted.png"))
        return makeResource (nova_royal670_BinaryData::screwslotted_png, nova_royal670_BinaryData::screwslotted_pngSize, "image/png");
    if (lowerUrl.contains ("screw-phillips.png"))
        return makeResource (nova_royal670_BinaryData::screwphillips_png, nova_royal670_BinaryData::screwphillips_pngSize, "image/png");
    if (lowerUrl.contains ("vu-face.jpg"))
        return makeResource (nova_royal670_BinaryData::vuface_jpg, nova_royal670_BinaryData::vuface_jpgSize, "image/jpeg");

    if (lowerUrl.contains ("juce/index.js"))
        return makeResource (nova_royal670_BinaryData::index_js2, nova_royal670_BinaryData::index_js2Size, "text/javascript");

    if (lowerUrl.contains ("juce/check_native_interop.js"))
        return makeResource (nova_royal670_BinaryData::check_native_interop_js, nova_royal670_BinaryData::check_native_interop_jsSize, "text/javascript");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_royal670_BinaryData::index_js, nova_royal670_BinaryData::index_jsSize, "text/javascript");

    if (lowerUrl.contains ("index.html") || lowerUrl.endsWith ("/") || lowerUrl.isEmpty())
        return makeResource (nova_royal670_BinaryData::index_html, nova_royal670_BinaryData::index_htmlSize, "text/html");

    return std::nullopt;
}
