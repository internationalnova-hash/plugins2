#include "NovaVUBallistics.h"
#include <cmath>

namespace nova::seven
{

void NovaVUBallistics::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    coeff = 1.0f - std::exp (-1.0f / (kTimeConstantSeconds * (float) sampleRate));
    reset();
}

void NovaVUBallistics::process (const juce::AudioBuffer<float>& buffer) noexcept
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numChannels == 0 || numSamples == 0)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float rectifiedSum = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
            rectifiedSum += std::abs (buffer.getSample (channel, sample));

        const float rectified = rectifiedSum / (float) numChannels;

        // Same one-pole coefficient in both directions - symmetric
        // attack/release is the defining trait of real VU ballistics
        // (unlike a peak meter's fast-attack/slow-release asymmetry).
        displayLevel += (rectified - displayLevel) * coeff;
    }
}

float NovaVUBallistics::getDisplayLevelVu() const noexcept
{
    const float dbfs = juce::Decibels::gainToDecibels (displayLevel, -100.0f);
    return dbfs - kVuReferenceDbfs;
}

} // namespace nova::seven
