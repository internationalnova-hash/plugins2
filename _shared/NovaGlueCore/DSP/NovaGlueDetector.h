#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace nova::glue
{

// Nova Glue's stereo VCA buss-compressor detector. Follows the spec's own
// block order - SC HPF -> Energy/RMS-style detection -> Attack/Release ->
// Ratio+Knee -> VCA Gain Control - which is the OPPOSITE order from
// Royal 670/Glow (which smooth a post-knee gain-reduction value). Here the
// Attack/Release envelope runs on the detected LEVEL (in dB, already
// threshold-relative) before the knee is applied, matching a real
// feedforward VCA gain-computer topology more closely.
//
// Attack is always one of 6 LOCKED fixed steps (0.1/0.3/1/3/10/30 ms) -
// never program-dependent. Release is either one of 4 LOCKED fixed steps
// (0.1/0.3/0.6/1.2 s) or AUTO - a program-dependent two-pole blend adapted
// from NovaOpticalGainCell's "cellCharge" mechanism
// (_shared/NovaGlowCore/DSP/NovaOpticalGainCell.cpp), proven by that
// plugin's differential-recovery test and re-verified here for Glue's own
// AUTO mode (see NovaGlueDetectorValidation). Because attack/release
// happens before the knee, cellCharge's "drivenness" signal is computed
// straight from the raw over-threshold amount at the input, not from a
// post-knee GR value - this avoids a circular dependency on the envelope's
// own output.
//
// The split computeSidechainMagnitude()/applyMagnitude() pair (same shape
// as NovaVariableMuDetector/NovaOpticalDetector) lets the engine blend two
// channels' magnitudes together before each applies its own envelope/knee,
// for click-free Stereo Link.
class NovaGlueDetector
{
public:
    NovaGlueDetector() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // thresholdDb: -30..+10. scHpfHz: 20-400, sidechain-only.
    // ratioChoice: 0=2:1, 1=4:1, 2=10:1. attackChoice: 0-5 (0.1/0.3/1/3/10/30ms).
    // releaseChoice: 0-3 fixed (0.1/0.3/0.6/1.2s), 4 = AUTO.
    void setParams (float thresholdDb, float scHpfHz, int ratioChoice, int attackChoice, int releaseChoice) noexcept;

    // GLUE macro nudges - see NovaGlueMacro. Each is capped low enough that
    // none of them may raise the steady-state/average gain-reduction depth
    // for a given Threshold/Ratio/input; they only reshape onset/trajectory.
    void setKneeWideningDb (float amountDb) noexcept { kneeWideningDb = juce::jmax (0.0f, amountDb); }
    void setAutoMemoryDepthNudge (float amount01) noexcept { autoMemoryDepthNudge = juce::jlimit (0.0f, 1.0f, amount01); }
    void setDetectorIntegrationNudge (float amount01) noexcept;

    // HPFs the sidechain sample and runs the short energy/RMS-style window
    // (NOT instantaneous peak - a single sample spike cannot dominate this),
    // updating this channel's own filter state, and returns the resulting
    // linear magnitude. Does not touch envelope or gain-reduction state.
    float computeSidechainMagnitude (float sidechainSample) noexcept;

    // Runs the dB conversion, Attack/Release envelope (fixed or AUTO), and
    // Ratio+Knee on an already-computed magnitude (this channel's own, or a
    // Stereo-Link-blended combination). Updates and returns gainReductionDb.
    float applyMagnitude (float magnitude) noexcept;

    // Convenience for callers that don't need Stereo Link blending (e.g.
    // validation harnesses).
    float processSample (float sidechainSample) noexcept { return applyMagnitude (computeSidechainMagnitude (sidechainSample)); }

    float getGainReductionDb() const noexcept { return gainReductionDb; }
    float getCellCharge() const noexcept { return cellCharge; }

private:
    static constexpr int kAutoReleaseChoice = 4;

    // Fixed energy-detection window - not user-adjustable, only nudged
    // slightly by GLUE's detectorIntegrationNudge. Short enough that this
    // is "not instantaneous peak" per spec, without becoming a second,
    // user-facing timing control.
    static constexpr float kEnergyWindowSecondsBase = 0.005f;
    static constexpr float kEnergyWindowNudgeMax = 0.0035f; // up to +3.5ms at Glue=100

    // AUTO release: same shape as NovaOpticalGainCell (see file header),
    // adapted to operate on overDb (level - threshold) rather than a
    // post-knee GR value.
    static constexpr float kAutoDrivenessRangeDb = 12.0f;
    static constexpr float kAutoChargeSeconds = 0.7f;
    static constexpr float kAutoFastReleaseSeconds = 0.15f;
    static constexpr float kAutoSlowReleaseBaseSeconds = 1.0f;
    static constexpr float kAutoSlowDeepenFactor = 2.5f; // effective slow tau up to ~3.5x at full charge

    double sampleRate = 44100.0;

    // Sidechain HPF (one-pole).
    float hpfCoeff = 0.0f;
    float hpfState = 0.0f;
    float hpfHz = 20.0f;

    // Short energy/RMS-style window on the squared, HPF'd signal.
    float energyCoeff = 0.0f;
    float energyState = 0.0f;

    float thresholdDb = -10.0f;
    float slope = 0.5f;
    float baseKneeDb = 6.0f;
    int attackChoice = 4;
    int releaseChoice = kAutoReleaseChoice;

    float attackCoeff = 0.0f;
    float fixedReleaseCoeff = 0.0f;

    // Fixed-release envelope (single pole, used when releaseChoice != AUTO).
    float envelopeOverDb = -160.0f;

    // AUTO release state (used only when releaseChoice == AUTO).
    float cellCharge = 0.0f;
    float chargeCoeff = 0.0f;
    float autoFastReleaseCoeff = 0.0f;
    float fastPoleOverDb = -160.0f;
    float slowPoleOverDb = -160.0f;

    float kneeWideningDb = 0.0f;
    float autoMemoryDepthNudge = 0.0f;
    float detectorIntegrationNudge = 0.0f;

    float gainReductionDb = 0.0f;

    void recomputeAttackCoeff() noexcept;
    void recomputeReleaseCoeff() noexcept;
    void recomputeEnergyCoeff() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaGlueDetector)
};

} // namespace nova::glue
