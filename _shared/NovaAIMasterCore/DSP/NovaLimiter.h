#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include "../Decision/NovaMasteringPlan.h"

// Soft-knee limiter + true-peak-safe ceiling.
//
// Design: (1) a sample-domain gain-reduction limiter using Nova Master's
// exact proven pattern (peak detector -> computeGainReductionDb ->
// asymmetric attack/release smoothing of the GR value itself); (2) a true
// peak verification pass - oversample the gain-reduced result, and if the
// true (inter-sample) peak still exceeds the ceiling, apply one corrective
// broadband trim to bring it under; (3) Master's tanh soft ceiling as a
// final safety net. This is a deliberately simpler hybrid rather than a
// full oversampled-domain per-sample limiter (which would need envelope
// tracking across the up/downsample boundary) - genuinely catches
// inter-sample peaks the original sample-domain-only design would miss,
// without the complexity of a from-scratch oversampled limiter for v0.1.
class NovaLimiter
{
public:
    NovaLimiter() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;
    void setPlan (const NovaMasteringPlan::Limiting& plan) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float getGainReductionDb() const noexcept { return gainReductionDb; }
    int getLatencySamples() const noexcept { return delay; }   // the same in every mode
    /// Take over another limiter's look-ahead audio (so a crossfade into this one starts from the same audio).
    void continueFrom (const NovaLimiter& o) noexcept
    {
        if (o.delay != delay || o.delayRing.size() != delayRing.size()) return;
        delayRing = o.delayRing; delayPos = o.delayPos;
        ispHist = o.ispHist; ispPos = o.ispPos;
        holdRing = o.holdRing; holdPos = o.holdPos;
        boxRing = o.boxRing; boxPos = o.boxPos; boxSum = o.boxSum; env = o.env;
    }

private:
    NovaMasteringPlan::Limiting plan;

    double sampleRate = 44100.0;
    float releaseCoeffRate = 0.006f;
    float gainReductionDb = 0.f;

    // look-ahead (brickwall plans) - and the matching plain delay for the original mode
    static constexpr int kIspTaps = 16;
    void processLookAhead (juce::AudioBuffer<float>& buffer) noexcept;
    void delayOnly (juce::AudioBuffer<float>& buffer) noexcept;
    int look = 72, delay = 76, hold = 77;
    std::vector<std::vector<float>> delayRing;
    std::vector<std::array<float, kIspTaps>> ispHist;
    std::array<std::array<float, kIspTaps>, 3> ispCoef {};
    std::vector<float> holdRing, boxRing;
    double boxSum = 0.0;
    float env = 1.f;
    int delayPos = 0, holdPos = 0, boxPos = 0, ispPos = 0, sinceResum = 0;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int preparedNumChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaLimiter)
};
