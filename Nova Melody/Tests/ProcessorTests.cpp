// Drives the real NovaMelodyProcessor: preview playback (sound + MIDI out), chords on
// MIDI in setting the key, session save/reload, and a voice memo turned into notes.
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
        if (done()) { juce::MessageManager::getInstance()->runDispatchLoopUntil (100); return; }
    }
}

struct RunResult { int noteOns = 0, noteOffs = 0; float peak = 0; double rms = 0; bool finite = true; };
static RunResult run (NovaMelodyProcessor& p, int blocks, juce::MidiBuffer* firstIn = nullptr)
{
    RunResult r;
    juce::AudioBuffer<float> buf (2, 512);
    double sq = 0; long cnt = 0;
    for (int b = 0; b < blocks; ++b)
    {
        buf.clear();
        juce::MidiBuffer midi;
        if (b == 0 && firstIn != nullptr) midi = *firstIn;
        p.processBlock (buf, midi);
        for (const auto m : midi) { if (m.getMessage().isNoteOn()) ++r.noteOns; if (m.getMessage().isNoteOff()) ++r.noteOffs; }
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
            {
                const float s = buf.getSample (c, i);
                if (! std::isfinite (s)) r.finite = false;
                r.peak = std::max (r.peak, std::abs (s)); sq += s * s; ++cnt;
            }
    }
    r.rms = std::sqrt (sq / std::max (1L, cnt));
    return r;
}

static juce::MemoryBlock humWav (double sr)
{
    // "dum dum daa" G3 G3 Bb3 F3 at 120 BPM, voice-ish
    const double notes[][3] = { { 0, 1, 55 }, { 1, 1, 55 }, { 2, 2, 58 }, { 4, 2, 53 } };
    juce::AudioBuffer<float> b (1, (int) (sr * 3.6));
    b.clear();
    for (auto& n : notes)
    {
        double ph = 0;
        const int a = (int) ((0.3 + n[0] * 0.5) * sr), e = (int) ((0.3 + (n[0] + n[1]) * 0.5 - 0.04) * sr);
        for (int i = a; i < e && i < b.getNumSamples(); ++i)
        {
            const double t = (i - a) / sr;
            ph += 2 * juce::MathConstants<double>::pi * 440.0 * std::pow (2.0, (n[2] + 0.12 * std::sin (6.28 * 5.5 * t) - 69.0) / 12.0) / sr;
            const double env = std::min (1.0, t / 0.015) * std::min (1.0, (e - i) / (0.02 * sr));
            b.setSample (0, i, (float) (env * 0.3 * (std::sin (ph) + 0.4 * std::sin (2 * ph))));
        }
    }
    juce::MemoryBlock mb;
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::MemoryOutputStream (mb, false), sr, 1, 24, {}, 0));
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
    return mb;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::printf ("[Processor]\n");
    auto proc = std::make_unique<NovaMelodyProcessor>();
    proc->setPlayConfigDetails (0, 2, 48000, 512);
    proc->prepareToPlay (48000, 512);
    check (! proc->currentNotes().empty(), "opens with a melody already written (" + juce::String ((int) proc->currentNotes().size()) + " notes)");

    auto r0 = run (*proc, 50);
    check (r0.noteOns == 0 && r0.peak < 1e-6f, "stopped (no host transport, preview off): silent, no MIDI");

    proc->setPreview (true);
    auto r1 = run (*proc, 375);          // 4 s at 120 BPM = 8 beats = 2 bars
    int expected = 0;
    for (auto& m : proc->currentNotes()) if (m.start < 8.0 - 0.01) ++expected;
    check (r1.noteOns >= expected - 1 && r1.noteOns <= expected + 1, "Preview plays the notes as MIDI out: " + juce::String (r1.noteOns) + " note-ons in 2 bars (expected " + juce::String (expected) + ")");
    check (r1.rms > 0.003 && r1.finite && r1.peak <= 1.f, "and you hear them (built-in sound, rms " + juce::String (r1.rms, 4) + ", peak " + juce::String (r1.peak, 2) + ")");

    proc->apvts.getParameter ("midiOut")->setValueNotifyingHost (0.f);
    auto r2 = run (*proc, 100);
    check (r2.noteOns == 0 && r2.rms > 0.001, "MIDI out off: still sounds, sends no MIDI");
    proc->apvts.getParameter ("midiOut")->setValueNotifyingHost (1.f);

    for (int s = 0; s < NovaMelody::kNumSounds; ++s)
    {
        proc->apvts.getParameter ("sound")->setValueNotifyingHost (proc->apvts.getParameter ("sound")->convertTo0to1 ((float) s));
        auto rs = run (*proc, 120);
        check (rs.finite && rs.peak <= 1.f && rs.rms > 0.0005, juce::String ("sound ") + NovaMelody::soundName (s) + ": clean (peak " + juce::String (rs.peak, 2) + ")");
    }
    proc->setPreview (false);
    auto r3 = run (*proc, 20);
    check (r3.noteOns == 0, "Preview off: notes stop (" + juce::String (r3.noteOffs) + " note-offs sent)");

    // chords played in -> key heard
    {
        juce::MidiBuffer in;
        for (int n : { 48, 52, 55, 60 }) in.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 90), 0);   // C major
        run (*proc, 200, &in);
        juce::MidiBuffer off;
        for (int n : { 48, 52, 55, 60 }) off.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        for (int n : { 45, 48, 52 }) off.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 90), 10);    // A minor
        run (*proc, 200, &off);
        proc->apvts.getParameter ("keyAutoGen")->setValueNotifyingHost (1.f);
        proc->generateAll();
        const auto st = juce::JSON::parse (proc->getStateJson());
        const auto name = st["genKey"]["name"].toString();
        check (name == "C major" || name == "A minor", "chords played in set the key: " + name);
        check (st["chords"].size() >= 2, "and the chords were heard (" + juce::String (st["chords"].size()) + ")");
        int outOfKey = 0;
        const NovaMelody::Key k { (int) st["genKey"]["tonic"], (int) st["genKey"]["scale"] };
        for (auto& m : proc->currentNotes()) outOfKey += NovaMelody::inKey (m.pitch, k) ? 0 : 1;
        check (outOfKey == 0, "melody written in that key (" + juce::String (outOfKey) + " notes out)");
    }

    // lock, redo
    {
        proc->toggleLock (0); proc->toggleLock (1);
        auto before = proc->currentNotes();
        proc->redoRest();
        auto after = proc->currentNotes();
        int kept = 0;
        for (auto& a : after) for (int i = 0; i < 2; ++i) if (a.locked && a.pitch == before[(size_t) i].pitch && std::abs (a.start - before[(size_t) i].start) < 1e-9) ++kept;
        check (kept == 2, "Redo the rest keeps the 2 locked notes");
    }

    // save + reload
    {
        juce::MemoryBlock mb;
        proc->getStateInformation (mb);
        auto other = std::make_unique<NovaMelodyProcessor>();
        other->setStateInformation (mb.getData(), (int) mb.getSize());
        const auto a = proc->currentNotes(), b = other->currentNotes();
        bool same = a.size() == b.size();
        for (size_t i = 0; same && i < a.size(); ++i) same = a[i].pitch == b[i].pitch && std::abs (a[i].start - b[i].start) < 0.002 && a[i].locked == b[i].locked;
        check (same, "session save/reload brings back the same notes and locks");
    }

    // voice memo -> notes
    {
        proc->loadHumBytes ("hum.wav", humWav (48000));
        pump (15000, [&] { return ! juce::JSON::parse (proc->getStateJson())["analysing"]; });
        proc->setMode (1);
        proc->remakeHum();
        const auto st = juce::JSON::parse (proc->getStateJson());
        const auto notes = proc->currentNotes();
        check ((bool) st["hum"]["ok"], "voice memo heard: " + st["hum"]["status"].toString());
        check (notes.size() == 4, "4 hummed notes -> " + juce::String ((int) notes.size()) + " bass notes");
        if (notes.size() == 4)
        {
            const int want[] = { 55 - 24, 55 - 24, 58 - 24, 53 - 24 };
            bool pitches = true;
            for (int i = 0; i < 4; ++i) pitches = pitches && (notes[(size_t) i].pitch % 12 == want[i] % 12) && notes[(size_t) i].pitch < 50;
            check (pitches, "same notes as hummed, down in bass range");
        }
        proc->setPreview (true);
        auto rh = run (*proc, 200);
        check (rh.noteOns > 0 && rh.rms > 0.001, "Hum it result plays (" + juce::String (rh.noteOns) + " notes)");
        const auto mid = proc->currentMidiFile();
        juce::MemoryInputStream ms (mid, false);
        juce::MidiFile f;
        check (f.readFrom (ms), "Export / drag gives a valid MIDI file (" + juce::String ((int) mid.getSize()) + " bytes)");
    }

    proc->releaseResources();
    proc.reset();
    std::printf ("\n%d passed, %d failed\n", passes, fails);
    return fails == 0 ? 0 : 1;
}
