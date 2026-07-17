#include "NovaSpaceDSP.h"
#include <cmath>

NovaSpaceDSP::NovaSpaceDSP() = default;

void NovaSpaceDSP::prepare (const juce::dsp::ProcessSpec& spec,
                            const NovaSpaceParameters& initial)
{
    currentSampleRate = spec.sampleRate;

    // All filters prepared with channels=1 — original uses mono ProcessSpec
    juce::dsp::ProcessSpec monoSpec { spec.sampleRate, spec.maximumBlockSize, 1 };

    preDelayLeft.prepare (monoSpec);
    preDelayRight.prepare (monoSpec);
    decorrelationDelay.prepare (monoSpec);
    preDelayLeft.reset();
    preDelayRight.reset();
    decorrelationDelay.reset();

    wetToneLeft.prepare (monoSpec);   wetToneLeft.reset();
    wetToneRight.prepare (monoSpec);  wetToneRight.reset();
    wetBodyLeft.prepare (monoSpec);   wetBodyLeft.reset();
    wetBodyRight.prepare (monoSpec);  wetBodyRight.reset();
    earlyToneLeft.prepare (monoSpec); earlyToneLeft.reset();
    earlyToneRight.prepare (monoSpec);earlyToneRight.reset();
    earlyBodyLeft.prepare (monoSpec); earlyBodyLeft.reset();
    earlyBodyRight.prepare (monoSpec);earlyBodyRight.reset();

    wetToneLeft.setType  (juce::dsp::StateVariableTPTFilterType::lowpass);
    wetToneRight.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    wetBodyLeft.setType  (juce::dsp::StateVariableTPTFilterType::highpass);
    wetBodyRight.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    earlyToneLeft.setType  (juce::dsp::StateVariableTPTFilterType::lowpass);
    earlyToneRight.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    earlyBodyLeft.setType  (juce::dsp::StateVariableTPTFilterType::highpass);
    earlyBodyRight.setType (juce::dsp::StateVariableTPTFilterType::highpass);

    dryBuffer.setSize   (2, (int)spec.maximumBlockSize);
    wetBuffer.setSize   (2, (int)spec.maximumBlockSize);
    earlyBuffer.setSize (2, (int)spec.maximumBlockSize);

    earlyTapBufferSize = juce::jmax (256, (int)(spec.sampleRate * 0.60));
    earlyTapBufferLeft.assign  ((size_t)earlyTapBufferSize, 0.0f);
    earlyTapBufferRight.assign ((size_t)earlyTapBufferSize, 0.0f);
    earlyTapWriteIndex     = 0;
    earlyDiffuseStateLeft  = 0.0f;
    earlyDiffuseStateRight = 0.0f;

    for (auto* sv : { &smoothedSpace, &smoothedAir, &smoothedDepth, &smoothedMix, &smoothedWidth })
        sv->reset (spec.sampleRate, 0.20);

    // Seed smoothers from the provided initial parameters
    seedSmoothedValues (initial);

    reverb.reset();
    motionPhase = 0.0f;
    haloPhase   = 1.7f;
}

void NovaSpaceDSP::reset() noexcept
{
    preDelayLeft.reset();
    preDelayRight.reset();
    decorrelationDelay.reset();
    wetToneLeft.reset();   wetToneRight.reset();
    wetBodyLeft.reset();   wetBodyRight.reset();
    earlyToneLeft.reset(); earlyToneRight.reset();
    earlyBodyLeft.reset(); earlyBodyRight.reset();
    reverb.reset();

    std::fill (earlyTapBufferLeft.begin(),  earlyTapBufferLeft.end(),  0.0f);
    std::fill (earlyTapBufferRight.begin(), earlyTapBufferRight.end(), 0.0f);
    earlyTapWriteIndex     = 0;
    earlyDiffuseStateLeft  = 0.0f;
    earlyDiffuseStateRight = 0.0f;
    motionPhase = 0.0f;
    haloPhase   = 1.7f;
}

void NovaSpaceDSP::seedSmoothedValues (const NovaSpaceParameters& p) noexcept  // private
{
    smoothedSpace.setCurrentAndTargetValue (p.space);
    smoothedAir.setCurrentAndTargetValue   (p.air);
    smoothedDepth.setCurrentAndTargetValue (p.depth);
    smoothedMix.setCurrentAndTargetValue   (p.mix);
    smoothedWidth.setCurrentAndTargetValue (p.width);
}

void NovaSpaceDSP::setParameters (const NovaSpaceParameters& p) noexcept
{
    params = p;
}

float NovaSpaceDSP::readEarlyTap (const std::vector<float>& source, float delaySamples) const noexcept
{
    if (source.empty() || earlyTapBufferSize <= 1)
        return 0.0f;

    const float wrapped = std::fmod (delaySamples + (float)earlyTapBufferSize,
                                     (float)earlyTapBufferSize);
    const int   offsetA = (int)wrapped;
    const int   offsetB = (offsetA + 1) % earlyTapBufferSize;
    const float frac    = wrapped - (float)offsetA;

    const int readA = (earlyTapWriteIndex - offsetA + earlyTapBufferSize) % earlyTapBufferSize;
    const int readB = (earlyTapWriteIndex - offsetB + earlyTapBufferSize) % earlyTapBufferSize;

    const float a = source[(size_t)readA];
    const float b = source[(size_t)readB];
    return a + ((b - a) * frac);
}

void NovaSpaceDSP::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    // Advance smoothers; use the block-end value for all DSP this block
    smoothedSpace.setTargetValue (params.space);
    smoothedAir.setTargetValue   (params.air);
    smoothedDepth.setTargetValue (params.depth);
    smoothedMix.setTargetValue   (params.mix);
    smoothedWidth.setTargetValue (params.width);

    const float space = smoothedSpace.skip (numSamples);
    const float air   = smoothedAir.skip   (numSamples);
    const float depth = smoothedDepth.skip (numSamples);
    const float mix   = smoothedMix.skip   (numSamples);
    const float width = smoothedWidth.skip (numSamples);

    // Un-smoothed params (no smoother in original for these)
    const float preDelayMsBase  = params.preDelayMs;
    const float decaySeconds    = params.decay;
    const float dampingControl  = params.damping / 100.0f;
    const float earlyControl    = params.early   / 100.0f;
    const int   modeIndex       = params.mode;

    // Normalise controls
    const float spaceNorm  = clamp01 (space / 10.0f);
    const float airNorm    = clamp01 (air   / 10.0f);
    const float depthNorm  = clamp01 (depth / 10.0f);
    const float mixNorm    = clamp01 (mix   / 100.0f);
    const float widthNorm  = clamp01 (width / 10.0f);
    const float decayNorm  = clamp01 ((decaySeconds - 0.5f) / 6.0f);
    const float airCurve   = 1.0f - ((1.0f - airNorm) * (1.0f - airNorm));
    const float airSafety  = airNorm * airNorm;
    const float attackSoftness = juce::jlimit (0.0f, 0.35f,
                                               0.04f + (0.16f * depthNorm) + (0.10f * spaceNorm));

    // Compute derived parameters (identical arithmetic to original)
    float roomSize = clamp01 (0.21f + (0.43f * spaceNorm) + (0.22f * decayNorm));
    float damping  = juce::jlimit (0.16f, 0.95f,
                                   (0.76f - (0.26f * airCurve))
                                   + (0.12f * spaceNorm)
                                   + (0.18f * dampingControl)
                                   + (0.08f * airSafety));

    float preDelayMs  = juce::jlimit (0.0f, 120.0f, preDelayMsBase);

    float earlyAmount = juce::jlimit (0.08f, 0.35f,
                                      (0.24f + (0.08f * earlyControl))
                                      - (0.16f * depthNorm)
                                      + (0.04f * spaceNorm));
    float earlySpreadMs = juce::jlimit (5.0f, 40.0f,
                                        10.0f + (spaceNorm * 20.0f) + (depthNorm * 10.0f));
    const float earlyDiffusion = juce::jlimit (0.35f, 0.90f,
                                               0.52f + (spaceNorm * 0.24f) + (depthNorm * 0.12f));
    const float earlyWidth    = juce::jmap (widthNorm, 0.20f, 0.68f);
    const float earlyTailFeed = juce::jlimit (0.05f, 0.22f,
                                              0.10f + (spaceNorm * 0.07f) - (depthNorm * 0.03f));

    float wetTrim        = juce::jlimit (0.65f, 1.08f,
                                         juce::jmap (mixNorm, 0.0f, 1.0f, 1.0f, 0.93f)
                                         + (0.06f * decayNorm)
                                         + (0.02f * spaceNorm));
    float sideGain       = juce::jmap (widthNorm, 0.72f, 1.78f);
    float decorrelationMs = juce::jmap (widthNorm, 0.0f, 12.0f);
    float toneCutoffHz   = juce::jlimit (3200.0f, 16000.0f,
                                         4600.0f + (airCurve * 9000.0f) - (spaceNorm * 1600.0f));
    float lowCutHz       = juce::jlimit (90.0f, 420.0f,
                                         120.0f + (spaceNorm * 120.0f) + (depthNorm * 55.0f));
    const float earlyHighCutHz = juce::jlimit (6000.0f, 10000.0f,
                                               6200.0f + (airNorm * 3600.0f));
    const float earlyLowCutHz  = juce::jlimit (150.0f, 220.0f,
                                               150.0f + (1.0f - airNorm) * 70.0f);
    float brightnessGain  = juce::jlimit (0.94f, 1.08f,
                                          0.97f + (0.08f * airCurve) - (0.03f * airSafety));
    float internalWidth   = juce::jlimit (0.25f, 1.0f, 0.50f + (0.38f * widthNorm));
    float duckAmount      = juce::jlimit (0.05f, 0.30f, 0.18f + (0.08f * (1.0f - depthNorm)));
    float glueAmount      = juce::jlimit (0.02f, 0.14f,
                                          0.03f + (0.03f * spaceNorm) + (0.03f * depthNorm));
    float haloBlend       = juce::jlimit (0.01f, 0.16f,
                                          0.03f + (0.05f * widthNorm) + (0.03f * airNorm));
    float motionDepth     = juce::jlimit (0.004f, 0.08f,
                                          0.008f + (0.020f * spaceNorm) + (0.012f * depthNorm));
    float tailLift        = juce::jlimit (0.01f, 0.12f,
                                          0.02f + (0.04f * airNorm) + (0.03f * decayNorm));

    // Mode modifiers (verbatim from original switch)
    switch (modeIndex)
    {
        case 1: // Arena
            roomSize       = clamp01 (roomSize + 0.09f);
            earlyAmount   *= 0.80f;
            earlySpreadMs += 4.0f;
            sideGain      += 0.16f;
            decorrelationMs += 2.1f;
            wetTrim       += 0.02f;
            toneCutoffHz   = juce::jmin (toneCutoffHz * 1.02f, 13200.0f);
            lowCutHz      += 25.0f;
            brightnessGain *= 1.01f;
            duckAmount     = 0.17f;
            glueAmount     = 0.08f;
            haloBlend     += 0.025f;
            motionDepth   += 0.010f;
            tailLift      += 0.020f;
            internalWidth  = juce::jlimit (0.25f, 1.0f, internalWidth + 0.10f);
            break;
        case 2: // Dream
            roomSize       = clamp01 (roomSize + 0.12f);
            earlyAmount   *= 0.46f;
            earlySpreadMs += 6.0f;
            damping        = juce::jlimit (0.16f, 0.95f, damping + 0.10f);
            sideGain      += 0.18f;
            decorrelationMs += 4.0f;
            wetTrim       += 0.02f;
            toneCutoffHz  *= 0.84f;
            lowCutHz      += 42.0f;
            brightnessGain *= 0.95f;
            duckAmount     = 0.14f;
            glueAmount     = 0.10f;
            haloBlend     += 0.040f;
            motionDepth   += 0.018f;
            tailLift      += 0.035f;
            internalWidth  = juce::jlimit (0.25f, 1.0f, internalWidth + 0.12f);
            break;
        case 3: // Vintage
            roomSize       = clamp01 (roomSize - 0.01f);
            earlyAmount    = juce::jlimit (0.10f, 0.60f, earlyAmount + 0.05f);
            earlySpreadMs  = juce::jmax  (5.0f, earlySpreadMs - 4.0f);
            damping        = juce::jlimit (0.16f, 0.95f, damping + 0.18f);
            sideGain      *= 0.72f;
            decorrelationMs *= 0.58f;
            toneCutoffHz  *= 0.70f;
            lowCutHz      += 10.0f;
            brightnessGain *= 0.91f;
            wetTrim       *= 0.96f;
            duckAmount     = 0.12f;
            glueAmount     = 0.07f;
            haloBlend     *= 0.65f;
            motionDepth   *= 0.55f;
            tailLift      *= 0.55f;
            internalWidth *= 0.74f;
            break;
        default: // Studio
            roomSize      *= 0.74f;
            earlyAmount    = juce::jlimit (0.12f, 0.58f, earlyAmount + 0.08f);
            earlySpreadMs  = juce::jmax  (5.0f, earlySpreadMs - 2.5f);
            wetTrim       *= 0.90f;
            sideGain      *= 0.74f;
            decorrelationMs *= 0.58f;
            toneCutoffHz   = juce::jmin (toneCutoffHz, 9000.0f);
            lowCutHz      += 60.0f;
            brightnessGain *= 0.97f;
            duckAmount     = 0.30f;
            glueAmount     = 0.05f;
            haloBlend     *= 0.55f;
            motionDepth   *= 0.50f;
            tailLift      *= 0.45f;
            internalWidth *= 0.78f;
            break;
    }

    preDelayMs    = juce::jlimit (0.0f, 40.0f, preDelayMs);
    earlySpreadMs = juce::jlimit (5.0f, 40.0f, earlySpreadMs);

    // Configure DSP units
    juce::Reverb::Parameters rp;
    rp.roomSize   = roomSize;
    rp.damping    = damping;
    rp.wetLevel   = 1.0f;
    rp.dryLevel   = 0.0f;
    rp.width      = internalWidth;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);

    wetToneLeft.setCutoffFrequency  (toneCutoffHz);
    wetToneRight.setCutoffFrequency (toneCutoffHz);
    wetBodyLeft.setCutoffFrequency  (lowCutHz);
    wetBodyRight.setCutoffFrequency (lowCutHz);
    earlyToneLeft.setCutoffFrequency  (earlyHighCutHz);
    earlyToneRight.setCutoffFrequency (earlyHighCutHz);
    earlyBodyLeft.setCutoffFrequency  (earlyLowCutHz);
    earlyBodyRight.setCutoffFrequency (earlyLowCutHz);

    const float preDelaySamples = juce::jlimit (0.0f,
                                                0.4f * (float)currentSampleRate,
                                                (float)currentSampleRate * preDelayMs * 0.001f);
    preDelayLeft.setDelay  (preDelaySamples);
    preDelayRight.setDelay (preDelaySamples);

    const float decorSamples = juce::jlimit (0.0f, 2048.0f,
                                             (float)currentSampleRate * decorrelationMs * 0.001f);
    decorrelationDelay.setDelay (decorSamples);

    // Resize working buffers (no-alloc path when size is already correct)
    dryBuffer.setSize   (juce::jmax (1, numChannels), numSamples, false, false, true);
    wetBuffer.setSize   (juce::jmax (1, numChannels), numSamples, false, false, true);
    earlyBuffer.setSize (juce::jmax (1, numChannels), numSamples, false, false, true);

    dryBuffer.makeCopyOf (buffer, true);
    wetBuffer.makeCopyOf (buffer, true);
    earlyBuffer.clear();

    auto* wetL  = wetBuffer.getWritePointer (0);
    auto* wetR  = wetBuffer.getNumChannels() > 1 ? wetBuffer.getWritePointer (1) : nullptr;
    auto* earlyL = earlyBuffer.getWritePointer (0);
    auto* earlyR = earlyBuffer.getNumChannels() > 1 ? earlyBuffer.getWritePointer (1) : nullptr;

    // --- Per-sample loop 1: pre-delay + early reflections → reverb feed ---
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float inputL = wetL[sample];
        const float inputR = wetR != nullptr ? wetR[sample] : inputL;

        const float delayedL = preDelayLeft.popSample  (0);
        const float delayedR = preDelayRight.popSample (0);
        preDelayLeft.pushSample  (0, inputL);
        preDelayRight.pushSample (0, inputR);

        earlyTapBufferLeft [(size_t)earlyTapWriteIndex] = delayedL;
        earlyTapBufferRight[(size_t)earlyTapWriteIndex] = delayedR;

        const float spreadSamples  = (float)currentSampleRate * earlySpreadMs * 0.001f;
        const float baseTapSamples = 0.001f * (float)currentSampleRate * juce::jmap (spaceNorm, 4.5f, 9.5f);
        const float densitySkew    = juce::jmap (spaceNorm, 0.85f, 0.55f);

        float earlyClusterL = 0.0f;
        float earlyClusterR = 0.0f;

        for (int tap = 0; tap < 6; ++tap)
        {
            const float tapNorm   = (float)tap / 5.0f;
            const float tapPos    = std::pow (tapNorm, densitySkew);
            const float tapDelay  = baseTapSamples + (tapPos * spreadSamples);
            const float tapWeight = (0.22f + (0.58f * (1.0f - tapNorm))) * (1.0f - (tapNorm * 0.12f));

            earlyClusterL += readEarlyTap (earlyTapBufferLeft,  tapDelay) * tapWeight;
            earlyClusterR += readEarlyTap (earlyTapBufferRight, tapDelay) * tapWeight;
        }

        const float diffuseMix = earlyDiffusion;
        earlyDiffuseStateLeft  += (earlyClusterL - earlyDiffuseStateLeft)  * (0.32f + (0.28f * diffuseMix));
        earlyDiffuseStateRight += (earlyClusterR - earlyDiffuseStateRight) * (0.32f + (0.28f * diffuseMix));

        float erL = juce::jmap (diffuseMix, earlyClusterL, earlyDiffuseStateLeft);
        float erR = juce::jmap (diffuseMix, earlyClusterR, earlyDiffuseStateRight);

        erL = earlyBodyLeft.processSample  (0, earlyToneLeft.processSample  (0, erL));
        erR = earlyBodyRight.processSample (0, earlyToneRight.processSample (0, erR));

        const float erMid  = 0.5f * (erL + erR);
        const float erSide = 0.5f * (erL - erR) * (0.65f + earlyWidth);
        erL = (erMid + erSide) * earlyAmount;
        erR = (erMid - erSide) * earlyAmount;

        earlyL[sample] = erL;
        if (earlyR != nullptr)
            earlyR[sample] = erR;

        const float bloomL = juce::jmap (attackSoftness, delayedL,
                                         (delayedL * 0.72f) + (inputL * 0.28f));
        wetL[sample] = bloomL + (erL * earlyTailFeed);

        if (wetR != nullptr)
        {
            const float bloomR = juce::jmap (attackSoftness, delayedR,
                                             (delayedR * 0.72f) + (inputR * 0.28f));
            wetR[sample] = bloomR + (erR * earlyTailFeed);
        }

        earlyTapWriteIndex = (earlyTapWriteIndex + 1) % earlyTapBufferSize;
    }

    // Reverb — full block
    if (wetR != nullptr)
        reverb.processStereo (wetL, wetR, numSamples);
    else
        reverb.processMono (wetL, numSamples);

    // Output pointers
    auto*       outL = buffer.getWritePointer (0);
    auto*       outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const auto* dryL = dryBuffer.getReadPointer (0);
    const auto* dryR = dryBuffer.getNumChannels() > 1 ? dryBuffer.getReadPointer (1) : nullptr;

    const float twoPi = juce::MathConstants<float>::twoPi;
    const float motionRateHz = juce::jmap (depthNorm, 0.11f, 0.33f);
    const float haloRateHz   = juce::jmap (spaceNorm, 0.07f, 0.19f);

    // --- Per-sample loop 2: post-reverb tone, motion, width, wet/dry blend ---
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float dryRight    = dryR != nullptr ? dryR[sample] : dryL[sample];
        const float vocalEnergy = 0.5f * (std::abs (dryL[sample]) + std::abs (dryRight));
        const float vocalSense  = clamp01 (std::sqrt (vocalEnergy * 3.2f));
        const float gapBloom    = 1.0f + ((1.0f - vocalSense) * tailLift);
        const float motionLfo   = std::sin (motionPhase);
        const float haloLfo     = 0.5f + (0.5f * std::sin (haloPhase));

        motionPhase += (twoPi * motionRateHz) / (float)currentSampleRate;
        haloPhase   += (twoPi * haloRateHz)   / (float)currentSampleRate;

        if (motionPhase > twoPi) motionPhase -= twoPi;
        if (haloPhase   > twoPi) haloPhase   -= twoPi;

        float processedL = wetBodyLeft.processSample (0,
                               wetToneLeft.processSample (0, wetL[sample] * brightnessGain));
        float processedR = wetR != nullptr
            ? wetBodyRight.processSample (0,
                  wetToneRight.processSample (0, wetR[sample] * brightnessGain))
            : processedL;

        const float earlyDynamic  = 1.0f - (0.32f * vocalSense);
        const float earlyDirectL  = earlyL[sample] * earlyDynamic;
        const float earlyDirectR  = (earlyR != nullptr ? earlyR[sample] : earlyDirectL) * earlyDynamic;

        // Wet glue saturator (inline lambda equivalent)
        auto smoothWetSample = [glueAmount] (float s) -> float
        {
            const float saturated = std::tanh (s * (1.0f + (glueAmount * 2.4f)));
            return juce::jmap (glueAmount, s, saturated);
        };

        processedL = smoothWetSample (processedL * (gapBloom + (motionLfo * motionDepth)));
        processedR = smoothWetSample (processedR * (gapBloom - (motionLfo * motionDepth)));

        processedL += earlyDirectL;
        processedR += earlyDirectR;

        if (wetR != nullptr)
        {
            const float delayedRight = decorrelationDelay.popSample (0);
            decorrelationDelay.pushSample (0, processedR);
            processedR = (processedR * (1.0f - (0.45f * widthNorm))) + (delayedRight * 0.45f * widthNorm);

            const float mid  = 0.5f * (processedL + processedR);
            const float side = 0.5f * (processedL - processedR) * sideGain;
            processedL = (mid + side) * wetTrim;
            processedR = (mid - side) * wetTrim;

            const float halo = mid * haloBlend * haloLfo;
            processedL += halo;
            processedR += halo;
        }
        else
        {
            processedL *= wetTrim * (1.0f + (0.5f * tailLift));
        }

        const float duckFactor = 1.0f - (duckAmount * vocalSense);
        processedL *= duckFactor;
        processedR *= duckFactor;

        outL[sample] = (dryL[sample] * (1.0f - mixNorm)) + (processedL * mixNorm);

        if (outR != nullptr)
            outR[sample] = (dryRight * (1.0f - mixNorm)) + (processedR * mixNorm);
    }
}
