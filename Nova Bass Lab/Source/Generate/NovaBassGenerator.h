#pragma once

#include "Preset/NovaBassPreset.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace nova::basslab
{

struct NovaBassParamRange
{
    float lo = 0.0f, hi = 1.0f;
    float sample (juce::Random& rng) const noexcept { return lo + rng.nextFloat() * (hi - lo); }
};

// Category-constrained generation ranges (spec section 34: "do not simply
// randomize every parameter from minimum to maximum" - each profile
// encodes a musically sensible sub-range per field, derived from the
// Milestone-A hand-authored factory presets' actual values for that
// category, not guessed).
struct NovaBassGeneratorProfile
{
    juce::String id;
    juce::String category;
    NovaBassParamRange sineTriBlend, pitchEnvAmount, pitchEnvDecay, ampAttack, ampDecay, ampSustain,
                        transientAmount, filterCutoff, filterResonance, colorDrive;
    std::vector<nova::bass::NovaColorMode> allowedColorModes;
};

// Constrained randomization (spec sections 33-34). Never touches
// _shared/NovaBassCore - produces a plain NovaBassPreset (empty macros;
// Lab's deep pages/Macro Editor operate on the raw params directly) that
// NovaBassLabAudioEngine::loadSound() and NovaBassRenderer both consume.
class NovaBassGenerator
{
public:
    static const std::vector<NovaBassGeneratorProfile>& getProfiles();
    static const NovaBassGeneratorProfile* findProfile (const juce::String& profileId);

    static nova::bass::NovaBassPreset generate (const juce::String& profileId, juce::Random& rng);
};

} // namespace nova::basslab
