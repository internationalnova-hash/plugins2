#include "NovaBassMutator.h"

using namespace nova::bass;

namespace nova::basslab
{

namespace
{
    float perturb (juce::Random& rng, float value, float span, float fraction, float clampLo, float clampHi) noexcept
    {
        const float delta = (rng.nextFloat() * 2.0f - 1.0f) * fraction * span;
        return juce::jlimit (clampLo, clampHi, value + delta);
    }

    // Categorical fields flip with probability equal to the intensity
    // fraction too - a Wild mutation is more likely to also try a
    // different retrigger/glide/color mode, a Subtle one almost never does.
    bool shouldFlip (juce::Random& rng, float fraction) noexcept
    {
        return rng.nextFloat() < fraction;
    }
}

NovaBassPreset NovaBassMutator::mutate (const NovaBassPreset& current, NovaMutateIntensity intensity, juce::Random& rng)
{
    NovaBassPreset result = current;
    const float f = mutateIntensityFraction (intensity);

    auto& v = result.voiceParams;
    v.sineTriBlend = perturb (rng, v.sineTriBlend, 1.0f, f, 0.0f, 1.0f);
    v.pitchEnvAmountSemitones = perturb (rng, v.pitchEnvAmountSemitones, 24.0f, f, 0.0f, 36.0f);
    v.pitchEnvDecaySeconds = perturb (rng, v.pitchEnvDecaySeconds, 0.5f, f, 0.01f, 2.0f);
    v.pitchEnvCurve = perturb (rng, v.pitchEnvCurve, 1.0f, f, 0.0f, 1.0f);
    v.ampAttackSeconds = perturb (rng, v.ampAttackSeconds, 0.05f, f, 0.0001f, 0.5f);
    v.ampDecaySeconds = perturb (rng, v.ampDecaySeconds, 1.0f, f, 0.01f, 3.0f);
    v.ampSustain = perturb (rng, v.ampSustain, 0.5f, f, 0.0f, 1.0f);
    v.ampReleaseSeconds = perturb (rng, v.ampReleaseSeconds, 0.3f, f, 0.001f, 2.0f);
    v.transientAmount = perturb (rng, v.transientAmount, 0.5f, f, 0.0f, 1.0f);
    v.transientToneHz = perturb (rng, v.transientToneHz, 4000.0f, f, 100.0f, 15000.0f);
    v.transientDecaySeconds = perturb (rng, v.transientDecaySeconds, 0.05f, f, 0.001f, 0.3f);
    v.filterCutoffHz = perturb (rng, v.filterCutoffHz, 6000.0f, f, 100.0f, 20000.0f);
    v.filterResonance = perturb (rng, v.filterResonance, 1.5f, f, 0.1f, 5.0f);
    v.glideTimeSeconds = perturb (rng, v.glideTimeSeconds, 0.2f, f, 0.0f, 1.0f);

    if (shouldFlip (rng, f))
        v.filterHighPass = ! v.filterHighPass;
    if (shouldFlip (rng, f))
        v.retrigger = v.retrigger == NovaVoiceRetrigger::Retrigger ? NovaVoiceRetrigger::Legato : NovaVoiceRetrigger::Retrigger;
    if (shouldFlip (rng, f))
        v.glideMode = v.glideMode == NovaGlideMode::Always ? NovaGlideMode::LegatoOnly : NovaGlideMode::Always;

    result.colorDrive = perturb (rng, result.colorDrive, 0.5f, f, 0.0f, 1.0f);
    if (shouldFlip (rng, f))
    {
        static const NovaColorMode modes[] = { NovaColorMode::Clean, NovaColorMode::Warm, NovaColorMode::Gritty };
        result.colorMode = modes[(size_t) rng.nextInt (3)];
    }

    result.stereoParams.widthAmount = perturb (rng, result.stereoParams.widthAmount, 0.5f, f, 0.0f, 1.0f);

    result.meta.name = current.meta.name + " (mutated)";
    // macros are intentionally NOT mutated - a mutation is a variation on
    // the raw sound, not on how a saved preset's macros would map it.
    result.macros.clear();

    return result;
}

} // namespace nova::basslab
