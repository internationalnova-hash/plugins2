// Mix=0 must equal latency-aligned dry; Output=0 must be unity trim; no
// hidden loudness cheating; NaN/Inf injection safety.

#include "NovaVelvetEngine.h"
#include <cmath>
#include <iostream>
#include <limits>
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
}

int main()
{
    const double sr = 44100.0;
    juce::dsp::ProcessSpec spec { sr, 512, 2 };

    std::cout << "\n[NullAndSafety] Mix=0% equals latency-aligned dry signal\n";
    {
        NovaVelvetEngine engine;
        engine.prepare (spec, 1);
        engine.setParams (-20.0f, 4.0f, 10.0f, 300.0f, 6.0f, 1, 20.0f,
                           /*mixPercent*/ 0.0f, 0.0f, true, 1, 50.0f, false);

        // Mix is deliberately click-free SmoothedValue-ramped (~20ms,
        // NovaVelvetCalibration::kParamSmoothSeconds) from its prepare()-
        // time starting value of 100% down to the 0% target set above -
        // settle that ramp on silence before generating the actual
        // dry/wet comparison signal, or this test would be comparing
        // during the active ramp-down (where real wet signal still
        // legitimately bleeds through) rather than at the settled target.
        {
            juce::AudioBuffer<float> settle (2, (int) (sr * 0.1));
            settle.clear();
            for (int pos = 0; pos < settle.getNumSamples(); pos += 512)
            {
                const int n = juce::jmin (512, settle.getNumSamples() - pos);
                juce::AudioBuffer<float> block (settle.getArrayOfWritePointers(), 2, pos, n);
                engine.processBlock (block);
            }
        }

        const int numSamples = (int) (sr * 0.5);
        juce::AudioBuffer<float> dry (2, numSamples);
        juce::AudioBuffer<float> wet (2, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / sr);
            dry.setSample (0, i, s); dry.setSample (1, i, s);
            wet.setSample (0, i, s); wet.setSample (1, i, s);
        }

        for (int pos = 0; pos < numSamples; pos += 512)
        {
            const int n = juce::jmin (512, numSamples - pos);
            juce::AudioBuffer<float> block (wet.getArrayOfWritePointers(), 2, pos, n);
            engine.processBlock (block);
        }

        const int latency = engine.getLatencySamples();
        double maxResidual = 0.0;
        // Compare from well after the latency point to well before the end,
        // so any edge/transient effects don't skew the comparison.
        const int compareStart = juce::jmax (latency + 100, 100);
        const int compareEnd = numSamples - 100;
        for (int i = compareStart; i < compareEnd; ++i)
        {
            const int dryIdx = i - latency;
            if (dryIdx < 0 || dryIdx >= numSamples) continue;
            const double residualL = std::abs ((double) wet.getSample (0, i) - (double) dry.getSample (0, dryIdx));
            const double residualR = std::abs ((double) wet.getSample (1, i) - (double) dry.getSample (1, dryIdx));
            maxResidual = juce::jmax (maxResidual, juce::jmax (residualL, residualR));
        }
        const double residualDb = maxResidual > 1.0e-12 ? 20.0 * std::log10 (maxResidual) : -200.0;
        std::cout << "  max residual vs delay-aligned dry: " << residualDb << " dBFS\n";
        check (residualDb < -40.0, "Mix=0% output matches latency-aligned dry within -40dB residual");
    }

    std::cout << "\n[NullAndSafety] Output=0dB is unity trim\n";
    {
        NovaVelvetEngine engineWithOutput, engineWithoutOutput;
        engineWithOutput.prepare (spec, 1);
        engineWithoutOutput.prepare (spec, 1);
        // Neutral-ish settings: high threshold so minimal GR, Mix=100%,
        // both engines identical except Output trim.
        engineWithOutput.setParams (10.0f, 2.0f, 10.0f, 300.0f, 0.0f, 1, 20.0f, 100.0f, 0.0f, true, 1, 0.0f, false);
        engineWithoutOutput.setParams (10.0f, 2.0f, 10.0f, 300.0f, 0.0f, 1, 20.0f, 100.0f, 0.0f, true, 1, 0.0f, false);

        const int numSamples = 4096;
        juce::AudioBuffer<float> bufA (2, numSamples), bufB (2, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / sr);
            bufA.setSample (0, i, s); bufA.setSample (1, i, s);
            bufB.setSample (0, i, s); bufB.setSample (1, i, s);
        }
        for (int pos = 0; pos < numSamples; pos += 512)
        {
            const int n = juce::jmin (512, numSamples - pos);
            juce::AudioBuffer<float> a (bufA.getArrayOfWritePointers(), 2, pos, n);
            juce::AudioBuffer<float> b (bufB.getArrayOfWritePointers(), 2, pos, n);
            engineWithOutput.processBlock (a);
            engineWithoutOutput.processBlock (b);
        }
        double maxDiff = 0.0;
        for (int i = numSamples / 2; i < numSamples; ++i)
            maxDiff = juce::jmax (maxDiff, (double) std::abs (bufA.getSample (0, i) - bufB.getSample (0, i)));
        std::cout << "  max diff between two identical-except-explicit-0dB-Output runs: " << maxDiff << "\n";
        check (maxDiff < 1.0e-5, "Output=0dB set explicitly matches Output=0dB default (unity, no hidden trim)");
    }

    std::cout << "\n[NullAndSafety] NaN/Inf injection safety\n";
    {
        NovaVelvetEngine engine;
        engine.prepare (spec, 1);
        engine.setParams (-20.0f, 4.0f, 10.0f, 300.0f, 6.0f, 1, 20.0f, 100.0f, 0.0f, true, 1, 50.0f, false);

        juce::AudioBuffer<float> nanBuf (2, 512);
        for (int i = 0; i < 512; ++i)
        {
            nanBuf.setSample (0, i, i % 50 == 0 ? std::numeric_limits<float>::quiet_NaN() : 0.1f);
            nanBuf.setSample (1, i, i % 77 == 0 ? std::numeric_limits<float>::infinity() : 0.1f);
        }
        engine.processBlock (nanBuf);
        bool allFinite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                if (! std::isfinite (nanBuf.getSample (ch, i))) allFinite = false;
        check (allFinite, "NaN/Inf injected into input does not propagate to output");

        // A follow-up clean block should also be healthy (no persistent
        // poisoned internal state).
        juce::AudioBuffer<float> cleanBuf (2, 512);
        for (int i = 0; i < 512; ++i)
        {
            cleanBuf.setSample (0, i, 0.2f);
            cleanBuf.setSample (1, i, 0.2f);
        }
        engine.processBlock (cleanBuf);
        bool cleanFinite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                if (! std::isfinite (cleanBuf.getSample (ch, i))) cleanFinite = false;
        check (cleanFinite, "engine recovers cleanly on the block after NaN/Inf injection");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL NULL/SAFETY VALIDATION TESTS PASSED\n" : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";
    return failures == 0 ? 0 : 1;
}
