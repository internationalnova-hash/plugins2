#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "NovaLevelParameters.h"

// Shared compressor DSP engine extracted from Nova Level.
// No APVTS. No UI. No allocations in process().
// Preserves the exact Nova Level algorithm: linked peak detector,
// attack/release envelope, gain reduction, optional magic (tanh) saturation.
//
// Lifecycle:
//   engine.prepare(spec [, initialParams]);
//   engine.setParameters(params);   // once per block
//   engine.process(buffer);          // once per block
//
// Used by: Nova Level standalone, Nova Vox (Comp section), future products.
class NovaLevelDSP
{
public:
    NovaLevelDSP() = default;

    // Prepares the engine for the given sample rate and block size.
    // Pass initialParams to seed the parameter state at startup;
    // omit to use parameter defaults.
    void prepare (const juce::dsp::ProcessSpec& spec,
                  const NovaLevelParameters& initial = {});

    // Clears all runtime state (envelope, meters). Safe to call at any time.
    void reset() noexcept;

    // Set once per block before process(). Cheap struct copy.
    void setParameters (const NovaLevelParameters& parameters) noexcept;

    // Process buffer in place — supports mono and stereo.
    // No memory allocation. No locks.
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    // Meter accessors — safe to call from the UI thread.
    float getGainReductionDb() const noexcept;
    float getOutputPeak()      const noexcept { return outputPeak.load (std::memory_order_relaxed); }
    bool  isOutputHot()        const noexcept { return outputIsHot.load (std::memory_order_relaxed); }

private:
    NovaLevelParameters params;
    double sampleRate = 44100.0;

    // Compressor state — written and read exclusively on the audio thread.
    float grEnvelopeDb = 0.0f;

    // Meters — written by audio thread, read by UI thread.
    std::atomic<float> gainReductionDbAtomic { 0.0f };
    std::atomic<float> outputPeak            { 0.0f };
    std::atomic<bool>  outputIsHot           { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaLevelDSP)
};
