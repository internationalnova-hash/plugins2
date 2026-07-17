#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "NovaLevelParameters.h"

// Shared compressor DSP engine extracted from Nova Level.
// No APVTS. No UI. No allocations in process().
// Used by: Nova Level standalone, Nova Vox (Comp section).
class NovaLevelDSP
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    // Call once per block before process(). Thread-safe read, then cache.
    void setParameters (const NovaLevelParameters& p);

    // Processes buffer in place. Supports mono and stereo.
    void process (juce::AudioBuffer<float>& buffer);

    // Read after process() on the UI thread (atomic).
    float getGainReductionDb()  const noexcept { return gainReductionDb.load(); }
    float getOutputPeak()       const noexcept { return outputPeak.load();      }
    bool  isOutputHot()         const noexcept { return outputIsHot.load();     }

private:
    NovaLevelParameters params;
    double sampleRate = 44100.0;

    // Per-block DSP state (audio thread only — not atomic)
    float grEnvelopeDb = 0.0f;

    // Meters (written by audio thread, read by UI thread)
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> outputPeak      { 0.0f };
    std::atomic<bool>  outputIsHot     { false };
};
