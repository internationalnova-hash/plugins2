// Nova Melody core checks: generator rules for every genre/part, locked notes,
// humming -> notes on a synthetic hum with known notes, and a real sung take.
#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "MelodyCore.h"
#include <cstdio>
#include <cmath>

using namespace NovaMelody;

static int failures = 0, passes = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("  %s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    (ok ? passes : failures)++;
}

// A hum: voice-like tone (harmonics, vibrato, a scoop into each note, short gaps between "dum"s)
static std::vector<float> synthHum (const std::vector<std::array<double, 3>>& notes, double sr, double bpm, double leadIn)
{
    const double spb = 60.0 / bpm;
    double endSec = leadIn;
    for (auto& n : notes) endSec = std::max (endSec, leadIn + (n[0] + n[1]) * spb);
    std::vector<float> x ((size_t) ((endSec + 0.5) * sr), 0.f);
    juce::Random rnd (3);
    for (auto& n : notes)
    {
        const double a = leadIn + n[0] * spb, e = a + n[1] * spb - 0.04;   // 40 ms gap ("d" of dum)
        double ph = 0;
        for (size_t i = (size_t) (a * sr); i < (size_t) (e * sr) && i < x.size(); ++i)
        {
            const double t = i / sr - a;
            const double scoop = t < 0.04 ? -0.6 * (1.0 - t / 0.04) : 0.0;
            const double midi = n[2] + scoop + 0.15 * std::sin (2 * juce::MathConstants<double>::pi * 5.5 * t) + 0.05 * (rnd.nextDouble() - 0.5);
            ph += 2 * juce::MathConstants<double>::pi * 440.0 * std::pow (2.0, (midi - 69.0) / 12.0) / sr;
            const double env = std::min (1.0, t / 0.015) * std::min (1.0, (e - i / sr) / 0.02);
            x[i] = (float) (env * 0.3 * (std::sin (ph) + 0.5 * std::sin (2 * ph) + 0.25 * std::sin (3 * ph)) + 0.002 * (rnd.nextFloat() - 0.5));
        }
    }
    return x;
}

int main (int argc, char** argv)
{
    juce::ignoreUnused (argc, argv);
    // ── generator ──────────────────────────────────────────────────────────
    std::printf ("[Generate] every genre x part x vibe\n");
    int combos = 0, emptyOnes = 0, outOfKey = 0, outOfRange = 0, outOfClip = 0;
    for (int g = 0; g < kNumGenres; ++g)
        for (int part = 0; part < kNumParts; ++part)
            for (int v = 0; v < kNumVibes; ++v)
            {
                GenParams p;
                p.genre = g; p.part = part; p.vibe = v; p.bars = 8; p.key = { 7, Minor }; p.seed = (juce::uint32) (g * 100 + part * 10 + v);
                const auto notes = generate (p);
                ++combos;
                if (notes.empty()) { ++emptyOnes; continue; }
                const int lo = part == Bass808 ? 22 : (part == Bassline ? 26 : 45), hi = part == Bass808 ? 47 : (part == Bassline ? 52 : 92);
                for (auto& m : notes)
                {
                    if (! inKey (m.pitch, p.key)) ++outOfKey;
                    if (m.pitch < lo || m.pitch > hi) ++outOfRange;
                    if (m.start < 0 || m.start + m.len > 32.0 + 1e-6) ++outOfClip;
                }
            }
    check (emptyOnes == 0, juce::String (combos) + " combinations all wrote notes (" + juce::String (emptyOnes) + " empty)");
    check (outOfKey == 0, "every note in key with Stay in key on (" + juce::String (outOfKey) + " out)");
    check (outOfRange == 0, "every note in its part's range (" + juce::String (outOfRange) + " out)");
    check (outOfClip == 0, "every note inside the 8 bars (" + juce::String (outOfClip) + " out)");

    {
        GenParams p; p.seed = 42;
        const auto a = generate (p), b = generate (p);
        bool same = a.size() == b.size();
        for (size_t i = 0; same && i < a.size(); ++i) same = a[i].pitch == b[i].pitch && std::abs (a[i].start - b[i].start) < 1e-9;
        check (same, "same settings + seed = the same melody (variations are repeatable)");
        p.seed = 43;
        const auto c = generate (p);
        bool diff = c.size() != a.size();
        for (size_t i = 0; ! diff && i < a.size(); ++i) diff = a[i].pitch != c[i].pitch || std::abs (a[i].start - c[i].start) > 1e-9;
        check (diff, "a different seed = a different variation");
    }
    {
        // density: more Notes = more notes
        GenParams p; p.seed = 7; p.density = 0.15f;
        const auto sparse = generate (p);
        p.density = 0.95f;
        const auto busy = generate (p);
        check (busy.size() > sparse.size() + 8, "Notes slider: " + juce::String ((int) sparse.size()) + " notes at 15%, " + juce::String ((int) busy.size()) + " at 95%");
    }
    {
        // lock some notes, redo the rest
        GenParams p; p.seed = 5;
        auto first = generate (p);
        std::vector<MNote> keep (first.begin(), first.begin() + std::min<size_t> (4, first.size()));
        p.keep = keep; p.seed = 6;
        const auto redo = generate (p);
        int kept = 0, overlapping = 0;
        for (auto& k : keep)
            for (auto& m : redo)
            {
                if (m.locked && m.pitch == k.pitch && std::abs (m.start - k.start) < 1e-9 && std::abs (m.len - k.len) < 1e-9) ++kept;
                if (! m.locked && m.start < k.start + k.len - 1e-6 && k.start < m.start + m.len - 1e-6) ++overlapping;
            }
        check (kept == (int) keep.size(), "Keep selected: all " + juce::String ((int) keep.size()) + " locked notes kept exactly");
        check (overlapping == 0, "Redo the rest: no new note plays over a locked one");
    }
    {
        // follow heard chords: strong-beat notes are chord tones
        GenParams p; p.seed = 11; p.key = { 0, Major };
        p.chords = defaultProgression (p.key, Pop, Bright, 8);
        const auto notes = generate (p);
        int strong = 0, chordTone = 0;
        for (auto& m : notes)
            if (std::abs (m.start - std::round (m.start)) < 1e-6 && ! m.locked)
            {
                ++strong;
                const auto& c = p.chords[(size_t) std::min (7, (int) (m.start / 4.0))];
                if (std::find (c.pcs.begin(), c.pcs.end(), ((m.pitch % 12) + 12) % 12) != c.pcs.end()) ++chordTone;
            }
        check (strong > 0 && chordTone >= strong * 0.75, "follows the chords: " + juce::String (chordTone) + "/" + juce::String (strong) + " on-beat notes are chord tones");
    }
    {
        std::array<float, 12> w {};
        for (int pc : { 7, 11, 2, 0, 4, 9, 6, 7, 2, 11 }) w[(size_t) pc] += 1.f;   // G major-ish
        const auto k = detectKey (w);
        check (k.tonic == 7 && k.scale == Major, "key from chords: G major heard as " + keyName (k));
    }

    // ── humming ────────────────────────────────────────────────────────────
    std::printf ("[Hum] synthetic hum, known notes\n");
    const double sr = 48000, bpm = 94;
    // start, length (beats), MIDI: a G minor line hummed around G3
    const std::vector<std::array<double, 3>> line = {
        { 0, 1, 55 }, { 1, 0.5, 55 }, { 1.5, 1.5, 58 }, { 3, 1, 53 }, { 4, 1.5, 55 }, { 5.5, 0.5, 58 }, { 6, 2, 50 },
        { 8, 1, 55 }, { 9, 0.5, 55 }, { 9.5, 1.5, 58 }, { 11, 1, 60 }, { 12, 1.5, 58 }, { 13.5, 0.5, 55 }, { 14, 2, 53 } };
    const auto hum = synthHum (line, sr, bpm, 0.37);
    HumParams hp; hp.bpm = bpm; hp.as = AsMelody;
    auto r = fromHum (hum.data(), (int) hum.size(), sr, hp);
    check (r.ok, "a hum was heard (" + r.message + ")");
    check ((int) r.notes.size() == (int) line.size(), "every hummed note found: " + juce::String ((int) r.notes.size()) + " of " + juce::String ((int) line.size()));
    if ((int) r.notes.size() == (int) line.size())
    {
        int pitchOk = 0; double worstMs = 0;
        for (size_t i = 0; i < line.size(); ++i)
        {
            if (r.notes[i].pitch == (int) line[i][2]) ++pitchOk;
            worstMs = std::max (worstMs, std::abs (r.notes[i].start - line[i][0]) * 60000.0 / bpm);
        }
        check (pitchOk == (int) line.size(), "Exact: every pitch as hummed (" + juce::String (pitchOk) + "/" + juce::String ((int) line.size()) + ")");
        check (worstMs < 35.0, "Exact: every note starts where it was hummed (worst " + juce::String (worstMs, 1) + " ms)");
    }
    check (r.keyWasHeard && (r.key.tonic == 7 || r.key.tonic == 10), "key heard from the humming: " + keyName (r.key));

    hp.as = AsBass;
    remakeFromHeard (r, hp);
    float med = 0; for (auto& m : r.notes) med += (float) m.pitch; med /= std::max<size_t> (1, r.notes.size());
    check (med > 30 && med < 48, "Bassline: dropped into bass range (average note " + juce::String (med, 1) + ")");
    check (! r.notes.empty() && r.notes[0].pitch % 12 == 55 % 12, "Bassline: same notes, an octave or two down");
    hp.as = As808;
    remakeFromHeard (r, hp);
    int glides = 0; for (auto& m : r.notes) glides += m.glide ? 1 : 0;
    check (! r.notes.empty(), "808: written (" + juce::String (glides) + " glides)");
    hp.as = AsChords;
    remakeFromHeard (r, hp);
    check (r.notes.size() >= 12 && r.notes.size() % 3 == 0, "Chords under it: " + juce::String ((int) r.notes.size() / 3) + " chords");

    // a slightly flat out-of-key note: kept at Exact, pulled into key at "cleaned up"
    {
        const std::vector<std::array<double, 3>> odd = { { 0, 1, 55 }, { 1, 1, 57.62 }, { 2, 1, 58 }, { 3, 1, 62 } };
        const auto h2 = synthHum (odd, sr, bpm, 0.2);
        HumParams p2; p2.bpm = bpm; p2.as = AsMelody; p2.autoKey = false; p2.key = { 7, Minor };
        auto r2 = fromHum (h2.data(), (int) h2.size(), sr, p2);
        const int exact = r2.notes.size() > 1 ? r2.notes[1].pitch : -1;
        p2.pitchSnap = 1.f;
        remakeFromHeard (r2, p2);
        const int snapped = r2.notes.size() > 1 ? r2.notes[1].pitch : -1;
        check (exact == 58 || exact == 57, "Exact pitch keeps what was hummed (" + juce::String (exact) + ")");
        check (inKey (snapped, p2.key), "Pitch at cleaned up: pulled into G minor (" + juce::String (snapped) + ")");
    }
    // timing snap
    {
        HumParams p3; p3.bpm = bpm; p3.as = AsMelody; p3.timeSnap = 1.f;
        auto r3 = fromHum (hum.data(), (int) hum.size(), sr, p3);
        bool onGrid = ! r3.notes.empty();
        for (auto& m : r3.notes) onGrid = onGrid && std::abs (m.start * 4.0 - std::round (m.start * 4.0)) < 1e-6;
        check (onGrid, "Timing at cleaned up: every note on the 1/16 grid");
    }
    // groove + answer
    {
        HumParams p4; p4.bpm = bpm; p4.as = AsBass;
        auto r4 = fromHum (hum.data(), (int) hum.size(), sr, p4);
        const auto g = makeItGroove (r4.notes, Reggaeton, r4.lengthBeats, true, 1);
        bool samePitches = g.size() == r4.notes.size();
        for (size_t i = 0; samePitches && i < g.size(); ++i) samePitches = g[i].pitch == r4.notes[i].pitch;
        check (samePitches, "Make it groove: same notes in the same order, on the groove");
        const auto a = answerIt (r4.notes, r4.key, LatinUrban, r4.lengthBeats + 8, 1);
        check (a.size() > r4.notes.size(), "Answer it: " + juce::String ((int) (a.size() - r4.notes.size())) + " answering notes added");
    }
    // silence / too short
    {
        std::vector<float> quiet (48000, 0.f);
        HumParams p5;
        check (! fromHum (quiet.data(), (int) quiet.size(), sr, p5).ok, "silence: says nothing was heard instead of making notes up");
    }

    // ── real sung take ─────────────────────────────────────────────────────
    const juce::File real ("/Users/internationalnova/Nova/dry vocal 11.wav");
    if (real.existsAsFile())
    {
        std::printf ("[Hum] real take: %s\n", real.getFileName().toRawUTF8());
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (real));
        const int n = (int) rd->lengthInSamples;
        juce::AudioBuffer<float> b ((int) rd->numChannels, n);
        rd->read (&b, 0, n, 0, true, true);
        std::vector<float> mono ((size_t) n);
        for (int i = 0; i < n; ++i) { float s = 0; for (int c = 0; c < b.getNumChannels(); ++c) s += b.getSample (c, i); mono[(size_t) i] = s / (float) b.getNumChannels(); }
        HumParams p6; p6.bpm = 120; p6.as = AsMelody;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto r6 = fromHum (mono.data(), n, rd->sampleRate, p6);
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        check (r6.ok && r6.notes.size() >= 8, "real take: " + juce::String ((int) r6.notes.size()) + " notes from " + juce::String (n / rd->sampleRate, 1) + " s in " + juce::String (ms, 0) + " ms");
        std::printf ("      heard key: %s (song is G major - short phrases can mislead Auto; that's what picking the key is for)\n      notes:", keyName (r6.key).toRawUTF8());
        for (auto& m : r6.notes) std::printf (" %s%d@%.2f", noteName (m.pitch % 12), m.pitch / 12 - 1, m.start);
        std::printf ("\n");
        HumParams p7 = p6; p7.autoKey = false; p7.key = { 7, Major }; p7.pitchSnap = 1.f;
        auto r7 = fromHum (mono.data(), n, rd->sampleRate, p7);
        int inG = 0; for (auto& m : r7.notes) inG += inKey (m.pitch, p7.key) ? 1 : 0;
        check (r7.ok && inG == (int) r7.notes.size(), "real take with G major picked + Pitch cleaned up: every note in G major");
        int inK = 0; for (auto& m : r6.notes) inK += inKey (m.pitch, r6.key) ? 1 : 0;
        std::printf ("      %d of %d notes land in key at Exact\n", inK, (int) r6.notes.size());
    }

    // ── MIDI file ──────────────────────────────────────────────────────────
    {
        GenParams p; p.seed = 9;
        const auto notes = generate (p);
        const auto mb = toMidiFile (notes, 94, "Nova Melody");
        juce::MemoryInputStream in (mb, false);
        juce::MidiFile f;
        const bool read = f.readFrom (in);
        int ons = 0;
        if (read) for (auto* e : *f.getTrack (0)) ons += e->message.isNoteOn() ? 1 : 0;
        check (read && ons == (int) notes.size(), "MIDI file: reads back with all " + juce::String ((int) notes.size()) + " notes");
    }

    std::printf ("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
