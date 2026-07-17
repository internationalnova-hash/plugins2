#pragma once

#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "NovaSpaceParameters.h"

// Shared Space reverb DSP engine.
// Rules: no APVTS, no ValueTree, no UI includes.
// No allocation inside process(). All state is owned by this class.
//
// SmoothedValues are advanced per-block via skip() — this IS intentional and
// matches the original algorithm. The smoothers interpolate between blocks,
// not between samples.
//
// Call seedSmoothedValues() immediately after prepare() in prepareToPlay()
// to match the original seeding behaviour (which read from APVTS).
class NovaSpaceDSP
{
public:
    NovaSpaceDSP();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    // Call once after prepare() to seed smoothers at the current parameter
    // values — mirrors the original prepareToPlay() APVTS read.
    void seedSmoothedValues (const NovaSpaceParameters& p) noexcept;

    void setParameters (const NovaSpaceParameters& p) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    using Filter   = juce::dsp::StateVariableTPTFilter<float>;
    using DelayLine = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;

    static float clamp01 (float v) noexcept { return juce::jlimit (0.0f, 1.0f, v); }

    float readEarlyTap (const std::vector<float>& source, float delaySamples) const noexcept;

    NovaSpaceParameters params;
    double currentSampleRate { 44100.0 };

    juce::Reverb reverb;
    DelayLine preDelayLeft  { 96000 };
    DelayLine preDelayRight { 96000 };
    DelayLine decorrelationDelay { 4096 };

    Filter wetToneLeft,  wetToneRight;
    Filter wetBodyLeft,  wetBodyRight;
    Filter earlyToneLeft, earlyToneRight;
    Filter earlyBodyLeft, earlyBodyRight;

    juce::LinearSmoothedValue<float> smoothedSpace;
    juce::LinearSmoothedValue<float> smoothedAir;
    juce::LinearSmoothedValue<float> smoothedDepth;
    juce::LinearSmoothedValue<float> smoothedMix;
    juce::LinearSmoothedValue<float> smoothedWidth;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> earlyBuffer;

    std::vector<float> earlyTapBufferLeft;
    std::vector<float> earlyTapBufferRight;
    int   earlyTapWriteIndex { 0 };
    int   earlyTapBufferSize { 0 };
    float earlyDiffuseStateLeft  { 0.0f };
    float earlyDiffuseStateRight { 0.0f };

    float motionPhase { 0.0f };
    float haloPhase   { 1.7f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaSpaceDSP)
};
