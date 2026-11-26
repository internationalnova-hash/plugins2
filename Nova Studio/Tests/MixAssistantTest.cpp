// Mix Assistant checks on real sessions: what each track is, as heard.
//   NovaStudioMixTest <session.novastudio> [...]
#include <JuceHeader.h>
#include "../Source/Session.h"
#include "../Source/mix/MixAnalysis.h"
#include "../Source/mix/MixPlanner.h"
#include "../Source/mix/MixApplier.h"
#include "../Source/StudioAudioEngine.h"
#include "../Source/ui/MixAssistantWindow.h"
#include <cstdio>
#include "NovaAIMasterEngine.h"

// --master-check in.wav out.wav [Streaming|Balanced|Loud]: Nova AI Master's engine alone (Punchy, 0.5)
static int masterCheck (const juce::File& in, const juce::File& outF, int loud)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (in));
    if (! rd) return 1;
    const int n = (int) rd->lengthInSamples;
    juce::AudioBuffer<float> all (2, n); rd->read (&all, 0, n, 0, true, true);
    NovaAIMasterEngine eng; eng.prepare ({ rd->sampleRate, 512, 2 });
    for (int p = 0; p < n; p += 8192) { const int k = juce::jmin (8192, n - p); juce::AudioBuffer<float> v (all.getArrayOfWritePointers(), 2, p, k); eng.analyze (v); }
    eng.finishAnalysis();
    NovaMasterUserPrefs pr; pr.style = NovaMasterStyle::Punchy; pr.loudness = (NovaLoudnessMode) loud;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto plan = eng.calibrateLoudness (eng.decide (pr), all);
    std::printf ("calibrated in %.1f s\n", (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0);
    std::printf ("in %.1f LUFS, plan gain %.1f target %.1f\n", eng.getProfile().integratedLufs, plan.output.gainDb, plan.output.targetLufs);
    eng.setPlan (plan);
    for (int p = 0; p < n; p += 512) { const int k = juce::jmin (512, n - p); juce::AudioBuffer<float> v (all.getArrayOfWritePointers(), 2, p, k); eng.process (v); }
    outF.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (outF.createOutputStream().release(), rd->sampleRate, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (all, 0, n);
    return 0;
}

using namespace NovaStudio;

// --bisect in.wav: which stage makes the master depend on block size
static float lufsOf (const juce::AudioBuffer<float>& b, double sr)
{
    NovaLoudnessAnalyzer m; m.prepare (sr); m.process (b); m.finalize(); return m.getIntegratedLufs();
}
static int bisect (const juce::File& in)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (in));
    const int n = (int) rd->lengthInSamples; const double sr = rd->sampleRate;
    juce::AudioBuffer<float> src (2, n); rd->read (&src, 0, n, 0, true, true);
    NovaAIMasterEngine eng; eng.prepare ({ sr, 4096, 2 }); eng.analyze (src); eng.finishAnalysis();
    NovaMasterUserPrefs pr; pr.style = NovaMasterStyle::Punchy;
    const auto base = eng.calibrateLoudness (eng.decide (pr), src);
    auto render = [&] (const NovaMasteringPlan& pl, int B)
    {
        NovaMasteringChain c; c.prepare ({ sr, (juce::uint32) B, 2 }); c.reset(); c.setPlan (pl);
        juce::AudioBuffer<float> w; w.makeCopyOf (src);
        for (int p = 0; p < n; p += B) { juce::AudioBuffer<float> v (w.getArrayOfWritePointers(), 2, p, juce::jmin (B, n - p)); c.process (v); }
        return lufsOf (w, sr);
    };
    const char* names[] = { "all on", "-eq", "-resonance", "-glue", "-saturation", "-stereo", "-clipper", "-limiter", "-gain" };
    for (int k = 0; k < 9; ++k)
    {
        auto pl = base;
        switch (k) { case 1: pl.correctiveEQ.active = false; break; case 2: pl.resonanceControl.active = false; break;
                     case 3: pl.compression.active = false; break; case 4: pl.saturation.active = false; break;
                     case 5: pl.stereo.active = false; break; case 6: pl.clipping.active = false; break;
                     case 7: pl.limiting.active = false; break; case 8: pl.output.applyGain = false; break; default: break; }
        std::printf ("%-12s 4096: %6.2f  512: %6.2f\n", names[k], render (pl, 4096), render (pl, 512));
    }
    return 0;
}

// --ui-shot <session> <out.png> [selectBus]: the Mix Assistant window as drawn, after it has listened
static int uiShot (const juce::File& session, const juce::File& png, int select)
{
    StudioAudioEngine engine;
    engine.initialize();
    if (! engine.loadSession (session)) return 1;
    NovaStudioUI::MixAssistantPanel panel (engine, {});
    panel.setSize (1200, 740);
    for (int i = 0; i < 600 && ! panel.isReady(); ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    if (select >= -3) panel.select (select);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    auto img = panel.createComponentSnapshot (panel.getLocalBounds(), true, 1.f);
    png.deleteFile();
    juce::FileOutputStream out (png);
    juce::PNGImageFormat().writeImageToStream (img, out);
    std::printf ("wrote %s\n", png.getFullPathName().toRawUTF8());
    engine.shutdown();
    return 0;
}

// --ui-apply <session> <outdir>: the window's own Apply (background AI Master render), then Before/After
static int uiApply (const juce::File& session, const juce::File& outDir)
{
    StudioAudioEngine engine;
    engine.initialize();
    if (! engine.loadSession (session)) return 1;
    outDir.createDirectory();
    const int tracksBefore = engine.getTrackCount();
    juce::File before, after;
    int sessionChanges = 0;
    NovaStudioUI::MixAssistantPanel::Callbacks cb;
    cb.sessionChanged = [&] { ++sessionChanges; };
    cb.applied = [&] (const juce::File& b, const juce::File& a) { before = b; after = a; };
    cb.status = [] (const juce::String& s) { std::printf ("status: %s\n", s.toRawUTF8()); };
    NovaStudioUI::MixAssistantPanel panel (engine, cb);
    panel.setSize (1200, 740);
    auto pump = [] (std::function<bool()> done, int maxMs)
    {
        for (int t = 0; t < maxMs && ! done(); t += 50) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        return done();
    };
    if (! pump ([&] { return panel.isReady(); }, 60000)) { std::printf ("never finished listening\n"); return 2; }
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    panel.pressApply();
    std::printf ("apply returned after %.1f s (UI free while Nova AI Master listens)\n", (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0);
    if (! pump ([&] { return after.existsAsFile(); }, 180000)) { std::printf ("apply never finished\n"); return 3; }
    std::printf ("applied in %.1f s: tracks %d -> %d, master plugins %d, session refreshes %d\n",
                 (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, tracksBefore, engine.getTrackCount(), engine.getMasterPluginCount(), sessionChanges);
    auto render = [&] (const char* name)
    {
        StudioAudioEngine::ExportOptions o; o.tailSeconds = 1.0;
        const auto r = engine.exportAudio (outDir.getChildFile (juce::String (name) + ".wav"), o);
        std::printf ("  %-7s tracks %2d, master plugins %d, peak %.1f dB\n", name, engine.getTrackCount(), engine.getMasterPluginCount(), r.peakDb);
    };
    render ("after");
    panel.pressBeforeAfter (false);
    render ("before");
    panel.pressBeforeAfter (true);
    render ("after2");
    engine.shutdown();
    return 0;
}

static int applyMode (const juce::File& session, const juce::File& outDir, const juce::String& style)
{
    std::fprintf (stderr, "[step] engine\n");
    StudioAudioEngine engine;
    std::fprintf (stderr, "[step] initialize\n");
    engine.initialize();
    std::fprintf (stderr, "[step] load\n");
    if (! engine.loadSession (session)) { std::printf ("can't load %s\n", session.getFullPathName().toRawUTF8()); return 1; }
    outDir.createDirectory();
    StudioAudioEngine::ExportOptions eo;
    eo.tailSeconds = 2.0;
    std::fprintf (stderr, "[step] export before\n");
    auto before = engine.exportAudio (outDir.getChildFile ("before.wav"), eo);
    std::printf ("before: peak %.1f dB, %.1f s\n", before.peakDb, before.seconds);

    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    std::fprintf (stderr, "[step] analyse\n");
    auto a = MixAI::analyse (engine.getSession());
    std::vector<MixAI::Role> roles;
    for (auto& t : a.tracks) roles.push_back (t.role);
    MixAI::Options o;
    o.style = style;
    std::fprintf (stderr, "[step] plan\n");
    auto plan = MixAI::makePlan (a, roles, engine.getSession(), o);
    std::fprintf (stderr, "[step] planned\n");
    std::printf ("plan: %d buses (of %d free), %d changes\n", (int) plan.buses.size(), plan.busesAvailable, plan.changeCount());
    for (auto& b : plan.buses)
    {
        juce::StringArray chain; for (auto& c : b.chain) chain.add (c.plugin);
        std::printf ("  BUS %-12s members %d  chain [%s]  sends %d\n", b.name.toRawUTF8(), (int) b.members.size(), chain.joinIntoString (", ").toRawUTF8(), (int) b.sends.size());
    }
    for (auto& m : plan.tracks)
    {
        if (! m.change) continue;
        juce::StringArray chain; for (auto& c : m.chain) chain.add (c.plugin);
        std::printf ("  %-30s %-14s clip %+5.1f vol %6.1f pan %5.2f -> %-11s [%s] sends %d eq %d\n", engine.getSession().getTrack (m.track).name.substring (0, 30).toRawUTF8(),
                     MixAI::roleName (m.role), m.clipGainDb, m.volumeDb, m.pan, m.toBus.isEmpty() ? "Main Out" : m.toBus.toRawUTF8(),
                     chain.joinIntoString (", ").toRawUTF8(), (int) m.sends.size(), (int) m.eq.size());
    }
    juce::StringArray master; for (auto& c : plan.master) master.add (c.plugin + " (" + c.note + ")");
    std::printf ("  MASTER %s\n", master.joinIntoString (" > ").toRawUTF8());
    for (auto& n : plan.notes) std::printf ("  note: %s\n", n.toRawUTF8());

    std::fprintf (stderr, "[step] snapshot\n");
    auto snapBefore = MixAI::takeSnapshot (engine, "before");
    std::fprintf (stderr, "[step] apply\n");
    auto r = MixAI::applyPlan (engine, plan);
    std::fprintf (stderr, "[step] applied\n");
    const auto t1 = juce::Time::getMillisecondCounterHiRes();
    std::printf ("applied in %.1f s: %d buses, %d tracks, %d plugins; missing [%s]\n", (t1 - t0) / 1000.0, r.busesMade, r.tracksChanged,
                 r.pluginsLoaded, r.missingPlugins.joinIntoString (", ").toRawUTF8());
    for (auto& l : r.log) std::printf ("  log: %s\n", l.toRawUTF8());

    std::fprintf (stderr, "[step] export after\n");
    auto after = engine.exportAudio (outDir.getChildFile ("after.wav"), eo);
    std::printf ("after: peak %.1f dB, clipped %s\n", after.peakDb, after.clipped ? "YES" : "no");
    std::fprintf (stderr, "[step] exported after\n");
    engine.saveSession (outDir.getChildFile ("after.novastudio"));
    std::fprintf (stderr, "[step] restore\n");

    // undo = restore the snapshot; the mix must come back exactly
    MixAI::restoreSnapshot (engine, snapBefore);
    auto undone = engine.exportAudio (outDir.getChildFile ("undone.wav"), eo);
    std::printf ("undo restored: %d tracks (was %d before apply)\n", engine.getTrackCount(), (int) engine.getSession().getNumTracks());
    juce::ignoreUnused (undone);
    std::fprintf (stderr, "[step] done, shutting down\n");
    engine.shutdown();
    std::fprintf (stderr, "[step] engine shut down\n");
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc >= 4 && juce::String (argv[1]) == "--ui-apply")
        return uiApply (juce::File (juce::String (argv[2])), juce::File (juce::String (argv[3])));
    if (argc >= 4 && juce::String (argv[1]) == "--ui-shot")
        return uiShot (juce::File (juce::String (argv[2])), juce::File (juce::String (argv[3])), argc > 4 ? juce::String (argv[4]).getIntValue() : -3);
    if (argc >= 3 && juce::String (argv[1]) == "--bisect") return bisect (juce::File (juce::String (argv[2])));
    if (argc >= 4 && juce::String (argv[1]) == "--master-check")
        return masterCheck (juce::File (juce::String (argv[2])), juce::File (juce::String (argv[3])), argc > 4 ? juce::StringArray { "Streaming", "Balanced", "Loud" }.indexOf (argv[4]) : 1);
    if (argc >= 4 && juce::String (argv[1]) == "--apply")
        return applyMode (juce::File (juce::String (argv[2])), juce::File (juce::String (argv[3])), argc > 4 ? juce::String (argv[4]) : juce::String ("Nova Balance"));
    for (int i = 1; i < argc; ++i)
    {
        Session s;
        if (! s.loadFromFile (juce::File (juce::String (argv[i])))) { std::printf ("can't load %s\n", argv[i]); continue; }
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto a = MixAI::analyse (s);
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        static const char* N[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        std::printf ("\n=== %s  (%.0f s, %d tracks, %.0f BPM, key %s, analysed in %.1f s)\n", s.getName().toRawUTF8(), a.seconds,
                     (int) a.tracks.size(), a.bpm, a.keyKnown ? (juce::String (N[a.keyTonic]) + (a.keyMinor ? " minor" : " major")).toRawUTF8() : "?",
                     (t1 - t0) / 1000.0);
        for (auto& t : a.tracks)
            std::printf ("  %-34s %-16s %3d%%  %-52s | act %3.0f%% loud %5.1f low %.2f hi %.2f on %.1f/s dec %4.0f vo %.2f wob %.2f sus %.2f beat %.2f\n",
                         t.name.substring (0, 34).toRawUTF8(), MixAI::roleName (t.role), juce::roundToInt (t.confidence * 100), t.heard.toRawUTF8(),
                         t.f.activeFrac * 100, t.f.loudDb, t.f.band[0] + t.f.band[1], t.f.band[4] + t.f.band[5], t.f.onsetRate, t.f.decayMs,
                         t.f.voicedFrac, t.f.pitchWobble, t.f.sustain, t.f.onBeat);
    }
    return 0;
}
