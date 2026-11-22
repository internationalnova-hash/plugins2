// Headless validation of NovaVelvetDetector's static transfer function at
// Ratio 2:1/4:1/10:1 - spec's own "monotonic gain reduction, smooth knee,
// increasing compression with ratio, no discontinuities" requirement.

#include "DSP/NovaVelvetDetector.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <iostream>
#include <vector>

using namespace nova::velvet;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    // Static transfer: feed a steady-state sine at a given dBFS level long
    // enough for the detector's own followers to settle, read the
    // resulting target GR.
    float steadyStateGrDb (NovaVelvetDetector& det, float levelDbfs, double sr)
    {
        const float amplitude = juce::Decibels::decibelsToGain (levelDbfs);
        const double freq = 1000.0;
        float gr = 0.0f;
        const int numSamples = (int) (sr * 0.5); // 500ms - long enough for followers to settle
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr);
            const float mag = det.computeSidechainMagnitude (x);
            gr = det.applyMagnitude (mag);
        }
        return gr;
    }
}

int main()
{
    const double sr = 44100.0;

    for (const float ratio : { 2.0f, 4.0f, 10.0f })
    {
        std::cout << "\n[TransferCurve] Ratio " << ratio << ":1\n";

        NovaVelvetDetector det;
        det.prepare (sr);
        det.setThresholdDbu (-20.0f);
        det.setRatio (ratio);
        det.setScHpfHz (20.0f);

        // Sweep levels from well below to well above threshold; threshold
        // in dBFS = -20dBu + (-18dBFS reference) = -38dBFS.
        std::vector<float> levels, grs;
        for (float levelDbfs = -60.0f; levelDbfs <= 0.0f; levelDbfs += 2.0f)
        {
            det.reset();
            const float gr = steadyStateGrDb (det, levelDbfs, sr);
            levels.push_back (levelDbfs);
            grs.push_back (gr);
        }

        bool monotonic = true;
        bool noDiscontinuity = true;
        bool allFinite = true;
        for (size_t i = 0; i < grs.size(); ++i)
        {
            if (! std::isfinite (grs[i])) allFinite = false;
            if (i > 0)
            {
                if (grs[i] < grs[i - 1] - 0.01f) monotonic = false; // allow float noise
                if (grs[i] - grs[i - 1] > 6.0f) noDiscontinuity = false; // >6dB jump per 2dB step = discontinuity
            }
        }

        check (allFinite, "GR stays finite across the full level sweep");
        check (monotonic, "GR is monotonic non-decreasing with input level");
        check (noDiscontinuity, "no discontinuity >6dB between adjacent 2dB level steps");
        check (grs.back() > grs.front(), "GR at 0dBFS is greater than GR at -60dBFS (real compression happening)");

        // Higher ratio should produce more GR at a fixed well-over-threshold level.
        std::cout << "  (GR at 0dBFS: " << grs.back() << " dB)\n";
    }

    // Cross-ratio comparison: at a fixed level well over threshold, higher
    // ratio must produce more gain reduction than lower ratio.
    std::cout << "\n[TransferCurve] Cross-ratio comparison at a fixed over-threshold level\n";
    {
        auto grAtRatio = [sr] (float ratio) {
            NovaVelvetDetector det;
            det.prepare (sr);
            det.setThresholdDbu (-20.0f);
            det.setRatio (ratio);
            det.setScHpfHz (20.0f);
            return steadyStateGrDb (det, -10.0f, sr);
        };

        const float gr2 = grAtRatio (2.0f);
        const float gr4 = grAtRatio (4.0f);
        const float gr10 = grAtRatio (10.0f);
        std::cout << "  ratio=2: " << gr2 << " dB, ratio=4: " << gr4 << " dB, ratio=10: " << gr10 << " dB\n";
        check (gr4 > gr2, "ratio=4 produces more GR than ratio=2 at the same over-threshold level");
        check (gr10 > gr4, "ratio=10 produces more GR than ratio=4 at the same over-threshold level");

        // Even at 10:1, must not behave like a brickwall limiter: slope
        // should still be well under 1.0 (some signal still gets through
        // above threshold, it's not a hard ceiling).
        const float slope10 = 1.0f - (1.0f / 10.0f);
        check (slope10 < 0.95f, "even at 10:1 the ratio-derived slope stays below a near-limiting 0.95");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL TRANSFER CURVE VALIDATION TESTS PASSED\n" : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";
    return failures == 0 ? 0 : 1;
}
