// Headless validation harness for the NovaMasteringChain DSP stages.
// Checks finite output, gain-staging sanity, and - critically - that each
// stage is a true passthrough when inactive (no coloration/clicks just
// from being wired into the chain). Follows the same pattern as
// AnalysisValidation.cpp / NovaMotionRegressionTest.h.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/NovaMasteringChain.h"
#include <iostream>
#include <cmath>

namespace
{
    int failures = 0;

    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    juce::AudioBuffer<float> makeSine (int nCh, int numSamples, double sr, double freq, float amp)
    {
        juce::AudioBuffer<float> buf (nCh, numSamples);
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / sr);
        }
        return buf;
    }

    bool isFinite (const juce::AudioBuffer<float>& buf)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* d = buf.getReadPointer (ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                if (! std::isfinite (d[i])) return false;
        }
        return true;
    }

    float maxAbsDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        float diff = 0.f;
        for (int ch = 0; ch < a.getNumChannels(); ++ch)
        {
            const float* da = a.getReadPointer (ch);
            const float* db = b.getReadPointer (ch);
            for (int i = 0; i < a.getNumSamples(); ++i)
                diff = juce::jmax (diff, std::abs (da[i] - db[i]));
        }
        return diff;
    }
}

int main()
{
    const double sr = 44100.0;
    const int blockSize = 512;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) blockSize, 2 };

    // ---- Every stage inactive: chain must be a true passthrough (minus the safety limiter) ----
    {
        std::cout << "\n[Chain] All stages inactive except limiter (default plan) -> near-identity\n";
        NovaMasteringChain chain;
        chain.prepare (spec);

        NovaMasteringPlan plan; // defaults: everything inactive except limiter at -1dB ceiling
        chain.setPlan (plan);

        auto original = makeSine (2, blockSize, sr, 440.0, 0.5f); // well under the limiter ceiling
        auto processed = original;
        chain.process (processed);

        const float diff = maxAbsDiff (original, processed);
        std::cout << "    max abs diff = " << diff << "\n";
        check (isFinite (processed), "output is finite");
        check (diff < 0.01f, "output matches input closely when nothing engages (diff < 0.01)");
    }

    // ---- Every stage active at moderate settings: must stay finite, bounded, and stable over many blocks ----
    {
        std::cout << "\n[Chain] All stages active at moderate settings -> stable over 5s\n";
        NovaMasteringChain chain;
        chain.prepare (spec);

        NovaMasteringPlan plan;
        plan.correctiveEQ.active = true;
        plan.correctiveEQ.lowShelfDb = 2.f; plan.correctiveEQ.lowMidDb = -1.5f;
        plan.correctiveEQ.highMidDb = 1.f;  plan.correctiveEQ.highShelfDb = 1.5f;
        plan.resonanceControl.active = true; plan.resonanceControl.amount = 0.6f;
        plan.compression.active = true;
        plan.compression.thresholdDb = -18.f; plan.compression.ratio = 3.f;
        plan.compression.attackMs = 20.f; plan.compression.releaseMs = 150.f;
        plan.saturation.active = true; plan.saturation.drive = 0.4f; plan.saturation.mix = 0.3f;
        plan.stereo.active = true; plan.stereo.widthAmount = 1.3f;
        plan.clipping.active = true; plan.clipping.ceilingDb = -0.3f;
        plan.limiting.active = true; plan.limiting.thresholdDb = -6.f; plan.limiting.truePeakCeilDb = -1.f;
        plan.output.gainDb = 3.f;
        chain.setPlan (plan);

        bool allFinite = true;
        float peakSeen = 0.f;
        const int numBlocks = (int) (sr * 5.0) / blockSize;
        for (int b = 0; b < numBlocks; ++b)
        {
            // Broadband-ish content with an occasional loud transient, to exercise compressor/limiter.
            auto buf = makeSine (2, blockSize, sr, 220.0 + (b % 7) * 130.0, (b % 11 == 0) ? 0.98f : 0.4f);
            chain.process (buf);
            if (! isFinite (buf)) { allFinite = false; break; }
            peakSeen = juce::jmax (peakSeen, buf.getMagnitude (0, blockSize));
        }

        std::cout << "    peak seen across 5s = " << juce::Decibels::gainToDecibels (peakSeen) << " dB\n";
        check (allFinite, "stays finite for 5s of varied, occasionally hot input");
        check (peakSeen <= juce::Decibels::decibelsToGain (0.0f) + 0.001f, "never exceeds 0dBFS despite +3dB output gain and hot input");
    }

    // ---- Limiter alone: true peak must never exceed the configured ceiling ----
    {
        std::cout << "\n[Chain] Limiter true-peak ceiling holds even with an inter-sample-peak-prone tone\n";
        NovaMasteringChain chain;
        chain.prepare (spec);

        NovaMasteringPlan plan;
        plan.limiting.active = true;
        plan.limiting.thresholdDb = -1.f;
        plan.limiting.truePeakCeilDb = -1.f; // dBTP ceiling
        chain.setPlan (plan);

        // A tone whose peaks deliberately fall between samples, pushed hot.
        auto buf = makeSine (2, (int) (sr * 1.0), sr, 3917.3, 1.0f);
        // Process in blockSize chunks like a real host would.
        float maxSamplePeak = 0.f;
        for (int start = 0; start < buf.getNumSamples(); start += blockSize)
        {
            const int len = juce::jmin (blockSize, buf.getNumSamples() - start);
            juce::AudioBuffer<float> chunk (2, len);
            for (int ch = 0; ch < 2; ++ch) chunk.copyFrom (ch, 0, buf, ch, start, len);
            chain.process (chunk);
            maxSamplePeak = juce::jmax (maxSamplePeak, chunk.getMagnitude (0, len));
        }

        const float peakDb = juce::Decibels::gainToDecibels (maxSamplePeak);
        std::cout << "    resulting sample peak = " << peakDb << " dB (ceiling was -1 dBTP)\n";
        // Sample peak should be at or below the ceiling; true peak (unmeasured
        // here) is what the limiter targets, so this is a coarse but
        // meaningful sanity check, not a full re-measurement.
        check (peakDb <= -0.9f, "resulting sample peak respects the -1dBTP ceiling (with small tolerance)");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL DSP CHAIN VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
