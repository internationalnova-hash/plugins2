#include "NovaBassStereoProcessor.h"
#include <cmath>

namespace nova::bass
{

void NovaBassStereoProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    reset();
    setParams (widthAmount, crossoverHz);
}

void NovaBassStereoProcessor::reset() noexcept { sideLowState = 0.f; }

void NovaBassStereoProcessor::setParams (float newWidthAmount, float newCrossoverHz) noexcept
{
    widthAmount = newWidthAmount;
    crossoverHz = newCrossoverHz;

    if (sampleRate <= 0.0) return;
    const double fc = juce::jmax (10.0, (double) crossoverHz);
    sideLowCoeff = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * fc / sampleRate);
}

void NovaBassStereoProcessor::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumChannels() < 2) return;

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float mid = 0.5f * (l[i] + r[i]);
        const float side = 0.5f * (l[i] - r[i]);

        sideLowState = (sideLowCoeff * sideLowState) + ((1.f - sideLowCoeff) * side);
        const float sideHigh = side - sideLowState;
        const float widenedSide = sideLowState + (sideHigh * widthAmount);

        l[i] = mid + widenedSide;
        r[i] = mid - widenedSide;
    }
}

} // namespace nova::bass
