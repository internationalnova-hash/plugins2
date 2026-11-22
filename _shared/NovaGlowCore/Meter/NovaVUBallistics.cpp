#include "NovaVUBallistics.h"
#include <cmath>

namespace nova::glow
{

void NovaVUBallistics::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    coeff = 1.0f - std::exp (-1.0f / (kTimeConstantSeconds * (float) sampleRate));
    reset();
}

void NovaVUBallistics::process (const float* channelData, int numSamples) noexcept
{
    if (channelData == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const float rectified = std::abs (channelData[i]);

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

} // namespace nova::glow
