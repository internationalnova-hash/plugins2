#pragma once

// Regression test: verifies NovaMotionDSP produces sample-identical output
// to the original inline processBlock algorithm.
//
// Usage:  NovaMotionRegressionTest::runAll();  // asserts on failure
//
// The original algorithm is reproduced verbatim inside runOriginalAlgorithm()
// so that both paths are exercised against the same buffer simultaneously.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cassert>
#include <cmath>
#include <vector>

#include "NovaMotionDSP.h"
#include "NovaMotionParameters.h"
#include "../Testing/NovaDSPTestResult.h"

class NovaMotionRegressionTest
{
public:
    struct Result
    {
        float peakAbsDiff = 0.f;
        float rmsAbsDiff  = 0.f;
        bool  passed      = false;
    };

    // -----------------------------------------------------------------------
    // Original processBlock algorithm, copied verbatim from
    // Nova Motion FX/Source/PluginProcessor.cpp at extraction time.
    // -----------------------------------------------------------------------
    struct OriginalState
    {
        juce::dsp::StateVariableTPTFilter<float> filterL, filterR;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL { 192000 };
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR { 192000 };
        juce::Reverb reverbL;
        juce::AudioBuffer<float> dryBuf;
        double currentSR       { 44100.0 };
        int    maxDelaySamples { 0 };
        bool prevFilterActive  { false };
        bool prevDelayActive   { false };
        bool prevReverbActive  { false };

        void prepare (double sr, int block)
        {
            currentSR = sr;
            juce::dsp::ProcessSpec spec { sr, (juce::uint32)block, 2 };
            filterL.prepare (spec); filterL.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
            filterR.prepare (spec); filterR.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
            maxDelaySamples = (int)(sr * 2.0);
            delayLineL.prepare (spec); delayLineL.setMaximumDelayInSamples (maxDelaySamples);
            delayLineR.prepare (spec); delayLineR.setMaximumDelayInSamples (maxDelaySamples);
            reverbL.reset();
            dryBuf.setSize (2, block);
        }

        void processBlock (juce::AudioBuffer<float>& buf, const NovaMotionParameters& p)
        {
            const int N   = buf.getNumSamples();
            const int nCh = buf.getNumChannels();
            float* dataL  = buf.getWritePointer (0);
            float* dataR  = nCh > 1 ? buf.getWritePointer (1) : dataL;

            const float inGain  = juce::Decibels::decibelsToGain (p.inputDb);
            const float outGain = juce::Decibels::decibelsToGain (p.outputDb);

            const float safeMaxCutoff = (float)(currentSR * 0.40f);
            const bool  filterOn = p.cutoff < safeMaxCutoff - 100.f;
            const bool  delayOn  = p.delayMix  > 0.001f;
            const bool  reverbOn = p.reverbMix > 0.001f;

            if (!filterOn && prevFilterActive) { filterL.reset(); filterR.reset(); }
            if (!delayOn  && prevDelayActive)  { delayLineL.reset(); delayLineR.reset(); }
            if (!reverbOn && prevReverbActive) { reverbL.reset(); }
            prevFilterActive = filterOn;
            prevDelayActive  = delayOn;
            prevReverbActive = reverbOn;

            for (int ch = 0; ch < nCh; ++ch)
                juce::FloatVectorOperations::multiply (buf.getWritePointer (ch), inGain, N);

            dryBuf.setSize (nCh, N, false, false, true);
            dryBuf.makeCopyOf (buf, true);

            if (filterOn)
            {
                const float safeCut = juce::jmin (p.cutoff, safeMaxCutoff);
                const float safeRes = juce::jlimit (0.1f, 4.0f, p.resonance);
                filterL.setCutoffFrequency (safeCut); filterL.setResonance (safeRes);
                filterR.setCutoffFrequency (safeCut); filterR.setResonance (safeRes);
                for (int i = 0; i < N; ++i)
                {
                    dataL[i] = filterL.processSample (0, dataL[i]);
                    dataR[i] = filterR.processSample (0, dataR[i]);
                }
            }

            if (delayOn)
            {
                const float ds = juce::jmin ((float)maxDelaySamples - 1.f,
                                             (float)(currentSR * 0.25));
                for (int i = 0; i < N; ++i)
                {
                    const float dL = delayLineL.popSample (0, ds);
                    const float dR = delayLineR.popSample (0, ds);
                    delayLineL.pushSample (0, dataL[i] + dL * p.feedback);
                    delayLineR.pushSample (0, dataR[i] + dR * p.feedback);
                    dataL[i] = dataL[i] * (1.f - p.delayMix) + dL * p.delayMix;
                    dataR[i] = dataR[i] * (1.f - p.delayMix) + dR * p.delayMix;
                }
            }

            if (reverbOn)
            {
                juce::Reverb::Parameters rp;
                rp.roomSize   = p.size;
                rp.wetLevel   = p.reverbMix;
                rp.dryLevel   = 1.f - p.reverbMix;
                rp.damping    = 0.45f;
                rp.width      = 1.f;
                rp.freezeMode = 0.f;
                reverbL.setParameters (rp);
                if (nCh >= 2)
                    reverbL.processStereo (dataL, dataR, N);
                else
                    reverbL.processMono (dataL, N);
            }

            if (p.mix < 0.999f)
            {
                const float dry = 1.f - p.mix;
                for (int ch = 0; ch < nCh; ++ch)
                {
                    auto*       w = buf.getWritePointer (ch);
                    const auto* d = dryBuf.getReadPointer (ch);
                    for (int i = 0; i < N; ++i)
                        w[i] = w[i] * p.mix + d[i] * dry;
                }
            }

            for (int i = 0; i < N; ++i)
            {
                const float l = dataL[i] * outGain;
                const float r = dataR[i] * outGain;
                dataL[i] = std::isfinite (l) ? juce::jlimit (-1.f, 1.f, l) : 0.f;
                dataR[i] = std::isfinite (r) ? juce::jlimit (-1.f, 1.f, r) : 0.f;
            }
        }
    };

    // -----------------------------------------------------------------------
    static Result compare (const NovaMotionParameters& p,
                           double sr, int blockSize, int numBlocks,
                           bool stereo = true)
    {
        const int nCh = stereo ? 2 : 1;

        // Shared white-noise source (deterministic)
        juce::AudioBuffer<float> noise (nCh, blockSize);
        {
            juce::Random rng (12345);
            for (int ch = 0; ch < nCh; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    noise.setSample (ch, i, rng.nextFloat() * 2.f - 1.f);
        }

        // Prepare both engines from identical state
        OriginalState orig;
        orig.prepare (sr, blockSize);

        NovaMotionDSP engine;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32)blockSize, (juce::uint32)nCh };
        engine.prepare (spec);
        engine.setParameters (p);

        float peakAbsDiff = 0.f;
        double sumSqDiff  = 0.0;
        int    totalSamples = 0;

        for (int block = 0; block < numBlocks; ++block)
        {
            juce::AudioBuffer<float> bufA (nCh, blockSize);
            juce::AudioBuffer<float> bufB (nCh, blockSize);

            for (int ch = 0; ch < nCh; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    bufA.setSample (ch, i, noise.getSample (ch, i));
                    bufB.setSample (ch, i, noise.getSample (ch, i));
                }

            orig.processBlock (bufA, p);
            engine.process (bufB);

            for (int ch = 0; ch < nCh; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const float diff = std::abs (bufA.getSample (ch, i) - bufB.getSample (ch, i));
                    peakAbsDiff = juce::jmax (peakAbsDiff, diff);
                    sumSqDiff  += (double)diff * (double)diff;
                    ++totalSamples;
                }
        }

        Result r;
        r.peakAbsDiff = peakAbsDiff;
        r.rmsAbsDiff  = totalSamples > 0
                          ? (float)std::sqrt (sumSqDiff / (double)totalSamples)
                          : 0.f;
        r.passed = (r.peakAbsDiff == 0.f);
        return r;
    }

    // -----------------------------------------------------------------------
    static void runAll()
    {
        struct Scenario
        {
            const char* name;
            NovaMotionParameters params;
            double sr;
            int    blockSize;
            int    numBlocks;
            bool   stereo;
        };

        auto makeParams = [](float cutoff, float res, float fb, float dMix,
                             float rMix, float size, float inDb, float outDb,
                             float mix) -> NovaMotionParameters
        {
            NovaMotionParameters p;
            p.cutoff    = cutoff;
            p.resonance = res;
            p.feedback  = fb;
            p.delayMix  = dMix;
            p.reverbMix = rMix;
            p.size      = size;
            p.inputDb   = inDb;
            p.outputDb  = outDb;
            p.mix       = mix;
            return p;
        };

        // All stages off (passthrough paths)
        Scenario passthrough44 = { "passthrough_44100", makeParams(20000.f,0.707f,0.f,0.f,0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario passthrough48 = { "passthrough_48000", makeParams(20000.f,0.707f,0.f,0.f,0.f,0.6f,0.f,0.f,1.f), 48000.0, 512, 4, true };
        Scenario passthrough96 = { "passthrough_96000", makeParams(20000.f,0.707f,0.f,0.f,0.f,0.6f,0.f,0.f,1.f), 96000.0, 512, 4, true };

        // Filter only
        Scenario filterLow  = { "filter_low_res",  makeParams(500.f,  0.1f, 0.f,0.f,0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario filterMid  = { "filter_mid_res",  makeParams(2000.f, 2.0f, 0.f,0.f,0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario filterHigh = { "filter_high_res", makeParams(8000.f, 3.9f, 0.f,0.f,0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        // resonance clamped to 4.0 — verify clamp path
        Scenario filterClamp = { "filter_res_clamp", makeParams(1000.f,20.f, 0.f,0.f,0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };

        // Delay only
        Scenario delayMin    = { "delay_min_mix",  makeParams(20000.f,0.707f,0.f,  0.01f,0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario delayMid    = { "delay_mid_mix",  makeParams(20000.f,0.707f,0.3f, 0.5f, 0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario delayMax    = { "delay_max_fb",   makeParams(20000.f,0.707f,0.95f,1.0f, 0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };

        // Reverb only
        Scenario reverbSmall  = { "reverb_small",  makeParams(20000.f,0.707f,0.f,0.f,0.2f,0.2f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario reverbLarge  = { "reverb_large",  makeParams(20000.f,0.707f,0.f,0.f,0.8f,1.0f,0.f,0.f,1.f), 44100.0, 512, 4, true };

        // All stages together
        Scenario allStages = { "all_stages", makeParams(2000.f,1.5f,0.5f,0.4f,0.5f,0.7f,3.f,-3.f,0.75f), 44100.0, 512, 8, true };

        // Wet/dry extremes
        Scenario dryOnly = { "dry_only",  makeParams(1000.f,1.0f,0.3f,0.3f,0.3f,0.5f,0.f,0.f,0.f), 44100.0, 512, 4, true };
        Scenario wetOnly = { "wet_only",  makeParams(1000.f,1.0f,0.3f,0.3f,0.3f,0.5f,0.f,0.f,1.f), 44100.0, 512, 4, true };

        // Gain extremes
        Scenario gainHot  = { "gain_hot",  makeParams(20000.f,0.707f,0.f,0.f,0.f,0.6f,12.f,12.f,1.f), 44100.0, 512, 4, true };
        Scenario gainCold = { "gain_cold", makeParams(20000.f,0.707f,0.f,0.f,0.f,0.6f,-24.f,-24.f,1.f), 44100.0, 512, 4, true };

        // Mono
        Scenario mono44 = { "mono_44100", makeParams(1000.f,1.5f,0.4f,0.3f,0.4f,0.6f,0.f,0.f,0.8f), 44100.0, 512, 4, false };

        // Block size variants
        Scenario block64  = { "block_64",  makeParams(1000.f,1.0f,0.2f,0.2f,0.2f,0.5f,0.f,0.f,1.f), 44100.0, 64,  8, true };
        Scenario block2048 = { "block_2048",makeParams(1000.f,1.0f,0.2f,0.2f,0.2f,0.5f,0.f,0.f,1.f), 44100.0,2048, 2, true };

        Scenario scenarios[] = {
            passthrough44, passthrough48, passthrough96,
            filterLow, filterMid, filterHigh, filterClamp,
            delayMin, delayMid, delayMax,
            reverbSmall, reverbLarge,
            allStages,
            dryOnly, wetOnly,
            gainHot, gainCold,
            mono44,
            block64, block2048
        };

        for (auto& s : scenarios)
        {
            Result r = compare (s.params, s.sr, s.blockSize, s.numBlocks, s.stereo);
            assert (r.passed && "NovaMotionDSP regression FAILED — output differs from original");
            juce::ignoreUnused (r);
        }
    }

    // Non-asserting variant: returns one result per scenario for the runner.
    static std::vector<NovaDSPTestResult> runAllResults()
    {
        std::vector<NovaDSPTestResult> results;

        struct Scenario
        {
            const char* name;
            NovaMotionParameters params;
            double sr;
            int    blockSize;
            int    numBlocks;
            bool   stereo;
        };

        auto makeParams = [](float cutoff, float res, float fb, float dMix,
                             float rMix, float size, float inDb, float outDb,
                             float mix) -> NovaMotionParameters
        {
            NovaMotionParameters p;
            p.cutoff    = cutoff;
            p.resonance = res;
            p.feedback  = fb;
            p.delayMix  = dMix;
            p.reverbMix = rMix;
            p.size      = size;
            p.inputDb   = inDb;
            p.outputDb  = outDb;
            p.mix       = mix;
            return p;
        };

        Scenario passthrough44  = { "passthrough_44100", makeParams(20000.f,0.707f,0.f,0.f,0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario passthrough48  = { "passthrough_48000", makeParams(20000.f,0.707f,0.f,0.f,0.f,0.6f,0.f,0.f,1.f), 48000.0, 512, 4, true };
        Scenario passthrough96  = { "passthrough_96000", makeParams(20000.f,0.707f,0.f,0.f,0.f,0.6f,0.f,0.f,1.f), 96000.0, 512, 4, true };
        Scenario filterLow      = { "filter_low",        makeParams(200.f, 1.5f, 0.f,0.f,0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario filterMid      = { "filter_mid",        makeParams(2000.f,1.5f, 0.f,0.f,0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario filterHigh     = { "filter_high",       makeParams(8000.f,1.5f, 0.f,0.f,0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario filterClamp    = { "filter_q_clamp",    makeParams(1000.f,4.5f, 0.f,0.f,0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario delayMin       = { "delay_min",         makeParams(20000.f,0.707f,0.f,0.1f,0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario delayMid       = { "delay_mid",         makeParams(20000.f,0.707f,0.f,0.5f,0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario delayMax       = { "delay_max",         makeParams(20000.f,0.707f,0.f,1.0f,0.f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario reverbSmall    = { "reverb_small",      makeParams(20000.f,0.707f,0.f,0.f,0.3f,0.3f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario reverbLarge    = { "reverb_large",      makeParams(20000.f,0.707f,0.f,0.f,0.8f,0.9f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario allStages      = { "all_stages",        makeParams(1000.f, 1.5f, 0.2f,0.4f,0.3f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario dryOnly        = { "mix_dry_only",      makeParams(1000.f, 1.5f, 0.2f,0.4f,0.3f,0.6f,0.f,0.f,0.f), 44100.0, 512, 4, true };
        Scenario wetOnly        = { "mix_wet_only",      makeParams(1000.f, 1.5f, 0.2f,0.4f,0.3f,0.6f,0.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario gainHot        = { "gain_hot",          makeParams(20000.f,0.707f,0.f,0.f,0.f,0.6f,6.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario gainCold       = { "gain_cold",         makeParams(20000.f,0.707f,0.f,0.f,0.f,0.6f,-20.f,0.f,1.f), 44100.0, 512, 4, true };
        Scenario mono44         = { "mono_44100",        makeParams(1000.f, 1.0f, 0.2f,0.2f,0.2f,0.5f,0.f,0.f,1.f), 44100.0, 512, 4, false };
        Scenario block64        = { "block_64",          makeParams(1000.f,1.0f,0.2f,0.2f,0.2f,0.5f,0.f,0.f,1.f), 44100.0, 64,  8, true };
        Scenario block2048      = { "block_2048",        makeParams(1000.f,1.0f,0.2f,0.2f,0.2f,0.5f,0.f,0.f,1.f), 44100.0,2048, 2, true };

        Scenario scenarios[] = {
            passthrough44, passthrough48, passthrough96,
            filterLow, filterMid, filterHigh, filterClamp,
            delayMin, delayMid, delayMax,
            reverbSmall, reverbLarge,
            allStages,
            dryOnly, wetOnly,
            gainHot, gainCold,
            mono44,
            block64, block2048
        };

        for (auto& s : scenarios)
        {
            Result r = compare (s.params, s.sr, s.blockSize, s.numBlocks, s.stereo);
            NovaDSPTestResult res;
            res.suite       = "NovaMotion";
            res.scenario    = s.name;
            res.passed      = r.passed;
            res.peakAbsDiff = r.peakAbsDiff;
            res.rmsAbsDiff  = r.rmsAbsDiff;
            results.push_back (res);
        }
        return results;
    }

private:
    NovaMotionRegressionTest() = delete;
};
