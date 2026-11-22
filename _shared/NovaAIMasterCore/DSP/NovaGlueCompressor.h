#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Decision/NovaMasteringPlan.h"

// Soft-knee glue compressor. Detector envelope + gain-reduction curve +
// asymmetric onset/release smoothing ported directly from Nova Master
// (Nova Master/Source/PluginProcessor.cpp:29-52, 408-427).
class NovaGlueCompressor
{
public:
    NovaGlueCompressor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;
    void setPlan (const NovaMasteringPlan::Compression& plan) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float getGainReductionDb() const noexcept { return gainReductionDb; }

private:
    NovaMasteringPlan::Compression plan;

    double sampleRate = 44100.0;
    float attackCoeff = 0.f, releaseCoeff = 0.f;
    float envelope = 0.f;
    float gainReductionDb = 0.f;

    static constexpr float kKneeDb = 6.f; // fixed, matches Nova Master's default

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaGlueCompressor)
};
