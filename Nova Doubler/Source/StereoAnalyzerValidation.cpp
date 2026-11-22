// Headless validation of NovaStereoFieldAnalyzer: correlation and spread
// hit their expected extremes against synthetic mono, decorrelated-noise,
// and phase-inverted signals; RMS metrics report sane values; sample-rate
// independence of the smoothing behavior.

#include "Analysis/NovaStereoFieldAnalyzer.h"
#include <juce_core/juce_core.h>
#include <iostream>
#include <cmath>
#include <random>

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }
}

int main()
{
    const double sr = 48000.0;
    const int n = (int) (sr * 2.0); // long enough for the tau=0.15s smoothing to settle well

    std::cout << "\n[StereoAnalyzer] Mono signal (L==R): correlation ~+1, spread ~0%, side RMS ~0\n";
    {
        NovaStereoFieldAnalyzer an;
        an.prepare (sr);
        for (int i = 0; i < n; ++i)
        {
            const float s = std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / sr);
            an.processSample (s, s);
        }
        check (an.getCorrelation() > 0.98f, "correlation near +1 for identical L/R, got " + juce::String (an.getCorrelation(), 4));
        check (an.getSpread() < 2.0f, "spread near 0% for identical L/R, got " + juce::String (an.getSpread(), 2));
        check (an.getSideRms() < 0.01f, "side RMS near 0 for identical L/R, got " + juce::String (an.getSideRms(), 5));
        check (std::abs (an.getMidRms() - an.getLeftRms()) < 0.02f, "mid RMS matches L/R RMS for identical L/R");
    }

    std::cout << "\n[StereoAnalyzer] Fully out-of-phase signal (R=-L): correlation ~-1, spread ~100%, mid RMS ~0\n";
    {
        NovaStereoFieldAnalyzer an;
        an.prepare (sr);
        for (int i = 0; i < n; ++i)
        {
            const float s = std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / sr);
            an.processSample (s, -s);
        }
        check (an.getCorrelation() < -0.98f, "correlation near -1 for phase-inverted L/R, got " + juce::String (an.getCorrelation(), 4));
        check (an.getSpread() > 98.0f, "spread near 100% for phase-inverted L/R, got " + juce::String (an.getSpread(), 2));
        check (an.getMidRms() < 0.01f, "mid RMS near 0 for phase-inverted L/R, got " + juce::String (an.getMidRms(), 5));
        check (std::abs (an.getSideRms() - an.getLeftRms()) < 0.02f, "side RMS matches L/R RMS for phase-inverted L/R");
    }

    std::cout << "\n[StereoAnalyzer] Decorrelated noise: correlation ~0, spread ~50%\n";
    {
        NovaStereoFieldAnalyzer an;
        an.prepare (sr);
        std::mt19937 rngL (1), rngR (2);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
        for (int i = 0; i < n; ++i)
            an.processSample (dist (rngL), dist (rngR));

        check (std::abs (an.getCorrelation()) < 0.15f, "correlation near 0 for independent noise, got " + juce::String (an.getCorrelation(), 4));
        check (std::abs (an.getSpread() - 50.0f) < 7.5f, "spread near 50% for independent noise, got " + juce::String (an.getSpread(), 2));
    }

    std::cout << "\n[StereoAnalyzer] Silence reads as coherent (no divide-by-zero/NaN)\n";
    {
        NovaStereoFieldAnalyzer an;
        an.prepare (sr);
        for (int i = 0; i < 1000; ++i)
            an.processSample (0.0f, 0.0f);
        check (std::isfinite (an.getCorrelation()) && std::isfinite (an.getSpread()),
               "correlation/spread stay finite on pure silence, corr=" + juce::String (an.getCorrelation(), 4) + " spread=" + juce::String (an.getSpread(), 2));
    }

    std::cout << "\n[StereoAnalyzer] RMS accuracy: known-amplitude signal reports the right level\n";
    {
        NovaStereoFieldAnalyzer an;
        an.prepare (sr);
        const float amp = 0.7f;
        for (int i = 0; i < n; ++i)
        {
            const float s = amp * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / sr);
            an.processSample (s, s);
        }
        const float expectedRms = amp / std::sqrt (2.0f);
        check (std::abs (an.getLeftRms() - expectedRms) < 0.02f,
               "measured L RMS (" + juce::String (an.getLeftRms(), 4) + ") matches expected sine RMS (" + juce::String (expectedRms, 4) + ")");
    }

    std::cout << "\n[StereoAnalyzer] Sample-rate independence: same signal shape reads the same correlation at all rates\n";
    {
        const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0 };
        for (double rate : rates)
        {
            NovaStereoFieldAnalyzer an;
            an.prepare (rate);
            const int rn = (int) (rate * 2.0);
            for (int i = 0; i < rn; ++i)
            {
                const float s = std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / rate);
                an.processSample (s, -s);
            }
            check (an.getCorrelation() < -0.98f,
                   "sr=" + juce::String (rate, 0) + "Hz: correlation near -1 for phase-inverted signal, got " + juce::String (an.getCorrelation(), 4));
        }
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL STEREO ANALYZER VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
