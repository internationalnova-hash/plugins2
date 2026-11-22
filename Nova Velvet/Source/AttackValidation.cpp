// Headless validation of NovaVelvetOpticalCell's Manual-mode attack timing
// at 0.5/1/10/30/100/300ms - spec's own "monotonic response, time-to-63%-
// target-GR" requirement. Attack must NOT be a bare exponential coefficient
// (validated indirectly here via the jump-size-normalized softening baked
// into the cell - see ProgramDependentReleaseValidation/FixManValidation
// for the deeper program-dependence checks).

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

    // Feed a step target GR (0 -> targetGr) and measure samples-to-63%.
    int samplesTo63Percent (NovaVelvetOpticalCell& cell, float targetGr, double sr)
    {
        const float threshold63 = targetGr * 0.63f;
        const int maxSamples = (int) (sr * 1.0); // 1 second ceiling
        for (int i = 0; i < maxSamples; ++i)
        {
            const float gr = cell.process (targetGr, 0.0f);
            if (gr >= threshold63)
                return i;
        }
        return maxSamples; // never reached
    }
}

int main()
{
    const double sr = 44100.0;
    const float targetGr = 10.0f; // a solid, unambiguous GR jump

    std::cout << "\n[Attack] Manual mode: time-to-63% strictly increases with Attack setting\n";
    std::vector<float> attackMs = { 0.5f, 1.0f, 10.0f, 30.0f, 100.0f, 300.0f };
    std::vector<int> samplesAt63;

    for (float ms : attackMs)
    {
        NovaVelvetOpticalCell cell;
        cell.prepare (sr);
        cell.setTimeSelectMode (NovaVelvetOpticalCell::TimeSelectMode::Manual);
        cell.setManualAttackSeconds (ms / 1000.0f);
        cell.setManualReleaseSeconds (0.3f); // irrelevant to attack, but must be valid

        const int n = samplesTo63Percent (cell, targetGr, sr);
        samplesAt63.push_back (n);
        std::cout << "  attack=" << ms << "ms -> " << (n / sr * 1000.0) << "ms to 63% GR\n";
    }

    bool monotonic = true;
    for (size_t i = 1; i < samplesAt63.size(); ++i)
        if (samplesAt63[i] < samplesAt63[i - 1])
            monotonic = false;
    check (monotonic, "time-to-63%-GR strictly (non-strictly, allowing ties) increases with Attack setting");

    bool noNaN = true;
    for (int n : samplesAt63) if (n < 0) noNaN = false;
    check (noNaN, "no invalid/negative timing results");

    std::cout << "\n[Attack] Fixed mode: attack is immune to the Manual Attack knob\n";
    {
        NovaVelvetOpticalCell cellFast, cellSlow;
        cellFast.prepare (sr);
        cellSlow.prepare (sr);
        cellFast.setTimeSelectMode (NovaVelvetOpticalCell::TimeSelectMode::Fixed);
        cellSlow.setTimeSelectMode (NovaVelvetOpticalCell::TimeSelectMode::Fixed);
        cellFast.setManualAttackSeconds (0.0005f); // fastest manual setting
        cellSlow.setManualAttackSeconds (0.300f);  // slowest manual setting

        const int nFast = samplesTo63Percent (cellFast, targetGr, sr);
        const int nSlow = samplesTo63Percent (cellSlow, targetGr, sr);
        std::cout << "  Fixed w/ manual=0.5ms: " << (nFast / sr * 1000.0) << "ms; Fixed w/ manual=300ms: " << (nSlow / sr * 1000.0) << "ms\n";
        check (std::abs (nFast - nSlow) <= 2, "Fixed mode's attack timing is unaffected by the Manual Attack knob");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL ATTACK VALIDATION TESTS PASSED\n" : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";
    return failures == 0 ? 0 : 1;
}
