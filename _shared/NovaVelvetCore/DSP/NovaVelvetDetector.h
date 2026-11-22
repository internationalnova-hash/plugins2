#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace nova::velvet
{

// Fresh design (not adapted from NovaOpticalDetector/NovaVariableMuDetector,
// both of which use a single rectified-then-enveloped magnitude): the spec
// explicitly requires the detector NOT be a pure instantaneous peak
// detector, so this runs two parallel followers on the HPF'd, rectified
// sidechain signal - a fast peak-aware follower and a smoothed energy/RMS-
// style follower - blended together, avoiding chatter/consonant-pumping/
// bass-triggered instability that a pure peak detector would produce on
// real vocal material.
//
// Split into computeSidechainMagnitude()/applyMagnitude() (reusing the
// proven shape from NovaOpticalDetector/NovaVariableMuDetector) so Stereo
// Link can combine two channels' magnitudes before each channel's own
// threshold/knee runs.
class NovaVelvetDetector
{
public:
    NovaVelvetDetector() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setThresholdDbu (float thresholdDbu) noexcept { thresholdDbu_ = thresholdDbu; }
    void setRatio (float ratio) noexcept; // 2..10, continuous
    void setScHpfHz (float hz) noexcept; // 20..400, sidechain-only

    // HPF + rectify + peak/RMS blend only - does not run threshold/knee.
    float computeSidechainMagnitude (float sidechainSample) noexcept;

    // Threshold/ratio/knee transfer on an already-computed magnitude
    // (this channel's own, or a Stereo-Link-blended combination). Returns
    // target GR in dB (>= 0).
    float applyMagnitude (float magnitude) noexcept;

    float processSample (float x) noexcept { return applyMagnitude (computeSidechainMagnitude (x)); }

    // Smoothed 0-1 detector energy, feeds NovaVelvetOpticalCell's Fix-Man blend.
    float getRecentEnergy01() const noexcept { return recentEnergy01; }

private:
    double sampleRate = 44100.0;

    // Sidechain HPF (one-pole).
    float hpfCoeff = 0.0f;
    float hpfState = 0.0f;
    float hpfHz = 20.0f;

    // Fast peak-aware follower.
    float peakAttackCoeff = 0.0f;
    float peakReleaseCoeff = 0.0f;
    float peakFollower = 0.0f;

    // Smoothed energy (mean-square) follower.
    float rmsCoeff = 0.0f;
    float meanSquare = 0.0f;

    // Slow energy-tracking output for Fix-Man (0-1 normalized against a
    // reasonable "loud" reference so it saturates gracefully).
    float energySmoothCoeff = 0.0f;
    float recentEnergy01 = 0.0f;

    float thresholdDbu_ = -20.0f;
    float ratio = 4.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVelvetDetector)
};

} // namespace nova::velvet
