#include "PluginEditor.h"
#include "BinaryData.h"
#include "NovaLicenseGate.h"

#include <cstring>
#include <iostream>

namespace
{
    const char* const kParamIds[] = { "genre", "vibe", "part", "register", "bars", "density", "rhythm", "swing", "repetition",
                                      "followChords", "stayInKey", "leaveSpace", "keyTonic", "keyScale", "keyAutoGen", "keyAutoHum",
                                      "humAs", "pitchSnap", "timeSnap", "minBlip", "octave", "autoOctave", "sound", "midiOut", "volume",
                                      "playStyle", "glow", "fixTiming", "strum", "playOctave", "recWhat" };
    // changing these re-makes the hummed notes straight away (it's quick)
    const juce::StringArray kHumParams { "humAs", "pitchSnap", "timeSnap", "minBlip", "octave", "autoOctave", "keyAutoHum", "keyTonic", "keyScale" };

    const juce::Colour kBg (0xff0c0e14), kPanel (0xff14161e), kPanel2 (0xff1a1d27), kLine (0xff232634), kLine2 (0xff2c3040),
                       kText (0xffe8e9f0), kDim (0xff8b8fa3), kAccent (0xff9650ff), kAccent2 (0xffb98cff);

    struct FooterLnf : juce::LookAndFeel_V4
    {
        float s = 1.f;
        FooterLnf()
        {
            setColour (juce::ComboBox::backgroundColourId, kPanel2);
            setColour (juce::ComboBox::outlineColourId, kLine2);
            setColour (juce::ComboBox::textColourId, kText);
            setColour (juce::ComboBox::arrowColourId, kDim);
            setColour (juce::PopupMenu::backgroundColourId, kPanel);
            setColour (juce::PopupMenu::textColourId, kText);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha (0.25f));
            setColour (juce::TextButton::buttonColourId, kPanel2);
            setColour (juce::TextButton::textColourOffId, kText);
            setColour (juce::ToggleButton::textColourId, kDim);
            setColour (juce::ToggleButton::tickColourId, kAccent);
        }
        juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (12.5f * s)); }
        juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.5f * s, juce::Font::bold)); }
        juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (13.f)); }
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& c, bool over, bool) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (over ? c.brighter (0.08f) : c);
            g.fillRoundedRectangle (r, 7.f * s);
            g.setColour (kLine2);
            g.drawRoundedRectangle (r, 7.f * s, 1.f);
        }
        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
            g.setColour (kPanel2); g.fillRoundedRectangle (r, 6.f * s);
            g.setColour (kLine2); g.drawRoundedRectangle (r, 6.f * s, 1.f);
            juce::Path p; const float cx = (float) w - 12.f * s, cy = (float) h * 0.5f;
            p.addTriangle (cx - 4 * s, cy - 2 * s, cx + 4 * s, cy - 2 * s, cx, cy + 3 * s);
            g.setColour (kDim); g.fillPath (p);
        }
        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds (juce::roundToInt (8 * s), 1, box.getWidth() - juce::roundToInt (28 * s), box.getHeight() - 2);
            label.setFont (getComboBoxFont (box));
        }
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
        {
            const float h = 17.f * s, w = 30.f * s;
            juce::Rectangle<float> sw (0, (b.getHeight() - h) * 0.5f, w, h);
            g.setColour (b.getToggleState() ? kAccent : juce::Colour (0xff333849));
            g.fillRoundedRectangle (sw, h * 0.5f);
            g.setColour (juce::Colours::white);
            const float d = h - 4 * s;
            g.fillEllipse (b.getToggleState() ? sw.getRight() - d - 2 * s : sw.getX() + 2 * s, sw.getY() + 2 * s, d, d);
            g.setColour (b.getToggleState() ? kText : kDim);
            g.setFont (juce::Font (juce::FontOptions (12.f * s)));
            g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft ((int) (w + 8 * s)), juce::Justification::centredLeft);
        }
    };
}

// ── footer pieces ───────────────────────────────────────────────────────────
void NovaMelodyEditor::PlayButton::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (1.f);
    g.setColour (over ? kPanel2.brighter (0.1f) : kPanel2);
    g.fillEllipse (r);
    g.setColour (playing ? kAccent : kLine2);
    g.drawEllipse (r, 1.2f);
    g.setColour (juce::Colours::white);
    const float c = r.getWidth() * 0.5f, cx = r.getCentreX(), cy = r.getCentreY();
    if (playing) { g.fillRect (cx - c * 0.32f, cy - c * 0.32f, c * 0.64f, c * 0.64f); return; }
    juce::Path p;
    p.addTriangle (cx - c * 0.25f, cy - c * 0.38f, cx - c * 0.25f, cy + c * 0.38f, cx + c * 0.42f, cy);
    g.fillPath (p);
}

void NovaMelodyEditor::DragHandle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float s = ed.scale();
    g.setColour (kAccent.withAlpha (isMouseOver() ? 0.22f : 0.12f));
    g.fillRoundedRectangle (r, 8.f * s);
    juce::Path dash;
    dash.addRoundedRectangle (r, 8.f * s);
    juce::PathStrokeType st (1.2f);
    const float lens[] = { 4.f, 3.f };
    juce::Path dashed;
    st.createDashedStroke (dashed, dash, lens, 2);
    g.setColour (kAccent.withAlpha (0.7f));
    g.fillPath (dashed);
    g.setColour (kText);
    g.setFont (juce::Font (juce::FontOptions (12.5f * s)));
    juce::AttributedString a;
    a.append (juce::String::fromUTF8 ("\xe2\xa0\xbf  Drag "), juce::Font (juce::FontOptions (12.5f * s)), kText);
    a.append ("MIDI", juce::Font (juce::FontOptions (12.5f * s, juce::Font::bold)), kAccent2);
    a.append (" into your DAW", juce::Font (juce::FontOptions (12.5f * s)), kText);
    a.setJustification (juce::Justification::centred);
    a.draw (g, r);
}

void NovaMelodyEditor::DragHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 4) return;
    dragging = true;
    const auto f = ed.writeTempMidi();
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
}

// ── editor ──────────────────────────────────────────────────────────────────
NovaMelodyEditor::NovaMelodyEditor (NovaMelodyProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    lnf = std::make_unique<FooterLnf>();
    webView = std::make_unique<SinglePageBrowser> (createWebOptions());
    addAndMakeVisible (*webView);
    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot() + "index.html?v="
                      + juce::String (juce::Time::getCurrentTime().toMilliseconds()));

    playBtn.onClick = [this] { proc.setPreview (! proc.isPreviewing()); };
    addAndMakeVisible (playBtn);
    for (int i = 0; i < NovaMelody::kNumSounds; ++i) soundBox.addItem (juce::String ("Sound: ") + NovaMelody::soundName (i), i + 1);
    soundBox.addSeparator();
    soundBox.addItem ("Sound: Off (MIDI only)", NovaMelody::kSoundOff + 1);
    soundBox.setLookAndFeel (lnf.get());
    soundAttach = std::make_unique<juce::ComboBoxParameterAttachment> (*proc.apvts.getParameter ("sound"), soundBox);
    addAndMakeVisible (soundBox);
    midiOutBtn.setLookAndFeel (lnf.get());
    midiAttach = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter ("midiOut"), midiOutBtn);
    addAndMakeVisible (midiOutBtn);
    addAndMakeVisible (dragHandle);
    exportBtn.setLookAndFeel (lnf.get());
    exportBtn.onClick = [this] { exportMidi(); };
    addAndMakeVisible (exportBtn);

    setResizable (true, true);
    setResizeLimits (kW * 7 / 10, (kPageH + kFootH) * 7 / 10, kW * 2, (kPageH + kFootH) * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kW / (double) (kPageH + kFootH));
    setSize (kW, kPageH + kFootH);
    startTimerHz (30);

    NovaLicenseOverlay::attachTo (*this);
}

NovaMelodyEditor::~NovaMelodyEditor()
{
    stopTimer();
    soundBox.setLookAndFeel (nullptr);
    midiOutBtn.setLookAndFeel (nullptr);
    exportBtn.setLookAndFeel (nullptr);
}

void NovaMelodyEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    const float s = scale();
    auto foot = getLocalBounds().removeFromBottom (juce::roundToInt (kFootH * s));
    g.setColour (kPanel);
    g.fillRect (foot);
    g.setColour (kLine);
    g.drawHorizontalLine (foot.getY(), 0.f, (float) getWidth());
}

void NovaMelodyEditor::resized()
{
    const float s = scale();
    static_cast<FooterLnf*> (lnf.get())->s = s;
    auto r = getLocalBounds();
    auto foot = r.removeFromBottom (juce::roundToInt (kFootH * s));
    if (webView != nullptr) webView->setBounds (r);

    auto f = foot.reduced (juce::roundToInt (18 * s), 0);
    const int h = juce::roundToInt (34 * s);
    auto row = [&] (int w) { auto b = f.removeFromLeft (juce::roundToInt (w * s)); f.removeFromLeft (juce::roundToInt (12 * s)); return b.withSizeKeepingCentre (b.getWidth(), h); };
    playBtn.setBounds (row (40).withSizeKeepingCentre (juce::roundToInt (40 * s), juce::roundToInt (40 * s)));
    soundBox.setBounds (row (180));
    midiOutBtn.setBounds (row (230));
    auto rightSide = [&] (int w) { auto b = f.removeFromRight (juce::roundToInt (w * s)); f.removeFromRight (juce::roundToInt (12 * s)); return b.withSizeKeepingCentre (b.getWidth(), h); };
    exportBtn.setBounds (rightSide (120));
    dragHandle.setBounds (rightSide (240));
}

void NovaMelodyEditor::timerCallback()
{
    if (playBtn.playing != proc.isPreviewing()) { playBtn.playing = proc.isPreviewing(); playBtn.repaint(); }
    if (webView == nullptr || ! pageReady) return;

    if (sentState != proc.getStateVersion())
    {
        sentState = proc.getStateVersion();
        webView->evaluateJavascript ("window.NMel && NMel.onState(" + proc.getStateJson() + ")");
    }
    juce::String params ("{");
    bool first = true;
    for (auto* id : kParamIds)
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id)))
        {
            params << (first ? "" : ",") << "\"" << id << "\":" << juce::String (prm->convertFrom0to1 (prm->getValue()), 3);
            first = false;
        }
    params << "}";
    if (params != sentParams)
    {
        sentParams = params;
        webView->evaluateJavascript ("window.NMel && NMel.onParams(" + params + ")");
    }
    webView->evaluateJavascript ("window.NMel && NMel.onFrame(" + proc.getFrameJson() + ")");
}

juce::File NovaMelodyEditor::writeTempMidi()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Nova Melody");
    dir.createDirectory();
    auto f = dir.getChildFile (juce::File::createLegalFileName (proc.suggestedMidiName()) + ".mid");
    f.deleteFile();
    return f.replaceWithData (proc.currentMidiFile().getData(), proc.currentMidiFile().getSize()) ? f : juce::File();
}

void NovaMelodyEditor::exportMidi()
{
    chooser = std::make_unique<juce::FileChooser> ("Export MIDI",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (juce::File::createLegalFileName (proc.suggestedMidiName()) + ".mid"),
        "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              f = f.withFileExtension ("mid");
                              const auto mb = proc.currentMidiFile();
                              f.replaceWithData (mb.getData(), mb.getSize());
                          });
}

juce::WebBrowserComponent::Options NovaMelodyEditor::createWebOptions()
{
    using Fn = juce::WebBrowserComponent::NativeFunctionCompletion;
    using Args = juce::Array<juce::var>;
    auto options = juce::WebBrowserComponent::Options {};
   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                         .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("NovaMelody")));
   #endif
    return options.withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
        .withNativeFunction ("ready", [this] (const Args&, Fn done)
        {
            pageReady = true; sentState = -1; sentParams = {};
            done ({});
        })
        .withNativeFunction ("setParam", [this] (const Args& a, Fn done)
        {
            if (a.size() >= 2)
                if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (a[0].toString())))
                {
                    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (double) a[1]));
                    if (kHumParams.contains (a[0].toString())) proc.remakeHum();
                }
            done ({});
        })
        .withNativeFunction ("gesture", [this] (const Args& a, Fn done)
        {
            if (a.size() >= 2)
                if (auto* prm = proc.apvts.getParameter (a[0].toString()))
                    (bool) a[1] ? prm->beginChangeGesture() : prm->endChangeGesture();
            done ({});
        })
        .withNativeFunction ("act", [this] (const Args& a, Fn done)
        {
            const auto what = a.isEmpty() ? juce::String() : a[0].toString();
            const auto arg = a.size() > 1 ? a[1] : juce::var();
            if (what == "generate") proc.generateAll();
            else if (what == "select") proc.selectVariation ((int) arg);
            else if (what == "lock") proc.toggleLock ((int) arg);
            else if (what == "clearLocks") proc.clearLocks();
            else if (what == "redo") proc.redoRest();
            else if (what == "mode") { proc.setMode ((int) arg); proc.remakeHum(); }
            else if (what == "record") proc.startRecording ((bool) arg);
            else if (what == "stop") proc.stopRecording();
            else if (what == "humExtra") proc.humExtra ((int) arg);
            else if (what == "preview") proc.setPreview ((bool) arg);
            else if (what == "playRec") (bool) arg ? proc.startPlayRecording() : proc.stopPlayRecording();
            else if (what == "clearTake") proc.clearTake();
            else if (what == "chooseFile")
            {
                chooser = std::make_unique<juce::FileChooser> ("Choose a voice memo or recording", juce::File(),
                                                               "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.caf");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                      [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) proc.loadHumFile (fc.getResult()); });
            }
            done ({});
        })
        .withNativeFunction ("pad", [this] (const Args& a, Fn done)
        {
            // kind (0 melody pad, 1 chord pad, 3 tap key), index, down, velocity
            if (a.size() >= 3)
            {
                if ((bool) a[2]) proc.padDown ((int) a[0], (int) a[1], a.size() > 3 ? (int) a[3] : 100);
                else proc.padUp ((int) a[0], (int) a[1]);
            }
            done ({});
        })
        .withNativeFunction ("dropChunk", [this] (const Args& a, Fn done)
        {
            // name, base64 chunk, index, total
            if (a.size() >= 4)
            {
                if ((int) a[2] == 0) { dropData.reset(); dropName = a[0].toString(); }
                juce::MemoryOutputStream mo (dropData, true);
                juce::Base64::convertFromBase64 (mo, a[1].toString());
                mo.flush();
                if ((int) a[2] == (int) a[3] - 1) { proc.loadHumBytes (dropName, dropData); dropData.reset(); }
            }
            done ({});
        })
        .withNativeFunction ("jsError", [] (const Args& a, Fn done)
        {
            if (! a.isEmpty()) std::cerr << "[NovaMelody][JS] " << a[0].toString() << std::endl;
            done ({});
        });
}

std::optional<juce::WebBrowserComponent::Resource> NovaMelodyEditor::getResource (const juce::String& url)
{
    auto make = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes ((size_t) size);
        std::memcpy (bytes.data(), data, (size_t) size);
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };
    // JUCE may hand us the full URL or just the path
    const auto root = juce::WebBrowserComponent::getResourceProviderRoot();
    auto path = (url.startsWith (root) ? url.substring (root.length()) : url).upToFirstOccurrenceOf ("?", false, false).toLowerCase();
    if (! path.startsWithChar ('/')) path = "/" + path;
    if (path == "/") path = "/index.html";

    if (path.endsWith ("/js/juce/index.js"))
        return make (nova_melody_BinaryData::index_js2, nova_melody_BinaryData::index_js2Size, "text/javascript");
    if (path.endsWith ("/js/juce/check_native_interop.js"))
        return make (nova_melody_BinaryData::check_native_interop_js, nova_melody_BinaryData::check_native_interop_jsSize, "text/javascript");
    if (path.endsWith ("/js/index.js"))
        return make (nova_melody_BinaryData::index_js, nova_melody_BinaryData::index_jsSize, "text/javascript");
    if (path.endsWith ("/index.html"))
        return make (nova_melody_BinaryData::index_html, nova_melody_BinaryData::index_htmlSize, "text/html");
    return std::nullopt;
}
