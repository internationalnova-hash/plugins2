#include "NovaGlowEngine.h"

namespace nova::glow
{

void NovaGlowEngine::prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages) noexcept
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    preparedOversamplingStages = oversamplingStages;

    inputStage.prepare (spec, oversamplingStages);
    outputStage.prepare (spec, oversamplingStages);

    detectorL.prepare (sampleRate);
    detectorR.prepare (sampleRate);
    gainCellL.prepare (sampleRate);
    gainCellR.prepare (sampleRate);

    noise.prepare (sampleRate);
    vuGr.prepare (sampleRate);
    vuOutput.prepare (sampleRate);
    vuInput.prepare (sampleRate);

    mixSmoothed.reset (sampleRate, kSmoothSeconds);
    stereoLinkSmoothed.reset (sampleRate, kSmoothSeconds);
    bypassSmoothed.reset (sampleRate, kBypassSmoothSeconds);
    outputTrimGainSmoothed.reset (sampleRate, kSmoothSeconds);

    mixSmoothed.setCurrentAndTargetValue (1.0f);
    stereoLinkSmoothed.setCurrentAndTargetValue (1.0f);
    bypassSmoothed.setCurrentAndTargetValue (1.0f); // 1 = active, 0 = bypassed
    outputTrimGainSmoothed.setCurrentAndTargetValue (1.0f);

    // Snap block-rate smoothers to their targets so plugin load doesn't
    // fade in from a wrong starting value.
    thresholdDbSmoothed = thresholdDbTarget;
    gainPercentSmoothed = gainPercentTarget;
    compressLimitSmoothed01 = compressLimitTarget01;
    glowSmoothed01 = glowTarget01;
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

void NovaGlowEngine::reset() noexcept
{
    inputStage.reset();
    outputStage.reset();
    detectorL.reset();
    detectorR.reset();
    gainCellL.reset();
    gainCellR.reset();
    noise.reset();
    vuGr.reset();
    vuOutput.reset();
    vuInput.reset();
    dryDelayLine.reset();
    lastGainReductionDb = 0.0f;
}

void NovaGlowEngine::setParams (float peakReductionPercent, float gainPercent,
                                 int compressLimitChoice, float newScHpfHz, float emphasisPercentIn,
                                 float glowPercent, int analogModeChoice,
                                 float mixPercent, float stereoLinkPercent, float noisePercent,
                                 bool bypassed) noexcept
{
    // Peak Reduction 0-100 -> threshold dB, shaped so lower rotation is
    // gentle and upper rotation is obvious, per spec.
    const float pr01 = juce::jlimit (0.0f, 1.0f, peakReductionPercent / 100.0f);
    const float shaped = std::pow (pr01, kPeakReductionShapeExp);
    thresholdDbTarget = juce::jmap (shaped, 0.0f, 1.0f, kThresholdDbAtZero, kThresholdDbAtMax);

    gainPercentTarget = juce::jlimit (0.0f, 100.0f, gainPercent);
    const float gain01 = gainPercentTarget / 100.0f;
    const float gainTrimDb = juce::jmap (gain01, kGainTrimDbMin, kGainTrimDbMax);
    outputTrimGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (gainTrimDb));

    compressLimitTarget01 = compressLimitChoice != 0 ? 1.0f : 0.0f;
    scHpfHz = juce::jlimit (20.0f, 400.0f, newScHpfHz);
    emphasisPercent = juce::jlimit (0.0f, 100.0f, emphasisPercentIn);

    glowTarget01 = juce::jlimit (0.0f, 100.0f, glowPercent) / 100.0f;
    profileTarget = NovaAnalogModeProfile::forModeIndex (analogModeChoice);

    mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, mixPercent / 100.0f));
    stereoLinkSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, stereoLinkPercent / 100.0f));
    bypassSmoothed.setTargetValue (bypassed ? 0.0f : 1.0f);

    noisePercentTarget = juce::jlimit (0.0f, 100.0f, noisePercent);
}

void NovaGlowEngine::processBlock (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    // Capture the delay-compensated dry tap: push + pop together per
    // sample (JUCE DelayLine's intended usage shape), before anything
    // else touches the buffer, so MIX stays phase-aligned against the
    // oversamplers' FIR latency further down the chain.
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
    // discrete choices that need continuous blending for click-free
    // switching (same idiom as NovaRoyal670Engine's hrmDriveSmoothed01).
    const float blockSeconds = (float) numSamples / (float) juce::jmax (1.0, sampleRate);
    const float blockSmoothCoeff = std::exp (-blockSeconds / kBlockSmoothSeconds);
    const float blockSmoothAmt = 1.0f - blockSmoothCoeff;
    thresholdDbSmoothed += (thresholdDbTarget - thresholdDbSmoothed) * blockSmoothAmt;
    gainPercentSmoothed += (gainPercentTarget - gainPercentSmoothed) * blockSmoothAmt;
    compressLimitSmoothed01 += (compressLimitTarget01 - compressLimitSmoothed01) * blockSmoothAmt;
    glowSmoothed01 += (glowTarget01 - glowSmoothed01) * blockSmoothAmt;
    profileSmoothed = NovaAnalogModeProfile::lerp (profileSmoothed, profileTarget, blockSmoothAmt);

    const NovaGlowMacro glowMacro = NovaGlowMacro::fromGlow (glowSmoothed01);
    const float gain01 = gainPercentSmoothed / 100.0f;

    detectorL.setParams (thresholdDbSmoothed, scHpfHz, emphasisPercent / 100.0f);
    detectorR.setParams (thresholdDbSmoothed, scHpfHz, emphasisPercent / 100.0f);
    detectorL.setCompressLimitBlend (compressLimitSmoothed01);
    detectorR.setCompressLimitBlend (compressLimitSmoothed01);

    gainCellL.setMemoryDepth (juce::jlimit (0.0f, 1.0f, glowMacro.gainCellMemoryDepth * profileSmoothed.gainCellMemoryScale));
    gainCellR.setMemoryDepth (juce::jlimit (0.0f, 1.0f, glowMacro.gainCellMemoryDepth * profileSmoothed.gainCellMemoryScale));
    gainCellL.setAttackSoftening (glowMacro.attackSofteningAmount);
    gainCellR.setAttackSoftening (glowMacro.attackSofteningAmount);

    // Input Transformer + Tube Gain Stage (oversampled, one wrapper) -
    // driven by the active Analog Mode profile's baselines plus GLOW's
    // contribution, at block granularity ahead of the per-sample loop.
    inputStage.setTransformerDrive (juce::jlimit (0.0f, 1.0f, profileSmoothed.transformerDriveBaseline));
    inputStage.setTransformerLfDrive (glowMacro.transformerLfDrive);
    inputStage.setTubeDrive (juce::jlimit (0.0f, 4.0f, gain01 * kGainOutputDriveMax * 0.5f + 0.05f));
    inputStage.setTubeEvenHarmonicBias (juce::jlimit (0.0f, 1.0f, profileSmoothed.tubeEvenHarmonicBiasBaseline + glowMacro.tubeEvenHarmonicBias * 0.5f));
    inputStage.process (buffer);

    // Optical detector + gain cell core, per-sample, Stereo-Link-blended.
    float* grScratch = grMonoScratch.getWritePointer (0);
    float grSum = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float linkAmount = stereoLinkSmoothed.getNextValue();

        const float magL = detectorL.computeSidechainMagnitude (L[i]);
        const float magR = detectorR.computeSidechainMagnitude (R[i]);
        const float combined = juce::jmax (magL, magR);

        const float targetGrL = detectorL.applyMagnitude (juce::jmap (linkAmount, magL, combined));
        const float targetGrR = detectorR.applyMagnitude (juce::jmap (linkAmount, magR, combined));

        const float grL = gainCellL.process (targetGrL);
        const float grR = gainCellR.process (targetGrR);

        L[i] *= juce::Decibels::decibelsToGain (-grL);
        R[i] *= juce::Decibels::decibelsToGain (-grR);

        const float grForMeter = juce::jmax (grL, grR);
        grScratch[i] = grForMeter;
        grSum += grForMeter;
    }
    lastGainReductionDb = numSamples > 0 ? grSum / (float) numSamples : 0.0f;

    // Tube Output Stage + Output Transformer (oversampled, one wrapper) -
    // driven by GAIN (the "not pure makeup gain" requirement) plus the
    // active profile/GLOW contribution.
    const float outputDriveCoupled = juce::jlimit (0.0f, 4.0f,
        (gain01 * kGainOutputDriveMax + glowMacro.outputStageDriveBoost * 0.3f)
            * (1.0f + 0.15f * juce::jlimit (0.0f, 1.0f, lastGainReductionDb / 12.0f)));
    outputStage.setTubeDrive (outputDriveCoupled);
    outputStage.setTubeEvenHarmonicBias (juce::jlimit (0.0f, 1.0f, profileSmoothed.tubeEvenHarmonicBiasBaseline + glowMacro.tubeEvenHarmonicBias));
    outputStage.setTransformerDrive (juce::jlimit (0.0f, 1.0f, profileSmoothed.transformerDriveBaseline + glowMacro.outputStageDriveBoost * 0.3f));
    outputStage.setTransformerLfDrive (glowMacro.transformerLfDrive);
    outputStage.process (buffer);

    noise.setLevel (noisePercentTarget / 100.0f, profileSmoothed.noiseFloorScale);
    noise.processAdd (L, R, numSamples);

    // Output trim (GAIN), equal-power MIX (phase-aligned against the
    // delay-compensated dry tap captured above), then Bypass crossfade -
    // all per-sample so every one of these stays click-free under
    // automation.
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

int NovaGlowEngine::getLatencySamples() const noexcept
{
    return inputStage.getLatencySamples() + outputStage.getLatencySamples();
}

} // namespace nova::glow
