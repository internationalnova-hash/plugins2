#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Decision/NovaMasteringPlan.h"

// Controlled soft clipping ahead of the limiter. Curve reused from Nova
// Master's applySoftCeiling (tanh-shaped).
//
// STATUS: skeleton only (scaffolding pass) - real clipping lands in
// task #83.
class NovaSoftClipper
{
public:
    NovaSoftClipper() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;
    void setPlan (const NovaMasteringPlan::Clipping& plan) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    NovaMasteringPlan::Clipping plan;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaSoftClipper)
};
