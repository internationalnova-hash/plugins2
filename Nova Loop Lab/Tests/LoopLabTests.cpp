// Nova Loop Lab engine tests on synthetic loops whose tempo, key, hit
// positions and chord changes are known exactly.
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "LoopLabEngine.h"

static int failures = 0, passes = 0;
static void check (bool ok, const juce::String& what)
{
    (ok ? passes : failures)++;
    std::cout << (ok ? "  pass  " : "  FAIL  ") << what << "\n";
}

static constexpr double SR = 44100.0;
static constexpr double TWO_PI = juce::MathConstants<double>::twoPi;

static double midiHz (int note) { return 440.0 * std::pow (2.0, (note - 69) / 12.0); }

// A drum + chord loop. chordRoots: one MIDI root per bar (minor triads).
struct Synth
{
    ll::Sample s;
    std::vector<int> hits; // sample positions of drum hits
};

static Synth makeLoop (double bpm, int bars, std::vector<int> chordRoots, double extraSeconds = 0.0, bool drums = true)
{
    Synth out;
    const double beat = SR * 60.0 / bpm;
    const int n = (int) std::lround (bars * 4 * beat + extraSeconds * SR);
    out.s.sampleRate = SR;
    out.s.audio.setSize (2, n);
    out.s.audio.clear();
    juce::Random rng (3);

    auto add = [&] (int i, float v) { if (i >= 0 && i < n) { out.s.audio.addSample (0, i, v); out.s.audio.addSample (1, i, v); } };

    if (drums)
        for (int b = 0; b < bars * 4; ++b)
        {
            const int kick = (int) std::lround (b * beat);
            out.hits.push_back (kick);
            const bool snare = b % 2 == 1;
            for (int i = 0; i < (int) (0.25 * SR); ++i)
            {
                const double t = i / SR;
                if (snare) add (kick + i, (float) (0.55 * std::exp (-t * 28.0) * (rng.nextFloat() * 2.0 - 1.0)));
                else       add (kick + i, (float) (0.8 * std::exp (-t * 18.0) * std::sin (TWO_PI * (55.0 + 90.0 * std::exp (-t * 30.0)) * t)));
            }
        }

    for (int bar = 0; bar < bars; ++bar)
    {
        const int root = chordRoots[(size_t) bar % chordRoots.size()];
        const int from = (int) (bar * 4 * beat), to = std::min (n, (int) ((bar + 1) * 4 * beat));
        for (int note : { root - 12, root, root + 3, root + 7 })
        {
            const double f = midiHz (note);
            for (int i = from; i < to; ++i)
            {
                const double t = (i - from) / SR;
                const double env = std::min (1.0, t * 50.0) * std::min (1.0, (to - i) / (0.01 * SR));
                add (i, (float) (0.08 * env * (std::sin (TWO_PI * f * t) + 0.3 * std::sin (TWO_PI * 2 * f * t))));
            }
        }
    }
    return out;
}

static float peakOf (const juce::AudioBuffer<float>& b, int from = 0, int to = -1)
{
    if (to < 0) to = b.getNumSamples();
    float p = 0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = from; i < to; ++i) p = std::max (p, std::abs (b.getSample (ch, i)));
    return p;
}

static bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i))) return false;
    return true;
}

static double zeroCrossHz (const juce::AudioBuffer<float>& b, int from, int to, double sr)
{
    int zc = 0;
    for (int i = from + 1; i < to; ++i)
        if ((b.getSample (0, i - 1) < 0) != (b.getSample (0, i) < 0)) ++zc;
    return zc / 2.0 / ((to - from) / sr);
}

static juce::AudioBuffer<float> sine (double hz, int n, double sr = SR)
{
    juce::AudioBuffer<float> b (2, n);
    for (int i = 0; i < n; ++i)
        for (int ch = 0; ch < 2; ++ch) b.setSample (ch, i, (float) (0.5 * std::sin (TWO_PI * hz * i / sr)));
    return b;
}

int main()
{
    std::cout << "Nova Loop Lab engine tests\n";

    // ── Tempo / key ────────────────────────────────────────────────────────
    {
        auto L = makeLoop (92.0, 2, { 60 });
        const auto a = ll::analyse (L.s);
        check (std::abs (a.bpm - 92.0) < 0.6 && a.loopFits && a.bars == 2,
               "92 BPM 2-bar loop: tempo " + juce::String (a.bpm, 2) + ", bars " + juce::String (a.bars));
        check (a.key == 12, "C minor chord loop detected as " + ll::keyName (a.key));
        check ((int) a.peaks.size() == ll::kPeakBins, "waveform overview has " + juce::String (ll::kPeakBins) + " bins");
    }
    {
        auto L = makeLoop (140.0, 4, { 57 });
        const auto a = ll::analyse (L.s);
        check (std::abs (a.bpm - 140.0) < 0.8 && a.bars == 4, "140 BPM 4-bar loop: tempo " + juce::String (a.bpm, 2));
        check (a.key == 12 + 9, "A minor loop detected as " + ll::keyName (a.key));
    }
    {
        auto L = makeLoop (96.0, 2, { 60 }, 0.41);
        const auto a = ll::analyse (L.s);
        check (std::abs (a.bpm - 96.0) < 2.0, "loop with a tail (not whole bars): tempo " + juce::String (a.bpm, 2));
    }

    {
        // Tempo sweep: exact, or at worst double/half time (fixable with the x2 / /2 tempo buttons).
        int exact = 0, octave = 0, total = 0;
        juce::String misses;
        for (double bpm : { 72.0, 78.0, 85.0, 90.0, 100.0, 110.0, 124.0, 133.0, 145.0, 155.0, 165.0, 172.0 })
            for (int bars : { 1, 2, 4 })
            {
                auto L = makeLoop (bpm, bars, { 62 });
                const double got = ll::analyse (L.s).bpm;
                ++total;
                if (std::abs (got - bpm) < 1.0) ++exact;
                else if (std::abs (got - bpm * 2) < 2.0 || std::abs (got - bpm / 2) < 1.0) { ++octave; misses << " " << bpm << "->" << juce::String (got, 1); }
                else misses << " " << bpm << "->" << juce::String (got, 1) << "(!)";
            }
        std::cout << "        tempo sweep: " << exact << "/" << total << " exact, " << octave << " double/half" << misses << "\n";
        check (exact + octave == total && exact >= total * 3 / 4, "tempo sweep 72-172 BPM: every loop exact or double/half, 75%+ exact");
    }

    // ── Chopping ───────────────────────────────────────────────────────────
    {
        auto L = makeLoop (92.0, 2, { 60 });
        const auto a = ll::analyse (L.s);
        ll::ChopSettings cs; cs.mode = ll::ChopMode::transient; cs.sensitivity = 0.33f;
        const auto r = ll::makeChops (L.s, a, cs);
        bool contiguous = ! r.chops.empty() && r.chops.front().start == 0 && r.chops.back().end == L.s.length();
        for (size_t i = 1; i < r.chops.size(); ++i) contiguous &= r.chops[i].start == r.chops[i - 1].end;
        int onHit = 0;
        for (size_t i = 1; i < r.chops.size(); ++i)
        {
            int best = 1 << 30;
            for (int h : L.hits) best = std::min (best, std::abs (r.chops[i].start - h));
            if (best < (int) (0.012 * SR)) ++onHit;
        }
        check (r.chops.size() == 8, "transient chops at 33% sensitivity: " + juce::String ((int) r.chops.size()) + " (want 8)");
        check (contiguous, "chops cover the whole sample with no gaps");
        check (onHit == (int) r.chops.size() - 1, "every transient chop starts on a real hit (within 12 ms): " + juce::String (onHit));

        cs.sensitivity = 1.0f;
        check (ll::makeChops (L.s, a, cs).chops.size() <= (size_t) ll::kMaxChops, "never more than 16 chops");
    }
    {
        auto L = makeLoop (92.0, 2, { 60 });
        const auto a = ll::analyse (L.s);
        ll::ChopSettings cs; cs.mode = ll::ChopMode::grid; cs.gridDiv = 8;
        auto r = ll::makeChops (L.s, a, cs);
        const int slice = (int) std::lround (SR * 60.0 / a.bpm / 2.0);
        bool even = r.chops.size() == 16;
        for (auto& c : r.chops) even &= std::abs ((c.end - c.start) - slice) <= 2 || &c == &r.chops.back();
        check (even && r.effectiveGridDiv == 8, "grid 1/8 on 2 bars: 16 equal slices");
        cs.gridDiv = 16;
        r = ll::makeChops (L.s, a, cs);
        check (r.chops.size() == 16 && r.effectiveGridDiv == 8, "grid 1/16 on 2 bars coarsens to 1/8 to fit 16 pads");
    }
    {
        // Chord change every bar: musical chops should land on the bar lines.
        auto L = makeLoop (90.0, 4, { 60, 56, 63, 58 }, 0.0, false);
        const auto a = ll::analyse (L.s);
        ll::ChopSettings cs; cs.mode = ll::ChopMode::musical; cs.sensitivity = 0.0f;
        const auto r = ll::makeChops (L.s, a, cs);
        std::cout << "        (chord loop tempo " << a.bpm << ", chop starts:";
        for (auto& c : r.chops) std::cout << " " << juce::String (c.start / (SR * 60.0 / 90.0), 2);
        std::cout << " beats)\n";
        const double bar = SR * 60.0 / a.bpm * 4.0;
        int onBar = 0;
        for (size_t i = 1; i < r.chops.size(); ++i)
        {
            const double b = r.chops[i].start / bar;
            if (std::abs (b - std::round (b)) * bar < 0.05 * SR) ++onBar;
        }
        check (r.chops.size() == 4 && onBar == 3, "musical chops split at the 3 chord changes (" + juce::String ((int) r.chops.size()) + " chops, " + juce::String (onBar) + " on bar lines)");
    }

    // ── Generator ──────────────────────────────────────────────────────────
    {
        auto L = makeLoop (92.0, 2, { 60 });
        const auto a = ll::analyse (L.s);
        ll::ChopSettings cs; cs.sensitivity = 0.6f;
        const auto chops = ll::makeChops (L.s, a, cs).chops;
        std::vector<ll::ChopFx> fx (chops.size());

        for (int bars : { 1, 2, 4 })
        {
            ll::GenSettings g; g.bars = bars; g.seed = 42;
            const auto steps = ll::generate (chops, fx, L.s, a, g);
            bool ordered = true, inRange = true;
            for (size_t i = 0; i < steps.size(); ++i)
            {
                inRange &= steps[i].t >= 0 && steps[i].len >= 1 && steps[i].t + steps[i].len <= bars * 16;
                if (i > 0) ordered &= steps[i].t >= steps[i - 1].t + steps[i - 1].len;
            }
            check (! steps.empty() && ordered && inRange, juce::String (bars) + "-bar pattern: steps fit the grid and never overlap");
        }

        ll::GenSettings g; g.seed = 7;
        const auto s1 = ll::generate (chops, fx, L.s, a, g), s2 = ll::generate (chops, fx, L.s, a, g);
        bool same = s1.size() == s2.size();
        for (size_t i = 0; same && i < s1.size(); ++i) same = s1[i].t == s2[i].t && s1[i].chop == s2[i].chop && s1[i].len == s2[i].len;
        g.seed = 8;
        const auto s3 = ll::generate (chops, fx, L.s, a, g);
        bool differs = s3.size() != s1.size();
        for (size_t i = 0; ! differs && i < s1.size(); ++i) differs = s1[i].chop != s3[i].chop || s1[i].len != s3[i].len;
        check (same && differs, "same seed repeats exactly, a new seed gives a new flip");

        auto fx2 = fx;
        for (size_t i = 0; i < fx2.size(); i += 2) fx2[i].use = false;
        bool respects = true;
        for (juce::uint32 seed = 1; seed < 30; ++seed)
        {
            g.seed = seed;
            for (auto& st : ll::generate (chops, fx2, L.s, a, g)) respects &= fx2[(size_t) st.chop].use;
        }
        check (respects, "chops switched out of IN LOOPS never appear");

        g.variation = 0.0f; g.stutter = 0.0f; g.bars = 1;
        int inOrder = 0, total = 0;
        for (juce::uint32 seed = 1; seed < 20; ++seed)
        {
            g.seed = seed;
            const auto st = ll::generate (chops, fx, L.s, a, g);
            for (size_t i = 1; i < st.size(); ++i, ++total)
                if (st[i].chop == (st[i - 1].chop + 1) % (int) chops.size()) ++inOrder;
        }
        check (total > 0 && inOrder > total * 6 / 10, "variation 0% mostly keeps the original chop order (" + juce::String (inOrder) + "/" + juce::String (total) + ")");
    }

    // ── Chop effects ───────────────────────────────────────────────────────
    {
        ll::Sample s; s.sampleRate = SR; s.audio = sine (440.0, (int) SR);
        const ll::Chop c { 0, (int) SR };
        ll::ChopFx fx;

        const auto plain = ll::renderChop (s, c, fx, SR, 1.0);
        check (std::abs (plain.getNumSamples() - (int) SR) <= 2, "no effects: chop length unchanged");

        fx.pitch = 12;
        const auto up = ll::renderChop (s, c, fx, SR, 1.0);
        check (std::abs (up.getNumSamples() - (int) SR / 2) < SR * 0.01 && std::abs (zeroCrossHz (up, 1000, up.getNumSamples() - 1000, SR) - 880.0) < 10.0,
               "pitch +12: an octave up and half as long (sampler-style)");
        fx.pitch = 0;

        fx.stretch = 200;
        const auto st = ll::renderChop (s, c, fx, SR, 1.0);
        check (std::abs (st.getNumSamples() - 2 * (int) SR) < SR * 0.01 && std::abs (zeroCrossHz (st, 2000, st.getNumSamples() - 2000, SR) - 440.0) < 12.0,
               "stretch 200%: twice as long, pitch kept (" + juce::String (zeroCrossHz (st, 2000, st.getNumSamples() - 2000, SR), 1) + " Hz)");
        fx.stretch = 100;

        const auto half = ll::renderChop (s, c, fx, SR, 0.5);
        check (std::abs (half.getNumSamples() - (int) SR / 2) < SR * 0.01, "host tempo twice the sample tempo: chop plays in half the time");

        const auto at48 = ll::renderChop (s, c, fx, 48000.0, 1.0);
        check (std::abs (at48.getNumSamples() - 48000) < 10 && std::abs (zeroCrossHz (at48, 1000, 47000, 48000.0) - 440.0) < 3.0,
               "44.1k sample in a 48k session keeps its pitch and length");

        auto ramp = s; for (int i = 0; i < ramp.length(); ++i) for (int ch = 0; ch < 2; ++ch) ramp.audio.setSample (ch, i, (float) i / ramp.length() - 0.5f);
        fx.reverse = true;
        const auto rev = ll::renderChop (ramp, c, fx, SR, 1.0);
        check (rev.getSample (0, 2000) > 0.4f && rev.getSample (0, rev.getNumSamples() - 2000) < -0.4f, "reverse plays the chop backwards");
        fx.reverse = false;

        ll::Sample hi; hi.sampleRate = SR; hi.audio = sine (6000.0, (int) SR);
        fx.filter = -100;
        const auto lp = ll::renderChop (hi, c, fx, SR, 1.0);
        fx.filter = 100;
        ll::Sample lo; lo.sampleRate = SR; lo.audio = sine (80.0, (int) SR);
        const auto hp = ll::renderChop (lo, c, fx, SR, 1.0);
        check (peakOf (lp, 4000, 40000) < 0.5f * 0.05f && peakOf (hp, 4000, 40000) < 0.5f * 0.1f,
               "filter: full low-pass kills 6 kHz, full high-pass kills 80 Hz");
        fx.filter = 0;

        fx.tone = 100;
        const auto lofi = ll::renderChop (s, c, fx, SR, 1.0);
        float diff = 0;
        for (int i = 0; i < 20000; ++i) diff = std::max (diff, std::abs (lofi.getSample (0, i) - plain.getSample (0, i)));
        check (allFinite (lofi) && diff > 0.02f && peakOf (lofi) <= 0.99f, "lo-fi changes the sound, stays clean of NaN and clipping");
    }

    // ── Loop render ────────────────────────────────────────────────────────
    {
        juce::AudioBuffer<float> click (2, 400);
        click.clear();
        for (int i = 0; i < 400; ++i) for (int ch = 0; ch < 2; ++ch) click.setSample (ch, i, i < 200 ? 0.8f : 0.0f);
        std::vector<juce::AudioBuffer<float>> chops { click };

        ll::LoopSpec spec; spec.bars = 2; spec.bpm = 100.0; spec.sampleRate = 48000.0;
        const int total = ll::loopLengthSamples (spec);
        check (total == (int) std::lround (8 * 60.0 / 100.0 * 48000.0), "2-bar loop at 100 BPM is exactly 8 beats long");

        const double stepLen = total / 32.0;
        auto firstSoundAfter = [] (const juce::AudioBuffer<float>& b, int from)
        {
            for (int i = from; i < b.getNumSamples(); ++i) if (std::abs (b.getSample (0, i)) > 0.05f) return i;
            return -1;
        };

        std::vector<ll::Step> steps { { 0, 1, 0, false }, { 5, 1, 0, false }, { 12, 2, 0, false } };
        auto loop = ll::renderLoop (chops, steps, spec);
        const int p5 = firstSoundAfter (loop, (int) (4.5 * stepLen));
        check (std::abs (p5 - (int) std::lround (5 * stepLen)) <= 50, "steps land on the 16th grid");

        spec.swing = 1.0f;
        loop = ll::renderLoop (chops, steps, spec);
        const int p5s = firstSoundAfter (loop, (int) (4.5 * stepLen));
        check (std::abs (p5s - (int) std::lround (5 * stepLen + stepLen / 3.0)) <= 50, "full swing pushes off-beat 16ths a triplet late");
        const int p12 = firstSoundAfter (loop, (int) (11.5 * stepLen));
        check (std::abs (p12 - (int) std::lround (12 * stepLen)) <= 50, "swing leaves on-beat steps alone");
        spec.swing = 0.0f;

        std::vector<ll::Step> stut { { 0, 2, 0, true } };
        auto sl = ll::renderLoop (chops, stut, spec);
        const int second = firstSoundAfter (sl, 300);
        check (std::abs (second - (int) std::lround (stepLen / 2.0)) <= 50, "stutter retriggers the chop every 1/32");

        // A loud chop summed with overlaps never clips or goes NaN.
        juce::AudioBuffer<float> loud (2, (int) (stepLen * 4));
        for (int i = 0; i < loud.getNumSamples(); ++i) for (int ch = 0; ch < 2; ++ch) loud.setSample (ch, i, (float) std::sin (i * 0.05) * 1.2f);
        std::vector<juce::AudioBuffer<float>> lc { loud };
        std::vector<ll::Step> dense;
        for (int t = 0; t < 32; ++t) dense.push_back ({ t, 1, 0, t % 3 == 0 });
        spec.swing = 0.7f;
        const auto dl = ll::renderLoop (lc, dense, spec);
        check (allFinite (dl) && peakOf (dl) <= 0.981f, "dense swung loop of a hot chop stays under 0 dBFS");
    }

    // ── End to end ─────────────────────────────────────────────────────────
    {
        auto L = makeLoop (92.0, 2, { 60 });
        const auto a = ll::analyse (L.s);
        const auto chops = ll::makeChops (L.s, a, {}).chops;
        std::vector<ll::ChopFx> fx (chops.size());
        std::vector<juce::AudioBuffer<float>> rendered;
        for (size_t i = 0; i < chops.size(); ++i) rendered.push_back (ll::renderChop (L.s, chops[i], fx[i], 48000.0, a.bpm / 120.0));
        ll::GenSettings g; g.seed = 99; g.bars = 4;
        const auto steps = ll::generate (chops, fx, L.s, a, g);
        ll::LoopSpec spec; spec.bars = 4; spec.bpm = 120.0; spec.sampleRate = 48000.0; spec.swing = 0.3f;
        const auto loop = ll::renderLoop (rendered, steps, spec);
        check (loop.getNumSamples() == 4 * 4 * 24000 && allFinite (loop) && peakOf (loop) > 0.1f,
               "92 BPM sample flipped into a 4-bar loop at 120 BPM: right length, audible, clean");
    }

    std::cout << "\n" << passes << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
