// Frequency response sweep at low/nominal/high operating level, Mix=100%,
// minimal GR (very high threshold) - spec's own "broad clean bandwidth, no
// exaggerated bass/treble tilt merely to sound analog" requirement.

#include "NovaVelvetEngine.h"
#include <algorithm>
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

    // Steady-state gain (dB) at a single test frequency: RMS(out)/RMS(in).
    double gainDbAt (NovaVelvetEngine& engine, double sr, double freq, float amplitude)
    {
        const int settleSamples = (int) (sr * 0.1);
        const int measureSamples = (int) (sr * 0.05);
        const int totalSamples = settleSamples + measureSamples;

        juce::AudioBuffer<float> buf (2, totalSamples);
        for (int i = 0; i < totalSamples; ++i)
        {
            const float s = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr);
            buf.setSample (0, i, s);
            buf.setSample (1, i, s);
        }

        juce::AudioBuffer<float> inputCopy (buf);

        for (int pos = 0; pos < totalSamples; pos += 512)
        {
            const int n = juce::jmin (512, totalSamples - pos);
            juce::AudioBuffer<float> block (buf.getArrayOfWritePointers(), 2, pos, n);
            engine.processBlock (block);
        }

        double inRms = 0.0, outRms = 0.0;
        for (int i = settleSamples; i < totalSamples; ++i)
        {
            inRms += (double) inputCopy.getSample (0, i) * inputCopy.getSample (0, i);
            outRms += (double) buf.getSample (0, i) * buf.getSample (0, i);
        }
        inRms = std::sqrt (inRms / measureSamples);
        outRms = std::sqrt (outRms / measureSamples);

        return 20.0 * std::log10 (juce::jmax (1.0e-9, outRms) / juce::jmax (1.0e-9, inRms));
    }
}

int main()
{
    const double sr = 44100.0;
    juce::dsp::ProcessSpec spec { sr, 512, 2 };
    const std::vector<double> testFreqs = { 40.0, 100.0, 250.0, 500.0, 1000.0, 2500.0, 5000.0, 10000.0, 15000.0 };

    // All three amplitudes stay under the "OFF" threshold's actual -8dBFS
    // (thresholdDbu=10 + kReferenceDbfs=-18) so this test isolates the
    // transformer/tube stages' own frequency response from the
    // compressor's gain reduction (a separate, legitimate source of
    // level-dependent gain that this test isn't meant to measure - the
    // compressor engaging IS the point of the compressor, not a frequency-
    // response flaw).
    for (float amplitude : { 0.05f, 0.15f, 0.3f })
    {
        std::cout << "\n[FrequencyResponse] amplitude=" << amplitude << "\n";

        NovaVelvetEngine engine;
        engine.prepare (spec, 1);
        // Very high threshold (effectively "OFF") so minimal GR is in play -
        // this test is about the transformer/tube stages' inherent
        // frequency response, not the compressor's.
        engine.setParams (10.0f, 2.0f, 10.0f, 300.0f, 0.0f, 1, 20.0f, 100.0f, 0.0f, true, 1, 0.0f, false);

        std::vector<double> gains;
        for (double freq : testFreqs)
        {
            const double gainDb = gainDbAt (engine, sr, freq, amplitude);
            gains.push_back (gainDb);
            std::cout << "  " << freq << "Hz: " << gainDb << " dB\n";
        }

        const double maxGain = *std::max_element (gains.begin(), gains.end());
        const double minGain = *std::min_element (gains.begin(), gains.end());
        const double spread = maxGain - minGain;
        std::cout << "  spread: " << spread << " dB\n";

        check (spread < 3.0, "amplitude=" + juce::String (amplitude) + ": response stays within a 3dB spread across 40Hz-15kHz (no manufactured analog tilt)");

        bool allFinite = true;
        for (double g : gains) if (! std::isfinite (g)) allFinite = false;
        check (allFinite, "amplitude=" + juce::String (amplitude) + ": all gain measurements finite");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL FREQUENCY RESPONSE VALIDATION TESTS PASSED\n" : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";
    return failures == 0 ? 0 : 1;
}
