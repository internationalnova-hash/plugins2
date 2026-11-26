// Nova Notes engine tests: synthetic vocal checks + an optional real WAV.
//   NovaNotesTests [vocal.wav] [out-dir]

#include "NotesCore.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <random>
#include <limits>

using namespace NovaNotes;

static int failures = 0, passes = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("%s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    (ok ? passes : failures)++;
}

// Glottal-ish source (band-limited saw) through two formant resonators.
struct SynthNote { double start, len; float midi; float vibCents; float scoop; };
static std::vector<float> synthVocal (double sr, const std::vector<SynthNote>& notes, double total)
{
    const int n = (int) (total * sr);
    std::vector<float> y ((size_t) n, 0.f);
    double phase = 0.0;
    std::mt19937 rng (5);
    std::normal_distribution<float> noise (0.f, 1.f);
    for (const auto& nt : notes)
    {
        const int a = (int) (nt.start * sr), b = std::min (n, (int) ((nt.start + nt.len) * sr));
        for (int i = a; i < b; ++i)
        {
            const double t = (i - a) / sr;
            const double sc = nt.scoop * std::exp (-t / 0.04);
            const double vib = nt.vibCents / 100.0 * std::sin (2 * juce::MathConstants<double>::pi * 5.5 * t) * std::min (1.0, t / 0.2);
            const double hz = 440.0 * std::exp2 ((nt.midi + sc + vib - 69.0) / 12.0);
            phase += hz / sr;
            phase -= std::floor (phase);
            float s = 0.f;
            for (int h = 1; h * hz < sr * 0.45 && h < 40; ++h)
                s += (float) (std::sin (2 * juce::MathConstants<double>::pi * h * phase) / h);
            const double env = std::min (1.0, t / 0.015) * std::min (1.0, ((b - i) / sr) / 0.02);
            y[(size_t) i] = (float) (0.25 * env) * s;
        }
    }
    // formants via two resonators (fixed spectral envelope -> realistic for PSOLA)
    auto reso = [&] (float f0, float bw, float g)
    {
        const double r = std::exp (-juce::MathConstants<double>::pi * bw / sr);
        const double c1 = 2 * r * std::cos (2 * juce::MathConstants<double>::pi * f0 / sr), c2 = -r * r;
        double z1 = 0, z2 = 0;
        std::vector<float> o ((size_t) n);
        for (int i = 0; i < n; ++i) { const double v = (1 - r) * y[(size_t) i] + c1 * z1 + c2 * z2; z2 = z1; z1 = v; o[(size_t) i] = (float) v; }
        return std::make_pair (o, g);
    };
    auto [f1, g1] = reso (700, 110, 1.0f);
    auto [f2, g2] = reso (1200, 140, 0.6f);
    for (int i = 0; i < n; ++i) y[(size_t) i] = 0.5f * y[(size_t) i] + 2.f * (g1 * f1[(size_t) i] + g2 * f2[(size_t) i]) + 0.0005f * noise (rng);
    float peak = 0.f;
    for (float v : y) peak = std::max (peak, std::abs (v));
    for (float& v : y) v *= 0.5f / peak;
    return y;
}

static void writeWav (const juce::File& f, const std::vector<float>& x, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 1, 24, {}, 0));
    if (w == nullptr) return;
    const float* ch[] = { x.data() };
    w->writeFromFloatArrays (ch, 1, (int) x.size());
}

static float centsErrorOfRender (const std::vector<float>& y, double sr, const std::vector<Note>& notes, float& worst)
{
    const auto t2 = trackPitch (y.data(), (int) y.size(), sr);
    float sum = 0.f; int c = 0;
    worst = 0.f;
    for (const auto& n : notes)
    {
        if (n.end - n.start < 30 || n.timeShift != 0.f) continue;
        const float got = measureCenter (t2, n.start, n.end);
        const float err = std::abs (got - n.pitch) * 100.f;
        sum += err; ++c;
        worst = std::max (worst, err);
    }
    return c ? sum / c : 999.f;
}

int main (int argc, char** argv)
{
    juce::ignoreUnused (argc);
    const double sr = 48000.0;

    // ── 1. Synthetic phrase: known notes, off by known amounts ───────────────
    const std::vector<SynthNote> phrase = {
        { 0.10, 0.45, 60.22f, 0.f, -0.6f },  { 0.60, 0.30, 62.f - 0.31f, 0.f, 0.f }, { 0.95, 0.80, 64.15f, 45.f, -0.8f },
        { 1.80, 0.25, 65.4f, 0.f, 0.f },     { 2.10, 0.70, 67.f, 30.f, 0.5f },       { 2.90, 0.12, 69.1f, 0.f, 0.f },
        { 3.05, 0.90, 64.8f, 60.f, -1.0f }
    };
    auto x = synthVocal (sr, phrase, 4.2);
    const auto t = trackPitch (x.data(), (int) x.size(), sr);
    auto notes = segmentNotes (t);
    check (notes.size() == phrase.size(), "synthetic: found " + juce::String ((int) notes.size()) + " notes (expected 7)");
    float worstCenter = 0.f;
    for (size_t i = 0; i < std::min (notes.size(), phrase.size()); ++i)
        worstCenter = std::max (worstCenter, std::abs (notes[i].center - phrase[i].midi) * 100.f);
    check (worstCenter < 12.f, "synthetic: note centres within " + juce::String (worstCenter, 1) + " cents");
    const auto key = detectKey (notes, t);
    {
        const int km = scaleMask (key);
        bool fits = true;
        for (auto& sn : phrase) fits = fits && (km & (1 << ((int) std::lround (sn.midi) % 12)));
        check (fits, "synthetic: detected key contains every sung note (tonic " + juce::String (key.tonic) + (key.minor ? " minor)" : " major)"));
    }

    const auto ep = findEpochs (x.data(), (int) x.size(), t);
    std::vector<float> y (x.size());

    // identity
    render (x.data(), (int) x.size(), t, ep, notes, y.data());
    float diff = 0.f;
    for (size_t i = 0; i < x.size(); ++i) diff = std::max (diff, std::abs (y[i] - x[i]));
    check (diff == 0.f, "no edits renders the original exactly");

    // correct pitch 100% -> every note lands on its semitone
    auto fixedNotes = notes;
    correctPitch (fixedNotes, {}, 1.f, 0.f, 0);
    render (x.data(), (int) x.size(), t, ep, fixedNotes, y.data());
    float worst = 0.f;
    const float mean = centsErrorOfRender (y, sr, fixedNotes, worst);
    check (mean < 6.f && worst < 15.f, "correct pitch: rendered centres mean " + juce::String (mean, 1) + "c, worst " + juce::String (worst, 1) + "c");

    // move one note +3 semitones, others untouched
    auto moved = notes;
    moved[2].pitch = moved[2].center + 3.f;
    render (x.data(), (int) x.size(), t, ep, moved, y.data());
    const auto t3 = trackPitch (y.data(), (int) y.size(), sr);
    check (std::abs (measureCenter (t3, moved[2].start, moved[2].end) - moved[2].pitch) < 0.1f, "moved note lands +3 semitones");
    float farDiff = 0.f;
    const int guard = moved[2].start * t.hop - (int) (0.25 * sr);
    for (int i = 0; i < guard; ++i) farDiff = std::max (farDiff, std::abs (y[(size_t) i] - x[(size_t) i]));
    check (farDiff < 1.0e-4f, "audio before the edited note is untouched (max diff " + juce::String (farDiff, 6) + ")");

    // AI assist on the synthetic phrase
    auto ai = notes;
    const auto rep = aiAssist (t, ai, {}, AssistStyle::tight, key);
    render (x.data(), (int) x.size(), t, ep, ai, y.data());
    const float aiMean = centsErrorOfRender (y, sr, ai, worst);
    float offTarget = 0.f;
    for (size_t i = 0; i < ai.size() && i < phrase.size(); ++i)
        offTarget = std::max (offTarget, std::abs (ai[i].pitch - std::round (phrase[i].midi)) * 100.f);
    check (offTarget < 20.f, "AI assist targets the right notes (worst " + juce::String (offTarget, 1) + "c from intended)");
    check (aiMean < 8.f, "AI assist render accuracy " + juce::String (aiMean, 1) + "c (fixed " + juce::String (rep.fixed)
           + ", vibrato kept " + juce::String (rep.keptVibrato) + ")");

    // split + time move + formant + gain should all render finite audio
    auto misc = notes;
    splitNote (misc, misc[2].id, (misc[2].start + misc[2].end) / 2, t);
    misc[0].timeShift = 0.03f;
    misc[1].formant = 200.f;
    misc[3].gainDb = -6.f;
    render (x.data(), (int) x.size(), t, ep, misc, y.data());
    bool finite = true;
    for (float v : y) finite = finite && std::isfinite (v) && std::abs (v) < 2.f;
    check (misc.size() == notes.size() + 1 && finite, "split/time/formant/gain render cleanly");

    // Draw tool: a hand-drawn line (rising 2 semitones across the note) is what you hear
    {
        auto drawnNotes = notes;
        auto& dn = drawnNotes[2];
        const int len = dn.end - dn.start;
        dn.drawn.assign ((size_t) len, std::numeric_limits<float>::quiet_NaN());
        for (int k = len / 5; k < len - len / 5; ++k) dn.drawn[(size_t) k] = 2.f * (float) k / (float) len - 0.5f;
        render (x.data(), (int) x.size(), t, ep, drawnNotes, y.data());
        const auto tr = trackPitch (y.data(), (int) y.size(), sr);
        float worstDraw = 0.f; int counted = 0;
        for (int k = len / 5 + 6; k < len - len / 5 - 6; ++k)
        {
            const float got = tr.midi[(size_t) (dn.start + k)];
            if (got <= 0.f) continue;
            worstDraw = std::max (worstDraw, std::abs (got - (dn.pitch + dn.drawn[(size_t) k])) * 100.f);
            ++counted;
        }
        check (counted > 20 && worstDraw < 25.f, "drawn pitch line is followed (worst " + juce::String (worstDraw, 1) + " cents over " + juce::String (counted) + " frames)");
        const auto rtDraw = notesFromJson (notesToJson (drawnNotes));
        check (rtDraw[2].hasDrawn() && ! std::isfinite (rtDraw[2].drawn[0]) && std::abs (rtDraw[2].drawn[(size_t) len / 2] - dn.drawn[(size_t) len / 2]) < 1.0e-5f,
               "drawn line survives save/reload");
        auto split = drawnNotes;
        NovaNotes::splitNote (split, split[2].id, split[2].start + len / 2, t);
        const float before = drawnNotes[2].pitch + drawnNotes[2].drawn[(size_t) len / 2 + 2];
        const float after = split[3].pitch + split[3].drawn[2];
        check (std::abs (before - after) < 1.0e-4f, "splitting a drawn note keeps the drawing where it was");
    }

    // JSON round trip
    const auto rt = notesFromJson (notesToJson (misc));
    check (rt.size() == misc.size() && std::abs (rt[1].formant - 200.f) < 0.5f, "notes JSON round trip");

    // ── 2. Real vocal (optional) ────────────────────────────────────────────
    if (argc > 1)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (juce::String (argv[1]))));
        if (r != nullptr)
        {
            const int len = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 60));
            juce::AudioBuffer<float> buf ((int) r->numChannels, len);
            r->read (&buf, 0, len, 0, true, true);
            std::vector<float> v ((size_t) len);
            for (int i = 0; i < len; ++i)
            {
                float s = 0.f;
                for (int c = 0; c < buf.getNumChannels(); ++c) s += buf.getSample (c, i);
                v[(size_t) i] = s / (float) buf.getNumChannels();
            }
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            const auto tr = trackPitch (v.data(), len, r->sampleRate);
            auto ns = segmentNotes (tr);
            const auto eps = findEpochs (v.data(), len, tr);
            const auto k = detectKey (ns, tr);
            const auto t1 = juce::Time::getMillisecondCounterHiRes();
            static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
            std::printf ("real vocal: %.1f s, %d notes, key %s %s (%.2f), analysis %.0f ms\n",
                         len / r->sampleRate, (int) ns.size(), names[k.tonic], k.minor ? "minor" : "major", k.confidence, t1 - t0);
            check (ns.size() > 5, "real vocal: notes found");

            auto aiNotes = ns;
            const auto rp = aiAssist (tr, aiNotes, {}, AssistStyle::tight, k);
            std::vector<float> out ((size_t) len);
            const auto t2 = juce::Time::getMillisecondCounterHiRes();
            render (v.data(), len, tr, eps, aiNotes, out.data());
            const auto t3 = juce::Time::getMillisecondCounterHiRes();
            float w = 0.f;
            const float m = centsErrorOfRender (out, r->sampleRate, aiNotes, w);
            std::printf ("real vocal AI assist: fixed %d, slides kept %d, vibrato kept %d, runs %d, left %d; render %.0f ms; mean %.1fc\n",
                         rp.fixed, rp.keptSlides, rp.keptVibrato, rp.runs, rp.leftAlone, t3 - t2, m);
            check (m < 15.f, "real vocal: rendered notes land on their targets (mean " + juce::String (m, 1) + "c)");
            if (argc > 2)
            {
                const juce::File dir { juce::String (argv[2]) };
                writeWav (dir.getChildFile ("notes_original.wav"), v, r->sampleRate);
                writeWav (dir.getChildFile ("notes_ai_tight.wav"), out, r->sampleRate);
                auto nat = ns;
                aiAssist (tr, nat, {}, AssistStyle::natural, k);
                render (v.data(), len, tr, eps, nat, out.data());
                writeWav (dir.getChildFile ("notes_ai_natural.wav"), out, r->sampleRate);
                writeWav (dir.getChildFile ("notes_synth_in.wav"), x, sr);
            }
        }
    }

    std::printf ("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
