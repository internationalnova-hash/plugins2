#include "NovaVelvetDetector.h"
#include "NovaVelvetCalibration.h"
#include <cmath>

namespace nova::velvet
{

namespace
{
    // Same soft-knee idiom as this catalog's NovaOpticalDetector/
    // NovaVariableMuDetector (traced to NovaGlueCompressor): a squared ramp
    // through the knee, linear beyond it. slope here is 1-1/ratio (a
    // conventional ratio-derived slope), so Ratio maps continuously and
    // even at 10:1 (slope=0.9) this never behaves like a brickwall limiter.
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

    float alphaFor (double seconds, double sampleRate) noexcept
    {
        return 1.0f - (float) std::exp (-1.0 / (seconds * sampleRate));
    }
}

void NovaVelvetDetector::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    hpfCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * hpfHz / (float) sampleRate);
    peakAttackCoeff = alphaFor (NovaVelvetCalibration::kDetectorPeakAttackSeconds, sampleRate);
    peakReleaseCoeff = alphaFor (NovaVelvetCalibration::kDetectorPeakReleaseSeconds, sampleRate);
    rmsCoeff = alphaFor (NovaVelvetCalibration::kDetectorRmsWindowSeconds, sampleRate);
    energySmoothCoeff = alphaFor (NovaVelvetCalibration::kDetectorEnergySmoothSeconds, sampleRate);

    reset();
}

void NovaVelvetDetector::reset() noexcept
{
    hpfState = 0.0f;
    peakFollower = 0.0f;
    meanSquare = 0.0f;
    recentEnergy01 = 0.0f;
}

void NovaVelvetDetector::setRatio (float newRatio) noexcept
{
    ratio = juce::jlimit (NovaVelvetCalibration::kRatioMin, NovaVelvetCalibration::kRatioMax, newRatio);
}

void NovaVelvetDetector::setScHpfHz (float hz) noexcept
{
    const float clamped = juce::jlimit (20.0f, 400.0f, hz);
    if (! juce::approximatelyEqual (clamped, hpfHz))
    {
        hpfHz = clamped;
        hpfCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * hpfHz / (float) sampleRate);
    }
}

float NovaVelvetDetector::computeSidechainMagnitude (float sidechainSample) noexcept
{
    if (! std::isfinite (sidechainSample))
        sidechainSample = 0.0f;

    // Sidechain-only HPF - the audible buffer is never touched by this class.
    hpfState = hpfCoeff * hpfState + (1.0f - hpfCoeff) * sidechainSample;
    const float hpfed = sidechainSample - hpfState;
    const float rectified = std::abs (hpfed);

    // Fast peak-aware follower.
    const float peakCoeff = rectified > peakFollower ? peakAttackCoeff : peakReleaseCoeff;
    peakFollower += peakCoeff * (rectified - peakFollower);

    // Smoothed energy (mean-square) follower - a genuine RMS-style
    // component, not just a slower peak follower.
    const float sq = rectified * rectified;
    meanSquare += rmsCoeff * (sq - meanSquare);
    const float rmsFollower = std::sqrt (juce::jmax (0.0f, meanSquare));

    // Musical blend: mostly peak-driven (catches vocal peaks/consonants)
    // with a real RMS floor contribution (keeps body/sustain smooth,
    // avoids bass-triggered peak chatter dominating the reading).
    const float magnitude = NovaVelvetCalibration::kDetectorPeakBlendAtNominal * peakFollower
                           + (1.0f - NovaVelvetCalibration::kDetectorPeakBlendAtNominal) * rmsFollower
                           + 1.0e-6f;

    // Telemetry: recentEnergy01, a slower-smoothed, roughly 0-1-normalized
    // reading of the RMS follower in dBFS, referenced a comfortable margin
    // above/below the calibrated nominal level - feeds Fix-Man's blend,
    // not the compression transfer function itself.
    const float energyDbfs = juce::Decibels::gainToDecibels (rmsFollower, -100.0f);
    const float energyNorm = juce::jlimit (0.0f, 1.0f,
        juce::jmap (energyDbfs, NovaVelvetCalibration::kReferenceDbfs - 24.0f, NovaVelvetCalibration::kReferenceDbfs + 12.0f, 0.0f, 1.0f));
    recentEnergy01 += energySmoothCoeff * (energyNorm - recentEnergy01);

    return magnitude;
}

float NovaVelvetDetector::applyMagnitude (float magnitude) noexcept
{
    if (! std::isfinite (magnitude) || magnitude < 0.0f)
        magnitude = 0.0f;

    const float magnitudeDbfs = juce::Decibels::gainToDecibels (magnitude, -160.0f);
    const float thresholdDbfs = thresholdDbu_ + NovaVelvetCalibration::kReferenceDbfs;

    const float ratioNorm = juce::jmap (ratio, NovaVelvetCalibration::kRatioMin, NovaVelvetCalibration::kRatioMax, 0.0f, 1.0f);
    const float slope = 1.0f - (1.0f / ratio);
    const float kneeDb = juce::jmap (ratioNorm, NovaVelvetCalibration::kKneeDbAtRatioMin, NovaVelvetCalibration::kKneeDbAtRatioMax);

    const float overDb = magnitudeDbfs - thresholdDbfs;
    const float targetGrDb = computeGainReductionDb (overDb, slope, kneeDb);

    return std::isfinite (targetGrDb) ? targetGrDb : 0.0f;
}

} // namespace nova::velvet
