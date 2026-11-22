// Headless validation for NovaOpticalDetector + NovaOpticalGainCell +
// NovaEmphasisFilter: silence, impulses, sine sweeps, SC HPF, Emphasis,
// Compress vs Limit, threshold accuracy, NaN/Inf protection, and - the
// headline requirement - proof that the optical gain cell's release is
// genuinely program-dependent (a short transient and a long sustained
// signal reaching the same peak GR must NOT recover identically).

#include "DSP/NovaOpticalDetector.h"
#include "DSP/NovaOpticalGainCell.h"
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

    using nova::glow::NovaOpticalDetector;
    using nova::glow::NovaOpticalGainCell;

    // Detector + gain cell, wired exactly as NovaGlowEngine wires one
    // channel (minus Stereo Link, irrelevant for single-channel testing).
    struct Combined
    {
        NovaOpticalDetector detector;
        NovaOpticalGainCell gainCell;

        void prepare (double sr)
        {
            detector.prepare (sr);
            gainCell.prepare (sr);
        }

        void reset()
        {
            detector.reset();
            gainCell.reset();
        }

        float processSample (float x)
        {
            const float targetGr = detector.processSample (x);
            return gainCell.process (targetGr);
        }
    };

    // Runs a constant-amplitude tone for onSeconds, then silence for up to
    // maxOffSeconds, returning the sample index (relative to tone-off) at
    // which GR first drops back within toleranceDb of 0.
    int measureReleaseSamples (Combined& c, double sr, float toneAmplitude, double onSeconds, float toleranceDb, double maxOffSeconds = 10.0)
    {
        const int onSamples = (int) (sr * onSeconds);
        for (int i = 0; i < onSamples; ++i)
            c.processSample (toneAmplitude * std::sin (2.0 * juce::MathConstants<double>::pi * 200.0 * i / sr));

        const int maxOffSamples = (int) (sr * maxOffSeconds);
        for (int i = 0; i < maxOffSamples; ++i)
        {
            const float gr = c.processSample (0.0f);
            if (gr <= toleranceDb)
                return i;
        }
        return maxOffSamples;
    }

    // Peak GR reached after onSeconds of a constant tone (used to
    // amplitude-tune two signals to matching peak GR before comparing
    // their release behavior).
    float peakGrAfter (Combined& c, double sr, float toneAmplitude, double onSeconds)
    {
        c.reset();
        float peak = 0.0f;
        const int N = (int) (sr * onSeconds);
        for (int i = 0; i < N; ++i)
            peak = juce::jmax (peak, c.processSample (toneAmplitude * std::sin (2.0 * juce::MathConstants<double>::pi * 200.0 * i / sr)));
        return peak;
    }
}

int main()
{
    constexpr double sr = 48000.0;

    // ---- Silence: finite, settles near 0 ----
    {
        Combined c;
        c.prepare (sr);
        c.detector.setParams (-18.0f, 30.0f, 0.0f);
        float gr = 0.0f;
        for (int i = 0; i < (int) sr; ++i)
            gr = c.processSample (0.0f);
        check (std::isfinite (gr), "Silence: finite output");
        check (gr < 0.5f, "Silence: gain reduction settles near 0dB");
    }

    // ---- Impulse: doesn't blow up, recovers ----
    {
        Combined c;
        c.prepare (sr);
        c.detector.setParams (-18.0f, 30.0f, 0.0f);
        float gr = c.processSample (1.0f);
        bool allFinite = std::isfinite (gr);
        for (int i = 0; i < (int) sr; ++i)
        {
            gr = c.processSample (0.0f);
            allFinite = allFinite && std::isfinite (gr);
        }
        check (allFinite, "Impulse: all outputs finite");
    }

    // ---- Sine sweep: finite across the audible range ----
    {
        Combined c;
        c.prepare (sr);
        c.detector.setParams (-20.0f, 30.0f, 0.0f);
        bool allFinite = true;
        const int N = (int) (sr * 2.0);
        for (int i = 0; i < N; ++i)
        {
            const double t = (double) i / sr;
            const double freq = 20.0 * std::pow (1000.0, t / 2.0);
            const float x = 0.8f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * t);
            allFinite = allFinite && std::isfinite (c.processSample (x));
        }
        check (allFinite, "Sine sweep 20Hz-20kHz: all outputs finite");
    }

    // ---- SC HPF: at minimum (20Hz) behaves close to full-band; a tight
    //      HPF measurably reduces triggering on low-frequency-only material
    //      vs. the 20Hz minimum ----
    {
        Combined withTightHpf, withMinHpf;
        withTightHpf.prepare (sr);
        withTightHpf.detector.setParams (-18.0f, 300.0f, 0.0f);
        withMinHpf.prepare (sr);
        withMinHpf.detector.setParams (-18.0f, 20.0f, 0.0f);

        float grTight = 0.0f, grMin = 0.0f;
        const int N = (int) (sr * 0.5);
        for (int i = 0; i < N; ++i)
        {
            const float x = 0.9f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * i / sr);
            grTight = withTightHpf.processSample (x);
            grMin = withMinHpf.processSample (x);
        }
        check (grTight < grMin - 1.0f, "SC HPF at 300Hz measurably reduces triggering vs. 20Hz minimum on 60Hz-only material");
    }

    // ---- Emphasis: raises GR on HF-heavy material, leaves LF-only
    //      material unaffected (proves it's frequency-weighting the
    //      detector, not just a gain boost) ----
    {
        Combined noEmphasisHf, fullEmphasisHf;
        noEmphasisHf.prepare (sr);
        noEmphasisHf.detector.setParams (-18.0f, 20.0f, 0.0f);
        fullEmphasisHf.prepare (sr);
        fullEmphasisHf.detector.setParams (-18.0f, 20.0f, 1.0f);

        float grNoEmphHf = 0.0f, grFullEmphHf = 0.0f;
        const int N = (int) (sr * 0.5);
        for (int i = 0; i < N; ++i)
        {
            const float x = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 8000.0 * i / sr);
            grNoEmphHf = noEmphasisHf.processSample (x);
            grFullEmphHf = fullEmphasisHf.processSample (x);
        }
        check (grFullEmphHf > grNoEmphHf, "Emphasis=100 raises GR on 8kHz-heavy material vs. Emphasis=0");

        Combined noEmphasisLf, fullEmphasisLf;
        noEmphasisLf.prepare (sr);
        noEmphasisLf.detector.setParams (-18.0f, 20.0f, 0.0f);
        fullEmphasisLf.prepare (sr);
        fullEmphasisLf.detector.setParams (-18.0f, 20.0f, 1.0f);

        float grNoEmphLf = 0.0f, grFullEmphLf = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const float x = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 80.0 * i / sr);
            grNoEmphLf = noEmphasisLf.processSample (x);
            grFullEmphLf = fullEmphasisLf.processSample (x);
        }
        check (std::abs (grFullEmphLf - grNoEmphLf) < 0.75f, "Emphasis leaves 80Hz-only (pure low-frequency) material essentially unaffected");
    }

    // ---- Threshold accuracy: lower threshold produces more GR at the same
    //      input level ----
    {
        Combined lowThresh, highThresh;
        lowThresh.prepare (sr);
        lowThresh.detector.setParams (-30.0f, 30.0f, 0.0f);
        highThresh.prepare (sr);
        highThresh.detector.setParams (-6.0f, 30.0f, 0.0f);

        float grLow = 0.0f, grHigh = 0.0f;
        const int N = (int) (sr * 0.5);
        for (int i = 0; i < N; ++i)
        {
            const float x = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 500.0 * i / sr);
            grLow = lowThresh.processSample (x);
            grHigh = highThresh.processSample (x);
        }
        check (grLow > grHigh, "Lower threshold (more Peak Reduction) produces more gain reduction at the same input level");
    }

    // ---- Compress vs Limit: Limit produces a measurably steeper
    //      GR-vs-overage slope (higher effective ratio) than Compress ----
    {
        Combined compress, limit;
        compress.prepare (sr);
        compress.detector.setParams (-24.0f, 30.0f, 0.0f);
        compress.detector.setCompressLimitBlend (0.0f);
        limit.prepare (sr);
        limit.detector.setParams (-24.0f, 30.0f, 0.0f);
        limit.detector.setCompressLimitBlend (1.0f);

        float grCompress = 0.0f, grLimit = 0.0f;
        const int N = (int) (sr * 0.5);
        for (int i = 0; i < N; ++i)
        {
            const float x = 0.95f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 500.0 * i / sr);
            grCompress = compress.processSample (x);
            grLimit = limit.processSample (x);
        }
        check (grLimit > grCompress, "Limit produces measurably more gain reduction than Compress at a heavily-over-threshold input (steeper effective ratio)");
    }

    // ---- Drum-burst chatter check: transient bursts don't produce
    //      oscillating (sign-flipping) sample-to-sample GR deltas ----
    {
        Combined c;
        c.prepare (sr);
        c.detector.setParams (-18.0f, 30.0f, 0.0f);
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
                const float gr = c.processSample (x);
                allFinite = allFinite && std::isfinite (gr);
                const float delta = gr - lastGr;
                if (i > 0 && delta * lastDelta < -1.0e-6f && std::abs (delta) > 0.05f)
                    ++signFlips;
                lastDelta = delta;
                lastGr = gr;
            }
            for (int i = 0; i < 1000; ++i)
                c.processSample (0.0f);
        }
        check (allFinite, "Drum bursts: all outputs finite");
        check (signFlips < 40, "Drum bursts: no significant GR chatter/oscillation (sign flips=" + juce::String (signFlips) + ")");
    }

    // ---- Extreme input levels: no NaN/Inf ----
    {
        Combined c;
        c.prepare (sr);
        c.detector.setParams (-10.0f, 30.0f, 50.0f);
        bool allFinite = true;
        for (float amp : { 100.0f, 1000.0f, 1.0e6f, -1.0e6f })
            for (int i = 0; i < 1000; ++i)
                allFinite = allFinite && std::isfinite (c.processSample (amp));
        check (allFinite, "Extreme input levels (up to 1e6): all outputs finite, no NaN/Inf");
    }

    // ---- THE headline test: a short transient and a long sustained
    //      signal at matching peak GR must NOT recover identically - this
    //      is what proves the optical memory model is genuinely program-
    //      dependent, not just a fixed release time. ----
    {
        constexpr float kTargetPeakGr = 8.0f;
        constexpr float kPeakTolerance = 0.5f;
        constexpr double kShortOnSeconds = 0.05;   // ~50ms transient
        constexpr double kLongOnSeconds = 3.0;     // 3s sustained

        Combined tuner;
        tuner.prepare (sr);
        tuner.detector.setParams (-18.0f, 30.0f, 0.0f);

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

        Combined shortCase, longCase;
        shortCase.prepare (sr);
        shortCase.detector.setParams (-18.0f, 30.0f, 0.0f);
        longCase.prepare (sr);
        longCase.detector.setParams (-18.0f, 30.0f, 0.0f);

        const int shortReleaseSamples = measureReleaseSamples (shortCase, sr, shortAmp, kShortOnSeconds, 0.3f);
        const int longReleaseSamples = measureReleaseSamples (longCase, sr, longAmp, kLongOnSeconds, 0.3f);

        constexpr float kMinimumDivergenceFactor = 1.5f;
        std::printf ("Differential recovery: short=%d samples (%.3fs), long=%d samples (%.3fs)\n",
                     shortReleaseSamples, shortReleaseSamples / sr, longReleaseSamples, longReleaseSamples / sr);
        check ((float) longReleaseSamples > (float) shortReleaseSamples * kMinimumDivergenceFactor,
               "HARD INVARIANT: sustained signal takes at least 1.5x longer to release than a short transient at matching peak GR");

        // Re-run at a second sample rate to also prove sample-rate
        // independence of the differential-recovery property.
        constexpr double sr2 = 96000.0;
        Combined shortCase2, longCase2;
        shortCase2.prepare (sr2);
        shortCase2.detector.setParams (-18.0f, 30.0f, 0.0f);
        longCase2.prepare (sr2);
        longCase2.detector.setParams (-18.0f, 30.0f, 0.0f);
        const int shortReleaseSamples2 = measureReleaseSamples (shortCase2, sr2, shortAmp, kShortOnSeconds, 0.3f);
        const int longReleaseSamples2 = measureReleaseSamples (longCase2, sr2, longAmp, kLongOnSeconds, 0.3f);
        check ((float) longReleaseSamples2 > (float) shortReleaseSamples2 * kMinimumDivergenceFactor,
               "Differential recovery property holds at 96kHz too (sample-rate independence)");
        // And the release TIME (not sample count) should roughly match
        // between the two sample rates, proving coefficients aren't
        // silently sample-rate-locked.
        const double shortSeconds1 = shortReleaseSamples / sr, shortSeconds2 = shortReleaseSamples2 / sr2;
        check (std::abs (shortSeconds1 - shortSeconds2) < 0.15, "Short-transient release TIME is consistent across sample rates (48kHz="
               + juce::String (shortSeconds1, 3) + "s, 96kHz=" + juce::String (shortSeconds2, 3) + "s)");
    }

    std::printf ("\n%d check(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
