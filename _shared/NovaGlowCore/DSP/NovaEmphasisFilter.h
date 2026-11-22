#pragma once

#include <juce_dsp/juce_dsp.h>

namespace nova::glow
{

// EMPHASIS reweights the DETECTOR's frequency sensitivity toward upper-mid/
// high content - it is never applied to the audible program signal. A
// single high-shelf IIR filter, recomputed at block rate whenever the
// Emphasis parameter changes, applied only inside the detector's sidechain
// tap (after SC HPF, before rectification) - the same "sidechain-only,
// audio buffer never touched" shape as NovaVariableMuDetector's own HPF.
class NovaEmphasisFilter
{
public:
    NovaEmphasisFilter() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept { filter.reset(); }

    // emphasis01: 0 = flat (no detector weighting change), 1 = full tilt.
    void setAmount (float emphasis01) noexcept;

    float processSample (float sidechainSample) noexcept { return filter.processSample (sidechainSample); }

private:
    static constexpr float kShelfHz = 3000.0f;
    static constexpr float kShelfQ = 0.7f;
    static constexpr float kMaxShelfGainDb = 12.0f;

    double sampleRate = 44100.0;
    float lastAmount01 = -1.0f; // forces a coefficient recompute on first setAmount()
    juce::dsp::IIR::Filter<float> filter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaEmphasisFilter)
};

} // namespace nova::glow
