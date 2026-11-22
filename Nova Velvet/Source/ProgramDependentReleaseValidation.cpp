// MANDATORY acceptance test (per the spec's own words): two events with
// matched peak GR - a short 20-50ms transient (A) vs a sustained 1-3s event
// (B) - must show measurably DIFFERENT recovery trajectories. If both
// recover identically, "the optical model is not complete."

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

    // Runs an event of `eventSeconds` at `peakGr` (constant target GR
    // during the event), then releases (target -> 0) for `releaseSeconds`,
    // returning the post-event GR trajectory (index 0 = first sample after
    // the event ends).
    std::vector<float> runEventAndRelease (NovaVelvetOpticalCell& cell, float peakGr, double eventSeconds,
                                            double releaseSeconds, double sr, float detectorEnergy01)
    {
        const int eventSamples = (int) (sr * eventSeconds);
        for (int i = 0; i < eventSamples; ++i)
            cell.process (peakGr, detectorEnergy01);

        std::vector<float> trajectory;
        const int releaseSamples = (int) (sr * releaseSeconds);
        trajectory.reserve ((size_t) releaseSamples);
        for (int i = 0; i < releaseSamples; ++i)
            trajectory.push_back (cell.process (0.0f, 0.0f));
        return trajectory;
    }

    // Time (in samples) until the trajectory first drops to <= 50% of its
    // own starting (peak) value.
    int samplesTo50PercentRecovered (const std::vector<float>& trajectory)
    {
        if (trajectory.empty()) return 0;
        const float startGr = trajectory.front();
        const float halfGr = startGr * 0.5f;
        for (size_t i = 0; i < trajectory.size(); ++i)
            if (trajectory[i] <= halfGr)
                return (int) i;
        return (int) trajectory.size(); // never recovered within window
    }
}

int main()
{
    const double sr = 44100.0;
    const float matchedPeakGr = 10.0f; // both events target the same peak GR

    for (const auto mode : { NovaVelvetOpticalCell::TimeSelectMode::Manual, NovaVelvetOpticalCell::TimeSelectMode::FixMan })
    {
        const juce::String modeName = mode == NovaVelvetOpticalCell::TimeSelectMode::Manual ? "Manual" : "FixMan";
        std::cout << "\n[ProgramDependentRelease] Mode = " << modeName << "\n";

        NovaVelvetOpticalCell cellShort, cellSustained;
        cellShort.prepare (sr);
        cellSustained.prepare (sr);
        cellShort.setTimeSelectMode (mode);
        cellSustained.setTimeSelectMode (mode);
        cellShort.setManualAttackSeconds (0.001f);
        cellShort.setManualReleaseSeconds (0.3f);
        cellSustained.setManualAttackSeconds (0.001f);
        cellSustained.setManualReleaseSeconds (0.3f);

        // A: short 30ms transient. B: sustained 2s event. Detector energy
        // fed as a plausible correlate (higher for the sustained event,
        // since real sustained material would read higher recent energy).
        auto trajA = runEventAndRelease (cellShort, matchedPeakGr, 0.030, 2.0, sr, 0.3f);
        auto trajB = runEventAndRelease (cellSustained, matchedPeakGr, 2.0, 2.0, sr, 0.8f);

        const int samplesA = samplesTo50PercentRecovered (trajA);
        const int samplesB = samplesTo50PercentRecovered (trajB);
        const double msA = samplesA / sr * 1000.0;
        const double msB = samplesB / sr * 1000.0;

        std::cout << "  short-event (A) time-to-50%-recovered:     " << msA << " ms\n";
        std::cout << "  sustained-event (B) time-to-50%-recovered: " << msB << " ms\n";

        check (trajA.front() > 0.0f && trajB.front() > 0.0f, modeName + ": both events actually established nonzero GR before release");

        // Peak GR should be approximately matched (both events ran long
        // enough - or in A's case, close enough - to reach a comparable
        // starting point). Not a strict equality since A is intentionally
        // shorter and its attack may not have fully settled - that's part
        // of the point.
        const bool peakRoughlyMatched = std::abs (trajA.front() - trajB.front()) < matchedPeakGr * 0.5f;
        check (peakRoughlyMatched, modeName + ": peak GR at release onset is roughly matched between A and B");

        // THE decisive assertion: sustained event B must recover measurably
        // SLOWER than short event A - if these are within a few percent of
        // each other, the optical model's program dependency is not doing
        // its job, per the spec's own explicit acceptance bar.
        const bool measurablyDifferent = msB > msA * 1.3; // B at least 30% slower to half-recover
        check (measurablyDifferent,
            modeName + ": sustained event recovers measurably slower than short event (B=" + juce::String (msB, 1)
            + "ms vs A=" + juce::String (msA, 1) + "ms) - if this fails, the optical model is not complete");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL PROGRAM-DEPENDENT RELEASE VALIDATION TESTS PASSED\n" : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";
    return failures == 0 ? 0 : 1;
}
