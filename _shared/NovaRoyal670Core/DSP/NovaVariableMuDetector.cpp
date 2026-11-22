#include "NovaVariableMuDetector.h"
#include <cmath>

namespace nova::royal670
{

namespace
{
    // Same soft-knee idiom as the catalog's proven NovaGlueCompressor
    // (_shared/NovaAIMasterCore/DSP/NovaGlueCompressor.cpp), parameterized
    // here by a caller-supplied slope/knee rather than fixed constants, so
    // DC Threshold can shape compression character.
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

    // Base release time in seconds per Time Constant mode 1-6, matching
    // the UI's exact displayed values.
    float baseReleaseSeconds (int mode) noexcept
    {
        switch (mode)
        {
            case 1: return 0.1f;
            case 2: return 0.2f;
            case 3: return 0.6f;
            case 4: return 1.2f;
            case 5: return 2.0f;
            case 6: return 4.0f;
            default: return 1.2f;
        }
    }

    // How strongly programDependence blends toward the slow pole, and
    // (for modes 3-6) toward the extra-slow pole. Faster modes stay close
    // to fixed/quick recovery; slower modes lean increasingly on the
    // program-dependent multi-stage tail.
    float slowBlendScale (int mode) noexcept
    {
        static constexpr float table[6] { 0.15f, 0.25f, 0.55f, 0.70f, 0.85f, 0.95f };
        return table[juce::jlimit (1, 6, mode) - 1];
    }

    float extraSlowBlendScale (int mode) noexcept
    {
        static constexpr float table[6] { 0.0f, 0.0f, 0.20f, 0.35f, 0.55f, 0.75f };
        return table[juce::jlimit (1, 6, mode) - 1];
    }
}

void NovaVariableMuDetector::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    attackCoeff = std::exp (-1.0f / (kAttackSeconds * (float) sampleRate));
    programDependenceCoeff = std::exp (-1.0f / (kProgramDependenceSeconds * (float) sampleRate));
    updateModeCoefficients();
    // hpfCoeff is otherwise only recomputed lazily in setParams() when the
    // requested cutoff *changes* - compute it here too, since a first
    // setParams() call whose cutoff happens to match the default hpfHz
    // would otherwise leave hpfCoeff at its zero-initialized value
    // (which makes the HPF remove the entire signal, not pass it).
    hpfCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * hpfHz / (float) sampleRate);
    reset();
}

void NovaVariableMuDetector::reset() noexcept
{
    hpfState = 0.0f;
    envelope = 0.0f;
    fastGr = slowGr = extraSlowGr = 0.0f;
    programDependence = 0.0f;
    gainReductionDb = 0.0f;
}

void NovaVariableMuDetector::updateModeCoefficients() noexcept
{
    const float base = baseReleaseSeconds (mode);
    const float sr = (float) sampleRate;
    fastCoeff = std::exp (-1.0f / (juce::jmax (0.001f, base * 0.25f) * sr));
    slowCoeff = std::exp (-1.0f / (juce::jmax (0.001f, base * 1.0f) * sr));
    extraSlowCoeff = std::exp (-1.0f / (juce::jmax (0.001f, base * 2.5f) * sr));
}

void NovaVariableMuDetector::setParams (float newAcThresholdDb, float newDcThresholdDb, int timeConstantMode, float sidechainHpfHz) noexcept
{
    acThresholdDb = newAcThresholdDb;
    dcThresholdDb = newDcThresholdDb;

    const int clampedMode = juce::jlimit (1, 6, timeConstantMode);
    if (clampedMode != mode)
    {
        mode = clampedMode;
        updateModeCoefficients();
    }

    const float clampedHpfHz = juce::jlimit (20.0f, 2000.0f, sidechainHpfHz);
    if (! juce::approximatelyEqual (clampedHpfHz, hpfHz))
    {
        hpfHz = clampedHpfHz;
        hpfCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * hpfHz / (float) sampleRate);
    }
}

float NovaVariableMuDetector::computeSidechainMagnitude (float sidechainSample) noexcept
{
    // One-pole HPF: subtract a tracked low-passed version from the input.
    hpfState = hpfCoeff * hpfState + (1.0f - hpfCoeff) * sidechainSample;
    const float hpfed = sidechainSample - hpfState;
    lastHpfedSignal = hpfed;
    return std::abs (hpfed) + 1.0e-6f;
}

float NovaVariableMuDetector::processSample (float sidechainSample) noexcept
{
    return applyMagnitude (computeSidechainMagnitude (sidechainSample));
}

float NovaVariableMuDetector::applyMagnitude (float magnitude) noexcept
{
    // Attack is fixed (not mode-dependent) - only release/time-constant
    // varies across the six modes - so this is a single one-pole follower
    // rather than an attack/release branch.
    envelope = attackCoeff * envelope + (1.0f - attackCoeff) * magnitude;

    const float detectorDb = juce::Decibels::gainToDecibels (envelope, -160.0f);

    // DC Threshold/Behavior shapes knee width and ratio-to-limiting slope
    // rather than acting as a second level threshold.
    const float behavior = juce::jlimit (-1.0f, 1.0f, dcThresholdDb / 20.0f); // dcThresholdDb range is roughly +-20dB per the UI
    const float kneeDb = juce::jmax (0.5f, kBaseKneeDb * (1.0f - behavior * 0.6f));
    const float compressorSlope = 1.0f - (1.0f / kBaseRatio);
    const float limiterSlope = 0.98f;
    const float slope = juce::jmap (juce::jlimit (0.0f, 1.0f, 0.5f + behavior * 0.5f), compressorSlope, limiterSlope);

    const float overDb = detectorDb - acThresholdDb;
    const float targetGrDb = computeGainReductionDb (overDb, slope, kneeDb);

    // Update the running "how deep/sustained has GR been" estimate that
    // drives program dependence.
    const float normalizedTarget = juce::jlimit (0.0f, 1.0f, targetGrDb / kDeepReductionReferenceDb);
    programDependence = programDependenceCoeff * programDependence + (1.0f - programDependenceCoeff) * normalizedTarget;

    // Three parallel one-pole followers on the target GR value: shared
    // fast attack when rising, mode-specific release coefficient when
    // falling.
    auto updatePole = [targetGrDb] (float current, float releaseCoeff, float sharedAttackCoeff) noexcept
    {
        const float coeff = targetGrDb > current ? sharedAttackCoeff : releaseCoeff;
        return coeff * current + (1.0f - coeff) * targetGrDb;
    };

    fastGr = updatePole (fastGr, fastCoeff, attackCoeff);
    slowGr = updatePole (slowGr, slowCoeff, attackCoeff);
    extraSlowGr = updatePole (extraSlowGr, extraSlowCoeff, attackCoeff);

    const float blendToSlow = programDependence * slowBlendScale (mode);
    const float withSlow = fastGr + (slowGr - fastGr) * juce::jlimit (0.0f, 1.0f, blendToSlow);

    const float blendToExtraSlow = programDependence * extraSlowBlendScale (mode);
    const float finalGr = withSlow + (extraSlowGr - withSlow) * juce::jlimit (0.0f, 1.0f, blendToExtraSlow);

    gainReductionDb = juce::jmax (0.0f, finalGr);
    return gainReductionDb;
}

} // namespace nova::royal670
