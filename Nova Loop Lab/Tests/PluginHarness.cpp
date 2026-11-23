// Drives the real Nova Loop Lab processor (no UI) the way a DAW does: load a
// sample, play the transport, send MIDI, save and restore the project.
#include "PluginProcessor.h"

static int failures = 0, passes = 0;
static void check (bool ok, const juce::String& what)
{
    (ok ? passes : failures)++;
    std::cout << (ok ? "  pass  " : "  FAIL  ") << what << "\n";
}

struct FakeHost : juce::AudioPlayHead
{
    bool playing = false;
    double ppq = 0.0, bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setPpqPosition (ppq);
        p.setBpm (bpm);
        p.setTimeInSamples ((juce::int64) (ppq * 60.0 / bpm * 48000.0));
        return p;
    }
};

static constexpr double SR = 48000.0;
static constexpr int BLOCK = 512;

static juce::File writeTestLoop (bool rich = false)
{
    const double bpm = 92.0, beat = 44100.0 * 60.0 / bpm;
    const int n = (int) (8 * beat);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    juce::Random rng (5);
    for (int k = 0; k < 8; ++k)
        for (int i = 0; i < 9000 && (int) (k * beat) + i < n; ++i)
        {
            const double t = i / 44100.0;
            const float v = k % 2 == 0 ? (float) (0.8 * std::exp (-t * 18.0) * std::sin (6.2831853 * (55.0 + 90.0 * std::exp (-t * 30.0)) * t))
                                       : (float) (0.5 * std::exp (-t * 28.0) * (rng.nextFloat() * 2.0 - 1.0));
            b.addSample (0, (int) (k * beat) + i, v);
            b.addSample (1, (int) (k * beat) + i, v);
        }
    if (rich)
    {
        // Soul-style chord stabs (Cm7, Abmaj7) and 8th-note hats for the demo.
        const double chords[2][4] = { { 130.81, 155.56, 196.0, 233.08 }, { 103.83, 130.81, 155.56, 196.0 } };
        for (int e = 0; e < 16; ++e)
        {
            const int at = (int) (e * beat / 2.0);
            for (int i = 0; i < 2500 && at + i < n; ++i)
            {
                const float h = (float) (0.12 * std::exp (-i / 300.0) * (rng.nextFloat() * 2.0 - 1.0));
                b.addSample (0, at + i, h); b.addSample (1, at + i, h * 0.8f);
            }
        }
        for (int stab = 0; stab < 8; ++stab)
        {
            const int at = (int) ((stab * 2 + (stab % 2 ? 1.5 : 0.0)) * beat * 0.5);
            const auto& c = chords[(stab / 4) % 2];
            for (int i = 0; i < (int) (0.9 * beat) && at + i < n; ++i)
            {
                const double t = i / 44100.0, env = std::min (1.0, t * 200.0) * std::exp (-t * 2.5);
                double v = 0;
                for (double f : c) v += std::sin (6.2831853 * f * t) + 0.35 * std::sin (6.2831853 * 2 * f * t);
                b.addSample (0, at + i, (float) (0.07 * env * v));
                b.addSample (1, at + i, (float) (0.065 * env * v));
            }
        }
    }
    else
        for (int i = 0; i < n; ++i)
            for (int ch = 0; ch < 2; ++ch)
                b.addSample (ch, i, 0.08f * (float) std::sin (6.2831853 * 261.63 * i / 44100.0));

    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("looplab_harness_92bpm.wav");
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), 44100.0, 2, 24, {}, 0));
    os.release();
    w->writeFromAudioSampleBuffer (b, 0, n);
    return f;
}

static bool waitForModel (NovaLoopLabAudioProcessor& p, int afterVersion, int timeoutMs = 20000)
{
    const auto start = juce::Time::getMillisecondCounter();
    while (juce::Time::getMillisecondCounter() - start < (juce::uint32) timeoutMs)
    {
        if (p.core.getModelVersion() > afterVersion && ! p.core.isBusy())
        {
            auto m = p.core.getModel();
            if (m != nullptr && m->hasSample()) return true;
        }
        juce::Thread::sleep (20);
    }
    return false;
}

// Renders numBlocks from the processor, advancing the fake transport.
static juce::AudioBuffer<float> run (NovaLoopLabAudioProcessor& p, FakeHost& host, int numBlocks, juce::MidiBuffer* firstBlockMidi = nullptr)
{
    juce::AudioBuffer<float> out (2, numBlocks * BLOCK), block (2, BLOCK);
    for (int b = 0; b < numBlocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0 && firstBlockMidi != nullptr) midi = *firstBlockMidi;
        p.processBlock (block, midi);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, b * BLOCK, block, ch, 0, BLOCK);
        if (host.playing) host.ppq += BLOCK / SR * host.bpm / 60.0;
    }
    return out;
}

static float peak (const juce::AudioBuffer<float>& b, int from = 0, int to = -1)
{
    if (to < 0) to = b.getNumSamples();
    float m = 0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = from; i < to; ++i)
        {
            const float v = b.getSample (ch, i);
            if (! std::isfinite (v)) return 99.0f;
            m = std::max (m, std::abs (v));
        }
    return m;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // --dump-state <out>: load the test loop and write a standalone-app state
    // blob (used to open the standalone with a sample already loaded).
    if (argc == 3 && juce::String (argv[1]) == "--dump-state")
    {
        NovaLoopLabAudioProcessor p;
        p.prepareToPlay (SR, BLOCK);
        p.core.updateSpec ([] (ll::Spec& s) { s.hostBpm = 92.0; s.sampleRate = SR; });
        const int v = p.core.getModelVersion();
        p.core.loadFile (writeTestLoop (true));
        if (! waitForModel (p, v)) return 1;
        const int v2 = p.core.getModelVersion();
        p.core.updateSpec ([] (ll::Spec& s) { s.sampleName = "Velvet Soul Loop 92bpm.wav"; });
        if (! waitForModel (p, v2)) return 1;
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        juce::File (argv[2]).replaceWithText (mb.toBase64Encoding());
        return 0;
    }

    // --analyse <file>: print what Loop Lab hears in a sample.
    if (argc == 3 && juce::String (argv[1]) == "--analyse")
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (argv[2])));
        if (r == nullptr) return 1;
        ll::Sample smp; smp.sampleRate = r->sampleRate; smp.audio.setSize (2, (int) r->lengthInSamples);
        r->read (&smp.audio, 0, (int) r->lengthInSamples, 0, true, true);
        const auto a = ll::analyse (smp);
        std::cout << "tempo " << a.bpm << "  key " << ll::keyName (a.key) << " (confidence " << a.keyConfidence << ")\n";
        return 0;
    }

    std::cout << "Nova Loop Lab plugin harness\n";

    const auto file = writeTestLoop();
    std::unique_ptr<NovaLoopLabAudioProcessor> p (new NovaLoopLabAudioProcessor());
    FakeHost host;
    p->setPlayHead (&host);
    p->prepareToPlay (SR, BLOCK);
    p->core.updateSpec ([] (ll::Spec& s) { s.hostBpm = 120.0; s.sampleRate = SR; }); // what the param timer does

    const int v0 = p->core.getModelVersion();
    p->core.loadFile (file);
    const bool loaded = waitForModel (*p, v0);
    check (loaded, "sample loads, gets analysed, chopped and rendered in the background");
    if (! loaded) return 1;

    auto m = p->core.getModel();
    check (std::abs (m->analysis.bpm - 92.0) < 0.6, "detected 92 BPM: " + juce::String (m->analysis.bpm, 2));
    check (! m->chops.empty() && m->chops.size() <= 16, juce::String ((int) m->chops.size()) + " chops");
    const int loopLen = (int) std::lround (2 * 4 * 60.0 / 120.0 * SR);
    bool allLoops = true;
    for (auto& l : m->loops) allLoops &= l.getNumSamples() == loopLen;
    check (allLoops, "8 flips rendered, each exactly 2 bars at the host's 120 BPM");
    check (juce::File (p->core.getSpec().samplePath).isAChildOf (ll::LoopLabCore::libraryFolder()), "sample copied into Music/Nova Loop Lab/Samples");

    // ── Playback follows the host transport ──
    host.playing = true; host.ppq = 0.0;
    const auto played = run (*p, host, (int) std::ceil (2.0 * loopLen / BLOCK) + 2);
    check (peak (played) > 0.05f && peak (played) <= 1.0f, "plays when the DAW plays: audible, no clipping, no NaN");
    const int off = 4800; // skip the 5 ms fade-in
    float maxDiff = 0;
    for (int i = off; i < loopLen - BLOCK; ++i)
        maxDiff = std::max (maxDiff, std::abs (played.getSample (0, i) - played.getSample (0, i + loopLen)));
    check (maxDiff < 1.0e-3f, "second time round is sample-identical: loop is locked to the bar grid");

    host.ppq = 3.0; // DAW jumps to beat 4
    const auto jumped = run (*p, host, 4);
    const int at = (int) std::lround (3.0 * 60.0 / 120.0 * SR);
    float jd = 0;
    for (int i = 600; i < 4 * BLOCK; ++i) jd = std::max (jd, std::abs (jumped.getSample (0, i) - played.getSample (0, at + i)));
    check (jd < 2.0e-2f, "jumping the playhead lands at the right spot in the loop");

    host.playing = false;
    run (*p, host, 20);
    const auto stopped = run (*p, host, 10);
    check (peak (stopped) < 1.0e-6f, "silent when the DAW stops");

    // ── MIDI ──
    juce::MidiBuffer pad;
    pad.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 110), 0);
    const auto hit = run (*p, host, 20, &pad);
    check (peak (hit) > 0.05f, "MIDI C1 plays chop 1 while stopped (pad mode)");

    host.playing = true; host.ppq = 0.0;
    juce::MidiBuffer sw;
    sw.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), 0);
    run (*p, host, 8, &sw);
    check (p->playingPattern.load() == 2, "MIDI D3 switches to flip V3");
    host.playing = false;
    run (*p, host, 20);

    // ── Audition from the UI while the DAW is stopped ──
    p->auditionLoop.store (true);
    const auto aud = run (*p, host, 40);
    p->auditionLoop.store (false);
    check (peak (aud) > 0.05f && p->playhead.load() >= 0.0f, "PLAY button previews the loop with the DAW stopped");

    // ── Save + reopen the project ──
    const int v1 = p->core.getModelVersion();
    p->core.updateSpec ([] (ll::Spec& s) { s.fx.resize (16); s.fx[1].pitch = 5; s.fx[1].reverse = true; s.fx[2].use = false; s.tempoMult = 1.0; });
    waitForModel (*p, v1);
    m = p->core.getModel();
    const auto v3 = m->steps[2];
    p->core.updateSpec ([&] (ll::Spec& s) { s.locked[2] = true; s.lockedSteps[2] = v3; });
    if (auto* prm = p->apvts.getParameter ("swing")) prm->setValueNotifyingHost (prm->convertTo0to1 (42.0f));

    juce::MemoryBlock saved;
    p->getStateInformation (saved);

    std::unique_ptr<NovaLoopLabAudioProcessor> q (new NovaLoopLabAudioProcessor());
    q->setPlayHead (&host);
    q->prepareToPlay (SR, BLOCK);
    q->core.updateSpec ([] (ll::Spec& s) { s.hostBpm = 120.0; s.sampleRate = SR; });
    const int qv = q->core.getModelVersion();
    q->setStateInformation (saved.getData(), (int) saved.getSize());
    const bool reopened = waitForModel (*q, qv);
    check (reopened, "reopened project reloads its sample");
    if (reopened)
    {
        const auto qs = q->core.getSpec();
        const auto qm = q->core.getModel();
        bool stepsSame = qm->steps[2].size() == v3.size();
        for (size_t i = 0; stepsSame && i < v3.size(); ++i) stepsSame = qm->steps[2][i].t == v3[i].t && qm->steps[2][i].chop == v3[i].chop && qm->steps[2][i].len == v3[i].len;
        check (qs.fx[1].pitch == 5 && qs.fx[1].reverse && ! qs.fx[2].use, "chop effects survive save/reopen");
        check (qs.locked[2] && stepsSame, "locked flip V3 comes back note-for-note");
        check (std::abs (q->apvts.getRawParameterValue ("swing")->load() - 42.0f) < 0.5f, "parameters survive save/reopen");
        check (qs.seeds == p->core.getSpec().seeds, "every flip's seed survives, so V1-V8 sound the same after reopening");
    }

    // ── Flip styles ──
    p->setCurrentProgram (5); // Lo-Fi Flip
    p->applyStyleFx (5);
    const auto st = p->core.getSpec();
    check (st.fx[0].tone == 45 && st.fx[0].filter == -30 && p->apvts.getRawParameterValue ("swing")->load() > 44.0f, "Lo-Fi Flip style sets swing and dusts every chop");

    // ── Bypass ──
    if (auto* bp = p->apvts.getParameter ("bypass")) bp->setValueNotifyingHost (1.0f);
    host.playing = true; host.ppq = 0.0;
    const auto byp = run (*p, host, 20);
    check (peak (byp) < 1.0e-6f, "bypass is silent");

    p->setPlayHead (nullptr);
    q->setPlayHead (nullptr);
    std::cout << "\n" << passes << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
