#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Decision/NovaMasteringPlan.h"
#include "NovaCorrectiveEQ.h"
#include "NovaResonanceControl.h"
#include "NovaGlueCompressor.h"
#include "NovaSaturation.h"
#include "NovaStereoProcessor.h"
#include "NovaSoftClipper.h"
#include "NovaLimiter.h"

// Orchestrates the full mastering chain in spec section 11's order:
// Corrective EQ -> Resonance Control -> Glue Compression -> Saturation ->
// Stereo -> Soft Clip -> True-Peak Limiting -> Output gain.
// Single prepare()/setPlan()/process() entry point, matching the
// NovaLevelDSP contract (NovaDSP/Dynamics/NovaLevelDSP.h) - this is the
// class both the plugin and the standalone app's offline engine call.
//
// Parameter smoothing (spec section 30 step 8: "DSP parameters transition
// smoothly into place"): a coefficient jump on filters holding persistent
// state (EQ, resonance control) can click. Rather than smoothing each
// stage's internals individually, setPlan() keeps two full instances of
// the chain and crossfades their OUTPUT over a short window whenever the
// plan changes - simple, stage-agnostic, and only costs double processing
// during the brief transition (a "Master" press happens once per session,
// not continuously).
class NovaMasteringChain
{
public:
    NovaMasteringChain() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;
    void setPlan (const NovaMasteringPlan& plan) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    struct ChainStages
    {
        NovaCorrectiveEQ      eq;
        NovaResonanceControl  resonance;
        NovaGlueCompressor    glue;
        NovaSaturation        saturation;
        NovaStereoProcessor   stereo;
        NovaSoftClipper       clipper;
        NovaLimiter           limiter;

        void prepare (const juce::dsp::ProcessSpec& spec);
        void reset() noexcept;
        void setPlan (const NovaMasteringPlan& plan) noexcept;
        void process (juce::AudioBuffer<float>& buffer) noexcept;
    };

    ChainStages stagesA, stagesB;
    bool usingA = true;
    bool hasAppliedFirstPlan = false;

    int crossfadeTotalSamples = 0;
    int crossfadeSamplesRemaining = 0;

    juce::AudioBuffer<float> crossfadeScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMasteringChain)
};
