#pragma once

// Regression test — verifies NovaSpaceDSP produces sample-identical output
// to the original inline processBlock algorithm across a wide scenario matrix.
//
// Usage: NovaSpaceRegressionTest::runAll();  // asserts on failure
//
// The original algorithm is reproduced verbatim inside OriginalState so that
// both paths exercise the same buffer with the same arithmetic.

#include <vector>
#include <cmath>
#include <cassert>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "NovaSpaceDSP.h"
#include "NovaSpaceParameters.h"

class NovaSpaceRegressionTest
{
public:
    struct Result
    {
        float peakAbsDiff = 0.f;
        float rmsAbsDiff  = 0.f;
        bool  passed      = false;
        const char* scenario = "";
    };

    // -----------------------------------------------------------------------
    // Original algorithm reproduced verbatim from
    // Space By Nova/Source/PluginProcessor.cpp at extraction time.
    // -----------------------------------------------------------------------
    struct OriginalState
    {
        using Filter    = juce::dsp::StateVariableTPTFilter<float>;
        using DelayLine = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;

        static float clamp01 (float v) noexcept { return juce::jlimit (0.0f, 1.0f, v); }

        juce::Reverb reverb;
        DelayLine preDelayLeft  { 96000 };
        DelayLine preDelayRight { 96000 };
        DelayLine decorrelationDelay { 4096 };

        Filter wetToneLeft, wetToneRight;
        Filter wetBodyLeft, wetBodyRight;
        Filter earlyToneLeft, earlyToneRight;
        Filter earlyBodyLeft, earlyBodyRight;

        juce::LinearSmoothedValue<float> smoothedSpace;
        juce::LinearSmoothedValue<float> smoothedAir;
        juce::LinearSmoothedValue<float> smoothedDepth;
        juce::LinearSmoothedValue<float> smoothedMix;
        juce::LinearSmoothedValue<float> smoothedWidth;

        juce::AudioBuffer<float> dryBuffer, wetBuffer, earlyBuffer;

        std::vector<float> earlyTapBufferLeft, earlyTapBufferRight;
        int   earlyTapWriteIndex { 0 };
        int   earlyTapBufferSize { 0 };
        float earlyDiffuseStateLeft  { 0.0f };
        float earlyDiffuseStateRight { 0.0f };
        double currentSampleRate { 44100.0 };
        float motionPhase { 0.0f };
        float haloPhase   { 1.7f };

        float readEarlyTap (const std::vector<float>& source, float delaySamples) const
        {
            if (source.empty() || earlyTapBufferSize <= 1)
                return 0.0f;
            const float wrapped = std::fmod (delaySamples + (float)earlyTapBufferSize, (float)earlyTapBufferSize);
            const int offsetA   = (int)wrapped;
            const int offsetB   = (offsetA + 1) % earlyTapBufferSize;
            const float frac    = wrapped - (float)offsetA;
            const int readA = (earlyTapWriteIndex - offsetA + earlyTapBufferSize) % earlyTapBufferSize;
            const int readB = (earlyTapWriteIndex - offsetB + earlyTapBufferSize) % earlyTapBufferSize;
            return source[(size_t)readA] + ((source[(size_t)readB] - source[(size_t)readA]) * frac);
        }

        void prepare (double sr, int block, const NovaSpaceParameters& initParams)
        {
            currentSampleRate = sr;
            juce::dsp::ProcessSpec spec { sr, (juce::uint32)block, 1 };

            preDelayLeft.prepare (spec); preDelayLeft.reset();
            preDelayRight.prepare (spec); preDelayRight.reset();
            decorrelationDelay.prepare (spec); decorrelationDelay.reset();

            for (auto* f : { &wetToneLeft,  &wetToneRight,  &wetBodyLeft,  &wetBodyRight,
                             &earlyToneLeft, &earlyToneRight, &earlyBodyLeft, &earlyBodyRight })
            { f->prepare (spec); f->reset(); }

            wetToneLeft.setType  (juce::dsp::StateVariableTPTFilterType::lowpass);
            wetToneRight.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
            wetBodyLeft.setType  (juce::dsp::StateVariableTPTFilterType::highpass);
            wetBodyRight.setType (juce::dsp::StateVariableTPTFilterType::highpass);
            earlyToneLeft.setType  (juce::dsp::StateVariableTPTFilterType::lowpass);
            earlyToneRight.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
            earlyBodyLeft.setType  (juce::dsp::StateVariableTPTFilterType::highpass);
            earlyBodyRight.setType (juce::dsp::StateVariableTPTFilterType::highpass);

            dryBuffer.setSize (2, block);
            wetBuffer.setSize (2, block);
            earlyBuffer.setSize (2, block);

            earlyTapBufferSize = juce::jmax (256, (int)(sr * 0.60));
            earlyTapBufferLeft.assign  ((size_t)earlyTapBufferSize, 0.0f);
            earlyTapBufferRight.assign ((size_t)earlyTapBufferSize, 0.0f);
            earlyTapWriteIndex = 0;
            earlyDiffuseStateLeft = earlyDiffuseStateRight = 0.0f;

            for (auto* sv : { &smoothedSpace, &smoothedAir, &smoothedDepth, &smoothedMix, &smoothedWidth })
                sv->reset (sr, 0.20);
            smoothedSpace.setCurrentAndTargetValue (initParams.space);
            smoothedAir.setCurrentAndTargetValue   (initParams.air);
            smoothedDepth.setCurrentAndTargetValue (initParams.depth);
            smoothedMix.setCurrentAndTargetValue   (initParams.mix);
            smoothedWidth.setCurrentAndTargetValue (initParams.width);

            reverb.reset();
            motionPhase = 0.0f;
            haloPhase   = 1.7f;
        }

        void processBlock (juce::AudioBuffer<float>& buffer, const NovaSpaceParameters& p)
        {
            const int N   = buffer.getNumSamples();
            const int nCh = buffer.getNumChannels();
            if (N == 0) return;

            dryBuffer.setSize   (juce::jmax(1,nCh), N, false, false, true);
            wetBuffer.setSize   (juce::jmax(1,nCh), N, false, false, true);
            earlyBuffer.setSize (juce::jmax(1,nCh), N, false, false, true);
            dryBuffer.makeCopyOf (buffer, true);
            wetBuffer.makeCopyOf (buffer, true);
            earlyBuffer.clear();

            smoothedSpace.setTargetValue (p.space);
            smoothedAir.setTargetValue   (p.air);
            smoothedDepth.setTargetValue (p.depth);
            smoothedMix.setTargetValue   (p.mix);
            smoothedWidth.setTargetValue (p.width);

            const float space = smoothedSpace.skip (N);
            const float air   = smoothedAir.skip (N);
            const float depth = smoothedDepth.skip (N);
            const float mix   = smoothedMix.skip (N);
            const float width = smoothedWidth.skip (N);

            const float preDelayMsBase = p.preDelayMs;
            const float decaySeconds   = p.decay;
            const float dampingControl = p.damping / 100.0f;
            const float earlyControl   = p.early   / 100.0f;
            const int   modeIndex      = p.mode;

            const float spaceNorm = clamp01 (space / 10.0f);
            const float airNorm   = clamp01 (air   / 10.0f);
            const float depthNorm = clamp01 (depth / 10.0f);
            const float mixNorm   = clamp01 (mix   / 100.0f);
            const float widthNorm = clamp01 (width / 10.0f);
            const float decayNorm = clamp01 ((decaySeconds - 0.5f) / 6.0f);
            const float airCurve  = 1.0f - ((1.0f - airNorm) * (1.0f - airNorm));
            const float airSafety = airNorm * airNorm;
            const float attackSoftness = juce::jlimit (0.0f, 0.35f, 0.04f + (0.16f * depthNorm) + (0.10f * spaceNorm));

            float roomSize = clamp01 (0.21f + (0.43f * spaceNorm) + (0.22f * decayNorm));
            float damping  = juce::jlimit (0.16f, 0.95f,
                                           (0.76f - (0.26f * airCurve))
                                           + (0.12f * spaceNorm)
                                           + (0.18f * dampingControl)
                                           + (0.08f * airSafety));
            float preDelayMs  = juce::jlimit (0.0f, 120.0f, preDelayMsBase);
            float earlyAmount = juce::jlimit (0.08f, 0.35f, (0.24f + (0.08f * earlyControl)) - (0.16f * depthNorm) + (0.04f * spaceNorm));
            float earlySpreadMs = juce::jlimit (5.0f, 40.0f, 10.0f + (spaceNorm * 20.0f) + (depthNorm * 10.0f));
            const float earlyDiffusion = juce::jlimit (0.35f, 0.90f, 0.52f + (spaceNorm * 0.24f) + (depthNorm * 0.12f));
            const float earlyWidth    = juce::jmap (widthNorm, 0.20f, 0.68f);
            const float earlyTailFeed = juce::jlimit (0.05f, 0.22f, 0.10f + (spaceNorm * 0.07f) - (depthNorm * 0.03f));
            float wetTrim       = juce::jlimit (0.65f, 1.08f, juce::jmap (mixNorm, 0.0f, 1.0f, 1.0f, 0.93f) + (0.06f * decayNorm) + (0.02f * spaceNorm));
            float sideGain      = juce::jmap (widthNorm, 0.72f, 1.78f);
            float decorrelationMs = juce::jmap (widthNorm, 0.0f, 12.0f);
            float toneCutoffHz  = juce::jlimit (3200.0f, 16000.0f, 4600.0f + (airCurve * 9000.0f) - (spaceNorm * 1600.0f));
            float lowCutHz      = juce::jlimit (90.0f, 420.0f, 120.0f + (spaceNorm * 120.0f) + (depthNorm * 55.0f));
            const float earlyHighCutHz = juce::jlimit (6000.0f, 10000.0f, 6200.0f + (airNorm * 3600.0f));
            const float earlyLowCutHz  = juce::jlimit (150.0f, 220.0f, 150.0f + (1.0f - airNorm) * 70.0f);
            float brightnessGain = juce::jlimit (0.94f, 1.08f, 0.97f + (0.08f * airCurve) - (0.03f * airSafety));
            float internalWidth  = juce::jlimit (0.25f, 1.0f, 0.50f + (0.38f * widthNorm));
            float duckAmount     = juce::jlimit (0.05f, 0.30f, 0.18f + (0.08f * (1.0f - depthNorm)));
            float glueAmount     = juce::jlimit (0.02f, 0.14f, 0.03f + (0.03f * spaceNorm) + (0.03f * depthNorm));
            float haloBlend      = juce::jlimit (0.01f, 0.16f, 0.03f + (0.05f * widthNorm) + (0.03f * airNorm));
            float motionDepth    = juce::jlimit (0.004f, 0.08f, 0.008f + (0.020f * spaceNorm) + (0.012f * depthNorm));
            float tailLift       = juce::jlimit (0.01f, 0.12f, 0.02f + (0.04f * airNorm) + (0.03f * decayNorm));

            switch (modeIndex)
            {
                case 1:
                    roomSize = clamp01(roomSize+0.09f); earlyAmount*=0.80f; earlySpreadMs+=4.0f; sideGain+=0.16f; decorrelationMs+=2.1f; wetTrim+=0.02f;
                    toneCutoffHz=juce::jmin(toneCutoffHz*1.02f,13200.0f); lowCutHz+=25.0f; brightnessGain*=1.01f; duckAmount=0.17f; glueAmount=0.08f;
                    haloBlend+=0.025f; motionDepth+=0.010f; tailLift+=0.020f; internalWidth=juce::jlimit(0.25f,1.0f,internalWidth+0.10f); break;
                case 2:
                    roomSize=clamp01(roomSize+0.12f); earlyAmount*=0.46f; earlySpreadMs+=6.0f; damping=juce::jlimit(0.16f,0.95f,damping+0.10f);
                    sideGain+=0.18f; decorrelationMs+=4.0f; wetTrim+=0.02f; toneCutoffHz*=0.84f; lowCutHz+=42.0f; brightnessGain*=0.95f;
                    duckAmount=0.14f; glueAmount=0.10f; haloBlend+=0.040f; motionDepth+=0.018f; tailLift+=0.035f;
                    internalWidth=juce::jlimit(0.25f,1.0f,internalWidth+0.12f); break;
                case 3:
                    roomSize=clamp01(roomSize-0.01f); earlyAmount=juce::jlimit(0.10f,0.60f,earlyAmount+0.05f);
                    earlySpreadMs=juce::jmax(5.0f,earlySpreadMs-4.0f); damping=juce::jlimit(0.16f,0.95f,damping+0.18f);
                    sideGain*=0.72f; decorrelationMs*=0.58f; toneCutoffHz*=0.70f; lowCutHz+=10.0f; brightnessGain*=0.91f;
                    wetTrim*=0.96f; duckAmount=0.12f; glueAmount=0.07f; haloBlend*=0.65f; motionDepth*=0.55f; tailLift*=0.55f; internalWidth*=0.74f; break;
                default:
                    roomSize*=0.74f; earlyAmount=juce::jlimit(0.12f,0.58f,earlyAmount+0.08f); earlySpreadMs=juce::jmax(5.0f,earlySpreadMs-2.5f);
                    wetTrim*=0.90f; sideGain*=0.74f; decorrelationMs*=0.58f; toneCutoffHz=juce::jmin(toneCutoffHz,9000.0f);
                    lowCutHz+=60.0f; brightnessGain*=0.97f; duckAmount=0.30f; glueAmount=0.05f; haloBlend*=0.55f;
                    motionDepth*=0.50f; tailLift*=0.45f; internalWidth*=0.78f; break;
            }

            preDelayMs    = juce::jlimit (0.0f, 40.0f,  preDelayMs);
            earlySpreadMs = juce::jlimit (5.0f, 40.0f, earlySpreadMs);

            juce::Reverb::Parameters rp;
            rp.roomSize=roomSize; rp.damping=damping; rp.wetLevel=1.0f; rp.dryLevel=0.0f; rp.width=internalWidth; rp.freezeMode=0.0f;
            reverb.setParameters (rp);

            wetToneLeft.setCutoffFrequency (toneCutoffHz);  wetToneRight.setCutoffFrequency (toneCutoffHz);
            wetBodyLeft.setCutoffFrequency (lowCutHz);       wetBodyRight.setCutoffFrequency (lowCutHz);
            earlyToneLeft.setCutoffFrequency (earlyHighCutHz); earlyToneRight.setCutoffFrequency (earlyHighCutHz);
            earlyBodyLeft.setCutoffFrequency (earlyLowCutHz);  earlyBodyRight.setCutoffFrequency (earlyLowCutHz);

            const float preDelaySamples = juce::jlimit (0.0f, 0.4f*(float)currentSampleRate, (float)currentSampleRate * preDelayMs * 0.001f);
            preDelayLeft.setDelay (preDelaySamples); preDelayRight.setDelay (preDelaySamples);
            const float decorSamples = juce::jlimit (0.0f, 2048.0f, (float)currentSampleRate * decorrelationMs * 0.001f);
            decorrelationDelay.setDelay (decorSamples);

            auto* wetL  = wetBuffer.getWritePointer (0);
            auto* wetR  = wetBuffer.getNumChannels() > 1 ? wetBuffer.getWritePointer (1) : nullptr;
            auto* earlyL = earlyBuffer.getWritePointer (0);
            auto* earlyR = earlyBuffer.getNumChannels() > 1 ? earlyBuffer.getWritePointer (1) : nullptr;

            for (int i = 0; i < N; ++i)
            {
                const float inputL = wetL[i];
                const float inputR = wetR != nullptr ? wetR[i] : inputL;
                const float delayedL = preDelayLeft.popSample  (0);
                const float delayedR = preDelayRight.popSample (0);
                preDelayLeft.pushSample  (0, inputL);
                preDelayRight.pushSample (0, inputR);

                earlyTapBufferLeft [(size_t)earlyTapWriteIndex] = delayedL;
                earlyTapBufferRight[(size_t)earlyTapWriteIndex] = delayedR;

                const float spreadSamples  = (float)currentSampleRate * earlySpreadMs * 0.001f;
                const float baseTapSamples = 0.001f * (float)currentSampleRate * juce::jmap (spaceNorm, 4.5f, 9.5f);
                const float densitySkew    = juce::jmap (spaceNorm, 0.85f, 0.55f);

                float ecL = 0.0f, ecR = 0.0f;
                for (int tap = 0; tap < 6; ++tap)
                {
                    const float tn  = (float)tap / 5.0f;
                    const float tp  = std::pow (tn, densitySkew);
                    const float td  = baseTapSamples + (tp * spreadSamples);
                    const float tw  = (0.22f + (0.58f * (1.0f - tn))) * (1.0f - (tn * 0.12f));
                    ecL += readEarlyTap (earlyTapBufferLeft,  td) * tw;
                    ecR += readEarlyTap (earlyTapBufferRight, td) * tw;
                }

                earlyDiffuseStateLeft  += (ecL - earlyDiffuseStateLeft)  * (0.32f + (0.28f * earlyDiffusion));
                earlyDiffuseStateRight += (ecR - earlyDiffuseStateRight) * (0.32f + (0.28f * earlyDiffusion));

                float erL = juce::jmap (earlyDiffusion, ecL, earlyDiffuseStateLeft);
                float erR = juce::jmap (earlyDiffusion, ecR, earlyDiffuseStateRight);
                erL = earlyBodyLeft.processSample  (0, earlyToneLeft.processSample  (0, erL));
                erR = earlyBodyRight.processSample (0, earlyToneRight.processSample (0, erR));

                const float erMid  = 0.5f * (erL + erR);
                const float erSide = 0.5f * (erL - erR) * (0.65f + earlyWidth);
                erL = (erMid + erSide) * earlyAmount;
                erR = (erMid - erSide) * earlyAmount;

                earlyL[i] = erL;
                if (earlyR) earlyR[i] = erR;

                const float bloomL = juce::jmap (attackSoftness, delayedL, (delayedL*0.72f)+(inputL*0.28f));
                wetL[i] = bloomL + (erL * earlyTailFeed);
                if (wetR)
                {
                    const float bloomR = juce::jmap (attackSoftness, delayedR, (delayedR*0.72f)+(inputR*0.28f));
                    wetR[i] = bloomR + (erR * earlyTailFeed);
                }
                earlyTapWriteIndex = (earlyTapWriteIndex + 1) % earlyTapBufferSize;
            }

            if (wetR) reverb.processStereo (wetL, wetR, N);
            else      reverb.processMono   (wetL, N);

            auto* outL = buffer.getWritePointer (0);
            auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
            const auto* dryL = dryBuffer.getReadPointer (0);
            const auto* dryR = dryBuffer.getNumChannels() > 1 ? dryBuffer.getReadPointer (1) : nullptr;

            const float twoPi = juce::MathConstants<float>::twoPi;
            const float motionRateHz = juce::jmap (depthNorm, 0.11f, 0.33f);
            const float haloRateHz   = juce::jmap (spaceNorm, 0.07f, 0.19f);

            for (int i = 0; i < N; ++i)
            {
                const float dryRight   = dryR ? dryR[i] : dryL[i];
                const float ve         = 0.5f * (std::abs(dryL[i]) + std::abs(dryRight));
                const float vs         = clamp01 (std::sqrt (ve * 3.2f));
                const float gb         = 1.0f + ((1.0f - vs) * tailLift);
                const float mLfo       = std::sin (motionPhase);
                const float hLfo       = 0.5f + (0.5f * std::sin (haloPhase));

                motionPhase += (twoPi * motionRateHz) / (float)currentSampleRate;
                haloPhase   += (twoPi * haloRateHz)   / (float)currentSampleRate;
                if (motionPhase > twoPi) motionPhase -= twoPi;
                if (haloPhase   > twoPi) haloPhase   -= twoPi;

                float pL = wetBodyLeft.processSample  (0, wetToneLeft.processSample  (0, wetL[i] * brightnessGain));
                float pR = wetR ? wetBodyRight.processSample (0, wetToneRight.processSample (0, wetR[i] * brightnessGain)) : pL;

                const float ed = 1.0f - (0.32f * vs);
                const float edL = earlyL[i] * ed;
                const float edR = (earlyR ? earlyR[i] : edL) * ed;

                auto sw = [glueAmount](float s) { return juce::jmap (glueAmount, s, std::tanh (s * (1.0f + (glueAmount*2.4f)))); };

                pL = sw (pL * (gb + (mLfo * motionDepth)));
                pR = sw (pR * (gb - (mLfo * motionDepth)));
                pL += edL; pR += edR;

                if (wetR)
                {
                    const float dr = decorrelationDelay.popSample (0);
                    decorrelationDelay.pushSample (0, pR);
                    pR = (pR * (1.0f - (0.45f*widthNorm))) + (dr * 0.45f * widthNorm);
                    const float mid  = 0.5f*(pL+pR);
                    const float side = 0.5f*(pL-pR)*sideGain;
                    pL = (mid+side)*wetTrim; pR = (mid-side)*wetTrim;
                    const float halo = mid*haloBlend*hLfo;
                    pL += halo; pR += halo;
                }
                else { pL *= wetTrim * (1.0f + (0.5f*tailLift)); }

                const float df = 1.0f - (duckAmount * vs);
                pL *= df; pR *= df;

                outL[i] = (dryL[i]  * (1.0f-mixNorm)) + (pL * mixNorm);
                if (outR) outR[i] = (dryRight * (1.0f-mixNorm)) + (pR * mixNorm);
            }
        }
    };

    // -----------------------------------------------------------------------
    static Result compare (const char* scenarioName,
                           const NovaSpaceParameters& p,
                           double sr, int blockSize, int numBlocks,
                           bool stereo = true)
    {
        const int nCh = stereo ? 2 : 1;

        // Deterministic white-noise source
        juce::AudioBuffer<float> noise (nCh, blockSize);
        {
            juce::Random rng (98765);
            for (int ch = 0; ch < nCh; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    noise.setSample (ch, i, rng.nextFloat() * 2.f - 1.f);
        }

        OriginalState orig;
        orig.prepare (sr, blockSize, p);

        NovaSpaceDSP engine;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32)blockSize, (juce::uint32)nCh };
        engine.prepare (spec, p);
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
                    sumSqDiff  += (double)diff * diff;
                    ++totalSamples;
                }
        }

        Result r;
        r.scenario    = scenarioName;
        r.peakAbsDiff = peakAbsDiff;
        r.rmsAbsDiff  = totalSamples > 0
                          ? (float)std::sqrt (sumSqDiff / (double)totalSamples)
                          : 0.f;
        r.passed = (r.peakAbsDiff == 0.f);
        return r;
    }

    // -----------------------------------------------------------------------
    // Automation simulation: setParameters() called with a different value
    // each block to stress-test smoother state consistency.
    // -----------------------------------------------------------------------
    static Result compareAutomation (const char* scenarioName,
                                     double sr, int blockSize, int numBlocks,
                                     bool stereo = true)
    {
        const int nCh = stereo ? 2 : 1;

        juce::AudioBuffer<float> noise (nCh, blockSize);
        {
            juce::Random rng (11111);
            for (int ch = 0; ch < nCh; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    noise.setSample (ch, i, rng.nextFloat() * 2.f - 1.f);
        }

        NovaSpaceParameters initP;
        OriginalState orig;
        orig.prepare (sr, blockSize, initP);

        NovaSpaceDSP engine;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32)blockSize, (juce::uint32)nCh };
        engine.prepare (spec, initP);

        float peakAbsDiff = 0.f;
        double sumSqDiff  = 0.0;
        int    totalSamples = 0;

        for (int block = 0; block < numBlocks; ++block)
        {
            // Sweep params across their range each block
            const float t = (float)block / (float)juce::jmax (1, numBlocks - 1);
            NovaSpaceParameters p;
            p.space      = t * 10.0f;
            p.air        = (1.0f - t) * 10.0f;
            p.depth      = juce::jmap (t, 0.0f, 10.0f);
            p.mix        = juce::jmap (t, 0.0f, 100.0f);
            p.width      = t * 10.0f;
            p.mode       = block % 4;
            p.preDelayMs = t * 120.0f;
            p.decay      = 0.5f + t * 6.0f;
            p.damping    = t * 100.0f;
            p.early      = t * 100.0f;

            juce::AudioBuffer<float> bufA (nCh, blockSize);
            juce::AudioBuffer<float> bufB (nCh, blockSize);
            for (int ch = 0; ch < nCh; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    bufA.setSample (ch, i, noise.getSample (ch, i));
                    bufB.setSample (ch, i, noise.getSample (ch, i));
                }

            orig.processBlock (bufA, p);
            engine.setParameters (p);
            engine.process (bufB);

            for (int ch = 0; ch < nCh; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const float diff = std::abs (bufA.getSample (ch, i) - bufB.getSample (ch, i));
                    peakAbsDiff = juce::jmax (peakAbsDiff, diff);
                    sumSqDiff  += (double)diff * diff;
                    ++totalSamples;
                }
        }

        Result r;
        r.scenario    = scenarioName;
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
        auto P = [](float space, float air, float depth, float mix, float width,
                    int mode, float preMs, float decay, float damp, float early) -> NovaSpaceParameters
        {
            NovaSpaceParameters p;
            p.space = space; p.air = air; p.depth = depth; p.mix = mix; p.width = width;
            p.mode = mode; p.preDelayMs = preMs; p.decay = decay; p.damping = damp; p.early = early;
            return p;
        };

        // Default parameters
        auto def = P (1.8f, 3.2f, 2.6f, 16.0f, 3.8f, 0, 22.0f, 2.2f, 45.0f, 35.0f);

        struct Scenario { const char* name; NovaSpaceParameters params; double sr; int block; int blocks; bool stereo; };

        Scenario scenarios[] = {
            // Sample rates
            { "default_44100_stereo",  def, 44100.0, 512, 8, true  },
            { "default_48000_stereo",  def, 48000.0, 512, 8, true  },
            { "default_96000_stereo",  def, 96000.0, 512, 8, true  },

            // Mono
            { "default_44100_mono",    def, 44100.0, 512, 8, false },

            // Block sizes
            { "block_64",              def, 44100.0,  64, 16, true },
            { "block_2048",            def, 44100.0, 2048, 2, true },

            // All four modes
            { "mode_studio",           P(1.8f,3.2f,2.6f,16.f,3.8f, 0,22.f,2.2f,45.f,35.f), 44100.0, 512, 8, true },
            { "mode_arena",            P(1.8f,3.2f,2.6f,16.f,3.8f, 1,22.f,2.2f,45.f,35.f), 44100.0, 512, 8, true },
            { "mode_dream",            P(1.8f,3.2f,2.6f,16.f,3.8f, 2,22.f,2.2f,45.f,35.f), 44100.0, 512, 8, true },
            { "mode_vintage",          P(1.8f,3.2f,2.6f,16.f,3.8f, 3,22.f,2.2f,45.f,35.f), 44100.0, 512, 8, true },

            // Min parameters
            { "min_all",               P(0.f,0.f,0.f,0.f,0.f,0,0.f,0.5f,0.f,0.f), 44100.0, 512, 8, true },

            // Max parameters
            { "max_all",               P(10.f,10.f,10.f,100.f,10.f,1,120.f,6.5f,100.f,100.f), 44100.0, 512, 8, true },

            // Mix extremes
            { "mix_zero",              P(1.8f,3.2f,2.6f,  0.f,3.8f,0,22.f,2.2f,45.f,35.f), 44100.0, 512, 8, true },
            { "mix_full",              P(1.8f,3.2f,2.6f,100.f,3.8f,0,22.f,2.2f,45.f,35.f), 44100.0, 512, 8, true },

            // Pre-delay extremes
            { "predelay_zero",         P(1.8f,3.2f,2.6f,16.f,3.8f,0,  0.f,2.2f,45.f,35.f), 44100.0, 512, 8, true },
            { "predelay_max",          P(1.8f,3.2f,2.6f,16.f,3.8f,0,120.f,2.2f,45.f,35.f), 44100.0, 512, 8, true },

            // Width extremes
            { "width_zero",            P(1.8f,3.2f,2.6f,16.f, 0.f,0,22.f,2.2f,45.f,35.f), 44100.0, 512, 8, true },
            { "width_max",             P(1.8f,3.2f,2.6f,16.f,10.f,0,22.f,2.2f,45.f,35.f), 44100.0, 512, 8, true },

            // Decay extremes
            { "decay_min",             P(1.8f,3.2f,2.6f,16.f,3.8f,0,22.f,0.5f,45.f,35.f), 44100.0, 512, 8, true },
            { "decay_max",             P(1.8f,3.2f,2.6f,16.f,3.8f,0,22.f,6.5f,45.f,35.f), 44100.0, 512, 8, true },

            // Damping extremes
            { "damp_zero",             P(1.8f,3.2f,2.6f,16.f,3.8f,0,22.f,2.2f,  0.f,35.f), 44100.0, 512, 8, true },
            { "damp_max",              P(1.8f,3.2f,2.6f,16.f,3.8f,0,22.f,2.2f,100.f,35.f), 44100.0, 512, 8, true },

            // Early reflections extremes
            { "early_zero",            P(1.8f,3.2f,2.6f,16.f,3.8f,0,22.f,2.2f,45.f,  0.f), 44100.0, 512, 8, true },
            { "early_max",             P(1.8f,3.2f,2.6f,16.f,3.8f,0,22.f,2.2f,45.f,100.f), 44100.0, 512, 8, true },

            // Dream mode + max space (large room)
            { "dream_max_space",       P(10.f,8.f,5.f,60.f,8.f,2,40.f,6.0f,20.f,50.f), 44100.0, 512, 8, true },

            // Vintage mode + low space (small tight room)
            { "vintage_min_space",     P(0.5f,2.f,1.f,30.f,2.f,3, 5.f,1.0f,80.f,20.f), 44100.0, 512, 8, true },
        };

        for (auto& s : scenarios)
        {
            Result r = compare (s.name, s.params, s.sr, s.block, s.blocks, s.stereo);
            assert (r.passed && "NovaSpaceDSP regression FAILED — output differs from original");
            juce::ignoreUnused (r);
        }

        // Rapid automation test (all blocks, stereo, 44100)
        {
            Result r = compareAutomation ("automation_sweep_stereo_44100", 44100.0, 512, 16, true);
            assert (r.passed && "NovaSpaceDSP automation regression FAILED");
            juce::ignoreUnused (r);
        }

        // Automation test, mono
        {
            Result r = compareAutomation ("automation_sweep_mono_44100", 44100.0, 512, 16, false);
            assert (r.passed && "NovaSpaceDSP automation regression (mono) FAILED");
            juce::ignoreUnused (r);
        }
    }

private:
    NovaSpaceRegressionTest() = delete;
};
