#include "NovaStereoModeCrossfader.h"
#include <cmath>

namespace nova::royal670
{

void NovaStereoModeCrossfader::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    transitionStep = 1.0f / (kTransitionSeconds * (float) sampleRate);
    reset();
}

void NovaStereoModeCrossfader::reset() noexcept
{
    previousMode = activeMode;
    transitioning = false;
    transitionPos = 1.0f;
}

void NovaStereoModeCrossfader::setMode (Mode newMode) noexcept
{
    if (newMode == activeMode)
        return;

    previousMode = activeMode;
    activeMode = newMode;
    transitioning = true;
    transitionPos = 0.0f;
}

float NovaStereoModeCrossfader::advanceSample() noexcept
{
    if (! transitioning)
        return 1.0f;

    transitionPos += transitionStep;
    if (transitionPos >= 1.0f)
    {
        transitionPos = 1.0f;
        transitioning = false;
    }

    // Equal-power crossfade shape, matching the catalog's established MIX
    // idiom (e.g. NovaDoublerEngine's dry/wet blend).
    return std::sin (transitionPos * juce::MathConstants<float>::halfPi);
}

} // namespace nova::royal670
