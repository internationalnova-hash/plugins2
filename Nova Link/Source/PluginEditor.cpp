#include "PluginEditor.h"
#include "NovaLicenseGate.h"
#include "BinaryData.h"
#include <cstring>

using NativeArgs = const juce::Array<juce::var>&;
using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;

static constexpr int kW = 1120, kH = 700;

NovaLinkAudioProcessorEditor::NovaLinkAudioProcessorEditor(NovaLinkAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    webView = std::make_unique<SinglePageBrowser>(createWebOptions());
    addAndMakeVisible(*webView);
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot()
                     + "index.html?v=" + juce::String(juce::Time::getCurrentTime().toMilliseconds()));
    setResizable(true, true);
    setResizeLimits(kW * 6 / 10, kH * 6 / 10, kW * 3 / 2, kH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio((double) kW / kH);
    setSize(kW, kH);
    startTimerHz(15);
    NovaLicenseOverlay::attachTo(*this);
}

NovaLinkAudioProcessorEditor::~NovaLinkAudioProcessorEditor() { stopTimer(); }

void NovaLinkAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour::fromRGB(3, 8, 15)); }
void NovaLinkAudioProcessorEditor::resized() { if (webView) webView->setBounds(getLocalBounds()); }

juce::var NovaLinkAudioProcessorEditor::buildState()
{
    auto st = processorRef.session.getStateJson();
    if (auto* o = st.getDynamicObject())
    {
        o->setProperty("myName", processorRef.displayName);
        o->setProperty("camera", processorRef.video.isRunning());
        o->setProperty("videoError", videoError);
        o->setProperty("remoteFps", processorRef.video.getRemoteFps());
        int counter = 0;
        processorRef.video.getRemoteFrame(counter);
        o->setProperty("remoteFrame", counter);
        o->setProperty("hostAllowsCamera", NovaLink::LinkVideo::hostAllowsCamera());
    }
    return st;
}

void NovaLinkAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || !webView->isVisible()) return;
    webView->evaluateJavascript("if (window.novaLinkUpdate) window.novaLinkUpdate(" + juce::JSON::toString(buildState(), true) + ");");
}

juce::WebBrowserComponent::Options NovaLinkAudioProcessorEditor::createWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {};
   #if JUCE_WINDOWS
    options = options.withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2 {}
                         .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("NovaLink")));
   #endif
    auto& proc = processorRef;
    auto& s = proc.session;
    auto ok = [] (Completion& done) { done(juce::var(true)); };

    options = options.withNativeIntegrationEnabled()
        .withResourceProvider([this] (const juce::String& url) { return getResource(url); })
        .withNativeFunction("getState", [this] (NativeArgs, Completion done) { done(buildState()); })
        .withNativeFunction("setName", [&proc, ok] (NativeArgs a, Completion done)
        {
            if (a.size() > 0 && a[0].toString().trim().isNotEmpty()) proc.displayName = a[0].toString().trim().substring(0, 40);
            ok(done);
        })
        .withNativeFunction("host", [&proc] (NativeArgs, Completion done) { done(proc.session.hostSession(proc.displayName)); })
        .withNativeFunction("join", [&proc] (NativeArgs a, Completion done)
        {
            done(a.size() > 0 && proc.session.joinSession(a[0].toString(), proc.displayName));
        })
        .withNativeFunction("leave", [&proc, ok] (NativeArgs, Completion done) { proc.video.stop(); proc.session.leave(); ok(done); })
        .withNativeFunction("copyCode", [&s, ok] (NativeArgs, Completion done) { juce::SystemClipboard::copyTextToClipboard(s.getCode()); ok(done); })
        .withNativeFunction("chat", [&s, ok] (NativeArgs a, Completion done) { if (a.size() > 0) s.sendChat(a[0].toString()); ok(done); })
        .withNativeFunction("marker", [&s, ok] (NativeArgs a, Completion done) { s.addMarker(a.size() > 0 ? a[0].toString() : juce::String("Marker")); ok(done); })
        .withNativeFunction("setSections", [&s, ok] (NativeArgs a, Completion done)
        {
            juce::Array<NovaLink::Section> list;
            if (a.size() > 0)
                if (auto* arr = a[0].getArray())
                    for (auto& x : *arr) list.add({ (double) x.getProperty("sec", 0.0), x.getProperty("name", "").toString() });
            s.setSections(list);
            ok(done);
        })
        .withNativeFunction("talk", [&s, ok] (NativeArgs a, Completion done) { s.setTalkback(a.size() > 0 && (bool) a[0]); ok(done); })
        .withNativeFunction("setRecordEnabled", [&s, ok] (NativeArgs a, Completion done) { s.setRecordEnabled(a.size() > 0 && (bool) a[0]); ok(done); })
        .withNativeFunction("setHearSelf", [&s, ok] (NativeArgs a, Completion done) { s.setHearSelf(a.size() > 0 && (bool) a[0]); ok(done); })
        .withNativeFunction("setSendClick", [&s, ok] (NativeArgs a, Completion done) { s.setSendClick(a.size() > 0 && (bool) a[0]); ok(done); })
        .withNativeFunction("setMonoTakes", [&s, ok] (NativeArgs a, Completion done) { s.setMonoTakes(a.size() > 0 && (bool) a[0]); ok(done); })
        .withNativeFunction("setLossless", [&s, ok] (NativeArgs a, Completion done) { s.setLossless(a.size() > 0 && (bool) a[0]); ok(done); })
        .withNativeFunction("setMonitor", [&s, ok] (NativeArgs a, Completion done)
        {
            const float db = a.size() > 0 ? (float) (double) a[0] : 0.0f;
            s.setMonitorGain(db <= -60.0f ? 0.0f : juce::Decibels::decibelsToGain(db));
            ok(done);
        })
        .withNativeFunction("camera", [this, &proc] (NativeArgs a, Completion done)
        {
            if (a.size() > 0 && (bool) a[0]) videoError = proc.video.start();
            else { proc.video.stop(); videoError.clear(); }
            done(videoError);
        })
        .withNativeFunction("dragTake", [this, &s] (NativeArgs a, Completion done)
        {
            const auto f = s.exportTakeAligned(a.size() > 0 ? (int) a[0] : 0);
            if (!f.existsAsFile()) { done(false); return; }
            // Drag the WAV (padded from 0:00) into the DAW; if the host won't start a drag, show it in Finder.
            if (!juce::DragAndDropContainer::performExternalDragDropOfFiles({ f.getFullPathName() }, false, this))
                f.revealToUser();
            done(true);
        })
        .withNativeFunction("playTake", [&s, ok] (NativeArgs a, Completion done) { if (a.size() > 0) s.playTake((int) a[0]); ok(done); })
        .withNativeFunction("deleteTake", [&s, ok] (NativeArgs a, Completion done) { if (a.size() > 0) s.deleteTake((int) a[0]); ok(done); })
        .withNativeFunction("revealTake", [&s, ok] (NativeArgs a, Completion done)
        {
            const auto st = s.getStateJson();
            if (auto* arr = st.getProperty("takes", {}).getArray())
                for (auto& t : *arr)
                    if ((int) t.getProperty("id", 0) == (a.size() > 0 ? (int) a[0] : 0))
                        juce::File(t.getProperty("file", "").toString()).revealToUser();
            ok(done);
        });
    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaLinkAudioProcessorEditor::getResource(const juce::String& url)
{
    auto makeResource = [] (const void* data, size_t size, const char* mime)
    {
        std::vector<std::byte> bytes(size);
        if (size > 0) std::memcpy(bytes.data(), data, size);
        return juce::WebBrowserComponent::Resource { std::move(bytes), juce::String(mime) };
    };
    const auto root = juce::WebBrowserComponent::getResourceProviderRoot();
    auto path = url.startsWith(root) ? url.substring(root.length()) : url;
    path = path.upToFirstOccurrenceOf("?", false, false).toLowerCase();
    while (path.startsWith("//")) path = path.substring(1);
    if (path.isEmpty() || path == "/") path = "/index.html";
    if (!path.startsWithChar('/')) path = "/" + path;

    // Live video frames
    if (path.endsWith("/video/remote.jpg"))
    {
        int c = 0;
        const auto f = processorRef.video.getRemoteFrame(c);
        return makeResource(f.getData(), f.getSize(), "image/jpeg");
    }
    if (path.endsWith("/video/self.jpg"))
    {
        const auto f = processorRef.video.getSelfFrame();
        return makeResource(f.getData(), f.getSize(), "image/jpeg");
    }

    if (path.endsWith("/js/juce/index.js"))
        return makeResource(nova_link_BinaryData::index_js2, (size_t) nova_link_BinaryData::index_js2Size, "text/javascript");
    if (path.endsWith("/js/juce/check_native_interop.js"))
        return makeResource(nova_link_BinaryData::check_native_interop_js, (size_t) nova_link_BinaryData::check_native_interop_jsSize, "text/javascript");
    if (path.endsWith("/js/index.js"))
        return makeResource(nova_link_BinaryData::index_js, (size_t) nova_link_BinaryData::index_jsSize, "text/javascript");
    if (path.endsWith("/n_logo.png"))
        return makeResource(nova_link_BinaryData::n_logo_png, (size_t) nova_link_BinaryData::n_logo_pngSize, "image/png");
    if (path.endsWith("/index.html"))
        return makeResource(nova_link_BinaryData::index_html, (size_t) nova_link_BinaryData::index_htmlSize, "text/html");
    return std::nullopt;
}
