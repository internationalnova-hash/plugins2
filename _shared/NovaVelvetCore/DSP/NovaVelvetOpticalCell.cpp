#include "NovaVelvetOpticalCell.h"
#include "NovaVelvetCalibration.h"
#include <cmath>

namespace nova::velvet
{

namespace
{
    float alphaFor (double seconds, double sampleRate) noexcept
    {
        return std::exp (-1.0f / (float) (seconds * sampleRate));
    }
}

void NovaVelvetOpticalCell::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    chargeCoeff = alphaFor (NovaVelvetCalibration::kCellChargeSeconds, sampleRate);
    fastReleaseCoeff = alphaFor (NovaVelvetCalibration::kFixedReleaseSeconds, sampleRate);
    peakHoldDecayCoeff = alphaFor (NovaVelvetCalibration::kPeakHoldDecaySeconds, sampleRate);
    durationHangCoeff = alphaFor (NovaVelvetCalibration::kFixManDurationHangSeconds, sampleRate);
    historyBlendSmoothCoeff = alphaFor (NovaVelvetCalibration::kFixManBlendSmoothSeconds, sampleRate);

    reset();
}

void NovaVelvetOpticalCell::reset() noexcept
{
    envelopeGr = 0.0f;
    fastPoleGr = 0.0f;
    slowPoleGr = 0.0f;
    cellCharge = 0.0f;
    peakGrHold = 0.0f;
    durationSeconds = 0.0f;
    historyDependentBlend = 0.0f;
    gainReductionDb = 0.0f;
}

void NovaVelvetOpticalCell::setManualAttackSeconds (float seconds) noexcept
{
    manualAttackSeconds = juce::jlimit (NovaVelvetCalibration::kManualAttackMinSeconds,
                                         NovaVelvetCalibration::kManualAttackMaxSeconds, seconds);
}

void NovaVelvetOpticalCell::setManualReleaseSeconds (float seconds) noexcept
{
    manualReleaseSeconds = juce::jlimit (NovaVelvetCalibration::kManualReleaseMinSeconds,
                                          NovaVelvetCalibration::kManualReleaseMaxSeconds, seconds);
}

float NovaVelvetOpticalCell::process (float targetGrDb, float detectorEnergy01) noexcept
{
    targetGrDb = juce::jmax (0.0f, targetGrDb);
    if (! std::isfinite (targetGrDb)) targetGrDb = 0.0f;
    detectorEnergy01 = juce::jlimit (0.0f, 1.0f, std::isfinite (detectorEnergy01) ? detectorEnergy01 : 0.0f);
    const float sr = (float) sampleRate;

    // --- mode blend selection (computed from the PREVIOUS sample's
    //     historyDependentBlend for FixMan - see below for why this ordering
    //     doesn't matter functionally, only which sample the debug getter
    //     reflects). ---
    float modeBlend = 0.0f;
    switch (timeSelectMode)
    {
        case TimeSelectMode::Fixed:  modeBlend = 0.0f; break;
        case TimeSelectMode::Manual: modeBlend = 1.0f; break;
        case TimeSelectMode::FixMan: modeBlend = historyDependentBlend; break;
    }

    // 1. Attack: program-dependent jump-size-normalized softening, anchored
    //    to the Manual Attack knob but blended toward the flat Fixed
    //    attack via modeBlend. Fixed mode (modeBlend=0) is therefore
    //    exactly kFixedAttackSeconds regardless of the manual knob value -
    //    jmap(0, a, b) == a unconditionally.
    const float jumpNorm = juce::jlimit (0.0f, 1.0f,
        std::abs (targetGrDb - envelopeGr) / NovaVelvetCalibration::kAttackJumpReferenceDb);
    const float softenedFactor = juce::jmap (jumpNorm, 1.0f + NovaVelvetCalibration::kAttackSofteningRange, 0.3f);
    const float manualAnchoredAttack = juce::jmax (0.0003f, manualAttackSeconds * softenedFactor);
    const float attackSeconds = juce::jmap (modeBlend, NovaVelvetCalibration::kFixedAttackSeconds, manualAnchoredAttack);
    const float attackCoeff = std::exp (-1.0f / (juce::jmax (0.0001f, attackSeconds) * sr));
    envelopeGr = attackCoeff * envelopeGr + (1.0f - attackCoeff) * targetGrDb;

    // 2. cellCharge: leaky integral of envelopeGr, normalized against a
    //    deep-reduction reference - same idiom as NovaOpticalGainCell.
    const float drivenness = juce::jlimit (0.0f, 1.0f, envelopeGr / NovaVelvetCalibration::kDeepReductionReferenceDb);
    cellCharge = chargeCoeff * cellCharge + (1.0f - chargeCoeff) * drivenness;

    // 3. peakGrHold: instant-attack, slow-decay peak-and-hold of envelopeGr
    //    (decays TOWARD envelopeGr, not toward 0, so it never gets stuck
    //    non-zero once the cell has genuinely released).
    if (envelopeGr >= peakGrHold)
        peakGrHold = envelopeGr;
    else
        peakGrHold = peakHoldDecayCoeff * peakGrHold + (1.0f - peakHoldDecayCoeff) * envelopeGr;

    // 4. durationSeconds: true elapsed time under active compression,
    //    accumulating in real seconds (not exponentially) while envelopeGr
    //    stays above the active-GR threshold, decaying back to 0 over
    //    kFixManDurationHangSeconds once it drops below.
    if (envelopeGr > NovaVelvetCalibration::kFixManActiveGrThresholdDb)
        durationSeconds = juce::jmin (30.0f, durationSeconds + 1.0f / sr);
    else
        durationSeconds *= durationHangCoeff;

    // 5. Two persistent release poles on envelopeGr - ALWAYS both running
    //    (never reset/skipped by mode) so Time Select switches stay
    //    click-free.
    const bool risingFast = envelopeGr > fastPoleGr;
    const float fastCoeffUsed = risingFast ? attackCoeff : fastReleaseCoeff;
    fastPoleGr = fastCoeffUsed * fastPoleGr + (1.0f - fastCoeffUsed) * envelopeGr;

    // Slow pole's own tau stretches by TWO independent mechanisms:
    // instantaneous GR-depth (deeper right now = slower) and cellCharge
    // (driven longer/harder = slower) - named separately per the design
    // spec rather than folded into one factor.
    const float depthNorm = juce::jlimit (0.0f, 1.0f, envelopeGr / NovaVelvetCalibration::kDeepReductionReferenceDb);
    const float depthStretch = 1.0f + NovaVelvetCalibration::kGrDepthReleaseStretch * depthNorm;
    const float chargeStretch = 1.0f + cellCharge * NovaVelvetCalibration::kSlowTailDeepenFactor;
    const float effectiveSlowSeconds = manualReleaseSeconds * depthStretch * chargeStretch;
    const float slowReleaseCoeff = std::exp (-1.0f / (juce::jmax (0.001f, effectiveSlowSeconds) * sr));
    const bool risingSlow = envelopeGr > slowPoleGr;
    const float slowCoeffUsed = risingSlow ? attackCoeff : slowReleaseCoeff;
    slowPoleGr = slowCoeffUsed * slowPoleGr + (1.0f - slowCoeffUsed) * envelopeGr;

    // 6. FixMan's historyDependentBlend: a live, continuously-smoothed
    //    weighted combination of peak-hold, true duration, integrated
    //    charge, and detector energy. The duration reference itself is
    //    shifted by the Manual Attack knob - a slower Attack setting
    //    raises the bar a short/percussive hit must clear before the slow
    //    character engages; a faster setting lowers it. This is how Attack
    //    participates in FixMan without acting like Manual's attack.
    const float manualAttackNorm01 = juce::jlimit (0.0f, 1.0f,
        juce::jmap (manualAttackSeconds, NovaVelvetCalibration::kManualAttackMinSeconds,
                    NovaVelvetCalibration::kManualAttackMaxSeconds, 0.0f, 1.0f));
    const float durationRefScale = juce::jmap (manualAttackNorm01,
        1.0f - NovaVelvetCalibration::kFixManAttackInfluenceRange * 0.5f,
        1.0f + NovaVelvetCalibration::kFixManAttackInfluenceRange);
    const float effectiveDurationRef = juce::jmax (0.01f, NovaVelvetCalibration::kFixManDurationRefSeconds * durationRefScale);

    const float historyRaw =
          NovaVelvetCalibration::kFixManPeakWeight       * juce::jlimit (0.0f, 1.0f, peakGrHold / NovaVelvetCalibration::kDeepReductionReferenceDb)
        + NovaVelvetCalibration::kFixManDurationWeight   * juce::jlimit (0.0f, 1.0f, durationSeconds / effectiveDurationRef)
        + NovaVelvetCalibration::kFixManIntegratedWeight * cellCharge
        + NovaVelvetCalibration::kFixManEnergyWeight     * detectorEnergy01;

    historyDependentBlend = historyBlendSmoothCoeff * historyDependentBlend
                           + (1.0f - historyBlendSmoothCoeff) * juce::jlimit (0.0f, 1.0f, historyRaw);

    // 7. Final gain reduction: blend fast->slow pole by modeBlend (computed
    //    at the top of this call from the PREVIOUS sample's blend value in
    //    FixMan mode - a one-sample lag on a value already smoothed over
    //    kFixManBlendSmoothSeconds, inaudible and irrelevant to the
    //    differential-recovery behavior the validation suite measures).
    const float finalGr = fastPoleGr + (slowPoleGr - fastPoleGr) * modeBlend;

    gainReductionDb = juce::jmax (0.0f, finalGr);
    if (! std::isfinite (gainReductionDb))
        gainReductionDb = 0.0f;

    return gainReductionDb;
}

} // namespace nova::velvet
