// Drives the real NovaMirrorProcessor: load a reference file, process audio,
// Power/Peak Control, targets, session save/reload.
//   NovaMirrorProcessorTests <reference.wav> <input.wav>
#include "PluginProcessor.h"
#include <cstdio>

static int fails = 0, passes = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("%s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    (ok ? passes : fails)++;
}
static void pump (int ms, std::function<bool()> done)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < end)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        if (done()) { juce::MessageManager::getInstance()->runDispatchLoopUntil (150); return; }
    }
}
static juce::AudioBuffer<float> readFile (const char* path, double& sr)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (juce::String (path))));
    if (r == nullptr) return {};
    sr = r->sampleRate;
    juce::AudioBuffer<float> b (2, (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (sr * 20)));
    r->read (&b, 0, b.getNumSamples(), 0, true, true);
    return b;
}
static juce::AudioBuffer<float> run (NovaMirrorProcessor& p, const juce::AudioBuffer<float>& in)
{
    juce::AudioBuffer<float> out (in);
    juce::MidiBuffer midi;
    for (int s = 0; s < out.getNumSamples(); s += 512)
    {
        const int n = std::min (512, out.getNumSamples() - s);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, s, n);
        p.processBlock (view, midi);
    }
    return out;
}
static void setP (NovaMirrorProcessor& p, const char* id, float v)
{
    auto* prm = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id));
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 3) { std::printf ("usage: NovaMirrorProcessorTests reference.wav input.wav\n"); return 2; }
    double sr = 48000, srRef = 48000;
    auto input = readFile (argv[2], sr);
    readFile (argv[1], srRef);

    auto p = std::make_unique<NovaMirrorProcessor>();
    p->prepareToPlay (sr, 512);
    check (p->getLatencySamples() > 0, "reports latency " + juce::String (p->getLatencySamples()) + " samples");

    // default: Nova Balance target is active
    auto out = run (*p, input);
    NovaMirror::EngineMeters m; p->getMeters (m);
    check (m.active, "a Nova Target is active by default");

    // load a reference file
    p->loadReferenceFile (juce::File (juce::String (argv[1])));
    pump (20000, [&] { return ! p->getInfoJson().contains ("\"loading\": true") && p->getInfoJson().contains ("\"ref\""); });
    const auto info = juce::JSON::parse (p->getInfoJson());
    check (info["kind"].toString() == "reference" && info["ref"].isObject(), "reference analysed: " + info["name"].toString());
    check ((bool) info["listenReady"], "reference audio ready for Listen");

    setP (*p, "cut", 100.f); setP (*p, "boost", 100.f);
    if (std::getenv ("NM_NOPEAK")) setP (*p, "peak", 0.f);
    out = run (*p, input);
    p->getMeters (m);
    bool finite = true;
    // compare the same stretch of audio (the output is delayed by the plugin latency)
    const int latA = p->getLatencySamples(), span = input.getNumSamples() - latA;
    float pkIn = input.getMagnitude (0, span), pkOut = out.getMagnitude (latA, span);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < out.getNumSamples(); ++i) finite = finite && std::isfinite (out.getSample (c, i));
    check (finite && m.match > 0.f, "processes cleanly (match " + juce::String (m.match, 0) + "%)");
    {
        const int h = input.getNumSamples() / 2, lt = p->getLatencySamples();
        const float rIn = input.getRMSLevel (0, h - lt, h / 2), rOut = out.getRMSLevel (0, h, h / 2);
        // with Peak Control on, holding back new peaks may cost ~1 dB; without it, loudness is exact
        const float tol = std::getenv ("NM_NOPEAK") ? 0.5f : 1.5f;
        check (std::abs (juce::Decibels::gainToDecibels (rOut / rIn)) < tol, juce::String (std::getenv ("NM_NOPEAK") ? "Auto level" : "Auto level + Peak Control") + " keeps loudness (" + juce::String (juce::Decibels::gainToDecibels (rOut / rIn), 2) + " dB)");
    }
    if (! std::getenv ("NM_NOPEAK")) check (pkOut <= pkIn * 1.02f, "Peak Control: output peak " + juce::String (juce::Decibels::gainToDecibels (pkOut), 1)
           + " dB vs input " + juce::String (juce::Decibels::gainToDecibels (pkIn), 1) + " dB");

    // Power off: the input comes back (delayed), level-matched
    setP (*p, "power", 0.f);
    out = run (*p, input);
    const int lat = p->getLatencySamples();
    const int a = input.getNumSamples() / 2, n = input.getNumSamples() / 4;
    double num = 0, den = 0;
    for (int i = a; i < a + n; ++i) { num += out.getSample (0, i) * input.getSample (0, i - lat); den += input.getSample (0, i - lat) * input.getSample (0, i - lat); }
    const double g = num / std::max (1e-12, den);
    double res = 0;
    for (int i = a; i < a + n; ++i) { const double d = out.getSample (0, i) - g * input.getSample (0, i - lat); res += d * d; }
    check (res / std::max (1e-12, den) < 1e-4 && std::abs (g - 1.0) < 0.01, "Power off plays the input in sync, unchanged (gain x" + juce::String (g, 3) + ")");
    setP (*p, "power", 1.f);

    // Nova target switch + user target save/delete
    p->setTarget ("nova", "Trap");
    check (juce::JSON::parse (p->getInfoJson())["name"].toString() == "Trap", "switch to Nova Target: Trap");
    p->setTarget ("reference", "");
    check (p->saveCurrentAsUserTarget ("Nova Mirror Test Target"), "save the reference as a user target");
    check (p->getInfoJson().contains ("Nova Mirror Test Target"), "user target listed");
    p->setTarget ("user", "Nova Mirror Test Target");
    run (*p, input);
    p->getMeters (m);
    check (m.active, "user target drives the engine");
    p->deleteUserTarget ("Nova Mirror Test Target");
    check (! p->getInfoJson().contains ("Nova Mirror Test Target"), "user target deleted");

    // session save -> reload
    p->setTarget ("reference", "");
    setP (*p, "from", 150.f);
    juce::MemoryBlock st;
    p->getStateInformation (st);
    auto q = std::make_unique<NovaMirrorProcessor>();
    q->prepareToPlay (sr, 512);
    q->setStateInformation (st.getData(), (int) st.getSize());
    pump (20000, [&] { return q->getInfoJson().contains ("\"listenReady\": true"); });
    const auto qi = juce::JSON::parse (q->getInfoJson());
    check (qi["kind"].toString() == "reference" && qi["ref"].isObject(), "reloaded session keeps the reference");
    check (std::abs (q->apvts.getRawParameterValue ("from")->load() - 150.f) < 1.f, "reloaded session keeps the settings");
    std::printf ("session state: %.1f KB\n", st.getSize() / 1024.0);

    p.reset(); q.reset();
    std::printf ("\n%d passed, %d failed\n", passes, fails);
    return fails == 0 ? 0 : 1;
}
