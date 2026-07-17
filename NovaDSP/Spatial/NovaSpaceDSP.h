#pragma once

#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

#include "NovaSpaceParameters.h"

// Shared Space reverb DSP engine.
// Rules: no APVTS, no ValueTree, no UI includes.
// No allocation inside process(). All state is owned by this class.
//
// Lifecycle:
//   engine.prepare(spec [, initialParams]);
//   engine.setParameters(params);   // once per block
//   engine.process(buffer);          // once per block
//
// SmoothedValues are advanced per-block via skip() — intentional, matching
// the original algorithm. Smoothers are seeded from initialParams in prepare().
class NovaSpaceDSP
{
public:
    NovaSpaceDSP() = default;

    // Prepares the engine for the given sample rate and block size.
    // Pass initialParams to seed the smoothers at startup (mirrors the original
    // prepareToPlay() APVTS read); omit to use parameter defaults.
    void prepare (const juce::dsp::ProcessSpec& spec,
                  const NovaSpaceParameters& initial = {});

    // Clears all runtime DSP state. Safe to call at any time.
    void reset() noexcept;

    // Set once per block before process(). Cheap struct copy.
    void setParameters (const NovaSpaceParameters& p) noexcept;

    // Process buffer in place — supports mono and stereo.
    // No memory allocation. No locks.
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    using Filter    = juce::dsp::StateVariableTPTFilter<float>;
    using DelayLine = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;

    static float clamp01 (float v) noexcept { return juce::jlimit (0.0f, 1.0f, v); }

    float readEarlyTap (const std::vector<float>& source, float delaySamples) const noexcept;

    // Seeds all five LinearSmoothedValues from a parameter struct.
    // Called internally from prepare().
    void seedSmoothedValues (const NovaSpaceParameters& p) noexcept;

    NovaSpaceParameters params;
    double currentSampleRate { 44100.0 };

    juce::Reverb reverb;
    DelayLine preDelayLeft   { 96000 };
    DelayLine preDelayRight  { 96000 };
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
