#include "NovaEmphasisFilter.h"

namespace nova::glow
{

void NovaEmphasisFilter::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    lastAmount01 = -1.0f;
    setAmount (0.0f);
    reset();
}

void NovaEmphasisFilter::setAmount (float emphasis01) noexcept
{
    const float amount = juce::jlimit (0.0f, 1.0f, emphasis01);
    if (juce::approximatelyEqual (amount, lastAmount01))
        return;
    lastAmount01 = amount;

    const float gainDb = amount * kMaxShelfGainDb;
    const float gainFactor = juce::Decibels::decibelsToGain (gainDb);
    *filter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, kShelfHz, kShelfQ, gainFactor);
}

} // namespace nova::glow
