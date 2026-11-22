// Headless validation of NovaPunchCrossover: unity-sum reconstruction
// (low+mid+high must sum back to a flat, uncolored copy of the input -
// the whole reason this was built, see NovaResonanceControl's own
// documented reason for avoiding a non-reconstructing split) and band
// frequency accuracy (each band actually carries the frequency content it
// claims to).

#include "Analysis/NovaPunchCrossover.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <iostream>
#include <vector>

using nova::punch::NovaPunchCrossover;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    float rms (const std::vector<float>& x, int skip)
    {
        double sum = 0.0; int count = 0;
        for (size_t i = (size_t) skip; i < x.size(); ++i) { sum += (double) x[i] * (double) x[i]; ++count; }
        return count > 0 ? (float) std::sqrt (sum / count) : 0.0f;
    }
}

int main()
{
    const double sr = 44100.0;

    std::cout << "\n[Crossover] Unity-sum reconstruction across the frequency range\n";
    {
        for (float freq : { 40.0f, 100.0f, 180.0f, 500.0f, 1000.0f, 2800.0f, 5000.0f, 12000.0f, 18000.0f })
        {
            NovaPunchCrossover crossover;
            crossover.prepare (sr);

            const int n = (int) (sr * 0.2);
            std::vector<float> input (n), reconstructed (n);
            for (int i = 0; i < n; ++i)
            {
                const float in = 0.5f * std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / sr);
                input[(size_t) i] = in;
                const auto bands = crossover.processSample (in);
                reconstructed[(size_t) i] = bands.low + bands.mid + bands.high;
            }

            const int skip = n / 2; // let filters settle
            const float inRms = rms (input, skip);
            const float outRms = rms (reconstructed, skip);
            const float ratio = outRms / juce::jmax (1.0e-9f, inRms);
            check (ratio > 0.85f && ratio < 1.15f,
                   juce::String (freq, 0) + "Hz: reconstructed/input RMS ratio=" + juce::String (ratio, 3) + " (want ~1.0)");
        }
    }

    std::cout << "\n[Crossover] Band frequency accuracy: each band carries the energy it claims to\n";
    {
        struct Case { float freq; const char* expectedDominant; };
        const Case cases[] = {
            { 50.0f, "low" }, { 100.0f, "low" },
            { 800.0f, "mid" }, { 1500.0f, "mid" },
            { 6000.0f, "high" }, { 14000.0f, "high" },
        };
        for (const auto& c : cases)
        {
            NovaPunchCrossover crossover;
            crossover.prepare (sr);

            const int n = (int) (sr * 0.15);
            std::vector<float> lowOut (n), midOut (n), highOut (n);
            for (int i = 0; i < n; ++i)
            {
                const float in = 0.5f * std::sin (2.0 * juce::MathConstants<double>::pi * c.freq * (double) i / sr);
                const auto bands = crossover.processSample (in);
                lowOut[(size_t) i] = bands.low;
                midOut[(size_t) i] = bands.mid;
                highOut[(size_t) i] = bands.high;
            }

            const int skip = n / 2;
            const float lowRms = rms (lowOut, skip), midRms = rms (midOut, skip), highRms = rms (highOut, skip);
            const char* dominant = (lowRms >= midRms && lowRms >= highRms) ? "low"
                                  : (midRms >= lowRms && midRms >= highRms) ? "mid" : "high";
            check (juce::String (dominant) == juce::String (c.expectedDominant),
                   juce::String (c.freq, 0) + "Hz: dominant band=" + dominant + " (expected " + c.expectedDominant
                       + "), low=" + juce::String (lowRms, 4) + " mid=" + juce::String (midRms, 4) + " high=" + juce::String (highRms, 4));
        }
    }

    std::cout << "\n[Crossover] Silence/DC/extreme input stays finite\n";
    {
        NovaPunchCrossover crossover;
        crossover.prepare (sr);
        bool allFinite = true;
        for (int i = 0; i < (int) sr; ++i)
        {
            const auto bands = crossover.processSample (0.0f);
            if (! std::isfinite (bands.low) || ! std::isfinite (bands.mid) || ! std::isfinite (bands.high)) { allFinite = false; break; }
        }
        for (int i = 0; i < (int) sr && allFinite; ++i)
        {
            const auto bands = crossover.processSample (1.0f);
            if (! std::isfinite (bands.low) || ! std::isfinite (bands.mid) || ! std::isfinite (bands.high)) { allFinite = false; break; }
        }
        check (allFinite, "silence and DC input: all bands stay finite throughout");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL CROSSOVER VALIDATION TESTS PASSED\n" : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";
    return failures == 0 ? 0 : 1;
}
