#pragma once

#include "Preset/NovaBassPreset.h"
#include <juce_core/juce_core.h>

namespace nova::basslab
{

// Subtle/Medium/Wild = fractional perturbation of each field's own
// sensible range (not its full theoretical range) - see header comment on
// NovaBassMutator::mutate(). Values chosen so Subtle is a genuinely small
// nudge and Wild is a dramatic but still-bounded variation.
enum class NovaMutateIntensity { Subtle, Medium, Wild };

inline float mutateIntensityFraction (NovaMutateIntensity intensity) noexcept
{
    switch (intensity)
    {
        case NovaMutateIntensity::Subtle: return 0.05f;
        case NovaMutateIntensity::Medium: return 0.15f;
        case NovaMutateIntensity::Wild:   return 0.35f;
    }
    return 0.15f;
}

// One-shot bounded perturbation of a sound (spec section 35). Deliberately
// uses juce::Random directly rather than NovaOrganicModulator (a
// continuous per-sample smoothed random walk meant for audio-rate
// modulation) - Mutate needs a single random nudge per field, not a
// continuous modulation source, so that reuse from the original
// Milestone-A-era sketch was the wrong tool and isn't used here.
class NovaBassMutator
{
public:
    static nova::bass::NovaBassPreset mutate (const nova::bass::NovaBassPreset& current,
                                               NovaMutateIntensity intensity,
                                               juce::Random& rng);
};

} // namespace nova::basslab
