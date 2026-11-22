#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Decision/NovaMasteringPlan.h"

// Harmonic saturation - tanh blend technique ported from Nova Master
// (PluginProcessor.cpp:431-434: `tanh(x*drive)` blended with dry via
// jmap). Adds 2x oversampling around the nonlinearity (none of the source
// plugins oversample; a mastering stage should, to avoid aliasing).
class NovaSaturation
{
public:
    NovaSaturation() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;
    void setPlan (const NovaMasteringPlan::Saturation& plan) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    NovaMasteringPlan::Saturation plan;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int preparedNumChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaSaturation)
};
