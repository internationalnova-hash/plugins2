#include "NovaGlueAutoGain.h"
#include <cmath>

namespace nova::glue
{

void NovaGlueAutoGain::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    coeff = std::exp (-1.0f / (kSmoothingSeconds * (float) sampleRate));
    reset();
}

void NovaGlueAutoGain::reset() noexcept
{
    averageGrDb = 0.0f;
}

float NovaGlueAutoGain::process (float gainReductionDb) noexcept
{
    averageGrDb = coeff * averageGrDb + (1.0f - coeff) * juce::jmax (0.0f, gainReductionDb);

    if (! enabled)
        return 1.0f;

    const float compensationDb = averageGrDb * kCompensationFraction;
    return juce::Decibels::decibelsToGain (compensationDb);
}

} // namespace nova::glue
