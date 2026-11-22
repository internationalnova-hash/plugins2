#include "NovaGlueEngine.h"

namespace nova::glue
{

void NovaGlueEngine::prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages) noexcept
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    preparedOversamplingStages = oversamplingStages;

    inputStage.prepare (spec, oversamplingStages);
    outputStage.prepare (spec, oversamplingStages);

    detectorL.prepare (sampleRate);
    detectorR.prepare (sampleRate);
    autoGain.prepare (sampleRate);
    vuGr.prepare (sampleRate);

    mixSmoothed.reset (sampleRate, kSmoothSeconds);
    stereoLinkSmoothed.reset (sampleRate, kSmoothSeconds);
    bypassSmoothed.reset (sampleRate, kBypassSmoothSeconds);
    makeupGainSmoothed.reset (sampleRate, kSmoothSeconds);

    mixSmoothed.setCurrentAndTargetValue (1.0f);
    stereoLinkSmoothed.setCurrentAndTargetValue (1.0f);
    bypassSmoothed.setCurrentAndTargetValue (1.0f); // 1 = active/IN, 0 = bypassed
    makeupGainSmoothed.setCurrentAndTargetValue (1.0f);

    // Snap block-rate smoothers to their targets so plugin load doesn't
    // fade in from a wrong starting value.
    thresholdDbSmoothed = thresholdDbTarget;
    driveSmoothed01 = driveTarget01;
    glueSmoothed01 = glueTarget01;

    const int maxBlockSize = (int) spec.maximumBlockSize;
    delayedDryBuffer.setSize (2, juce::jmax (1, maxBlockSize), false, false, true);
    grMonoScratch.setSize (1, juce::jmax (1, maxBlockSize), false, false, true);

    juce::dsp::ProcessSpec delaySpec = spec;
    dryDelayLine.prepare (delaySpec);
    dryDelayLine.setMaximumDelayInSamples (juce::jmax (1, getLatencySamples() + 64));
    dryDelayLine.setDelay ((float) getLatencySamples());

    reset();
}

void NovaGlueEngine::reset() noexcept
{
    inputStage.reset();
    outputStage.reset();
    detectorL.reset();
    detectorR.reset();
    autoGain.reset();
    vuGr.reset();
    dryDelayLine.reset();
    lastGainReductionDb = 0.0f;
}

void NovaGlueEngine::setParams (float newThresholdDb, int newAttackChoice, int newReleaseChoice, float makeupDb,
                                  int newRatioChoice, float newScHpfHz, float mixPercent, float gluePercent,
                                  float drivePercent, bool newAutoGainEnabled, bool stereoLinkEnabled,
                                  bool bypassed) noexcept
{
    thresholdDbTarget = juce::jlimit (-30.0f, 10.0f, newThresholdDb);
    attackChoice = juce::jlimit (0, 5, newAttackChoice);
    releaseChoice = juce::jlimit (0, 4, newReleaseChoice);
    ratioChoice = juce::jlimit (0, 2, newRatioChoice);
    scHpfHz = juce::jlimit (20.0f, 400.0f, newScHpfHz);

    driveTarget01 = juce::jlimit (0.0f, 100.0f, drivePercent) / 100.0f;
    glueTarget01 = juce::jlimit (0.0f, 100.0f, gluePercent) / 100.0f;

    // Centralized Drive loudness-compensation curve, folded into the same
    // smoothed gain as the manual MAKEUP trim - so "more Drive" isn't
    // perceived as "better" purely from being louder.
    const float driveCompDb = driveTarget01 * kDriveCompensationDbPerUnit;
    makeupGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (juce::jlimit (-20.0f, 20.0f, makeupDb) + driveCompDb));

    autoGainEnabled = newAutoGainEnabled;
    autoGain.setEnabled (autoGainEnabled);

    mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, mixPercent / 100.0f));
    stereoLinkSmoothed.setTargetValue (stereoLinkEnabled ? 1.0f : 0.0f);
    bypassSmoothed.setTargetValue (bypassed ? 0.0f : 1.0f);
}

void NovaGlueEngine::processBlock (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    // Capture the delay-compensated dry tap: push + pop together per
    // sample, before anything else touches the buffer, so MIX stays
    // phase-aligned against the oversamplers' FIR latency further down.
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
    // discrete choices that need continuous blending - same idiom as
    // NovaGlowEngine's thresholdDbSmoothed etc.
    const float blockSeconds = (float) numSamples / (float) juce::jmax (1.0, sampleRate);
    const float blockSmoothCoeff = std::exp (-blockSeconds / kBlockSmoothSeconds);
    const float blockSmoothAmt = 1.0f - blockSmoothCoeff;
    thresholdDbSmoothed += (thresholdDbTarget - thresholdDbSmoothed) * blockSmoothAmt;
    driveSmoothed01 += (driveTarget01 - driveSmoothed01) * blockSmoothAmt;
    glueSmoothed01 += (glueTarget01 - glueSmoothed01) * blockSmoothAmt;

    const NovaGlueMacro glueMacro = NovaGlueMacro::fromGlue (glueSmoothed01);

    detectorL.setParams (thresholdDbSmoothed, scHpfHz, ratioChoice, attackChoice, releaseChoice);
    detectorR.setParams (thresholdDbSmoothed, scHpfHz, ratioChoice, attackChoice, releaseChoice);
    detectorL.setKneeWideningDb (glueMacro.kneeWideningDb);
    detectorR.setKneeWideningDb (glueMacro.kneeWideningDb);
    detectorL.setAutoMemoryDepthNudge (glueMacro.autoMemoryDepthNudge);
    detectorR.setAutoMemoryDepthNudge (glueMacro.autoMemoryDepthNudge);
    detectorL.setDetectorIntegrationNudge (glueMacro.detectorIntegrationNudge01);
    detectorR.setDetectorIntegrationNudge (glueMacro.detectorIntegrationNudge01);

    // Console input/output stages - DRIVE (plus GLUE's tiny analog-density
    // nudge) shared across both boundaries, output position weighted more
    // heavily as the more audible "VCA output" position.
    const float totalDrive01 = juce::jlimit (0.0f, 1.0f, driveSmoothed01 + glueMacro.analogDensityNudge01);
    inputStage.setDrive (totalDrive01 * 0.6f);
    inputStage.setEvenHarmonicBias (totalDrive01 * 0.3f);
    outputStage.setDrive (totalDrive01);
    outputStage.setEvenHarmonicBias (totalDrive01 * 0.5f);

    inputStage.process (buffer);

    // Detector + VCA gain core, per-sample, Stereo-Link-blended.
    float* grScratch = grMonoScratch.getWritePointer (0);
    float grSum = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float linkAmount = stereoLinkSmoothed.getNextValue();

        const float magL = detectorL.computeSidechainMagnitude (L[i]);
        const float magR = detectorR.computeSidechainMagnitude (R[i]);
        const float combined = juce::jmax (magL, magR);

        const float grL = detectorL.applyMagnitude (juce::jmap (linkAmount, magL, combined));
        const float grR = detectorR.applyMagnitude (juce::jmap (linkAmount, magR, combined));

        L[i] *= juce::Decibels::decibelsToGain (-grL);
        R[i] *= juce::Decibels::decibelsToGain (-grR);

        const float grForMeter = juce::jmax (grL, grR);
        grScratch[i] = grForMeter;
        grSum += grForMeter;
    }
    lastGainReductionDb = numSamples > 0 ? grSum / (float) numSamples : 0.0f;

    outputStage.process (buffer);

    // Auto Gain (slow, shared) + Makeup trim, equal-power MIX (phase-
    // aligned against the delay-compensated dry tap), then Bypass
    // crossfade (linear, matching the catalog's own bypass convention) -
    // all per-sample so every one of these stays click-free under
    // automation.
    for (int i = 0; i < numSamples; ++i)
    {
        const float autoGainMultiplier = autoGain.process (grScratch[i]);
        const float trimGain = makeupGainSmoothed.getNextValue() * autoGainMultiplier;
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
    }

    vuGr.process (grScratch, numSamples);
}

int NovaGlueEngine::getLatencySamples() const noexcept
{
    return inputStage.getLatencySamples() + outputStage.getLatencySamples();
}

} // namespace nova::glue
