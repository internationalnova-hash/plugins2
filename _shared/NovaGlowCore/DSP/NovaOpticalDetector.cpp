#include "NovaOpticalDetector.h"
#include <cmath>

namespace nova::glow
{

namespace
{
    // Same soft-knee idiom as NovaVariableMuDetector::computeGainReductionDb
    // (itself traced to the catalog's NovaGlueCompressor) - a squared ramp
    // through the knee, linear beyond it.
    float computeGainReductionDb (float overDb, float slope, float kneeDb) noexcept
    {
        if (overDb <= 0.0f) return 0.0f;
        if (kneeDb <= 0.0f) return slope * overDb;

        const float kneeStart = -0.5f * kneeDb;
        const float kneeEnd = 0.5f * kneeDb;

        if (overDb <= kneeStart) return 0.0f;
        if (overDb < kneeEnd)
        {
            const float x = overDb - kneeStart;
            return slope * (x * x) / (2.0f * kneeDb);
        }
        return slope * overDb;
    }
}

void NovaOpticalDetector::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    emphasisFilter.prepare (sampleRate);
    hpfCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * hpfHz / (float) sampleRate);
    reset();
}

void NovaOpticalDetector::reset() noexcept
{
    hpfState = 0.0f;
    lastMagnitude = 0.0f;
    emphasisFilter.reset();
}

void NovaOpticalDetector::setParams (float newThresholdDb, float scHpfHz, float emphasis01) noexcept
{
    thresholdDb = newThresholdDb;

    const float clampedHpfHz = juce::jlimit (20.0f, 400.0f, scHpfHz);
    if (! juce::approximatelyEqual (clampedHpfHz, hpfHz))
    {
        hpfHz = clampedHpfHz;
        hpfCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * hpfHz / (float) sampleRate);
    }

    emphasisFilter.setAmount (emphasis01);
}

void NovaOpticalDetector::setCompressLimitBlend (float compressLimitBlend01) noexcept
{
    compressLimitBlend = juce::jlimit (0.0f, 1.0f, compressLimitBlend01);
}

float NovaOpticalDetector::computeSidechainMagnitude (float sidechainSample) noexcept
{
    if (! std::isfinite (sidechainSample))
        sidechainSample = 0.0f;

    // SC HPF: one-pole, sidechain-only (audible buffer is never touched by
    // this class at all). At the 20Hz minimum this is effectively
    // full-band detection, per spec.
    hpfState = hpfCoeff * hpfState + (1.0f - hpfCoeff) * sidechainSample;
    const float hpfed = sidechainSample - hpfState;

    // Emphasis: sidechain-only frequency tilt, applied after HPF.
    const float tilted = emphasisFilter.processSample (hpfed);

    lastMagnitude = std::abs (tilted) + 1.0e-6f;
    return lastMagnitude;
}

float NovaOpticalDetector::applyMagnitude (float magnitude) noexcept
{
    if (! std::isfinite (magnitude) || magnitude < 0.0f)
        magnitude = 0.0f;

    const float detectorDb = juce::Decibels::gainToDecibels (magnitude, -160.0f);

    const float slope = juce::jmap (compressLimitBlend, kCompressSlope, kLimitSlope);
    const float kneeDb = juce::jmap (compressLimitBlend, kCompressKneeDb, kLimitKneeDb);

    const float overDb = detectorDb - thresholdDb;
    const float targetGrDb = computeGainReductionDb (overDb, slope, kneeDb);

    return std::isfinite (targetGrDb) ? targetGrDb : 0.0f;
}

} // namespace nova::glow
