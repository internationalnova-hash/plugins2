#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

// Offline loudness analyzer: integrated/short-term LUFS per ITU-R BS.1770-4
// (K-weighting + two-stage gating), loudness range (LRA) per EBU Tech 3342,
// and oversampled true peak (dBTP). Runs faster-than-realtime over a whole
// imported/captured buffer, fed via successive process() calls - not called
// from an audio callback.
//
// K-weighting filter coefficients are derived at prepare()-time from the
// documented BS.1770 analog-prototype f0/Q/gain values via the standard RBJ
// biquad cookbook formulas, so they're correct at any sample rate (not just
// the commonly-quoted 48kHz table values).
class NovaLoudnessAnalyzer
{
public:
    NovaLoudnessAnalyzer() = default;

    void prepare (double sampleRate);
    void reset() noexcept;

    // Feed the whole mix (or successive chunks of it, in order) before
    // reading results below.
    void process (const juce::AudioBuffer<float>& buffer) noexcept;

    // Call once after all process() calls, before reading getters.
    void finalize() noexcept;

    float getIntegratedLufs() const noexcept  { return integratedLufs; }
    float getShortTermLufs() const noexcept   { return shortTermLufs; }
    float getLoudnessRangeLu() const noexcept { return loudnessRangeLu; }
    float getTruePeakDb() const noexcept      { return truePeakDb; }
    float getSamplePeakDb() const noexcept    { return samplePeakDb; }

private:
    struct Biquad
    {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
        float z1 = 0.f, z2 = 0.f;

        void reset() noexcept { z1 = z2 = 0.f; }

        inline float processSample (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x + z2 - a1 * y;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    struct KWeightFilter
    {
        Biquad stage1; // pre-filter, high shelf
        Biquad stage2; // RLB, high-pass

        void reset() noexcept { stage1.reset(); stage2.reset(); }
        inline float processSample (float x) noexcept { return stage2.processSample (stage1.processSample (x)); }
    };

    void designKWeightFilter (KWeightFilter& f) const;

    double sampleRate = 44100.0;

    KWeightFilter kFilterL, kFilterR;

    int    hopSamples = 0;
    int    hopSampleCount = 0;
    double hopSumSqL = 0.0, hopSumSqR = 0.0;

    // One entry per completed 100ms hop: mean-square (already averaged over
    // the hop's samples) per channel, post K-weighting.
    std::vector<std::pair<float, float>> hopMeanSquares;

    // True peak via 4x oversampling (FIR, linear-phase - appropriate for
    // peak measurement, latency is irrelevant for offline analysis).
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    float integratedLufs  = -70.f;
    float shortTermLufs   = -70.f;
    float loudnessRangeLu = 0.f;
    float truePeakDb      = -100.f;
    float samplePeakDb    = -100.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaLoudnessAnalyzer)
};
