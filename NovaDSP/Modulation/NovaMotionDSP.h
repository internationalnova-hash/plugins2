#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

#include "NovaMotionParameters.h"

// Shared Motion FX DSP engine.
// Rules: no APVTS, no ValueTree, no UI includes.
// No allocation inside process(). All state is owned by this class.
class NovaMotionDSP
{
public:
    NovaMotionDSP();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setParameters (const NovaMotionParameters& p) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float getPeakL() const noexcept { return peakL.load(); }
    float getPeakR() const noexcept { return peakR.load(); }

private:
    NovaMotionParameters params;

    double currentSR        { 44100.0 };
    int    maxDelaySamples  { 0 };

    juce::dsp::StateVariableTPTFilter<float> filterL, filterR;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL { 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR { 192000 };
    juce::Reverb reverbL;

    // SmoothedValues are initialised in prepare() — preserved for future use
    // (they are NOT advanced in process() — matching original behaviour exactly)
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
