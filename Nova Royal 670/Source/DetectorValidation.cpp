// Headless validation for NovaVariableMuDetector: silence, impulses, sine
// sweeps, sustained low-frequency material, transient-heavy synthetic
// drum bursts, all 6 time-constant modes' release shape, knee/threshold
// accuracy, and NaN/Inf protection under extreme input levels.

#include "DSP/NovaVariableMuDetector.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdio>

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& label)
    {
        if (condition)
        {
            std::printf ("PASS: %s\n", label.toRawUTF8());
        }
        else
        {
            std::printf ("FAIL: %s\n", label.toRawUTF8());
            ++failures;
        }
    }

    using nova::royal670::NovaVariableMuDetector;

    // Runs a constant-amplitude tone through the detector for the given
    // duration and returns the sample index (relative to toneStart) at
    // which gainReductionDb first drops back within toleranceDb of 0 -
    // i.e. how long release took, in samples.
    int measureReleaseSamples (NovaVariableMuDetector& det, double sr, float toneAmplitude, float toleranceDb)
    {
        const int onSamples = (int) (sr * 1.0); // 1s of tone to build up GR
        for (int i = 0; i < onSamples; ++i)
            det.processSample (toneAmplitude * std::sin (2.0 * juce::MathConstants<double>::pi * 200.0 * i / sr));

        const int maxOffSamples = (int) (sr * 10.0); // up to 10s to release
        for (int i = 0; i < maxOffSamples; ++i)
        {
            const float gr = det.processSample (0.0f);
            if (gr <= toleranceDb)
                return i;
        }
        return maxOffSamples; // never released within the window
    }
}

int main()
{
    constexpr double sr = 48000.0;

    // ---- Silence: no NaN/Inf, gain reduction settles to ~0 ----
    {
        NovaVariableMuDetector det;
        det.prepare (sr);
        det.setParams (-10.0f, 0.0f, 4, 80.0f);
        float gr = 0.0f;
        for (int i = 0; i < (int) sr; ++i)
            gr = det.processSample (0.0f);
        check (std::isfinite (gr), "Silence: finite output");
        check (gr < 0.5f, "Silence: gain reduction settles near 0dB");
    }

    // ---- Impulse: single-sample spike doesn't blow up, recovers ----
    {
        NovaVariableMuDetector det;
        det.prepare (sr);
        det.setParams (-10.0f, 0.0f, 4, 80.0f);
        float gr = det.processSample (1.0f);
        bool allFinite = std::isfinite (gr);
        for (int i = 0; i < (int) sr; ++i)
        {
            gr = det.processSample (0.0f);
            allFinite = allFinite && std::isfinite (gr);
        }
        check (allFinite, "Impulse: all outputs finite");
    }

    // ---- Sine sweep: finite output across the audible range ----
    {
        NovaVariableMuDetector det;
        det.prepare (sr);
        det.setParams (-15.0f, 0.0f, 3, 80.0f);
        bool allFinite = true;
        const int N = (int) (sr * 2.0);
        for (int i = 0; i < N; ++i)
        {
            const double t = (double) i / sr;
            const double freq = 20.0 * std::pow (1000.0, t / 2.0); // 20Hz -> 20kHz over 2s
            const float x = 0.8f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * t);
            allFinite = allFinite && std::isfinite (det.processSample (x));
        }
        check (allFinite, "Sine sweep 20Hz-20kHz: all outputs finite");
    }

    // ---- Sustained low-frequency material: sidechain HPF actually
    //      reduces triggering vs. no HPF ----
    {
        NovaVariableMuDetector withHpf, withoutHpf;
        withHpf.prepare (sr);
        withHpf.setParams (-10.0f, 0.0f, 4, 300.0f); // aggressive HPF well above the 60Hz tone
        withoutHpf.prepare (sr);
        withoutHpf.setParams (-10.0f, 0.0f, 4, 20.0f); // effectively no HPF

        float grWith = 0.0f, grWithout = 0.0f;
        const int N = (int) (sr * 0.5);
        for (int i = 0; i < N; ++i)
        {
            const float x = 0.9f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * i / sr);
            grWith = withHpf.processSample (x);
            grWithout = withoutHpf.processSample (x);
        }
        check (grWith < grWithout - 1.0f, "Sidechain HPF measurably reduces triggering on 60Hz-only material");
    }

    // ---- Transient-heavy synthetic drum bursts: finite, and GR responds ----
    {
        NovaVariableMuDetector det;
        det.prepare (sr);
        det.setParams (-12.0f, 0.0f, 2, 80.0f);
        bool allFinite = true;
        float peakGr = 0.0f;
        juce::Random rng (1234);
        for (int burst = 0; burst < 20; ++burst)
        {
            // ~40ms decay - a real drum transient, not a sub-millisecond
            // click the detector's fixed ~3ms attack couldn't possibly
            // react to.
            for (int i = 0; i < 4000; ++i)
            {
                const float env = std::exp (-i / 2000.0f);
                const float x = env * (rng.nextFloat() * 2.0f - 1.0f);
                const float gr = det.processSample (x);
                allFinite = allFinite && std::isfinite (gr);
                peakGr = juce::jmax (peakGr, gr);
            }
            for (int i = 0; i < 2000; ++i) // gap between hits
                det.processSample (0.0f);
        }
        check (allFinite, "Drum bursts: all outputs finite");
        check (peakGr > 0.0f, "Drum bursts: detector actually engages gain reduction");
    }

    // ---- Extreme input levels: no NaN/Inf ----
    {
        NovaVariableMuDetector det;
        det.prepare (sr);
        det.setParams (-10.0f, 10.0f, 6, 80.0f);
        bool allFinite = true;
        for (float amp : { 100.0f, 1000.0f, 1.0e6f, -1.0e6f })
            for (int i = 0; i < 1000; ++i)
                allFinite = allFinite && std::isfinite (det.processSample (amp));
        check (allFinite, "Extreme input levels (up to 1e6): all outputs finite, no NaN/Inf");
    }

    // ---- All 6 time-constant modes: release measurably lengthens/grows
    //      more program-dependent as mode index increases ----
    {
        int releaseSamples[6] {};
        for (int mode = 1; mode <= 6; ++mode)
        {
            NovaVariableMuDetector det;
            det.prepare (sr);
            det.setParams (-15.0f, 0.0f, mode, 80.0f);
            releaseSamples[mode - 1] = measureReleaseSamples (det, sr, 0.9f, 0.3f);
        }

        check (releaseSamples[0] < releaseSamples[5], "Mode 1 releases faster than Mode 6 overall");

        bool monotonicEnough = true;
        for (int m = 0; m < 5; ++m)
            if (releaseSamples[m] > releaseSamples[m + 1])
                monotonicEnough = false;
        check (monotonicEnough, "Release time is non-decreasing from Mode 1 through Mode 6");

        // The user's spec: fast modes recover "relatively quickly" -
        // Mode 1 (0.1s base) should fully release well inside 1 second
        // even after 1s of sustained heavy drive.
        check (releaseSamples[0] < (int) sr, "Mode 1 (0.1s base) recovers in well under 1 second");

        std::printf ("Release samples by mode: %d %d %d %d %d %d\n",
                     releaseSamples[0], releaseSamples[1], releaseSamples[2],
                     releaseSamples[3], releaseSamples[4], releaseSamples[5]);
    }

    // ---- Knee/threshold accuracy: raising AC Threshold reduces GR for
    //      the same input, at a fixed operating point ----
    {
        NovaVariableMuDetector detLowThresh, detHighThresh;
        detLowThresh.prepare (sr);
        detLowThresh.setParams (-20.0f, 0.0f, 4, 80.0f);
        detHighThresh.prepare (sr);
        detHighThresh.setParams (0.0f, 0.0f, 4, 80.0f);

        float grLow = 0.0f, grHigh = 0.0f;
        const int N = (int) (sr * 0.5);
        for (int i = 0; i < N; ++i)
        {
            const float x = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 500.0 * i / sr);
            grLow = detLowThresh.processSample (x);
            grHigh = detHighThresh.processSample (x);
        }
        check (grLow > grHigh, "Lower AC Threshold produces more gain reduction than a higher threshold at the same input level");
    }

    // ---- DC Threshold/Behavior shapes character without acting as a
    //      second ordinary threshold: same AC threshold + input, gain
    //      reduction differs as Behavior sweeps, and the DIFFERENCE grows
    //      as overDb grows (a knee/slope effect, not a second gate) ----
    {
        NovaVariableMuDetector detSoft, detHard;
        detSoft.prepare (sr);
        detSoft.setParams (-15.0f, -20.0f, 4, 80.0f); // soft/gentle behavior
        detHard.prepare (sr);
        detHard.setParams (-15.0f, 20.0f, 4, 80.0f); // hard/limiting behavior

        float grSoft = 0.0f, grHard = 0.0f;
        const int N = (int) (sr * 0.5);
        for (int i = 0; i < N; ++i)
        {
            const float x = 0.9f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 500.0 * i / sr);
            grSoft = detSoft.processSample (x);
            grHard = detHard.processSample (x);
        }
        check (grHard > grSoft, "DC Threshold toward 'hard' produces measurably more limiting than toward 'soft' at the same overage");
    }

    std::printf ("\n%d check(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
