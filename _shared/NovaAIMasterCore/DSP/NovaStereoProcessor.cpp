#include "NovaStereoProcessor.h"
#include <cmath>

void NovaStereoProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    reset();
    setPlan (plan);
}

void NovaStereoProcessor::reset() noexcept { sideLowState = 0.f; }

void NovaStereoProcessor::setPlan (const NovaMasteringPlan::Stereo& p) noexcept
{
    plan = p;
    if (sampleRate <= 0.0) return;
    const double fc = juce::jmax (10.0, (double) plan.bassMonoHz);
    sideLowCoeff = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * fc / sampleRate);
}

void NovaStereoProcessor::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! plan.active || buffer.getNumChannels() < 2) return;

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);
    const int N = buffer.getNumSamples();

    for (int i = 0; i < N; ++i)
    {
        const float mid  = 0.5f * (l[i] + r[i]);
        const float side = 0.5f * (l[i] - r[i]);

        sideLowState = (sideLowCoeff * sideLowState) + ((1.f - sideLowCoeff) * side);
        const float sideHigh = side - sideLowState;
        const float widenedSide = sideLowState + (sideHigh * plan.widthAmount);

        l[i] = mid + widenedSide;
        r[i] = mid - widenedSide;
    }
}
