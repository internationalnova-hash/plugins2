#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "NovaEmphasisFilter.h"

namespace nova::glow
{

// Dedicated optical detector: SC HPF (sidechain-only) -> Emphasis tilt
// (sidechain-only) -> rectify -> soft-knee "illumination" curve that
// increasingly drives gain reduction as the signal goes further over
// threshold (the knee's nonlinear ramp is what "increasingly illuminate as
// signal grows" concretely means here - a smooth, non-instantaneous-peak
// response, not a literal photocell circuit model). Compress/Limit changes
// the knee width and slope directly (see setCompressLimitBlend()), not
// just a label.
//
// This does NOT include the optical gain cell's program-dependent
// attack/release/memory - that lives in NovaOpticalGainCell, driven by
// this detector's per-sample targetGrDb output. Splitting the two lets
// NovaGlowDetectorValidation test the detector's threshold/knee/HPF/
// Emphasis behavior independently from the gain cell's timing behavior.
class NovaOpticalDetector
{
public:
    NovaOpticalDetector() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // thresholdDb: from Peak Reduction's shaped 0-100->dB mapping (done by
    // the caller/engine). scHpfHz: 20-400Hz, sidechain-only. emphasis01:
    // 0-1, sidechain-only detector frequency weighting.
    void setParams (float thresholdDb, float scHpfHz, float emphasis01) noexcept;

    // compressLimitBlend01: 0 = Compress (soft knee, moderate ratio),
    // 1 = Limit (narrow knee, near-limiting ratio). Caller SmoothedValue-
    // ramps this for click-free switching - see NovaGlowEngine.
    void setCompressLimitBlend (float compressLimitBlend01) noexcept;

    // HPFs + Emphasis-tilts + rectifies sidechainSample, updating internal
    // filter state, and returns the resulting magnitude - does NOT run the
    // threshold/knee. Split out (rather than one combined call) so Stereo
    // Link can blend two channels' magnitudes together before each
    // channel's own applyMagnitude() - same shape as
    // NovaVariableMuDetector::computeSidechainMagnitude().
    float computeSidechainMagnitude (float sidechainSample) noexcept;

    // Runs the threshold/knee on an already-computed magnitude (this
    // channel's own, or a Stereo-Link-blended combination) and returns the
    // target GR in dB (>= 0). Does not touch the optical gain cell.
    float applyMagnitude (float magnitude) noexcept;

    // Convenience: computeSidechainMagnitude() + applyMagnitude() on this
    // channel's own signal, for callers that don't need Stereo Link
    // blending (e.g. validation harnesses).
    float processSample (float sidechainSample) noexcept { return applyMagnitude (computeSidechainMagnitude (sidechainSample)); }

    float getLastSidechainMagnitude() const noexcept { return lastMagnitude; }

private:
    static constexpr float kCompressSlope = 1.0f - (1.0f / 3.0f); // ~3:1 classic optical ratio
    static constexpr float kCompressKneeDb = 9.0f;                // wide, soft
    static constexpr float kLimitSlope = 0.95f;                   // near-limiting
    static constexpr float kLimitKneeDb = 2.5f;                   // narrow, assertive

    double sampleRate = 44100.0;

    // Sidechain HPF (one-pole), mirroring NovaVariableMuDetector's idiom.
    float hpfCoeff = 0.0f;
    float hpfState = 0.0f;
    float hpfHz = 30.0f;

    NovaEmphasisFilter emphasisFilter;

    float thresholdDb = -18.0f;
    float compressLimitBlend = 0.0f;
    float lastMagnitude = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaOpticalDetector)
};

} // namespace nova::glow
