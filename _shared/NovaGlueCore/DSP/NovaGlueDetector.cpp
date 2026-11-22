#include "NovaGlueDetector.h"
#include <cmath>

namespace nova::glue
{

namespace
{
    // Same soft-knee idiom as NovaVariableMuDetector
    // (_shared/NovaRoyal670Core/DSP/NovaVariableMuDetector.cpp): quadratic
    // ramp inside the knee, linear beyond it.
    float computeGainReductionDb (float overDb, float slope, float kneeDb) noexcept
    {
        if (overDb <= 0.0f) return 0.0f;
        if (kneeDb <= 0.0f) return slope * overDb;

        const auto kneeStart = -0.5f * kneeDb;
        const auto kneeEnd = 0.5f * kneeDb;

        if (overDb <= kneeStart) return 0.0f;
        if (overDb < kneeEnd)
        {
            const auto x = overDb - kneeStart;
            return slope * (x * x) / (2.0f * kneeDb);
        }
        return slope * overDb;
    }

    // Attack seconds per LOCKED choice 0-5: 0.1/0.3/1/3/10/30 ms.
    constexpr float kAttackSecondsTable[6] { 0.0001f, 0.0003f, 0.001f, 0.003f, 0.01f, 0.03f };

    // Fixed release seconds per LOCKED choice 0-3: 0.1/0.3/0.6/1.2 s.
    // Index 4 (AUTO) never reads this table.
    constexpr float kFixedReleaseSecondsTable[4] { 0.1f, 0.3f, 0.6f, 1.2f };

    // Ratio -> slope/base-knee. 2:1 stays wide/gentle ("mix-bus glue"),
    // 10:1 narrows toward a limiting character, per spec.
    constexpr float kSlopeTable[3] { 0.5f, 0.75f, 0.9f };     // 1-1/ratio
    constexpr float kBaseKneeTable[3] { 6.0f, 4.0f, 2.0f };
}

void NovaGlueDetector::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    hpfCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * hpfHz / (float) sampleRate);
    chargeCoeff = std::exp (-1.0f / (kAutoChargeSeconds * (float) sampleRate));
    autoFastReleaseCoeff = std::exp (-1.0f / (kAutoFastReleaseSeconds * (float) sampleRate));
    recomputeAttackCoeff();
    recomputeReleaseCoeff();
    recomputeEnergyCoeff();
    reset();
}

void NovaGlueDetector::reset() noexcept
{
    hpfState = 0.0f;
    energyState = 0.0f;
    envelopeOverDb = -160.0f;
    cellCharge = 0.0f;
    fastPoleOverDb = -160.0f;
    slowPoleOverDb = -160.0f;
    gainReductionDb = 0.0f;
}

void NovaGlueDetector::recomputeAttackCoeff() noexcept
{
    const int idx = juce::jlimit (0, 5, attackChoice);
    attackCoeff = std::exp (-1.0f / (juce::jmax (0.00005f, kAttackSecondsTable[idx]) * (float) sampleRate));
}

void NovaGlueDetector::recomputeReleaseCoeff() noexcept
{
    const int idx = juce::jlimit (0, 3, releaseChoice);
    fixedReleaseCoeff = std::exp (-1.0f / (juce::jmax (0.001f, kFixedReleaseSecondsTable[idx]) * (float) sampleRate));
}

void NovaGlueDetector::recomputeEnergyCoeff() noexcept
{
    const float seconds = kEnergyWindowSecondsBase + detectorIntegrationNudge * kEnergyWindowNudgeMax;
    energyCoeff = std::exp (-1.0f / (juce::jmax (0.0005f, seconds) * (float) sampleRate));
}

void NovaGlueDetector::setDetectorIntegrationNudge (float amount01) noexcept
{
    detectorIntegrationNudge = juce::jlimit (0.0f, 1.0f, amount01);
    recomputeEnergyCoeff();
}

void NovaGlueDetector::setParams (float newThresholdDb, float newScHpfHz, int newRatioChoice, int newAttackChoice, int newReleaseChoice) noexcept
{
    thresholdDb = newThresholdDb;

    const float clampedHpfHz = juce::jlimit (20.0f, 400.0f, newScHpfHz);
    if (! juce::approximatelyEqual (clampedHpfHz, hpfHz))
    {
        hpfHz = clampedHpfHz;
        hpfCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * hpfHz / (float) sampleRate);
    }

    const int r = juce::jlimit (0, 2, newRatioChoice);
    slope = kSlopeTable[r];
    baseKneeDb = kBaseKneeTable[r];

    const int newAttack = juce::jlimit (0, 5, newAttackChoice);
    if (newAttack != attackChoice)
    {
        attackChoice = newAttack;
        recomputeAttackCoeff();
    }

    const int newRelease = juce::jlimit (0, kAutoReleaseChoice, newReleaseChoice);
    if (newRelease != releaseChoice)
    {
        releaseChoice = newRelease;
        recomputeReleaseCoeff();
    }
}

float NovaGlueDetector::computeSidechainMagnitude (float sidechainSample) noexcept
{
    // One-pole HPF: subtract a tracked low-passed version from the input.
    hpfState = hpfCoeff * hpfState + (1.0f - hpfCoeff) * sidechainSample;
    const float hpfed = sidechainSample - hpfState;

    // Short energy/RMS-style window on the squared signal - a single
    // sample-level spike cannot dominate this the way an instantaneous
    // |x| detector would, per spec's explicit requirement.
    const float squared = hpfed * hpfed;
    energyState = energyCoeff * energyState + (1.0f - energyCoeff) * squared;

    return std::sqrt (juce::jmax (0.0f, energyState)) + 1.0e-8f;
}

float NovaGlueDetector::applyMagnitude (float magnitude) noexcept
{
    const float levelDb = juce::Decibels::gainToDecibels (magnitude, -160.0f);
    const float overDbRaw = levelDb - thresholdDb;

    float overDbSmoothed;

    if (releaseChoice == kAutoReleaseChoice)
    {
        // AUTO: program-dependent two-pole blend adapted from
        // NovaOpticalGainCell (_shared/NovaGlowCore/DSP/NovaOpticalGainCell.cpp),
        // operating on overDb rather than a post-knee GR value since the
        // knee hasn't been applied yet at this point in the chain.
        // cellCharge tracks how far/long the RAW input has sat over
        // threshold - independent of the envelope's own smoothing, so
        // there's no circular dependency on the value being computed.
        const float drivenness = juce::jlimit (0.0f, 1.0f, overDbRaw / kAutoDrivenessRangeDb);
        cellCharge = chargeCoeff * cellCharge + (1.0f - chargeCoeff) * drivenness;

        const bool risingFast = overDbRaw > fastPoleOverDb;
        const float fastCoeffUsed = risingFast ? attackCoeff : autoFastReleaseCoeff;
        fastPoleOverDb = fastCoeffUsed * fastPoleOverDb + (1.0f - fastCoeffUsed) * overDbRaw;

        // The slow pole's OWN time constant stretches with cellCharge -
        // the same "optical memory deepens with sustained use" mechanism
        // Glow's gain cell uses, adapted here to the pre-knee level.
        const float effectiveSlowSeconds = kAutoSlowReleaseBaseSeconds * (1.0f + cellCharge * kAutoSlowDeepenFactor);
        const float slowReleaseCoeff = std::exp (-1.0f / (juce::jmax (0.001f, effectiveSlowSeconds) * (float) sampleRate));
        const bool risingSlow = overDbRaw > slowPoleOverDb;
        const float slowCoeffUsed = risingSlow ? attackCoeff : slowReleaseCoeff;
        slowPoleOverDb = slowCoeffUsed * slowPoleOverDb + (1.0f - slowCoeffUsed) * overDbRaw;

        const float blend = juce::jlimit (0.0f, 1.0f, cellCharge * (1.0f + autoMemoryDepthNudge * 0.3f));
        overDbSmoothed = fastPoleOverDb + (slowPoleOverDb - fastPoleOverDb) * blend;
    }
    else
    {
        // Fixed release: a single one-pole, shared attack coefficient when
        // rising, the chosen fixed release coefficient when falling -
        // genuinely fixed regardless of signal duration/history, per spec's
        // "Fixed settings must behave distinctly" requirement.
        const bool rising = overDbRaw > envelopeOverDb;
        const float coeff = rising ? attackCoeff : fixedReleaseCoeff;
        envelopeOverDb = coeff * envelopeOverDb + (1.0f - coeff) * overDbRaw;
        overDbSmoothed = envelopeOverDb;
    }

    // GLUE's kneeWideningDb softens the onset shape only - it does not
    // change the eventual over-knee slope, so it cannot raise the
    // steady-state gain-reduction depth for a sustained over-threshold
    // signal (see NovaGlueMacro's header comment for the full constraint).
    const float effectiveKneeDb = baseKneeDb + kneeWideningDb;
    gainReductionDb = juce::jmax (0.0f, computeGainReductionDb (overDbSmoothed, slope, effectiveKneeDb));
    if (! std::isfinite (gainReductionDb))
        gainReductionDb = 0.0f;

    return gainReductionDb;
}

} // namespace nova::glue
