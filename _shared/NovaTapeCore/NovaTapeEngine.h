#pragma once

#include "NovaTapeTypes.h"
#include "DSP/NovaTapeSaturator.h"
#include "DSP/NovaTapeHeadFilter.h"
#include "DSP/NovaWowFlutter.h"
#include "DSP/NovaTapeNoise.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

// Top-level facade tying the whole Nova Tape signal chain together - the
// only class PluginProcessor talks to for audio.
//
// Signal path: input gain -> pre-emphasis -> [oversampled: tape
// nonlinearity -> head bump/rolloff] -> de-emphasis -> wow/flutter ->
// noise -> low/high cut -> output gain -> dry/wet mix -> trim -> bypass
// crossfade.
namespace nova::tape
{
    class NovaTapeEngine
    {
    public:
        NovaTapeEngine() = default;

        void prepare (double sampleRate, int maxBlockSize) noexcept;
        void reset() noexcept;

        struct Params
        {
            float inputDb = 0.0f;
            float tapeAmountPercent = 65.0f;
            TapeType tapeType = TapeType::Studio;
            IpsSpeed ips = IpsSpeed::Speed15;
            float biasMinus100to100 = 0.0f;
            float wowFlutter0to10 = 2.4f;
            float noise0to10 = 1.8f;
            float lowCutHz = 30.0f;
            float highCutHz = 18000.0f;
            float mixPercent = 50.0f;
            float outputDb = -1.3f;
            float trimDb = 0.0f;
            int oversamplingStages = 1; // 0=Off, 1=2x, 2=4x
            bool bypassed = false;
        };

        void setParams (const Params& p) noexcept;

        // Message-thread-only - rebuilds the oversampler when the
        // oversamplingStages choice changes (heap-allocates). Mirrors Nova
        // Seven's NovaOversampledColorStage::prepare()-on-change pattern;
        // caller is responsible for suspendProcessing() around this.
        void reconfigureOversamplingIfNeeded (double sampleRate, int maxBlockSize, int desiredStages) noexcept;

        void process (juce::AudioBuffer<float>& buffer) noexcept;

        int getLatencySamples() const noexcept { return oversampler != nullptr ? (int) oversampler->getLatencyInSamples() : 0; }

        // Debug/metering - real (not decorative) level for UI VU meters.
        float getInputLevelDebug() const noexcept { return inputLevelDebug; }
        float getOutputLevelDebug() const noexcept { return outputLevelDebug; }

    private:
        void applyTapeStage (juce::dsp::AudioBlock<float>& block) noexcept;

        double sr = 44100.0;
        int preparedMaxBlockSize = 512;
        int appliedOversamplingStages = -1;

        NovaTapeSaturator saturator; // handles both channels internally (see processSampleL/R)
        NovaTapeHeadFilter headFilter;
        NovaWowFlutter wowFlutter;
        NovaTapeNoise noiseGen;

        juce::dsp::IIR::Filter<float> preEmphasisL, preEmphasisR;
        juce::dsp::IIR::Filter<float> deEmphasisL, deEmphasisR;
        juce::dsp::IIR::Filter<float> lowCutL, lowCutR;
        juce::dsp::IIR::Filter<float> highCutL, highCutR;

        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

        juce::AudioBuffer<float> dryBuffer;
        // Internal 2-channel scratch buffer - ALL internal processing
        // assumes exactly 2 channels (matching how the oversampler is
        // constructed); process() copies a genuinely-mono caller buffer
        // in/out of this at its boundary instead of letting a 1-channel
        // buffer reach internal code that indexes channel 1 unconditionally.
        juce::AudioBuffer<float> workBuffer;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGainSmoothed, outputGainSmoothed,
            mixSmoothed, trimSmoothed, bypassSmoothed;

        TapeType currentTapeType = TapeType::Studio;
        IpsSpeed currentIps = IpsSpeed::Speed15;
        float currentLowCutHz = 30.0f, currentHighCutHz = 18000.0f;
        float currentPreEmphasisDb = 1.0f, currentDeEmphasisDb = 1.0f;

        float inputLevelDebug = 0.0f, outputLevelDebug = 0.0f;
    };
}
