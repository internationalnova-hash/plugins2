#include "NovaSoftClipper.h"
#include <cmath>

namespace
{
    // Core shaping curve ported verbatim from Nova Master
    // (PluginProcessor.cpp:54-60) - shaped(ceiling) == ceiling exactly, so
    // it's continuous at the boundary, but is NOT identity below it: the
    // /tanh(0.82) normalization means anything under the ceiling gets a
    // consistent ~1.2x boost, not left alone. That's fine in Master's own
    // chain (signal is already riding near the ceiling by the time it gets
    // here), but wrong as a general-purpose safety net that should do
    // nothing when nothing needs limiting - gate it to only shape samples
    // actually at or above the ceiling; everything below passes through
    // untouched.
    float applySoftCeilingCurve (float sample, float ceilingLinear) noexcept
    {
        const auto safeCeiling = juce::jmax (0.05f, ceilingLinear);
        const auto normalised = sample / safeCeiling;
        const auto shaped = std::tanh (normalised * 0.82f) / std::tanh (0.82f);
        return shaped * safeCeiling;
    }

    float applySoftCeiling (float sample, float ceilingLinear) noexcept
    {
        return std::abs (sample) >= ceilingLinear ? applySoftCeilingCurve (sample, ceilingLinear) : sample;
    }
}

void NovaSoftClipper::prepare (const juce::dsp::ProcessSpec&) { reset(); }
void NovaSoftClipper::reset() noexcept {}
void NovaSoftClipper::setPlan (const NovaMasteringPlan::Clipping& p) noexcept { plan = p; }

void NovaSoftClipper::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! plan.active) return;

    const float ceiling = juce::Decibels::decibelsToGain (plan.ceilingDb);
    const int nCh = buffer.getNumChannels();
    const int N   = buffer.getNumSamples();

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < N; ++i)
            d[i] = applySoftCeiling (d[i], ceiling);
    }
}
