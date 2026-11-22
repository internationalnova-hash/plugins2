#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Decision/NovaMasteringPlan.h"
#include <array>

// Dynamic resonance/harshness suppression - lightened adaptation of Nova
// Silk's technique (Nova Silk/Source/PluginProcessor.cpp:115-165,371-425):
// a bank of dynamic peak-cut filters applied IN SERIES directly on the
// signal (not a parallel band-split + recombine, which would need a
// complementary crossover design to avoid coloring the signal even at
// zero reduction - cascaded peak filters are exactly unity/transparent
// when their gain is 0dB, so "little to no audible change" when the mix
// doesn't need suppression, spec section 14, falls out for free). 7 bands
// instead of Silk's 24, driven by MasteringPlan::ResonanceControl::amount
// rather than manual macro knobs.
class NovaResonanceControl
{
public:
    NovaResonanceControl() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;
    void setPlan (const NovaMasteringPlan::ResonanceControl& plan) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    static constexpr int kNumBands = 7;

private:
    struct Biquad
    {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
        float z1 = 0.f, z2 = 0.f;
        void reset() noexcept { z1 = z2 = 0.f; }
        inline float processSample (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x + z2 - a1 * y;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    NovaMasteringPlan::ResonanceControl plan;
    double sampleRate = 44100.0;

    // Detection: probe bandpass (measurement only) + envelope follower, per band/channel.
    std::array<Biquad, kNumBands> probeL, probeR;
    std::array<float, kNumBands> envelope {};
    float envAttackCoeff = 0.f, envReleaseCoeff = 0.f;

    // Suppression: cascaded dynamic peak-cut filters applied to the main signal.
    std::array<Biquad, kNumBands> cutL, cutR;
    std::array<float, kNumBands> currentReductionDb {};

    // Slow trend of each band's envelope (~1s-ish), so reduction only
    // engages when a band stands out above its own recent average rather
    // than whenever it's simply loud.
    std::array<float, kNumBands> slowTrend {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaResonanceControl)
};
