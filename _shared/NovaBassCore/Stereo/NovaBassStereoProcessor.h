#pragma once

#include <juce_dsp/juce_dsp.h>

namespace nova::bass
{

// Mono-bass-protected stereo width for Nova Bass's WIDTH control. Forked
// from _shared/NovaAIMasterCore/DSP/NovaStereoProcessor (itself ported
// from Nova One's inline widener, PluginProcessor.cpp:248-254,464-474),
// decoupled from NovaMasteringPlan since this catalog's proven one-pole
// leaky-integrator M/S split technique is exactly what spec section 22
// calls for: content below the crossover stays centered/mono-compatible,
// only the high-passed remainder of the side signal gets scaled by width.
// Not a true Linkwitz-Riley crossover, just a cheap phase-safe one-pole
// split - already shipping in production (Nova One), proven adequate.
class NovaBassStereoProcessor
{
public:
    NovaBassStereoProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    // widthAmount: 0 = mono, 1 = unaltered stereo image (matches the input
    // side signal above the crossover). crossoverHz: content below this
    // frequency is protected from widening.
    void setParams (float widthAmount, float crossoverHz) noexcept;

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    double sampleRate = 44100.0;
    float widthAmount = 1.0f;
    float crossoverHz = 150.0f;
    float sideLowCoeff = 0.995f;
    float sideLowState = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaBassStereoProcessor)
};

} // namespace nova::bass
