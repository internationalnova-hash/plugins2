// Headless validation for NovaGlueDetector: silence, impulses, sine
// sweeps, SC HPF, Ratio ordering, all 6 Attack settings, all 4 fixed
// Release settings, chatter/drum-burst safety, NaN/Inf protection, and -
// the headline requirement - proof that AUTO release is genuinely
// program-dependent (a short transient and a long sustained signal
// reaching the same peak GR must NOT recover identically).

#include "DSP/NovaGlueDetector.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdio>

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& label)
    {
        if (condition)
            std::printf ("PASS: %s\n", label.toRawUTF8());
        else
        {
            std::printf ("FAIL: %s\n", label.toRawUTF8());
            ++failures;
        }
    }

    using nova::glue::NovaGlueDetector;
    constexpr int kAuto = 4;

    // Runs a constant-amplitude tone for onSeconds, then silence for up to
    // maxOffSeconds, returning the sample index (relative to tone-off) at
    // which GR first drops back within toleranceDb of 0.
    int measureReleaseSamples (NovaGlueDetector& d, double sr, float toneAmplitude, double onSeconds, float toleranceDb, double maxOffSeconds = 10.0)
    {
        const int onSamples = (int) (sr * onSeconds);
        for (int i = 0; i < onSamples; ++i)
            d.processSample (toneAmplitude * std::sin (2.0 * juce::MathConstants<double>::pi * 200.0 * i / sr));

        const int maxOffSamples = (int) (sr * maxOffSeconds);
        for (int i = 0; i < maxOffSamples; ++i)
        {
            const float gr = d.processSample (0.0f);
            if (gr <= toleranceDb)
                return i;
        }
        return maxOffSamples;
    }

    float peakGrAfter (NovaGlueDetector& d, double sr, float toneAmplitude, double onSeconds)
    {
        d.reset();
        float peak = 0.0f;
        const int N = (int) (sr * onSeconds);
        for (int i = 0; i < N; ++i)
            peak = juce::jmax (peak, d.processSample (toneAmplitude * std::sin (2.0 * juce::MathConstants<double>::pi * 200.0 * i / sr)));
        return peak;
    }
}

int main()
{
    constexpr double sr = 48000.0;

    // ---- Silence: finite, settles near 0 ----
    {
        NovaGlueDetector d;
        d.prepare (sr);
        d.setParams (-10.0f, 20.0f, 0, 4, kAuto);
        float gr = 0.0f;
        for (int i = 0; i < (int) sr; ++i)
            gr = d.processSample (0.0f);
        check (std::isfinite (gr), "Silence: finite output");
        check (gr < 0.5f, "Silence: gain reduction settles near 0dB");
    }

    // ---- Impulse: doesn't blow up ----
    {
        NovaGlueDetector d;
        d.prepare (sr);
        d.setParams (-10.0f, 20.0f, 0, 4, kAuto);
        bool allFinite = std::isfinite (d.processSample (1.0f));
        for (int i = 0; i < (int) sr; ++i)
            allFinite = allFinite && std::isfinite (d.processSample (0.0f));
        check (allFinite, "Impulse: all outputs finite");
    }

    // ---- Sine sweep: finite across the audible range ----
    {
        NovaGlueDetector d;
        d.prepare (sr);
        d.setParams (-12.0f, 20.0f, 1, 4, kAuto);
        bool allFinite = true;
        const int N = (int) (sr * 2.0);
        for (int i = 0; i < N; ++i)
        {
            const double t = (double) i / sr;
            const double freq = 20.0 * std::pow (1000.0, t / 2.0);
            const float x = 0.8f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * t);
            allFinite = allFinite && std::isfinite (d.processSample (x));
        }
        check (allFinite, "Sine sweep 20Hz-20kHz: all outputs finite");
    }

    // ---- SC HPF: a tight HPF measurably reduces triggering on
    //      low-frequency-only material vs. the 20Hz minimum ----
    {
        NovaGlueDetector tight, minHpf;
        tight.prepare (sr);
        tight.setParams (-10.0f, 300.0f, 0, 4, kAuto);
        minHpf.prepare (sr);
        minHpf.setParams (-10.0f, 20.0f, 0, 4, kAuto);

        float grTight = 0.0f, grMin = 0.0f;
        const int N = (int) (sr * 0.5);
        for (int i = 0; i < N; ++i)
        {
            const float x = 0.9f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * i / sr);
            grTight = tight.processSample (x);
            grMin = minHpf.processSample (x);
        }
        check (grTight < grMin - 1.0f, "SC HPF at 300Hz measurably reduces triggering vs. 20Hz minimum on 60Hz-only material");
    }

    // ---- Ratio ordering: 2:1 < 4:1 < 10:1 GR at a heavily-over-threshold
    //      input ----
    {
        NovaGlueDetector r2, r4, r10;
        r2.prepare (sr);  r2.setParams (-24.0f, 20.0f, 0, 4, kAuto);
        r4.prepare (sr);  r4.setParams (-24.0f, 20.0f, 1, 4, kAuto);
        r10.prepare (sr); r10.setParams (-24.0f, 20.0f, 2, 4, kAuto);

        float gr2 = 0.0f, gr4 = 0.0f, gr10 = 0.0f;
        const int N = (int) (sr * 1.0);
        for (int i = 0; i < N; ++i)
        {
            const float x = 0.95f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 500.0 * i / sr);
            gr2 = r2.processSample (x);
            gr4 = r4.processSample (x);
            gr10 = r10.processSample (x);
        }
        std::printf ("Ratio ordering GR: 2:1=%.2fdB 4:1=%.2fdB 10:1=%.2fdB\n", gr2, gr4, gr10);
        check (gr4 > gr2, "4:1 produces more gain reduction than 2:1 at matching over-threshold input");
        check (gr10 > gr4, "10:1 produces more gain reduction than 4:1 at matching over-threshold input");
    }

    // ---- All 6 Attack settings produce measurably different onset speed:
    //      time-to-reach-90%-of-settled-GR should strictly increase as the
    //      attack choice moves from fastest (0.1ms) to slowest (30ms). A
    //      fixed-instant probe (e.g. "GR at 1ms") can't distinguish the
    //      slower settings, since none of them have risen at all yet by
    //      1ms - this measures actual rise time instead. ----
    {
        NovaGlueDetector reference;
        reference.prepare (sr);
        reference.setParams (-24.0f, 20.0f, 0, 4, 3);
        float finalGr = 0.0f;
        const int settleSamples = (int) (sr * 0.5);
        for (int i = 0; i < settleSamples; ++i)
            finalGr = reference.processSample (0.95f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 500.0 * i / sr));

        int samplesToReach[6];
        for (int choice = 0; choice < 6; ++choice)
        {
            NovaGlueDetector d;
            d.prepare (sr);
            d.setParams (-24.0f, 20.0f, 0, choice, 3); // fixed 1.2s release, irrelevant here
            int reachedAt = settleSamples; // fallback if never reached
            for (int i = 0; i < settleSamples; ++i)
            {
                const float gr = d.processSample (0.95f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 500.0 * i / sr));
                if (gr >= finalGr * 0.9f) { reachedAt = i; break; }
            }
            samplesToReach[choice] = reachedAt;
        }
        std::printf ("Samples to reach 90%% of settled GR by attack choice: %d %d %d %d %d %d\n",
                     samplesToReach[0], samplesToReach[1], samplesToReach[2], samplesToReach[3], samplesToReach[4], samplesToReach[5]);
        bool strictlyIncreasing = true;
        for (int i = 1; i < 6; ++i)
            if (! (samplesToReach[i] > samplesToReach[i - 1]))
                strictlyIncreasing = false;
        check (strictlyIncreasing, "All 6 Attack settings produce measurably different (monotonically slower) onset speed");
    }

    // ---- All 4 fixed Release settings produce measurably different fixed
    //      release times, strictly increasing from choice 0 to 3 ----
    {
        int releaseSamples[4];
        for (int choice = 0; choice < 4; ++choice)
        {
            NovaGlueDetector d;
            d.prepare (sr);
            d.setParams (-18.0f, 20.0f, 0, 4, choice);
            releaseSamples[choice] = measureReleaseSamples (d, sr, 0.9f, 0.5, 0.3f);
        }
        std::printf ("Fixed release samples by choice: %d %d %d %d\n",
                     releaseSamples[0], releaseSamples[1], releaseSamples[2], releaseSamples[3]);
        check (releaseSamples[1] > releaseSamples[0], "Fixed Release 0.3s > 0.1s");
        check (releaseSamples[2] > releaseSamples[1], "Fixed Release 0.6s > 0.3s");
        check (releaseSamples[3] > releaseSamples[2], "Fixed Release 1.2s > 0.6s");
    }

    // ---- Drum-burst chatter check: transient bursts don't produce
    //      oscillating (sign-flipping) sample-to-sample GR deltas ----
    {
        NovaGlueDetector d;
        d.prepare (sr);
        d.setParams (-18.0f, 20.0f, 1, 3, kAuto);
        bool allFinite = true;
        int signFlips = 0;
        float lastDelta = 0.0f;
        float lastGr = 0.0f;
        juce::Random rng (99);
        for (int burst = 0; burst < 20; ++burst)
        {
            for (int i = 0; i < 2000; ++i)
            {
                const float env = std::exp (-i / 1000.0f);
                const float x = env * (rng.nextFloat() * 2.0f - 1.0f);
                const float gr = d.processSample (x);
                allFinite = allFinite && std::isfinite (gr);
                const float delta = gr - lastGr;
                if (i > 0 && delta * lastDelta < -1.0e-6f && std::abs (delta) > 0.05f)
                    ++signFlips;
                lastDelta = delta;
                lastGr = gr;
            }
            for (int i = 0; i < 1000; ++i)
                d.processSample (0.0f);
        }
        check (allFinite, "Drum bursts: all outputs finite");
        check (signFlips < 40, "Drum bursts: no significant GR chatter/oscillation (sign flips=" + juce::String (signFlips) + ")");
    }

    // ---- Extreme input levels: no NaN/Inf ----
    {
        NovaGlueDetector d;
        d.prepare (sr);
        d.setParams (-10.0f, 20.0f, 2, 0, kAuto);
        bool allFinite = true;
        for (float amp : { 100.0f, 1000.0f, 1.0e6f, -1.0e6f })
            for (int i = 0; i < 1000; ++i)
                allFinite = allFinite && std::isfinite (d.processSample (amp));
        check (allFinite, "Extreme input levels (up to 1e6): all outputs finite, no NaN/Inf");
    }

    // ---- THE headline test: with AUTO release, a short transient and a
    //      long sustained signal at matching peak GR must NOT recover
    //      identically - this is what proves AUTO is genuinely
    //      program-dependent, not just another fixed release time. ----
    {
        constexpr float kTargetPeakGr = 8.0f;
        constexpr float kPeakTolerance = 0.5f;
        constexpr double kShortOnSeconds = 0.05;   // ~50ms transient
        constexpr double kLongOnSeconds = 3.0;     // 3s sustained

        NovaGlueDetector tuner;
        tuner.prepare (sr);
        tuner.setParams (-18.0f, 20.0f, 0, 4, kAuto);

        // Binary-search a tone amplitude that reaches kTargetPeakGr for the
        // SHORT duration (peak GR is monotonic in amplitude for a fixed
        // duration, so binary search is valid).
        float loAmp = 0.0f, hiAmp = 4.0f;
        float shortAmp = 1.0f;
        for (int iter = 0; iter < 40; ++iter)
        {
            const float mid = 0.5f * (loAmp + hiAmp);
            const float peak = peakGrAfter (tuner, sr, mid, kShortOnSeconds);
            if (peak < kTargetPeakGr) loAmp = mid; else hiAmp = mid;
            shortAmp = mid;
        }
        const float shortPeak = peakGrAfter (tuner, sr, shortAmp, kShortOnSeconds);

        loAmp = 0.0f; hiAmp = 4.0f;
        float longAmp = 1.0f;
        for (int iter = 0; iter < 40; ++iter)
        {
            const float mid = 0.5f * (loAmp + hiAmp);
            const float peak = peakGrAfter (tuner, sr, mid, kLongOnSeconds);
            if (peak < kTargetPeakGr) loAmp = mid; else hiAmp = mid;
            longAmp = mid;
        }
        const float longPeak = peakGrAfter (tuner, sr, longAmp, kLongOnSeconds);

        check (std::abs (shortPeak - kTargetPeakGr) < kPeakTolerance, "Precondition: short-transient amplitude tuned to target peak GR (got " + juce::String (shortPeak, 2) + "dB)");
        check (std::abs (longPeak - kTargetPeakGr) < kPeakTolerance, "Precondition: sustained-signal amplitude tuned to target peak GR (got " + juce::String (longPeak, 2) + "dB)");

        NovaGlueDetector shortCase, longCase;
        shortCase.prepare (sr);
        shortCase.setParams (-18.0f, 20.0f, 0, 4, kAuto);
        longCase.prepare (sr);
        longCase.setParams (-18.0f, 20.0f, 0, 4, kAuto);

        const int shortReleaseSamples = measureReleaseSamples (shortCase, sr, shortAmp, kShortOnSeconds, 0.3f);
        const int longReleaseSamples = measureReleaseSamples (longCase, sr, longAmp, kLongOnSeconds, 0.3f);

        constexpr float kMinimumDivergenceFactor = 1.5f;
        std::printf ("AUTO differential recovery: short=%d samples (%.3fs), long=%d samples (%.3fs)\n",
                     shortReleaseSamples, shortReleaseSamples / sr, longReleaseSamples, longReleaseSamples / sr);
        check ((float) longReleaseSamples > (float) shortReleaseSamples * kMinimumDivergenceFactor,
               "HARD INVARIANT: sustained signal takes at least 1.5x longer to release than a short transient at matching peak GR (AUTO)");

        // Re-run at a second sample rate to also prove sample-rate
        // independence of the differential-recovery property.
        constexpr double sr2 = 96000.0;
        NovaGlueDetector shortCase2, longCase2;
        shortCase2.prepare (sr2);
        shortCase2.setParams (-18.0f, 20.0f, 0, 4, kAuto);
        longCase2.prepare (sr2);
        longCase2.setParams (-18.0f, 20.0f, 0, 4, kAuto);
        const int shortReleaseSamples2 = measureReleaseSamples (shortCase2, sr2, shortAmp, kShortOnSeconds, 0.3f);
        const int longReleaseSamples2 = measureReleaseSamples (longCase2, sr2, longAmp, kLongOnSeconds, 0.3f);
        check ((float) longReleaseSamples2 > (float) shortReleaseSamples2 * kMinimumDivergenceFactor,
               "Differential recovery property holds at 96kHz too (sample-rate independence)");
        const double shortSeconds1 = shortReleaseSamples / sr, shortSeconds2 = shortReleaseSamples2 / sr2;
        check (std::abs (shortSeconds1 - shortSeconds2) < 0.15, "Short-transient release TIME is consistent across sample rates (48kHz="
               + juce::String (shortSeconds1, 3) + "s, 96kHz=" + juce::String (shortSeconds2, 3) + "s)");
    }

    std::printf ("\n%d check(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
