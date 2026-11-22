#pragma once
// ---------------------------------------------------------------------------
// NovaLevelRegressionTest
//
// Verifies that NovaLevelDSP produces sample-identical output to the original
// inline Nova Level algorithm. Run from a JUCE unit test or a standalone test
// target that links NovaDSP and juce_audio_basics.
//
// Usage:
//   NovaLevelRegressionTest::runAll();
// ---------------------------------------------------------------------------
#include "NovaLevelDSP.h"
#include "../Testing/NovaDSPTestResult.h"
#include <cmath>
#include <string>
#include <vector>
#include <cassert>

struct NovaLevelRegressionTest
{
    struct Result
    {
        std::string label;
        float peakAbsDiff = 0.f;
        float rmsAbsDiff  = 0.f;
        bool  identical   = false;
    };

    // -----------------------------------------------------------------------
    // Original algorithm — preserved verbatim from Nova Level processBlock.
    // This stays here until the extracted engine is confirmed identical,
    // then it may be removed.
    // -----------------------------------------------------------------------
    static void runOriginalAlgorithm (juce::AudioBuffer<float>& buffer,
                                      const NovaLevelParameters& p,
                                      double sampleRate,
                                      float& grEnv)
    {
        const int totalChannels = buffer.getNumChannels();
        const int numSamples    = buffer.getNumSamples();

        float thresholdDb = -18.0f, ratioMax = 4.0f, attackMs = 20.0f, releaseMs = 140.0f;
        if (p.mode == 1) { thresholdDb = -16.f; ratioMax = 6.f;  attackMs = 8.f;  releaseMs = 90.f;  }
        if (p.mode == 2) { thresholdDb = -12.f; ratioMax = 10.f; attackMs = 2.f;  releaseMs = 55.f;  }

        const float amount       = juce::jlimit (0.f, 10.f, p.compression) / 10.f;
        const float ratio        = 1.f + (ratioMax - 1.f) * amount;
        const float sr           = static_cast<float> (sampleRate);
        const float attackCoeff  = std::exp (-1.f / (0.001f * attackMs  * sr));
        const float releaseCoeff = std::exp (-1.f / (0.001f * releaseMs * sr));
        const float outputGain   = juce::Decibels::decibelsToGain (p.outputDb);
        const bool  magicActive  = p.magic > 0.5f;

        for (int s = 0; s < numSamples; ++s)
        {
            float detector = 0.f;
            for (int ch = 0; ch < totalChannels; ++ch)
                detector = juce::jmax (detector, std::abs (buffer.getSample (ch, s)));

            const float inputDb    = juce::Decibels::gainToDecibels (juce::jmax (detector, 1.e-6f));
            const float overDb     = juce::jmax (0.f, inputDb - thresholdDb);
            const float targetGrDb = overDb * (1.f - 1.f / ratio);
            const float coeff      = targetGrDb > grEnv ? attackCoeff : releaseCoeff;
            grEnv = coeff * grEnv + (1.f - coeff) * targetGrDb;

            const float gain = juce::Decibels::decibelsToGain (-grEnv) * outputGain;
            for (int ch = 0; ch < totalChannels; ++ch)
            {
                float y = buffer.getSample (ch, s) * gain;
                if (magicActive) y = std::tanh (1.25f * y);
                buffer.setSample (ch, s, y);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Fills a buffer with a deterministic test signal (mixed sine tones)
    // -----------------------------------------------------------------------
    static void fillTestSignal (juce::AudioBuffer<float>& buf, double sr)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            for (int s = 0; s < buf.getNumSamples(); ++s)
            {
                const float t = static_cast<float> (s) / static_cast<float> (sr);
                buf.setSample (ch, s,
                    0.6f * std::sin (2.f * 3.14159265f * 440.f * t)
                    + 0.3f * std::sin (2.f * 3.14159265f * 1200.f * t));
            }
    }

    // -----------------------------------------------------------------------
    // Run one scenario; returns diff stats
    // -----------------------------------------------------------------------
    static Result compare (const char* label,
                           int numChannels, int numSamples, double sampleRate,
                           const NovaLevelParameters& params)
    {
        // Reference buffer → run original algorithm
        juce::AudioBuffer<float> refBuf (numChannels, numSamples);
        fillTestSignal (refBuf, sampleRate);
        float refGrEnv = 0.f;
        runOriginalAlgorithm (refBuf, params, sampleRate, refGrEnv);

        // Extracted DSP buffer
        juce::AudioBuffer<float> dspBuf (numChannels, numSamples);
        fillTestSignal (dspBuf, sampleRate);
        {
            NovaLevelDSP dsp;
            juce::dsp::ProcessSpec spec { sampleRate,
                static_cast<juce::uint32> (numSamples),
                static_cast<juce::uint32> (numChannels) };
            dsp.prepare (spec);
            dsp.setParameters (params);
            dsp.process (dspBuf);
        }

        // Measure diff
        float peakDiff = 0.f, sumSqDiff = 0.f;
        int   total    = 0;
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < numSamples; ++s)
            {
                const float d = std::abs (refBuf.getSample (ch, s) - dspBuf.getSample (ch, s));
                peakDiff = std::max (peakDiff, d);
                sumSqDiff += d * d;
                ++total;
            }

        Result r;
        r.label       = label;
        r.peakAbsDiff = peakDiff;
        r.rmsAbsDiff  = total > 0 ? std::sqrt (sumSqDiff / static_cast<float> (total)) : 0.f;
        r.identical   = (peakDiff == 0.f);
        return r;
    }

    // -----------------------------------------------------------------------
    // Run the full regression suite
    // -----------------------------------------------------------------------
    static void runAll()
    {
        constexpr int N = 4096;

        const struct { double sr; int ch; } configs[] = {
            { 44100.0, 1 }, { 44100.0, 2 },
            { 48000.0, 2 }, { 88200.0, 2 },
            { 96000.0, 2 }, { 192000.0, 2 }
        };

        for (auto [sr, ch] : configs)
        {
            for (int mode = 0; mode <= 2; ++mode)
            {
                for (float comp : { 0.f, 5.f, 10.f })
                {
                    for (bool magic : { false, true })
                    {
                        NovaLevelParameters p;
                        p.compression = comp;
                        p.outputDb    = 0.f;
                        p.mode        = mode;
                        p.magic       = magic ? 1.f : 0.f;

                        char label[128];
                        std::snprintf (label, sizeof (label),
                            "sr=%.0f ch=%d mode=%d comp=%.0f magic=%d",
                            sr, ch, mode, comp, (int) magic);

                        auto r = compare (label, ch, N, sr, p);

                        // Log result (replace with juce::UnitTest expectation as needed)
                        // Peak diff should be 0 (sample-identical) because we preserved
                        // the exact arithmetic order.
                        assert (r.peakAbsDiff == 0.f &&
                            "NovaLevelDSP output differs from reference — algorithm diverged!");
                    }
                }
            }
        }
    }

    // Non-asserting variant: returns one result per scenario for the runner.
    static std::vector<NovaDSPTestResult> runAllResults()
    {
        std::vector<NovaDSPTestResult> results;
        constexpr int N = 4096;

        const struct { double sr; int ch; } configs[] = {
            { 44100.0, 1 }, { 44100.0, 2 },
            { 48000.0, 2 }, { 88200.0, 2 },
            { 96000.0, 2 }, { 192000.0, 2 }
        };

        for (auto [sr, ch] : configs)
        {
            for (int mode = 0; mode <= 2; ++mode)
            {
                for (float comp : { 0.f, 5.f, 10.f })
                {
                    for (bool magic : { false, true })
                    {
                        NovaLevelParameters p;
                        p.compression = comp;
                        p.outputDb    = 0.f;
                        p.mode        = mode;
                        p.magic       = magic ? 1.f : 0.f;

                        char label[128];
                        std::snprintf (label, sizeof (label),
                            "sr=%.0f ch=%d mode=%d comp=%.0f magic=%d",
                            sr, ch, mode, comp, (int) magic);

                        auto r = compare (label, ch, N, sr, p);

                        NovaDSPTestResult res;
                        res.suite       = "NovaLevel";
                        res.scenario    = label;
                        res.passed      = r.identical;
                        res.peakAbsDiff = r.peakAbsDiff;
                        res.rmsAbsDiff  = r.rmsAbsDiff;
                        results.push_back (res);
                    }
                }
            }
        }
        return results;
    }
};
