// Headless validation of NovaVelvetOpticalCell's Manual-mode release timing
// at 50ms/100ms/500ms/1s/3s/10s from established 3/6/10dB GR - spec's own
// "no discontinuities, no stuck GR" requirement.

#include "DSP/NovaVelvetOpticalCell.h"
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

    // Establish a steady-state GR, then release (target -> 0) and track
    // the GR trajectory sample-by-sample for `seconds`.
    std::vector<float> releaseTrajectory (NovaVelvetOpticalCell& cell, float establishGr, double sr, double seconds)
    {
        // Establish: run long enough for the fast attack to fully settle.
        for (int i = 0; i < (int) (sr * 0.5); ++i)
            cell.process (establishGr, 0.0f);

        std::vector<float> trajectory;
        const int n = (int) (sr * seconds);
        trajectory.reserve ((size_t) n);
        for (int i = 0; i < n; ++i)
            trajectory.push_back (cell.process (0.0f, 0.0f));
        return trajectory;
    }
}

int main()
{
    const double sr = 44100.0;

    for (const float establishGr : { 3.0f, 6.0f, 10.0f })
    {
        std::cout << "\n[Release] Manual mode, established GR = " << establishGr << " dB\n";

        for (const float releaseMs : { 50.0f, 100.0f, 500.0f, 1000.0f, 3000.0f, 10000.0f })
        {
            NovaVelvetOpticalCell cell;
            cell.prepare (sr);
            cell.setTimeSelectMode (NovaVelvetOpticalCell::TimeSelectMode::Manual);
            cell.setManualAttackSeconds (0.001f); // fast, so establish phase is clean
            cell.setManualReleaseSeconds (releaseMs / 1000.0f);

            // Cap observation window: release range goes to 10s, so give
            // slow settings enough real time to actually recover (allowing
            // for this cell's own depth/charge stretch factors on top of
            // the raw manual release time).
            const double observeSeconds = juce::jmax (2.0, (double) releaseMs / 1000.0 * 6.0);
            auto trajectory = releaseTrajectory (cell, establishGr, sr, observeSeconds);

            bool allFinite = true, monotonicDecrease = true, noDiscontinuity = true;
            for (size_t i = 0; i < trajectory.size(); ++i)
            {
                if (! std::isfinite (trajectory[i])) allFinite = false;
                if (i > 0)
                {
                    if (trajectory[i] > trajectory[i - 1] + 0.001f) monotonicDecrease = false;
                    if (trajectory[i - 1] - trajectory[i] > establishGr * 0.5f) noDiscontinuity = false; // no huge single-sample jump
                }
            }

            const float finalGr = trajectory.back();
            const bool recovered = finalGr < 0.1f;

            std::cout << "  release=" << releaseMs << "ms: finalGR=" << finalGr << "dB after " << observeSeconds << "s"
                       << (allFinite ? "" : " [NOT FINITE]") << (monotonicDecrease ? "" : " [NOT MONOTONIC]") << "\n";

            check (allFinite, juce::String ("release=") + juce::String (releaseMs) + "ms, GR=" + juce::String (establishGr) + "dB: trajectory stays finite");
            check (monotonicDecrease, juce::String ("release=") + juce::String (releaseMs) + "ms, GR=" + juce::String (establishGr) + "dB: GR decreases monotonically once release begins");
            check (noDiscontinuity, juce::String ("release=") + juce::String (releaseMs) + "ms, GR=" + juce::String (establishGr) + "dB: no single-sample discontinuity");
            check (recovered, juce::String ("release=") + juce::String (releaseMs) + "ms, GR=" + juce::String (establishGr) + "dB: GR recovers to near-zero, not stuck");
        }
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL RELEASE VALIDATION TESTS PASSED\n" : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";
    return failures == 0 ? 0 : 1;
}
