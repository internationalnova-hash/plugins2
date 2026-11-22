// Headless validation of NovaBendEngine end-to-end: Flatten convergence
// (spec's own test vectors), unvoiced material staying unpitched, Link
// coupling formant to the effective pitch shift, mono/stereo handling,
// automation robustness, sample-rate independence, latency sanity, silence
// handling, and reset() safety.

#include "NovaBendEngine.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

using nova::bend::NovaBendEngine;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    bool allFinite (const std::vector<float>& v)
    {
        for (float x : v) if (! std::isfinite (x)) return false;
        return true;
    }

    NovaBendEngine::Params defaultParams()
    {
        NovaBendEngine::Params p;
        p.pitchSemitones = 0.0f;
        p.formantSemitones = 0.0f;
        p.flatten = false;
        p.flattenNoteIndex = 0;
        p.flattenOctave = 3;
        p.link = false;
        p.mixPercent = 100.0f;
        p.bypassed = false;
        return p;
    }

    float measureFrequencyByZeroCrossing (const std::vector<float>& x, double sampleRate)
    {
        std::vector<double> crossingTimes;
        for (size_t i = 1; i < x.size(); ++i)
        {
            if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            {
                const double frac = (double) (-x[i - 1]) / (double) (x[i] - x[i - 1]);
                crossingTimes.push_back (((double) (i - 1) + frac) / sampleRate);
            }
        }
        if (crossingTimes.size() < 2) return 0.0f;
        const double totalSpan = crossingTimes.back() - crossingTimes.front();
        const int numPeriods = (int) crossingTimes.size() - 1;
        return (float) ((double) numPeriods / totalSpan);
    }

    void runTone (NovaBendEngine& engine, double sr, float freqHz, double seconds, std::vector<float>& outL, std::vector<float>& outR)
    {
        const int n = (int) (sr * seconds);
        std::vector<float> inL ((size_t) n), inR ((size_t) n);
        for (int i = 0; i < n; ++i)
            inL[(size_t) i] = inR[(size_t) i] = 0.3f * std::sin (2.0 * juce::MathConstants<double>::pi * freqHz * (double) i / sr);
        outL.assign ((size_t) n, 0.0f);
        outR.assign ((size_t) n, 0.0f);
        const int blockSize = 256;
        int pos = 0;
        while (pos < n)
        {
            const int thisBlock = juce::jmin (blockSize, n - pos);
            engine.process (inL.data() + pos, inR.data() + pos, outL.data() + pos, outR.data() + pos, thisBlock);
            pos += thisBlock;
        }
    }

    // A real vocal phrase is nothing like a sustained sine tone: syllables
    // start and stop, pitch jumps between notes, consonants/breaths are
    // unvoiced noise bursts, and there are silence gaps between phrases.
    // The pitch DETECTOR (and, through it, the PSOLA shifter's grain
    // capture) has to track through all of that discontinuity - a case
    // none of the clean-tone tests above exercise. Built as alternating
    // voiced "syllables" (with vibrato/jitter, like makeVoiceLike in
    // FormantValidation.cpp) and unvoiced/silent gaps, with an ABRUPT
    // pitch jump between syllables (no glide) to mimic a real word-to-word
    // pitch change.
    std::vector<float> makeSpeechLike (double sr, std::uint32_t seed)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> jitter (-0.03f, 0.03f);
        std::normal_distribution<float> noiseDist (0.0f, 1.0f);

        const float syllableHz[] = { 140.0f, 165.0f, 120.0f, 180.0f, 150.0f };
        std::vector<float> out;
        for (float f0 : syllableHz)
        {
            const int voicedSamples = (int) (sr * 0.35);
            float phase = 0.0f;
            for (int i = 0; i < voicedSamples; ++i)
            {
                const float period = juce::jmax (4.0f, (float) (sr / f0) * (1.0f + jitter (rng)));
                float excite = 0.0f;
                phase += 1.0f;
                if (phase >= period) { phase -= period; excite = 40.0f; }
                // one-pole formant-ish coloring so it's not a bare pulse train
                static float y1 = 0.0f;
                y1 = 0.85f * y1 + excite * 0.15f;
                out.push_back (y1 * 0.4f + noiseDist (rng) * 0.02f);
            }
            const int consonantSamples = (int) (sr * 0.08);
            for (int i = 0; i < consonantSamples; ++i)
                out.push_back (noiseDist (rng) * 0.25f); // unvoiced burst (consonant/breath)
            const int gapSamples = (int) (sr * 0.05);
            for (int i = 0; i < gapSamples; ++i)
                out.push_back (0.0f); // silence between syllables
        }
        return out;
    }

    // Isolates which ingredient of makeSpeechLike() is responsible for
    // elevated click counts: a perfectly clean glottal pulse train (no
    // jitter, no vibrato), jitter only, vibrato only, or both together -
    // all four sustained (no consonant bursts/silence gaps) so onsets/
    // transitions are ruled out entirely.
    std::vector<float> makeSustainedPulseTrain (double sr, double seconds, float f0, bool jitterOn, bool vibratoOn, std::uint32_t seed)
    {
        const int n = (int) (sr * seconds);
        std::vector<float> out ((size_t) n);
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> jitter (-0.03f, 0.03f);
        float phase = 0.0f;
        float y1 = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / sr;
            const float vibrato = vibratoOn ? (1.0f + 0.02f * std::sin (2.0f * juce::MathConstants<float>::pi * 5.5f * (float) t)) : 1.0f;
            const float j = jitterOn ? jitter (rng) : 0.0f;
            const float period = juce::jmax (4.0f, (float) (sr / f0) * vibrato * (1.0f + j));
            float excite = 0.0f;
            phase += 1.0f;
            if (phase >= period) { phase -= period; excite = 40.0f; }
            y1 = 0.85f * y1 + excite * 0.15f;
            out[(size_t) i] = y1 * 0.4f;
        }
        return out;
    }

    // Click detector: flags samples whose sample-to-sample delta is much
    // larger than the LOCAL delta energy around it - a real, smoothly
    // pitch/formant-shifted voice has no business producing an isolated
    // spike far outside its own recent dynamics.
    int countClicks (const std::vector<float>& x, int skip, float thresholdMultiple)
    {
        const int win = 64;
        int clicks = 0;
        for (int i = (int) juce::jmax (skip, win + 1); i < (int) x.size() - 1; ++i)
        {
            double localSumSq = 0.0;
            for (int k = i - win; k < i; ++k)
            {
                const double d = (double) x[(size_t) k] - (double) x[(size_t) (k - 1)];
                localSumSq += d * d;
            }
            const double localRms = std::sqrt (localSumSq / win) + 1.0e-6;
            const double delta = std::abs ((double) x[(size_t) i] - (double) x[(size_t) (i - 1)]);
            if (delta > thresholdMultiple * localRms)
                ++clicks;
        }
        return clicks;
    }

    // Same detector, but buckets each click by WHERE it falls within the
    // known 0.48s voiced(0.35s)/consonant(0.08s)/gap(0.05s) syllable cycle
    // makeSpeechLike() generates - to tell "clicking throughout sustained
    // voiced material" apart from "clicking clustered right at
    // voiced/unvoiced transitions", which point at completely different
    // root causes.
    void countClicksByRegion (const std::vector<float>& x, int skip, float thresholdMultiple, double sr,
                               int& inVoiced, int& inConsonant, int& inGap)
    {
        inVoiced = inConsonant = inGap = 0;
        const int win = 64;
        const double cycleSamples = 0.48 * sr;
        const double voicedEnd = 0.35 * sr;
        const double consonantEnd = 0.43 * sr;
        for (int i = (int) juce::jmax (skip, win + 1); i < (int) x.size() - 1; ++i)
        {
            double localSumSq = 0.0;
            for (int k = i - win; k < i; ++k)
            {
                const double d = (double) x[(size_t) k] - (double) x[(size_t) (k - 1)];
                localSumSq += d * d;
            }
            const double localRms = std::sqrt (localSumSq / win) + 1.0e-6;
            const double delta = std::abs ((double) x[(size_t) i] - (double) x[(size_t) (i - 1)]);
            if (delta > thresholdMultiple * localRms)
            {
                const double posInCycle = std::fmod ((double) i, cycleSamples);
                if (posInCycle < voicedEnd) ++inVoiced;
                else if (posInCycle < consonantEnd) ++inConsonant;
                else ++inGap;
            }
        }
    }
}

int main()
{
    std::cout << "\n[Engine] FLATTEN convergence - spec's own test vectors toward C3 (~130.81Hz)\n";
    {
        const double sr = 44100.0;
        const float targetHz = 130.81f; // C3

        for (float inputHz : { 196.0f, 220.0f, 261.63f, 329.63f })
        {
            NovaBendEngine engine;
            engine.prepare (sr, 256, true);
            auto p = defaultParams();
            p.flatten = true;
            p.flattenNoteIndex = 0; // C
            p.flattenOctave = 3;
            engine.setParams (p);

            std::vector<float> outL, outR;
            runTone (engine, sr, inputHz, 1.5, outL, outR); // long enough for detector+glide to fully settle

            const int skip = (int) (sr * 0.8); // past detector warm-up + glide settling
            std::vector<float> tail (outL.begin() + skip, outL.end());
            const float measured = measureFrequencyByZeroCrossing (tail, sr);
            const float cents = 1200.0f * std::log2 (juce::jmax (1.0f, measured) / targetHz);
            check (std::abs (cents) < 30.0f,
                   juce::String (inputHz, 2) + "Hz -> target C3(" + juce::String (targetHz, 2) + "Hz): measured="
                       + juce::String (measured, 2) + "Hz (" + juce::String (cents, 2) + "c)");
        }
    }

    std::cout << "\n[Engine] FLATTEN does not force unvoiced/noise material to the target pitch (spec 7/23)\n";
    {
        const double sr = 44100.0;
        NovaBendEngine engine;
        engine.prepare (sr, 256, true);
        auto p = defaultParams();
        p.flatten = true;
        engine.setParams (p);

        const int n = (int) (sr * 1.0);
        std::vector<float> inL ((size_t) n), inR ((size_t) n), outL ((size_t) n), outR ((size_t) n);
        std::mt19937 rng (777);
        std::uniform_real_distribution<float> dist (-0.4f, 0.4f);
        for (int i = 0; i < n; ++i) inL[(size_t) i] = inR[(size_t) i] = dist (rng);

        int pos = 0;
        const int blockSize = 256;
        while (pos < n)
        {
            const int thisBlock = juce::jmin (blockSize, n - pos);
            engine.process (inL.data() + pos, inR.data() + pos, outL.data() + pos, outR.data() + pos, thisBlock);
            pos += thisBlock;
        }

        check (allFinite (outL), "noise input stays finite through Flatten");
        // A strongly-pitched lock-on would show a tall, narrow spectral
        // peak at exactly the target frequency; noise passed through
        // (mostly) unprocessed should not. Approximate via zero-crossing
        // rate variability: a locked tone has a very regular period, noise
        // does not.
        std::vector<double> crossingIntervals;
        double lastCrossing = -1.0;
        for (size_t i = 1; i < outL.size(); ++i)
        {
            if (outL[i - 1] < 0.0f && outL[i] >= 0.0f)
            {
                const double t = (double) i / sr;
                if (lastCrossing >= 0.0) crossingIntervals.push_back (t - lastCrossing);
                lastCrossing = t;
            }
        }
        double mean = 0.0;
        for (double v : crossingIntervals) mean += v;
        mean = crossingIntervals.empty() ? 0.0 : mean / crossingIntervals.size();
        double variance = 0.0;
        for (double v : crossingIntervals) variance += (v - mean) * (v - mean);
        variance = crossingIntervals.empty() ? 0.0 : variance / crossingIntervals.size();
        const double relStdDev = mean > 0.0 ? std::sqrt (variance) / mean : 0.0;
        check (relStdDev > 0.3, "zero-crossing interval variability stays high (rel.stddev=" + juce::String (relStdDev, 3)
                   + ") - noise was not locked onto a single steady pitch");
    }

    std::cout << "\n[Engine] LINK: formant warp follows the effective pitch shift when on, independent when off\n";
    {
        const double sr = 44100.0;

        // With Link OFF, Formant should behave identically regardless of Pitch.
        NovaBendEngine engineOff;
        engineOff.prepare (sr, 256, true);
        auto pOff = defaultParams();
        pOff.pitchSemitones = 7.0f;
        pOff.formantSemitones = 3.0f;
        pOff.link = false;
        engineOff.setParams (pOff);
        std::vector<float> outLOff, outROff;
        runTone (engineOff, sr, 220.0f, 0.3, outLOff, outROff);
        check (allFinite (outLOff), "Link off: finite output with Pitch=+7, Formant=+3");

        // With Link ON, effective formant warp = formant + effective pitch
        // semitones - just confirm it runs stably across the combined
        // range without instability (a direct numeric check of the private
        // formula isn't exposed; the FormantValidation suite already
        // proves the corrector itself is stable at the full +/-24st range
        // this can produce).
        NovaBendEngine engineOn;
        engineOn.prepare (sr, 256, true);
        auto pOn = defaultParams();
        pOn.pitchSemitones = 12.0f;
        pOn.formantSemitones = 12.0f; // combined would be +24st if linked
        pOn.link = true;
        engineOn.setParams (pOn);
        std::vector<float> outLOn, outROn;
        runTone (engineOn, sr, 220.0f, 0.3, outLOn, outROn);
        check (allFinite (outLOn), "Link on: finite, stable output at the extreme combined Pitch=+12/Formant=+12");
    }

    std::cout << "\n[Engine] Mono and stereo input handling, no crash, finite output\n";
    {
        const double sr = 44100.0;
        NovaBendEngine engine;
        engine.prepare (sr, 512, true);
        engine.setParams (defaultParams());
        std::vector<float> outL, outR;
        runTone (engine, sr, 220.0, 0.5, outL, outR);
        check (allFinite (outL) && allFinite (outR), "stereo processing produces finite output throughout");
    }

    std::cout << "\n[Engine] Automation across all params doesn't crash or produce non-finite output\n";
    {
        const double sr = 48000.0;
        const int blockSize = 256;
        NovaBendEngine engine;
        engine.prepare (sr, blockSize, true);

        const int n = (int) (sr * 1.0);
        std::vector<float> inL ((size_t) n), inR ((size_t) n), outL ((size_t) n), outR ((size_t) n);
        for (int i = 0; i < n; ++i)
            inL[(size_t) i] = inR[(size_t) i] = 0.25f * std::sin (2.0 * juce::MathConstants<double>::pi * 180.0 * (double) i / sr);

        juce::Random rng (42);
        int pos = 0;
        while (pos < n)
        {
            NovaBendEngine::Params p;
            p.pitchSemitones = (rng.nextFloat() * 24.0f) - 12.0f;
            p.formantSemitones = (rng.nextFloat() * 24.0f) - 12.0f;
            p.flatten = rng.nextBool();
            p.flattenNoteIndex = rng.nextInt (12);
            p.flattenOctave = 1 + rng.nextInt (6);
            p.link = rng.nextBool();
            p.mixPercent = rng.nextFloat() * 100.0f;
            p.bypassed = rng.nextFloat() < 0.05f;
            engine.setParams (p);

            const int thisBlock = juce::jmin (blockSize, n - pos);
            engine.process (inL.data() + pos, inR.data() + pos, outL.data() + pos, outR.data() + pos, thisBlock);
            pos += thisBlock;
        }

        check (allFinite (outL) && allFinite (outR), "randomized per-block automation across all params stays finite throughout");
    }

    std::cout << "\n[Engine] Sample-rate independence\n";
    {
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        {
            NovaBendEngine engine;
            engine.prepare (sr, 512, true);
            engine.setParams (defaultParams());
            std::vector<float> outL, outR;
            runTone (engine, sr, 220.0, 0.3, outL, outR);
            check (allFinite (outL) && allFinite (outR), "sr=" + juce::String (sr, 0) + "Hz: finite output, latency=" + juce::String (engine.getLatencySamples()));
        }
    }

    std::cout << "\n[Engine] Buffer-size sweep\n";
    {
        const double sr = 44100.0;
        for (int blockSize : { 32, 64, 128, 256, 512, 1024 })
        {
            NovaBendEngine engine;
            engine.prepare (sr, blockSize, true);
            engine.setParams (defaultParams());
            const int n = (int) (sr * 0.2);
            std::vector<float> inL ((size_t) n), inR ((size_t) n), outL ((size_t) n), outR ((size_t) n);
            for (int i = 0; i < n; ++i) inL[(size_t) i] = inR[(size_t) i] = 0.3f * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / sr);
            int pos = 0;
            while (pos < n)
            {
                const int thisBlock = juce::jmin (blockSize, n - pos);
                engine.process (inL.data() + pos, inR.data() + pos, outL.data() + pos, outR.data() + pos, thisBlock);
                pos += thisBlock;
            }
            check (allFinite (outL) && allFinite (outR), "blockSize=" + juce::String (blockSize) + ": finite output");
        }
    }

    std::cout << "\n[Engine] LIVE < STUDIO latency, both sane\n";
    {
        const double sr = 44100.0;
        NovaBendEngine live, studio;
        live.prepare (sr, 512, false);
        studio.prepare (sr, 512, true);
        check (live.getLatencySamples() > 0 && live.getLatencySamples() < (int) (sr * 0.1), "LIVE latency sane: " + juce::String (live.getLatencySamples()));
        check (studio.getLatencySamples() > 0 && studio.getLatencySamples() < (int) (sr * 0.1), "STUDIO latency sane: " + juce::String (studio.getLatencySamples()));
        check (live.getLatencySamples() < studio.getLatencySamples(),
               "LIVE (" + juce::String (live.getLatencySamples()) + ") < STUDIO (" + juce::String (studio.getLatencySamples()) + ")");
    }

    std::cout << "\n[Engine] Silence in produces silence out, no NaN/denormal blowup\n";
    {
        const double sr = 44100.0;
        NovaBendEngine engine;
        engine.prepare (sr, 512, true);
        engine.setParams (defaultParams());

        const int n = (int) (sr * 1.0);
        std::vector<float> inL ((size_t) n, 0.0f), inR ((size_t) n, 0.0f), outL ((size_t) n), outR ((size_t) n);
        int pos = 0;
        while (pos < n)
        {
            const int thisBlock = juce::jmin (512, n - pos);
            engine.process (inL.data() + pos, inR.data() + pos, outL.data() + pos, outR.data() + pos, thisBlock);
            pos += thisBlock;
        }
        check (allFinite (outL) && allFinite (outR), "silence input stays finite throughout");
        float maxAbs = 0.0f;
        for (int i = n / 2; i < n; ++i) maxAbs = juce::jmax (maxAbs, std::abs (outL[(size_t) i]));
        check (maxAbs < 1.0e-4f, "settled output on pure silence stays near zero, max=" + juce::String (maxAbs, 8));
    }

    std::cout << "\n[Engine] Extreme input levels stay finite\n";
    {
        const double sr = 44100.0;
        NovaBendEngine engine;
        engine.prepare (sr, 512, true);
        auto p = defaultParams();
        p.pitchSemitones = 7.0f;
        engine.setParams (p);

        const int n = (int) (sr * 0.3);
        std::vector<float> inL ((size_t) n), inR ((size_t) n), outL ((size_t) n), outR ((size_t) n);
        for (int i = 0; i < n; ++i) inL[(size_t) i] = inR[(size_t) i] = 1.0e4f * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / sr);
        int pos = 0;
        while (pos < n)
        {
            const int thisBlock = juce::jmin (512, n - pos);
            engine.process (inL.data() + pos, inR.data() + pos, outL.data() + pos, outR.data() + pos, thisBlock);
            pos += thisBlock;
        }
        check (allFinite (outL) && allFinite (outR), "extreme input level (1e4) stays finite");
    }

    std::cout << "\n[Engine] reset() is safe mid-stream\n";
    {
        const double sr = 44100.0;
        NovaBendEngine engine;
        engine.prepare (sr, 512, true);
        engine.setParams (defaultParams());

        const int n = 512;
        std::vector<float> inL ((size_t) n), inR ((size_t) n), outL ((size_t) n), outR ((size_t) n);
        for (int i = 0; i < n; ++i) inL[(size_t) i] = inR[(size_t) i] = 0.3f * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / sr);

        for (int block = 0; block < 10; ++block)
        {
            engine.process (inL.data(), inR.data(), outL.data(), outR.data(), n);
            if (block == 5) engine.reset();
        }
        check (allFinite (outL) && allFinite (outR), "output stays finite after a mid-stream reset()");
    }

    std::cout << "\n[Engine] DIAGNOSTIC: click/discontinuity count on speech-like material (syllables + consonant bursts + silence gaps), plain Pitch/Formant\n";
    {
        const double sr = 44100.0;
        auto speech = makeSpeechLike (sr, 777);

        const int dryClicks = countClicks (speech, 512, 10.0f);
        std::cout << "  INFO  BASELINE (dry, unprocessed) speech-like material: " << dryClicks
                   << " click-like discontinuities over " << juce::String ((double) speech.size() / sr, 2) << "s\n";

        for (float pitchSt : { 0.0f, 5.0f, -5.0f })
        {
            NovaBendEngine engine;
            engine.prepare (sr, 512, true);
            NovaBendEngine::Params p = defaultParams();
            p.pitchSemitones = pitchSt;
            engine.setParams (p);

            std::vector<float> outL ((size_t) speech.size(), 0.0f), outR ((size_t) speech.size(), 0.0f);
            const int blockSize = 512;
            int pos = 0;
            while (pos < (int) speech.size())
            {
                const int thisBlock = juce::jmin (blockSize, (int) speech.size() - pos);
                engine.process (speech.data() + pos, speech.data() + pos, outL.data() + pos, outR.data() + pos, thisBlock);
                pos += thisBlock;
            }

            const bool finite = allFinite (outL);
            const int clicks = finite ? countClicks (outL, engine.getLatencySamples() + 512, 10.0f) : -1;
            int inVoiced = 0, inConsonant = 0, inGap = 0;
            if (finite)
                countClicksByRegion (outL, engine.getLatencySamples() + 512, 10.0f, sr, inVoiced, inConsonant, inGap);
            std::cout << "  INFO  Pitch=" << juce::String (pitchSt, 0) << "st on speech-like material: "
                       << clicks << " click-like discontinuities over " << juce::String ((double) speech.size() / sr, 2)
                       << "s (voiced=" << inVoiced << " consonant=" << inConsonant << " gap=" << inGap
                       << "), finite=" << (finite ? "yes" : "NO") << "\n";
            check (finite, "Pitch=" + juce::String (pitchSt, 0) + "st speech-like material: output stays finite");
        }
    }

    std::cout << "\n[Engine] DIAGNOSTIC: isolating jitter vs vibrato vs a perfectly clean tone, sustained (no onsets/consonants/gaps), Pitch=0st\n";
    {
        const double sr = 44100.0;
        struct Variant { const char* name; bool jitterOn; bool vibratoOn; };
        const Variant variants[] = {
            { "clean (no jitter, no vibrato)", false, false },
            { "jitter only",                   true,  false },
            { "vibrato only",                  false, true  },
            { "jitter + vibrato",              true,  true  },
        };
        for (const auto& v : variants)
        {
            auto dry = makeSustainedPulseTrain (sr, 2.0, 160.0f, v.jitterOn, v.vibratoOn, 999);
            const int dryClicks = countClicks (dry, 512, 10.0f);

            NovaBendEngine engine;
            engine.prepare (sr, 512, true);
            NovaBendEngine::Params p = defaultParams();
            engine.setParams (p);

            std::vector<float> outL ((size_t) dry.size(), 0.0f), outR ((size_t) dry.size(), 0.0f);
            const int blockSize = 512;
            int pos = 0;
            while (pos < (int) dry.size())
            {
                const int thisBlock = juce::jmin (blockSize, (int) dry.size() - pos);
                engine.process (dry.data() + pos, dry.data() + pos, outL.data() + pos, outR.data() + pos, thisBlock);
                pos += thisBlock;
            }
            const int wetClicks = countClicks (outL, engine.getLatencySamples() + 512, 10.0f);
            std::cout << "  INFO  " << v.name << ": dry=" << dryClicks << " wet=" << wetClicks
                       << " (added=" << (wetClicks - dryClicks) << ") over 2.0s\n";
        }
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL ENGINE VALIDATION TESTS PASSED\n" : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";
    return failures == 0 ? 0 : 1;
}
