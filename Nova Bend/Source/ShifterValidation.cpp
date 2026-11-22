// Headless validation of NovaBendShifter (the delay-line dual-tap design,
// adapted from Nova Voice's proven-clean pitch stage but with an
// ADAPTIVE, period-hint-sized crossfade window - see the class's own
// header comment for why a truly fixed window was measured to produce
// large frequency errors on real material): ratio accuracy across the
// spec range and realistic vocal frequencies, the LIVE/STUDIO latency
// difference, and - the properties this rewrite specifically exists to
// guarantee - that accuracy survives material with no tonal content, and
// that a WRONG or JUMPING period hint degrades gracefully (never
// instability/clicking), since this design's crossfade math never
// depends on the hint being right.

#include "DSP/NovaBendShifter.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

using nova::bend::NovaBendShifter;

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

    // hintHz: the period hint passed to the shifter each sample - defaults
    // to the true input frequency (the common case: the engine's detector
    // is tracking correctly), but tests below override it to probe
    // hint-error robustness.
    float runShiftTest (double sr, bool highQuality, float inFreq, float shiftSemitones, float& outMeasuredHz, float hintHz = -1.0f)
    {
        NovaBendShifter shifter;
        shifter.prepare (sr, highQuality);

        const float ratio = std::pow (2.0f, shiftSemitones / 12.0f);
        const float outFreq = inFreq * ratio;
        const float hint = hintHz > 0.0f ? hintHz : inFreq;
        const int n = (int) (sr * 1.5);
        std::vector<float> output ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const float in = std::sin (2.0 * juce::MathConstants<double>::pi * inFreq * (double) i / sr);
            output[(size_t) i] = shifter.processSample (in, ratio, hint);
        }

        const int skip = shifter.getLatencySamples() + 4000;
        std::vector<float> tail (output.begin() + skip, output.end());
        outMeasuredHz = measureFrequencyByZeroCrossing (tail, sr);
        return 1200.0f * std::log2 (outMeasuredHz / outFreq);
    }
}

int main()
{
    const double sr = 44100.0;

    std::cout << "\n[Shifter] Unity-ratio steady state\n";
    {
        float measured = 0.0f;
        const float cents = runShiftTest (sr, true, 220.0f, 0.0f, measured);
        check (std::abs (cents) < 8.0f, "unity ratio: measured=" + juce::String (measured, 2) + "Hz (" + juce::String (cents, 2) + "c)");
    }

    std::cout << "\n[Shifter] Spec-mandated 220Hz reference: +12 semitones ~= 440Hz, -12 ~= 110Hz\n";
    {
        float measured = 0.0f;
        float cents = runShiftTest (sr, true, 220.0f, 12.0f, measured);
        check (std::abs (measured - 440.0f) < 6.0f && std::abs (cents) < 15.0f,
               "+12st: expected=440.00Hz, measured=" + juce::String (measured, 2) + "Hz (" + juce::String (cents, 2) + "c)");

        cents = runShiftTest (sr, true, 220.0f, -12.0f, measured);
        check (std::abs (measured - 110.0f) < 3.0f && std::abs (cents) < 15.0f,
               "-12st: expected=110.00Hz, measured=" + juce::String (measured, 2) + "Hz (" + juce::String (cents, 2) + "c)");
    }

    std::cout << "\n[Shifter] Known-shift cents accuracy across the full range, at realistic vocal frequencies\n";
    {
        for (float inFreq : { 110.0f, 220.0f, 440.0f })
        {
            for (float shiftSemitones : { -7.0f, -5.0f, -1.0f, 1.0f, 5.0f, 7.0f })
            {
                float measured = 0.0f;
                const float cents = runShiftTest (sr, true, inFreq, shiftSemitones, measured);
                check (std::abs (cents) < 15.0f,
                       juce::String (inFreq, 0) + "Hz, " + juce::String (shiftSemitones, 0) + "st: measured=" + juce::String (measured, 2)
                           + "Hz (" + juce::String (cents, 2) + "c)");
            }
        }
    }

    std::cout << "\n[Shifter] Ratio accuracy is independent of input CONTENT (no alignment search to corrupt) - same ratio, wildly different material\n";
    {
        std::mt19937 rng (42);
        std::normal_distribution<float> noiseDist (0.0f, 1.0f);
        const float shiftSemitones = 7.0f;
        const float ratio = std::pow (2.0f, shiftSemitones / 12.0f);
        const float inFreq = 220.0f;
        const float outFreq = inFreq * ratio;
        const int n = (int) (sr * 1.5);

        // A pure tone PRECEDED by a full second of broadband noise, then
        // silence, then the tone - proving the shifter's ratio accuracy on
        // the CLEAN portion is unaffected by whatever came before it.
        NovaBendShifter shifter;
        shifter.prepare (sr, true);
        std::vector<float> output ((size_t) (n * 3));
        int idx = 0;
        for (int i = 0; i < n; ++i) output[(size_t) idx++] = shifter.processSample (noiseDist (rng) * 0.5f, ratio, 220.0f);
        for (int i = 0; i < n; ++i) output[(size_t) idx++] = shifter.processSample (0.0f, ratio, 220.0f);
        for (int i = 0; i < n; ++i)
        {
            const float in = std::sin (2.0 * juce::MathConstants<double>::pi * inFreq * (double) i / sr);
            output[(size_t) idx++] = shifter.processSample (in, ratio, inFreq);
        }

        check (allFinite (output), "noise -> silence -> tone: output stays finite throughout");

        const int toneStart = n * 2;
        const int skip = toneStart + shifter.getLatencySamples() + 4000;
        std::vector<float> tail (output.begin() + skip, output.end());
        const float measured = measureFrequencyByZeroCrossing (tail, sr);
        const float cents = 1200.0f * std::log2 (measured / outFreq);
        check (std::abs (cents) < 15.0f, "+7st on the clean tone AFTER noise+silence: measured=" + juce::String (measured, 2)
                   + "Hz (" + juce::String (cents, 2) + "c) - unaffected by what preceded it");
    }

    std::cout << "\n[Shifter] Graceful degradation: a WRONG period hint reduces accuracy smoothly, never causes instability (spec: no click risk from a bad detector estimate)\n";
    {
        const float inFreq = 220.0f, shiftSemitones = 7.0f;
        for (float hintErrorPercent : { -25.0f, 25.0f, -50.0f, 50.0f })
        {
            const float wrongHint = inFreq * (1.0f + hintErrorPercent / 100.0f);
            float measured = 0.0f;
            const float cents = runShiftTest (sr, true, inFreq, shiftSemitones, measured, wrongHint);
            check (std::isfinite (measured) && std::abs (cents) < 200.0f,
                   "hint error " + juce::String (hintErrorPercent, 0) + "%: still finite and within 200c (measured="
                       + juce::String (measured, 2) + "Hz, " + juce::String (cents, 2) + "c)");
        }
    }

    std::cout << "\n[Shifter] A period hint that JUMPS abruptly mid-stream (simulating a detector octave-flip) causes no instability\n";
    {
        NovaBendShifter shifter;
        shifter.prepare (sr, true);
        const float ratio = std::pow (2.0f, 7.0f / 12.0f);
        const int n = (int) (sr * 2.0);
        std::vector<float> output ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const float in = std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / sr);
            const float hint = i < n / 2 ? 110.0f : 440.0f; // abrupt, wrong, octave-flip-style hint change
            output[(size_t) i] = shifter.processSample (in, ratio, hint);
        }
        check (allFinite (output), "output stays finite through an abrupt period-hint jump");
        float peak = 0.0f;
        for (float x : output) peak = juce::jmax (peak, std::abs (x));
        check (peak < 4.0f, "output stays within a sane bound (peak=" + juce::String (peak, 3) + ") through the hint jump");
    }

    std::cout << "\n[Shifter] Extreme/degenerate input stays finite (silence, DC, full-scale noise)\n";
    {
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> loud (-1.0f, 1.0f);
        for (float shiftSemitones : { -12.0f, 7.0f, 12.0f })
        {
            NovaBendShifter shifter;
            shifter.prepare (sr, true);
            const float ratio = std::pow (2.0f, shiftSemitones / 12.0f);
            std::vector<float> out;
            for (int i = 0; i < (int) sr; ++i) out.push_back (shifter.processSample (0.0f, ratio, 220.0f)); // silence
            for (int i = 0; i < (int) sr; ++i) out.push_back (shifter.processSample (1.0f, ratio, 220.0f));  // DC
            for (int i = 0; i < (int) sr; ++i) out.push_back (shifter.processSample (loud (rng), ratio, 220.0f)); // full-scale noise
            check (allFinite (out), juce::String (shiftSemitones, 0) + "st: silence/DC/full-scale-noise all stay finite");
        }
    }

    std::cout << "\n[Shifter] LIVE vs STUDIO: Quality is a real DSP config difference, not cosmetic (spec 10/23)\n";
    {
        NovaBendShifter live, studio;
        live.prepare (sr, false);
        studio.prepare (sr, true);
        check (live.getLatencySamples() < studio.getLatencySamples(),
               "LIVE latency (" + juce::String (live.getLatencySamples()) + ") < STUDIO latency ("
                   + juce::String (studio.getLatencySamples()) + ")");

        for (bool hq : { false, true })
        {
            float measured = 0.0f;
            const float cents = runShiftTest (sr, hq, 220.0f, 7.0f, measured);
            check (std::abs (cents) < 15.0f, juce::String (hq ? "STUDIO" : "LIVE") + " +7st still accurate ("
                       + juce::String (cents, 2) + "c)");
        }
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL SHIFTER VALIDATION TESTS PASSED\n" : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";
    return failures == 0 ? 0 : 1;
}
