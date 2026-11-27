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
    {
        auto* snd = proc->apvts.getParameter ("sound");
        snd->setValueNotifyingHost (snd->convertTo0to1 ((float) NovaMelody::kSoundOff));
        auto ro = run (*proc, 200);
        check (ro.peak < 1e-6f && ro.noteOns > 0, "Sound: Off (MIDI only): silent, still sends MIDI (" + juce::String (ro.noteOns) + " notes)");
        snd->setValueNotifyingHost (snd->convertTo0to1 (0.f));
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

    // ── Play tab ───────────────────────────────────────────────────────────
    {
        std::printf ("[Play]\n");
        proc->setPreview (false);
        run (*proc, 10);
        proc->setMode (2);
        auto set = [&] (const char* id, float v) { auto* q = proc->apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
        set ("keyAutoGen", 0); set ("keyTonic", 7); set ("keyScale", 1);     // G minor
        set ("sound", 1); set ("strum", 0);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);    // the processor's timer picks up the key
        const NovaMelody::Key gm { 7, NovaMelody::Minor };

        // collects the MIDI out pitches of one block after an action
        auto notesAfter = [&] (std::function<void()> action, juce::MidiBuffer* in = nullptr)
        {
            action();
            std::vector<int> ons;
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            if (in != nullptr) midi = *in;
            proc->processBlock (buf, midi);
            for (const auto m : midi) if (m.getMessage().isNoteOn()) ons.push_back (m.getMessage().getNoteNumber());
            return ons;
        };
        bool allInKey = true; int count = 0;
        for (int i = 0; i < 8; ++i)
        {
            auto ons = notesAfter ([&] { proc->padDown (0, i, 100); });
            for (int pch : ons) { allInKey = allInKey && NovaMelody::inKey (pch, gm); ++count; }
            notesAfter ([&] { proc->padUp (0, i); });
        }
        check (count == 8 && allInKey, "8 melody pads: 8 notes, all in G minor");
        auto chord = notesAfter ([&] { proc->padDown (1, 5, 100); });
        notesAfter ([&] { proc->padUp (1, 5); });
        bool chordOk = chord.size() == 4;
        for (int pch : chord) chordOk = chordOk && (pch % 12 == 3 || pch % 12 == 7 || pch % 12 == 10);
        check (chordOk, "chord pad 6 in G minor = Eb chord (" + juce::String ((int) chord.size()) + " notes, one finger)");

        // your MIDI keyboard: every key in key (white and black)
        set ("playStyle", 0);
        int out = 0, any = 0;
        for (int k = 48; k < 84; ++k)
        {
            juce::MidiBuffer in; in.addEvent (juce::MidiMessage::noteOn (1, k, (juce::uint8) 90), 0);
            auto ons = notesAfter ([] {}, &in);
            for (int pch : ons) { ++any; if (! NovaMelody::inKey (pch, gm)) ++out; }
            juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, k), 0);
            notesAfter ([] {}, &off);
        }
        check (any == 36 && out == 0, "MIDI keyboard, every key (36): all play notes in G minor (" + juce::String (out) + " out)");
        {
            juce::MidiBuffer in; in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 90), 0);
            auto ons = notesAfter ([] {}, &in);
            check (ons.size() == 1 && ons[0] % 12 == 7, "middle C plays the key's home note (G)");
            juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 60), 0); notesAfter ([] {}, &off);
        }
        // one finger chords: left hand = chords
        set ("playStyle", 1);
        {
            juce::MidiBuffer in; in.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 90), 0);   // C3 -> chord 1
            auto ons = notesAfter ([] {}, &in);
            bool gmChord = ons.size() == 4;
            for (int pch : ons) gmChord = gmChord && (pch % 12 == 7 || pch % 12 == 10 || pch % 12 == 2);
            check (gmChord, "One finger chords: a left-hand key plays a whole Gm chord");
            juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 48), 0); notesAfter ([] {}, &off);
        }
        // strum
        set ("strum", 1);
        {
            std::vector<int> samples;
            proc->padDown (1, 0, 100);
            for (int b = 0; b < 10; ++b)
            {
                juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
                proc->processBlock (buf, midi);
                for (const auto m : midi) if (m.getMessage().isNoteOn()) samples.push_back (b * 512 + m.samplePosition);
            }
            notesAfter ([&] { proc->padUp (1, 0); });
            check (samples.size() == 4 && samples.back() - samples.front() > 1500, "Strum: chord notes roll in one after another (" + juce::String (samples.size() ? samples.back() - samples.front() : 0) + " samples)");
        }
        set ("strum", 0);
        // tap the melody
        set ("playStyle", 2);
        proc->setMode (0); proc->selectVariation (0); proc->setMode (2);
        const auto mel = proc->currentNotes();       // Play's take (empty) - the tap list is the Generate melody
        juce::ignoreUnused (mel);
        std::vector<int> tapped;
        for (int i = 0; i < 6; ++i)
        {
            auto ons = notesAfter ([&] { proc->padDown (3, 0, 100); });
            tapped.insert (tapped.end(), ons.begin(), ons.end());
            notesAfter ([&] { proc->padUp (3, 0); });
        }
        proc->setMode (0);
        const auto gen = proc->currentNotes();
        proc->setMode (2);
        bool inOrder = tapped.size() == 6;
        for (size_t i = 0; inOrder && i < 6; ++i) inOrder = tapped[i] == gen[(size_t) i % gen.size()].pitch;
        check (inOrder, "Tap the melody: each tap plays the melody's next note, in order");

        // record what you play
        set ("playStyle", 0); set ("fixTiming", 1); set ("recWhat", 0);
        proc->startPlayRecording();
        juce::AudioBuffer<float> buf (2, 512);
        auto blocks = [&] (int k) { for (int b = 0; b < k; ++b) { juce::MidiBuffer m; proc->processBlock (buf, m); } };
        blocks (3);
        for (int i : { 0, 2, 4, 7 })
        {
            proc->padDown (0, i, 100); blocks (11);       // ~ an eighth at 120 BPM
            proc->padUp (0, i); blocks (12);
        }
        proc->padDown (1, 3, 100); blocks (40); proc->padUp (1, 3); blocks (5);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        proc->stopPlayRecording();
        const auto take = proc->currentNotes();
        bool grid = true;
        for (auto& m : take) grid = grid && std::abs (m.start * 4 - std::round (m.start * 4)) < 1e-6;
        int melodyFound = 0;
        for (int i : { 0, 2, 4, 7 })
            for (auto& m : take) if (m.pitch == NovaMelody::padPitch (gm, i, 0) && m.len <= 0.75) { ++melodyFound; break; }
        check (take.size() == 8 && melodyFound == 4, "Record: the 4 melody notes + the 4-note chord came back as MIDI (" + juce::String ((int) take.size()) + " notes)");
        check (grid, "Fix my timing: recorded notes snapped to the 1/16 grid");
        const auto mf = proc->currentMidiFile();
        juce::MemoryInputStream ms (mf, false); juce::MidiFile f;
        check (f.readFrom (ms), "the recorded take drags / exports as a MIDI file");
        set ("recWhat", 1);
        proc->startPlayRecording();
        blocks (2); proc->padDown (0, 1, 100); blocks (10); proc->padUp (0, 1); proc->padDown (1, 2, 100); blocks (10); proc->padUp (1, 2); blocks (4);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        proc->stopPlayRecording();
        check (proc->currentNotes().size() == 1, "Melody only: chords aren't recorded (" + juce::String ((int) proc->currentNotes().size()) + " note)");
    }

    proc->releaseResources();
    proc.reset();
    std::printf ("\n%d passed, %d failed\n", passes, fails);
    return fails == 0 ? 0 : 1;
}
