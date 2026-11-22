#include "NovaVelvetEngine.h"

namespace nova::velvet
{

void NovaVelvetEngine::prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages) noexcept
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    preparedOversamplingStages = oversamplingStages;

    inputStage.prepare (spec, oversamplingStages);
    outputStage.prepare (spec, oversamplingStages);

    detectorL.prepare (sampleRate);
    detectorR.prepare (sampleRate);
    cellL.prepare (sampleRate);
    cellR.prepare (sampleRate);

    inputLevelEnvelope.prepare (sampleRate, NovaVelvetCalibration::kLevelEnvelopeAttackSeconds, NovaVelvetCalibration::kLevelEnvelopeReleaseSeconds);
    outputLevelEnvelope.prepare (sampleRate, NovaVelvetCalibration::kLevelEnvelopeAttackSeconds, NovaVelvetCalibration::kLevelEnvelopeReleaseSeconds);

    vuGr.prepare (sampleRate);
    vuOutput.prepare (sampleRate);
    vuInput.prepare (sampleRate);

    mixSmoothed.reset (sampleRate, NovaVelvetCalibration::kParamSmoothSeconds);
    stereoLinkSmoothed.reset (sampleRate, NovaVelvetCalibration::kParamSmoothSeconds);
    bypassSmoothed.reset (sampleRate, NovaVelvetCalibration::kBypassSmoothSeconds);
    outputTrimGainSmoothed.reset (sampleRate, NovaVelvetCalibration::kParamSmoothSeconds);

    mixSmoothed.setCurrentAndTargetValue (1.0f);
    stereoLinkSmoothed.setCurrentAndTargetValue (1.0f);
    bypassSmoothed.setCurrentAndTargetValue (1.0f); // 1 = active, 0 = bypassed
    outputTrimGainSmoothed.setCurrentAndTargetValue (1.0f);

    // Snap block-rate smoothers to their targets so plugin load doesn't
    // fade in from a wrong starting value.
    thresholdDbuSmoothed = thresholdDbuTarget;
    ratioSmoothed = ratioTarget;
    attackSecondsSmoothed = attackSecondsTarget;
    releaseSecondsSmoothed = releaseSecondsTarget;
    gainDbSmoothed = gainDbTarget;
    tubeDrivePercentSmoothed = tubeDrivePercentTarget;
    profileSmoothed = profileTarget;

    const int maxBlockSize = (int) spec.maximumBlockSize;
    delayedDryBuffer.setSize (2, juce::jmax (1, maxBlockSize), false, false, true);
    grMonoScratch.setSize (1, juce::jmax (1, maxBlockSize), false, false, true);
    outputMonoScratch.setSize (1, juce::jmax (1, maxBlockSize), false, false, true);
    inputMonoScratch.setSize (1, juce::jmax (1, maxBlockSize), false, false, true);

    juce::dsp::ProcessSpec delaySpec = spec;
    dryDelayLine.prepare (delaySpec);
    dryDelayLine.setMaximumDelayInSamples (juce::jmax (1, getLatencySamples() + 64));
    dryDelayLine.setDelay ((float) getLatencySamples());

    reset();
}

void NovaVelvetEngine::reset() noexcept
{
    inputStage.reset();
    outputStage.reset();
    detectorL.reset();
    detectorR.reset();
    cellL.reset();
    cellR.reset();
    inputLevelEnvelope.reset();
    outputLevelEnvelope.reset();
    vuGr.reset();
    vuOutput.reset();
    vuInput.reset();
    dryDelayLine.reset();
    lastGainReductionDb = 0.0f;
}

void NovaVelvetEngine::setParams (float thresholdDbu, float ratio, float attackMs, float releaseMs,
                                    float gainDb, int timeSelectChoice, float newScHpfHz,
                                    float mixPercent, float outputDb, bool stereoLinkOn,
                                    int analogModeChoice, float tubeDrivePercent,
                                    bool bypassed) noexcept
{
    thresholdDbuTarget = juce::jlimit (NovaVelvetCalibration::kThresholdMinDbu, NovaVelvetCalibration::kThresholdMaxDbu, thresholdDbu);
    ratioTarget = juce::jlimit (NovaVelvetCalibration::kRatioMin, NovaVelvetCalibration::kRatioMax, ratio);
    attackSecondsTarget = juce::jlimit (NovaVelvetCalibration::kManualAttackMinSeconds, NovaVelvetCalibration::kManualAttackMaxSeconds, attackMs / 1000.0f);
    releaseSecondsTarget = juce::jlimit (NovaVelvetCalibration::kManualReleaseMinSeconds, NovaVelvetCalibration::kManualReleaseMaxSeconds, releaseMs / 1000.0f);
    gainDbTarget = juce::jlimit (0.0f, 30.0f, gainDb);

    switch (timeSelectChoice)
    {
        case 0:  timeSelectMode = NovaVelvetOpticalCell::TimeSelectMode::Fixed; break;
        case 2:  timeSelectMode = NovaVelvetOpticalCell::TimeSelectMode::Manual; break;
        default: timeSelectMode = NovaVelvetOpticalCell::TimeSelectMode::FixMan; break;
    }

    scHpfHz = juce::jlimit (20.0f, 400.0f, newScHpfHz);
    outputDbTarget = juce::jlimit (-10.0f, 10.0f, outputDb);
    outputTrimGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputDbTarget));

    mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, mixPercent / 100.0f));
    stereoLinkSmoothed.setTargetValue (stereoLinkOn ? 1.0f : 0.0f);
    bypassSmoothed.setTargetValue (bypassed ? 0.0f : 1.0f);

    profileTarget = NovaVelvetAnalogModeProfile::forModeIndex (analogModeChoice);
    tubeDrivePercentTarget = juce::jlimit (0.0f, 100.0f, tubeDrivePercent);
}

void NovaVelvetEngine::processBlock (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    // Capture the delay-compensated dry tap before anything else touches
    // the buffer, so MIX stays phase-aligned against the oversamplers'
    // FIR latency further down the chain.
    float* delayedDryL = delayedDryBuffer.getWritePointer (0);
    float* delayedDryR = delayedDryBuffer.getWritePointer (1);
    for (int i = 0; i < numSamples; ++i)
    {
        dryDelayLine.pushSample (0, L[i]);
        dryDelayLine.pushSample (1, R[i]);
        delayedDryL[i] = dryDelayLine.popSample (0);
        delayedDryR[i] = dryDelayLine.popSample (1);
    }

    // Block-rate smoothing for values that drive whole-block stages or
    // discrete choices needing continuous blending for click-free switching.
    const float blockSeconds = (float) numSamples / (float) juce::jmax (1.0, sampleRate);
    const float blockSmoothCoeff = std::exp (-blockSeconds / NovaVelvetCalibration::kBlockSmoothSeconds);
    const float blockSmoothAmt = 1.0f - blockSmoothCoeff;
    thresholdDbuSmoothed += (thresholdDbuTarget - thresholdDbuSmoothed) * blockSmoothAmt;
    ratioSmoothed += (ratioTarget - ratioSmoothed) * blockSmoothAmt;
    attackSecondsSmoothed += (attackSecondsTarget - attackSecondsSmoothed) * blockSmoothAmt;
    releaseSecondsSmoothed += (releaseSecondsTarget - releaseSecondsSmoothed) * blockSmoothAmt;
    gainDbSmoothed += (gainDbTarget - gainDbSmoothed) * blockSmoothAmt;
    tubeDrivePercentSmoothed += (tubeDrivePercentTarget - tubeDrivePercentSmoothed) * blockSmoothAmt;
    profileSmoothed = NovaVelvetAnalogModeProfile::lerp (profileSmoothed, profileTarget, blockSmoothAmt);

    detectorL.setThresholdDbu (thresholdDbuSmoothed);
    detectorR.setThresholdDbu (thresholdDbuSmoothed);
    detectorL.setRatio (ratioSmoothed);
    detectorR.setRatio (ratioSmoothed);
    detectorL.setScHpfHz (scHpfHz);
    detectorR.setScHpfHz (scHpfHz);

    cellL.setTimeSelectMode (timeSelectMode);
    cellR.setTimeSelectMode (timeSelectMode);
    cellL.setManualAttackSeconds (attackSecondsSmoothed);
    cellR.setManualAttackSeconds (attackSecondsSmoothed);
    cellL.setManualReleaseSeconds (releaseSecondsSmoothed);
    cellR.setManualReleaseSeconds (releaseSecondsSmoothed);

    // Input Transformer (oversampled) - level-coupled drive computed from
    // THIS block's own raw input level (immediately available, no lag
    // needed), so saturation audibly increases only near high operating
    // levels rather than sitting at a constant amount.
    float inputLevel01 = inputLevelEnvelope.get01();
    for (int i = 0; i < numSamples; ++i)
        inputLevel01 = inputLevelEnvelope.processSample (juce::jmax (std::abs (L[i]), std::abs (R[i])));

    const float inputTransformerDrive = juce::jlimit (0.0f, 1.0f,
        NovaVelvetCalibration::kInputTransformerBaselineDrive * profileSmoothed.transformerDriveScale
        + NovaVelvetCalibration::kInputTransformerLevelCoupling * inputLevel01);
    inputStage.setTransformerDrive (inputTransformerDrive);
    inputStage.setTransformerLfDrive (0.0f);
    inputStage.setTransformerHfSoftenHzScale (profileSmoothed.bandwidthSofteningHzScale);
    inputStage.process (buffer);

    // Optical detector + gain cell core, per-sample, Stereo-Link-blended -
    // same jmax(magL,magR)+jmap idiom proven in this catalog's Glow/
    // Royal670 engines. Gain (makeup) is applied here too, immediately
    // after gain reduction, since it represents the modeled amplifier
    // stage feeding forward into the tube stage - its own per-sample level
    // is what drives the output stage's level-coupled drive below.
    const float gainLinear = juce::Decibels::decibelsToGain (gainDbSmoothed);
    float* grScratch = grMonoScratch.getWritePointer (0);
    float grSum = 0.0f;
    float outputLevel01 = outputLevelEnvelope.get01();

    for (int i = 0; i < numSamples; ++i)
    {
        const float linkAmount = stereoLinkSmoothed.getNextValue();

        const float magL = detectorL.computeSidechainMagnitude (L[i]);
        const float magR = detectorR.computeSidechainMagnitude (R[i]);
        const float combined = juce::jmax (magL, magR);

        const float targetGrL = detectorL.applyMagnitude (juce::jmap (linkAmount, magL, combined));
        const float targetGrR = detectorR.applyMagnitude (juce::jmap (linkAmount, magR, combined));

        const float energyL = detectorL.getRecentEnergy01();
        const float energyR = detectorR.getRecentEnergy01();

        const float grL = cellL.process (targetGrL, energyL);
        const float grR = cellR.process (targetGrR, energyR);

        float sampleL = L[i] * juce::Decibels::decibelsToGain (-grL) * gainLinear;
        float sampleR = R[i] * juce::Decibels::decibelsToGain (-grR) * gainLinear;
        if (! std::isfinite (sampleL)) sampleL = 0.0f;
        if (! std::isfinite (sampleR)) sampleR = 0.0f;

        L[i] = sampleL;
        R[i] = sampleR;

        outputLevel01 = outputLevelEnvelope.processSample (juce::jmax (std::abs (sampleL), std::abs (sampleR)));

        const float grForMeter = juce::jmax (grL, grR);
        grScratch[i] = grForMeter;
        grSum += grForMeter;
    }
    lastGainReductionDb = numSamples > 0 ? grSum / (float) numSamples : 0.0f;

    // Tube Push-Pull Amplifier + Tube-Drive makeup + Output Transformer
    // (oversampled, one wrapper) - Tube Drive's compensation is
    // deliberately PARTIAL (see NovaVelvetCalibration::kTubeDriveMakeupDbAtMax),
    // so loudness is mostly-but-not-fully flattened and THD still rises
    // monotonically and audibly with drive.
    const float tubeDrive01 = juce::jlimit (0.0f, 1.0f, (tubeDrivePercentSmoothed / 100.0f) * profileSmoothed.tubeDriveScale);
    const float tubeDriveMakeupDb = tubeDrive01 * NovaVelvetCalibration::kTubeDriveMakeupDbAtMax;

    outputStage.setTubeDrive (tubeDrive01);
    outputStage.setTubeLevelEnvelope01 (outputLevel01);
    outputStage.setTubeEvenHarmonicBiasScale (profileSmoothed.tubeEvenHarmonicBiasScale);
    outputStage.setTubeDriveMakeupGainLinear (juce::Decibels::decibelsToGain (tubeDriveMakeupDb));

    const float outputTransformerDrive = juce::jlimit (0.0f, 1.0f,
        NovaVelvetCalibration::kOutputTransformerBaselineDrive * profileSmoothed.transformerDriveScale
        + NovaVelvetCalibration::kOutputTransformerLevelCoupling * outputLevel01);
    outputStage.setTransformerDrive (outputTransformerDrive);
    outputStage.setTransformerLfDrive (NovaVelvetCalibration::kOutputTransformerLfDrive);
    outputStage.setTransformerHfSoftenHzScale (profileSmoothed.bandwidthSofteningHzScale);
    outputStage.process (buffer);

    // Output trim, equal-power MIX (phase-aligned against the delay-
    // compensated dry tap captured above), then Bypass crossfade - all
    // per-sample so every one of these stays click-free under automation.
    float* outScratch = outputMonoScratch.getWritePointer (0);
    float* inScratch = inputMonoScratch.getWritePointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        const float trimGain = outputTrimGainSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();
        const float bypassMix = bypassSmoothed.getNextValue();

        float wetL = L[i] * trimGain;
        float wetR = R[i] * trimGain;
        if (! std::isfinite (wetL)) wetL = 0.0f;
        if (! std::isfinite (wetR)) wetR = 0.0f;

        float dryL = delayedDryL[i];
        float dryR = delayedDryR[i];
        if (! std::isfinite (dryL)) dryL = 0.0f;
        if (! std::isfinite (dryR)) dryR = 0.0f;

        const float mixAngle = mix * juce::MathConstants<float>::halfPi;
        const float dryGain = std::cos (mixAngle);
        const float wetGain = std::sin (mixAngle);
        const float mixedL = dryL * dryGain + wetL * wetGain;
        const float mixedR = dryR * dryGain + wetR * wetGain;

        const float outL = dryL * (1.0f - bypassMix) + mixedL * bypassMix;
        const float outR = dryR * (1.0f - bypassMix) + mixedR * bypassMix;

        L[i] = outL;
        R[i] = outR;

        outScratch[i] = juce::jmax (std::abs (outL), std::abs (outR));
        inScratch[i] = juce::jmax (std::abs (dryL), std::abs (dryR));
    }

    vuGr.process (grScratch, numSamples);
    vuOutput.process (outScratch, numSamples);
    vuInput.process (inScratch, numSamples);
}

int NovaVelvetEngine::getLatencySamples() const noexcept
{
    return inputStage.getLatencySamples() + outputStage.getLatencySamples();
}

} // namespace nova::velvet
