#include "NovaOpticalGainCell.h"
#include <cmath>

namespace nova::glow
{

void NovaOpticalGainCell::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    chargeCoeff = std::exp (-1.0f / (kCellChargeSeconds * (float) sampleRate));
    fastReleaseCoeff = std::exp (-1.0f / (kFastReleaseSeconds * (float) sampleRate));
    reset();
}

void NovaOpticalGainCell::reset() noexcept
{
    envelopeGr = 0.0f;
    fastPoleGr = 0.0f;
    slowPoleGr = 0.0f;
    cellCharge = 0.0f;
    gainReductionDb = 0.0f;
}

float NovaOpticalGainCell::process (float targetGrDb) noexcept
{
    targetGrDb = juce::jmax (0.0f, targetGrDb);
    const float sr = (float) sampleRate;

    // 1. Attack: program-dependent (~10ms nominal). A bigger jump from the
    //    currently-tracked envelope attacks faster (down toward
    //    kAttackFastSeconds); a small jump attacks slower (up toward
    //    kAttackSlowSeconds, still centered near 10ms). attackSoftening
    //    (GLOW's contribution) gently widens the slow end of that range -
    //    capped so transients are softened, never destroyed.
    const float jumpNorm = juce::jlimit (0.0f, 1.0f, std::abs (targetGrDb - envelopeGr) / kAttackReferenceDb);
    const float softenedSlowSeconds = kAttackSlowSeconds * (1.0f + attackSoftening * 0.6f);
    const float attackSeconds = juce::jmap (jumpNorm, softenedSlowSeconds, kAttackFastSeconds);
    const float attackCoeff = std::exp (-1.0f / (juce::jmax (0.0005f, attackSeconds) * sr));
    envelopeGr = attackCoeff * envelopeGr + (1.0f - attackCoeff) * targetGrDb;

    // 2. cellCharge: slow "how hard/long has this been driven" tracker.
    //    A short transient can't push this far before the transient ends
    //    and drivenness falls back; a long sustained signal holds
    //    drivenness near 1 long enough for charge to climb close to 1 and
    //    stay there. This single scalar is what makes the cell's release
    //    behavior program-dependent rather than just level-dependent.
    const float drivenness = juce::jlimit (0.0f, 1.0f, envelopeGr / kDeepReductionReferenceDb);
    cellCharge = chargeCoeff * cellCharge + (1.0f - chargeCoeff) * drivenness;

    // 3. Two release poles on envelopeGr (shared attack coefficient when
    //    rising, each pole's own release coefficient when falling) - same
    //    updatePole idiom as NovaVariableMuDetector.
    const bool rising = envelopeGr > fastPoleGr;
    fastPoleGr = (rising ? attackCoeff : fastReleaseCoeff) * fastPoleGr + (1.0f - (rising ? attackCoeff : fastReleaseCoeff)) * envelopeGr;

    // The slow pole's OWN time constant stretches with cellCharge - not
    // just a blend toward a fixed slow value, but a slow pole that itself
    // gets slower the harder/longer the cell has been driven. This is the
    // "slow recovery component becomes more pronounced" requirement.
    const float effectiveSlowSeconds = kSlowReleaseBaseSeconds * (1.0f + cellCharge * kSlowTailDeepenFactor);
    const float slowReleaseCoeff = std::exp (-1.0f / (juce::jmax (0.001f, effectiveSlowSeconds) * sr));
    const bool risingSlow = envelopeGr > slowPoleGr;
    const float slowCoeffUsed = risingSlow ? attackCoeff : slowReleaseCoeff;
    slowPoleGr = slowCoeffUsed * slowPoleGr + (1.0f - slowCoeffUsed) * envelopeGr;

    // 4. Blend fast -> slow by charge*memoryDepth (GLOW raises memoryDepth
    //    to let more of the charged tail show through at full charge).
    const float blend = juce::jlimit (0.0f, 1.0f, cellCharge * memoryDepth);
    const float finalGr = fastPoleGr + (slowPoleGr - fastPoleGr) * blend;

    gainReductionDb = juce::jmax (0.0f, finalGr);
    if (! std::isfinite (gainReductionDb))
        gainReductionDb = 0.0f;

    return gainReductionDb;
}

} // namespace nova::glow
