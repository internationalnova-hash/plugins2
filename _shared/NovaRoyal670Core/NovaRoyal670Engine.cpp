#include "NovaRoyal670Engine.h"

namespace nova::royal670
{

void NovaRoyal670Engine::prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages) noexcept
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    preparedOversamplingStages = oversamplingStages;

    inputStage.prepare (spec, oversamplingStages);
    outputStage.prepare (spec, oversamplingStages);

    detectorStereoA.prepare (sampleRate);
    detectorStereoB.prepare (sampleRate);
    detectorMidSideA.prepare (sampleRate);
    detectorMidSideB.prepare (sampleRate);
    stereoCrossfader.prepare (sampleRate);

    vuA.prepare (sampleRate);
    vuB.prepare (sampleRate);

    inputGainSmoothed.reset (sampleRate, kSmoothSeconds);
    outputGainSmoothed.reset (sampleRate, kSmoothSeconds);
    mixSmoothed.reset (sampleRate, kSmoothSeconds);
    outputTrimGainSmoothed.reset (sampleRate, kSmoothSeconds);
    linkSmoothed.reset (sampleRate, kSmoothSeconds);
    analogNoiseSmoothed.reset (sampleRate, kSmoothSeconds);
    bypassSmoothed.reset (sampleRate, kBypassSmoothSeconds);
    autoGainDbSmoothed.reset (sampleRate, kAutoGainSmoothSeconds);

    inputGainSmoothed.setCurrentAndTargetValue (1.0f);
    outputGainSmoothed.setCurrentAndTargetValue (1.0f);
    mixSmoothed.setCurrentAndTargetValue (1.0f);
    outputTrimGainSmoothed.setCurrentAndTargetValue (1.0f);
    linkSmoothed.setCurrentAndTargetValue (1.0f);
    analogNoiseSmoothed.setCurrentAndTargetValue (0.0f);
    bypassSmoothed.setCurrentAndTargetValue (1.0f); // 1 = active, 0 = bypassed
    autoGainDbSmoothed.setCurrentAndTargetValue (0.0f);

    dryBuffer.setSize (2, (int) spec.maximumBlockSize, false, false, true);

    // Snap the block-rate smoothers to their current targets so plugin
    // load doesn't fade in from zero drive.
    hrmDriveSmoothed01 = hrmDriveTarget01;
    tubeSaturationSmoothed01 = tubeSaturationTarget01;
    transformerSmoothed01 = transformerTarget01;

    reset();
}

void NovaRoyal670Engine::reset() noexcept
{
    inputStage.reset();
    outputStage.reset();
    detectorStereoA.reset();
    detectorStereoB.reset();
    detectorMidSideA.reset();
    detectorMidSideB.reset();
    stereoCrossfader.reset();
    vuA.reset();
    vuB.reset();
    lastGainReductionDb = 0.0f;
}

void NovaRoyal670Engine::setParams (float inputDb, float hrmDrivePercent,
                                     float newAcThresholdDb, float newDcThresholdDb, int newTimeConstantMode,
                                     float outputDb, float mixPercent,
                                     bool midSideMode, float linkPercent,
                                     float newSidechainHpfHz, bool newSidechainListen,
                                     float weightPercent, float tubeSaturationPercent, float transformerPercent,
                                     float outputTrimDb, bool autoGain,
                                     float analogNoisePercent, bool bypassed) noexcept
{
    inputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (inputDb));
    outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputDb));
    mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, mixPercent / 100.0f));
    outputTrimGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputTrimDb));
    linkSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, linkPercent / 100.0f));
    analogNoiseSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, analogNoisePercent / 100.0f));
    bypassSmoothed.setTargetValue (bypassed ? 0.0f : 1.0f);

    hrmDriveTarget01 = juce::jlimit (0.0f, 1.0f, hrmDrivePercent / 100.0f);
    tubeSaturationTarget01 = juce::jlimit (0.0f, 1.0f, tubeSaturationPercent / 100.0f);
    transformerTarget01 = juce::jlimit (0.0f, 1.0f, transformerPercent / 100.0f);
    weightMacro = NovaWeightMacro::fromWeight (juce::jlimit (0.0f, 1.0f, weightPercent / 100.0f));

    acThresholdDb = newAcThresholdDb;
    dcThresholdDb = newDcThresholdDb;
    timeConstantMode = newTimeConstantMode;
    sidechainHpfHz = newSidechainHpfHz;
    sidechainListen = newSidechainListen;
    autoGainEnabled = autoGain;

    detectorStereoA.setParams (acThresholdDb, dcThresholdDb, timeConstantMode, sidechainHpfHz);
    detectorStereoB.setParams (acThresholdDb, dcThresholdDb, timeConstantMode, sidechainHpfHz);
    detectorMidSideA.setParams (acThresholdDb, dcThresholdDb, timeConstantMode, sidechainHpfHz);
    detectorMidSideB.setParams (acThresholdDb, dcThresholdDb, timeConstantMode, sidechainHpfHz);

    stereoCrossfader.setMode (midSideMode ? NovaStereoModeCrossfader::Mode::MidSide
                                           : NovaStereoModeCrossfader::Mode::Stereo);
}

void NovaRoyal670Engine::runDomainDetectors (float l, float r, NovaStereoModeCrossfader::Mode mode, float linkAmount,
                                              float& outL, float& outR, float& gainReductionDbOut) noexcept
{
    float chA, chB;
    NovaStereoModeCrossfader::encode (l, r, mode, chA, chB);

    auto& detA = detectorFor (mode, true);
    auto& detB = detectorFor (mode, false);

    const float magA = detA.computeSidechainMagnitude (chA);
    const float magB = detB.computeSidechainMagnitude (chB);
    const float combined = juce::jmax (magA, magB);

    const float grA = detA.applyMagnitude (juce::jmap (linkAmount, magA, combined));
    const float grB = detB.applyMagnitude (juce::jmap (linkAmount, magB, combined));

    if (sidechainListen)
    {
        // Audition exactly what the detector hears (post sidechain-HPF),
        // bypassing gain reduction and everything downstream of it.
        NovaStereoModeCrossfader::decode (detA.getLastHpfedSignal(), detB.getLastHpfedSignal(), mode, outL, outR);
        gainReductionDbOut = juce::jmax (grA, grB);
        return;
    }

    const float gainA = juce::Decibels::decibelsToGain (-grA);
    const float gainB = juce::Decibels::decibelsToGain (-grB);

    const float processedChA = chA * gainA;
    const float processedChB = chB * gainB;

    NovaStereoModeCrossfader::decode (processedChA, processedChB, mode, outL, outR);
    gainReductionDbOut = juce::jmax (grA, grB);
}

void NovaRoyal670Engine::processBlock (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    // Snapshot the dry signal for the bypass crossfade, before any
    // processing touches the buffer.
    dryBuffer.copyFrom (0, 0, L, numSamples);
    dryBuffer.copyFrom (1, 0, R, numSamples);

    // Block-rate smoothing for the tube/transformer drive parameters -
    // see the header comment on hrmDriveSmoothed01 etc.
    const float blockSeconds = (float) numSamples / (float) juce::jmax (1.0, sampleRate);
    const float blockSmoothCoeff = std::exp (-blockSeconds / kBlockSmoothSeconds);
    hrmDriveSmoothed01 += (hrmDriveTarget01 - hrmDriveSmoothed01) * (1.0f - blockSmoothCoeff);
    tubeSaturationSmoothed01 += (tubeSaturationTarget01 - tubeSaturationSmoothed01) * (1.0f - blockSmoothCoeff);
    transformerSmoothed01 += (transformerTarget01 - transformerSmoothed01) * (1.0f - blockSmoothCoeff);

    // Input gain, then the input-position tube stage (oversampled,
    // driven by HRM Drive) - both run at block granularity ahead of the
    // per-sample gain-control loop below.
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = inputGainSmoothed.getNextValue();
        L[i] *= g;
        R[i] *= g;
    }

    inputStage.setDrive (hrmDriveSmoothed01);
    inputStage.process (buffer);

    // Variable-mu gain-control core: click-free across Stereo<->M/S mode
    // switches by running both domains' detectors during a transition.
    float grSum = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float linkAmount = linkSmoothed.getNextValue();
        const bool wasTransitioning = stereoCrossfader.isTransitioning();
        const auto activeMode = stereoCrossfader.getActiveMode();

        float activeOutL, activeOutR, activeGrDb;
        runDomainDetectors (L[i], R[i], activeMode, linkAmount, activeOutL, activeOutR, activeGrDb);

        float finalOutL = activeOutL;
        float finalOutR = activeOutR;
        float grForMeter = activeGrDb;

        if (wasTransitioning)
        {
            const auto previousMode = stereoCrossfader.getPreviousMode();
            float prevOutL, prevOutR, prevGrDb;
            runDomainDetectors (L[i], R[i], previousMode, linkAmount, prevOutL, prevOutR, prevGrDb);

            const float blend = stereoCrossfader.advanceSample(); // 0 = previous, 1 = active
            finalOutL = prevOutL * (1.0f - blend) + activeOutL * blend;
            finalOutR = prevOutR * (1.0f - blend) + activeOutR * blend;
            grForMeter = juce::jmax (activeGrDb, prevGrDb);
        }
        else
        {
            stereoCrossfader.advanceSample();
        }

        L[i] = finalOutL;
        R[i] = finalOutR;
        grSum += grForMeter;
    }

    lastGainReductionDb = numSamples > 0 ? grSum / (float) numSamples : 0.0f;

    if (! sidechainListen)
    {
        // Output tube stage (Tube Saturation, additionally boosted by the
        // just-measured gain reduction - the concrete GR/harmonics
        // coupling point) + output transformer (Transformer + Weight's LF
        // contribution), one oversampled block.
        const float grCoupledDrive = tubeSaturationSmoothed01
            * (1.0f + weightMacro.tubeGrCoupling * juce::jlimit (0.0f, 1.0f, lastGainReductionDb / 12.0f));
        outputStage.setTubeDrive (grCoupledDrive);
        outputStage.setTubeEvenHarmonicBias (weightMacro.tubeEvenHarmonicBias);
        outputStage.setTransformerDrive (transformerSmoothed01);
        outputStage.setTransformerLfDrive (weightMacro.transformerLfDrive);
        outputStage.process (buffer);
    }

    // Analog Noise, Output gain, Output Trim, Auto Gain, Mix, Bypass -
    // all per-sample so every one of these stays click-free under
    // automation.
    for (int i = 0; i < numSamples; ++i)
    {
        const float noiseAmount = analogNoiseSmoothed.getNextValue();
        const float outGain = outputGainSmoothed.getNextValue();
        const float trimGain = outputTrimGainSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();
        const float bypassMix = bypassSmoothed.getNextValue();

        // Auto Gain: smoothly compensate output level against the
        // currently-measured gain reduction, so overall loudness stays
        // roughly consistent as GR varies - matches the mockup's simple
        // on/off toggle rather than an amount knob.
        autoGainDbSmoothed.setTargetValue (autoGainEnabled ? lastGainReductionDb : 0.0f);
        const float autoGainDb = autoGainDbSmoothed.getNextValue();
        const float autoGainLinear = juce::Decibels::decibelsToGain (autoGainDb);

        float wetL = L[i] * outGain * trimGain * autoGainLinear;
        float wetR = R[i] * outGain * trimGain * autoGainLinear;

        if (noiseAmount > 0.0f)
        {
            // Very low noise floor even at 100% - a subtle analog touch,
            // never loud enough to read as hiss on typical material.
            const float noiseGain = juce::Decibels::decibelsToGain (-70.0f + 30.0f * (1.0f - noiseAmount));
            wetL += (noiseRandom.nextFloat() * 2.0f - 1.0f) * noiseAmount * noiseGain;
            wetR += (noiseRandom.nextFloat() * 2.0f - 1.0f) * noiseAmount * noiseGain;
        }

        const float dryL = dryBuffer.getSample (0, i);
        const float dryR = dryBuffer.getSample (1, i);

        // Equal-power MIX crossfade (matches the catalog's established
        // dry/wet idiom, e.g. NovaDoublerEngine).
        const float mixAngle = mix * juce::MathConstants<float>::halfPi;
        const float dryGain = std::cos (mixAngle);
        const float wetGain = std::sin (mixAngle);
        const float mixedL = dryL * dryGain + wetL * wetGain;
        const float mixedR = dryR * dryGain + wetR * wetGain;

        // Bypass ramp: crossfade between fully dry and the MIX-blended
        // signal, so toggling bypass never jumps the MIX setting itself.
        L[i] = dryL * (1.0f - bypassMix) + mixedL * bypassMix;
        R[i] = dryR * (1.0f - bypassMix) + mixedR * bypassMix;
    }

    vuA.process (L, numSamples);
    vuB.process (R, numSamples);
}

int NovaRoyal670Engine::getLatencySamples() const noexcept
{
    return inputStage.getLatencySamples() + outputStage.getLatencySamples();
}

} // namespace nova::royal670
