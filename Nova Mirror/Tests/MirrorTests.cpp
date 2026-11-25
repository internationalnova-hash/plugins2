// Nova Mirror engine tests (synthetic signals).
#include "MirrorCore.h"
#include <cstdio>
#include <random>

using namespace NovaMirror;

static int fails = 0, passes = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("%s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    (ok ? passes : fails)++;
}

// Noise with a spectral tilt (dB/oct around 1 kHz) and optional bump, stereo identical or decorrelated
static juce::AudioBuffer<float> colouredNoise (double sr, double secs, float tiltDbOct, float bumpHz, float bumpDb, unsigned seed, bool decorrelated = false, float gain = 0.1f)
{
    const int n = (int) (secs * sr), N = 1 << 14;
    juce::AudioBuffer<float> out (2, n);
    juce::dsp::FFT fft (14);
    std::mt19937 rng (seed);
    std::normal_distribution<float> g (0.f, 1.f);
    std::vector<float> win ((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (6.2831853f * i / N);
    for (int c = 0; c < 2; ++c)
    {
        if (c == 1 && ! decorrelated) { out.copyFrom (1, 0, out, 0, 0, n); break; }
        std::vector<float> acc ((size_t) n + N, 0.f);
        for (int start = 0; start < n; start += N / 2)
        {
            std::vector<float> f ((size_t) N * 2, 0.f);
            for (int i = 0; i < N; ++i) f[(size_t) i] = g (rng);
            fft.performRealOnlyForwardTransform (f.data(), true);
            for (int k = 0; k <= N / 2; ++k)
            {
                const float hz = std::max (10.f, (float) (k * sr / N));
                float db = -3.f * std::log2 (hz / 1000.f) + tiltDbOct * std::log2 (hz / 1000.f);   // pink + tilt
                if (bumpDb != 0.f) { const float x = std::log2 (hz / bumpHz); db += bumpDb * std::exp (-x * x * 4.f); }
                const float m = std::pow (10.f, db / 20.f);
                f[(size_t) 2 * k] *= m; f[(size_t) 2 * k + 1] *= m;
            }
            fft.performRealOnlyInverseTransform (f.data());
            for (int i = 0; i < N; ++i) acc[(size_t) (start + i)] += f[(size_t) i] * win[(size_t) i];
        }
        float pk = 0.f;
        for (int i = 0; i < n; ++i) pk = std::max (pk, std::abs (acc[(size_t) i]));
        for (int i = 0; i < n; ++i) out.setSample (c, i, acc[(size_t) i] / pk * gain * 3.f);
    }
    return out;
}

static juce::AudioBuffer<float> run (MatchEngine& e, const juce::AudioBuffer<float>& in)
{
    juce::AudioBuffer<float> b (in);
    for (int s = 0; s < b.getNumSamples(); s += 512)
    {
        const int n = std::min (512, b.getNumSamples() - s);
        float* ch[2] = { b.getWritePointer (0) + s, b.getWritePointer (1) + s };
        e.process (ch, 2, n);
    }
    return b;
}

static float shapeDistance (const Profile& a, const Profile& b, float from = 60.f, float to = 16000.f)
{
    float ma = 0, mb = 0; int c = 0;
    for (int k = 0; k < kBands; ++k) if (bandCenter (k) >= from && bandCenter (k) <= to) { ma += a.mono[0][(size_t) k]; mb += b.mono[0][(size_t) k]; ++c; }
    ma /= c; mb /= c;
    float s = 0;
    for (int k = 0; k < kBands; ++k) if (bandCenter (k) >= from && bandCenter (k) <= to)
    {
        const float d = (a.mono[0][(size_t) k] - ma) - (b.mono[0][(size_t) k] - mb);
        s += d * d;
    }
    return std::sqrt (s / c);
}

static juce::AudioBuffer<float> tail (const juce::AudioBuffer<float>& b, double sr, double secs)
{
    const int n = (int) (secs * sr);
    juce::AudioBuffer<float> t (b.getNumChannels(), n);
    for (int c = 0; c < b.getNumChannels(); ++c) t.copyFrom (c, 0, b, c, b.getNumSamples() - n, n);
    return t;
}

int main()
{
    const double sr = 48000.0;
    MatchEngine eng;

    // 1. No target: output = input delayed by the reported latency
    {
        eng.prepare (sr, 512, 2);
        eng.setProfile (nullptr);
        auto in = colouredNoise (sr, 3.0, 0.f, 0, 0, 1, true);
        auto out = run (eng, in);
        const int lat = eng.getLatency();
        float err = 0.f;
        for (int c = 0; c < 2; ++c)
            for (int i = lat; i < in.getNumSamples(); ++i)
                err = std::max (err, std::abs (out.getSample (c, i) - in.getSample (c, i - lat)));
        check (err < 1.0e-4f, "no target: passes through, delayed by " + juce::String (lat) + " samples (max err " + juce::String (err, 7) + ")");
    }

    // 2. A bright track moves toward a dark reference
    const auto refAudio = colouredNoise (sr, 20.0, -0.5f, 80.f, 5.f, 7);   // a realistic gap: ~1.5 dB/oct + a bass and a presence difference
    const auto ref = analyseReference (refAudio, sr, "dark ref");
    const auto bright = colouredNoise (sr, 12.0, +1.f, 3000.f, 4.f, 3);
    const auto inProf = analyseReference (tail (bright, sr, 6.0), sr, "in");
    float dBefore = shapeDistance (inProf, ref);
    {
        eng.prepare (sr, 512, 2);
        eng.setProfile (&ref);
        EngineParams p; p.cut = 1.f; p.boost = 1.f; p.mode = 2; p.sections = false;
        eng.setParams (p);
        auto out = run (eng, bright);
        const auto outProf = analyseReference (tail (out, sr, 6.0), sr, "out");
        const float dAfter = shapeDistance (outProf, ref);
        check (dAfter < dBefore * 0.45f, "match: tonal distance " + juce::String (dBefore, 2) + " dB -> " + juce::String (dAfter, 2) + " dB");
        EngineMeters m; eng.getMeters (m);
        check (m.match > 70.f, "match readout " + juce::String (m.match, 0) + "%");

        // loudness roughly kept
        const float rin = bright.getRMSLevel (0, bright.getNumSamples() / 2, bright.getNumSamples() / 2);
        const float rout = out.getRMSLevel (0, out.getNumSamples() / 2, out.getNumSamples() / 2);
        check (std::abs (juce::Decibels::gainToDecibels (rout / rin)) < 3.f, "overall level stays within 3 dB (" + juce::String (juce::Decibels::gainToDecibels (rout / rin), 1) + " dB)");
    }

    // 3. FROM/TO: nothing below FROM changes
    {
        eng.prepare (sr, 512, 2);
        eng.setProfile (&ref);
        EngineParams p; p.cut = 1.f; p.boost = 1.f; p.fromHz = 1000.f; p.sections = false;
        eng.setParams (p);
        auto out = run (eng, bright);
        const auto o = analyseReference (tail (out, sr, 6.0), sr, "o");
        float worst = 0.f;
        for (int k = 0; k < kBands; ++k)
            if (bandCenter (k) > 60.f && bandCenter (k) < 600.f)
                worst = std::max (worst, std::abs (o.mono[0][(size_t) k] - inProf.mono[0][(size_t) k]));
        check (worst < 0.75f, "FROM 1 kHz leaves 60-600 Hz alone (worst " + juce::String (worst, 2) + " dB)");
    }

    // 4. Cut only never boosts
    {
        eng.prepare (sr, 512, 2);
        eng.setProfile (&ref);
        EngineParams p; p.cut = 1.f; p.boost = 0.f; p.sections = false;
        eng.setParams (p);
        auto out = run (eng, bright);
        EngineMeters m; eng.getMeters (m);
        float maxG = -99.f, minG = 99.f;
        for (float g : m.gainDb) { maxG = std::max (maxG, g); minG = std::min (minG, g); }
        check (maxG <= 0.01f && minG < -1.f, "BOOST 0: only cuts (max " + juce::String (maxG, 2) + " dB, min " + juce::String (minG, 1) + " dB)");
    }

    // 5. Side-only leaves a mono (L=R) signal untouched
    {
        eng.prepare (sr, 512, 2);
        eng.setProfile (&ref);
        EngineParams p; p.cut = 1.f; p.boost = 1.f; p.stereo = 2; p.sections = false;
        eng.setParams (p);
        auto out = run (eng, bright);   // bright is L=R
        const int lat = eng.getLatency();
        float err = 0.f;
        for (int i = lat; i < bright.getNumSamples(); ++i) err = std::max (err, std::abs (out.getSample (0, i) - bright.getSample (0, i - lat)));
        check (err < 1.0e-4f, "Side mode leaves the centre alone (max err " + juce::String (err, 6) + ")");
    }

    // 6. Sections: quiet dark verse + loud bright hook
    {
        auto verse = colouredNoise (sr, 15.0, -2.f, 0, 0, 11, false, 0.03f);
        auto hook = colouredNoise (sr, 15.0, +1.f, 0, 0, 12, false, 0.12f);
        juce::AudioBuffer<float> song (2, verse.getNumSamples() + hook.getNumSamples());
        for (int c = 0; c < 2; ++c) { song.copyFrom (c, 0, verse, c, 0, verse.getNumSamples()); song.copyFrom (c, verse.getNumSamples(), hook, c, 0, hook.getNumSamples()); }
        const auto pr = analyseReference (song, sr, "song");
        check (pr.has[secHook] && (pr.has[secVerse] || pr.has[secBreak]), "reference split into sections (" + juce::String ((int) pr.sections.size()) + " sections)");
        const float hookTilt = pr.mono[secHook][(size_t) bandIndexFor (8000)] - pr.mono[secHook][(size_t) bandIndexFor (200)];
        const int quiet = pr.has[secBreak] ? secBreak : secVerse;
        const float verseTilt = pr.mono[(size_t) quiet][(size_t) bandIndexFor (8000)] - pr.mono[(size_t) quiet][(size_t) bandIndexFor (200)];
        check (hookTilt > verseTilt + 3.f, "hook and verse keep their own tone (" + juce::String (hookTilt, 1) + " vs " + juce::String (verseTilt, 1) + " dB)");

        const auto rt = Profile::fromVar (pr.toVar());
        check (rt.sections.size() == pr.sections.size() && std::abs (rt.mono[secHook][10] - pr.mono[secHook][10]) < 0.02f, "profile save/load round trip");
    }

    // 7. Nova Targets exist and differ
    {
        const auto names = novaTargetNames();
        const auto trap = novaTarget ("Trap"), pop = novaTarget ("Pop");
        check (names.size() >= 8 && trap.mono[0][(size_t) bandIndexFor (48)] - trap.mono[0][(size_t) bandIndexFor (1000)]
                                     > pop.mono[0][(size_t) bandIndexFor (48)] - pop.mono[0][(size_t) bandIndexFor (1000)],
               "Nova Targets: " + names.joinIntoString (", "));
    }

    std::printf ("\n%d passed, %d failed\n", passes, fails);
    return fails == 0 ? 0 : 1;
}
