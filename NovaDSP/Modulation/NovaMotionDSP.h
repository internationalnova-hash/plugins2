#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

#include "NovaMotionParameters.h"

// Shared Motion FX DSP engine.
// Rules: no APVTS, no ValueTree, no UI includes.
// No allocation inside process(). All state is owned by this class.
//
// Lifecycle:
//   engine.prepare(spec [, initialParams]);
//   engine.setParameters(params);   // once per block
//   engine.process(buffer);          // once per block
//
// Note (TD-001): 10 SmoothedValues are initialised in prepare() but are NOT
// advanced in process() — matching original Nova Motion FX behaviour exactly.
// Fix scheduled for NovaDSP v2 after full migration is complete.
class NovaMotionDSP
{
public:
    NovaMotionDSP() = default;

    // Prepares the engine for the given sample rate and block size.
    // Pass initialParams to seed the parameter state at startup;
    // omit to use parameter defaults.
    void prepare (const juce::dsp::ProcessSpec& spec,
                  const NovaMotionParameters& initial = {});

    // Clears all runtime DSP state. Safe to call at any time.
    void reset() noexcept;

    // Set once per block before process(). Cheap struct copy.
    void setParameters (const NovaMotionParameters& p) noexcept;

    // Process buffer in place — supports mono and stereo.
    // No memory allocation. No locks.
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    // Meter accessors — safe to call from the UI thread.
    float getPeakL() const noexcept { return peakL.load (std::memory_order_relaxed); }
    float getPeakR() const noexcept { return peakR.load (std::memory_order_relaxed); }

private:
    NovaMotionParameters params;

    double currentSR        { 44100.0 };
    int    maxDelaySamples  { 0 };

    juce::dsp::StateVariableTPTFilter<float> filterL, filterR;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL { 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR { 192000 };
    juce::Reverb reverbL;

    // SmoothedValues initialised in prepare(); not advanced in process() — see TD-001.
    juce::SmoothedValue<float> smCutoff, smResonance, smDrive;
    juce::SmoothedValue<float> smFeedback, smDelayMix;
    juce::SmoothedValue<float> smSize, smReverbMix;
    juce::SmoothedValue<float> smInput, smOutput, smMix;

    juce::AudioBuffer<float> dryBuf;

    bool prevFilterActive { false };
    bool prevDelayActive  { false };
    bool prevReverbActive { false };

    std::atomic<float> peakL { 0.f };
    std::atomic<float> peakR { 0.f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMotionDSP)
};
