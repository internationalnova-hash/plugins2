#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Decision/NovaMasteringPlan.h"

// Adaptive M/S width. Technique ported from Nova Master
// (PluginProcessor.cpp:436-445): splits the side signal into a slow
// leaky-integrator low band (left untouched, protecting bass mono
// stability) and a high band (scaled by widthAmount). Master hardcodes
// its leaky coefficient (0.995); here it's derived from
// MasteringPlan::Stereo::bassMonoHz via the same one-pole formula used
// elsewhere in this codebase, so the cutoff is correct at any sample rate.
class NovaStereoProcessor
{
public:
    NovaStereoProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;
    void setPlan (const NovaMasteringPlan::Stereo& plan) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    NovaMasteringPlan::Stereo plan;
    double sampleRate = 44100.0;
    float sideLowCoeff = 0.995f;
    float sideLowState = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaStereoProcessor)
};
