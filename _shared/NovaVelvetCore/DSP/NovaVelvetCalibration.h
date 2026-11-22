#pragma once

namespace nova::velvet
{

// Centralized calibration constants for the entire engine. Every DSP class
// reads these directly (NovaVelvetCalibration::kFoo) rather than keeping its
// own private constexpr copies - a deliberately stronger centralization than
// this catalog's usual per-class-constexpr convention (see NovaOpticalGainCell,
// NovaTubeStage), because Nova Velvet's spec explicitly requires a later
// real-hardware-capture calibration pass to be able to retune the whole
// engine from one file without touching class internals.
//
// The hardware reference spec is in dBu; the plugin runs in dBFS. 0dBu is
// NOT 0dBFS - kReferenceDbfs defines the mapping (dBFS = dBu + kReferenceDbfs).
// -18dBFS = 0dBu is chosen to match this catalog's own existing convention
// (NovaVUBallistics already treats -18dBFS as its 0VU reference point in
// Glow/Royal670/Glue/7even), not because it has been measured against a real
// CL 1B - that measurement is Phase 14, PENDING_HARDWARE_REFERENCE.
struct NovaVelvetCalibration
{
    static constexpr float kReferenceDbfs = -18.0f; // 0dBu == this many dBFS. PENDING_HARDWARE_REFERENCE.
    static constexpr float kInputCalibrationTrimDb = 0.0f; // PENDING_HARDWARE_REFERENCE

    // Threshold: UI range is dBu (see ParameterIDs); "OFF" sits at the top.
    static constexpr float kThresholdMinDbu = -40.0f;
    static constexpr float kThresholdMaxDbu = 10.0f; // effectively "OFF"

    // Ratio / knee: soft, ratio-scaled, never a hard corner even at 10:1.
    static constexpr float kRatioMin = 2.0f;
    static constexpr float kRatioMax = 10.0f;
    static constexpr float kKneeDbAtRatioMin = 10.0f;
    static constexpr float kKneeDbAtRatioMax = 5.0f; // still soft at 10:1, never a hard corner

    // Detector: fast peak-aware component blended with a smoothed energy
    // component - not a pure peak detector (spec's explicit requirement).
    static constexpr float kDetectorPeakAttackSeconds = 0.0008f;
    static constexpr float kDetectorPeakReleaseSeconds = 0.015f;
    static constexpr float kDetectorRmsWindowSeconds = 0.030f;
    static constexpr float kDetectorPeakBlendAtNominal = 0.6f; // 60% peak / 40% energy
    static constexpr float kDetectorEnergySmoothSeconds = 0.25f; // feeds Fix-Man + telemetry

    // Fixed timing (Time Select = FIXED) - hardcoded, immune to manual knobs.
    static constexpr float kFixedAttackSeconds = 0.001f;
    static constexpr float kFixedReleaseSeconds = 0.050f;

    // Manual mapping ranges (APVTS ranges mirror these).
    static constexpr float kManualAttackMinSeconds = 0.0005f;
    static constexpr float kManualAttackMaxSeconds = 0.300f;
    static constexpr float kManualReleaseMinSeconds = 0.050f;
    static constexpr float kManualReleaseMaxSeconds = 10.0f;

    // Optical cell: jump-size-normalized attack softening.
    static constexpr float kAttackJumpReferenceDb = 10.0f;
    static constexpr float kAttackSofteningRange = 0.6f;

    // Optical memory / program dependency.
    static constexpr float kCellChargeSeconds = 0.9f; // leaky "integrated GR" accumulator
    static constexpr float kDeepReductionReferenceDb = 14.0f;
    static constexpr float kSlowTailDeepenFactor = 2.0f; // duration/charge-driven tau stretch
    static constexpr float kGrDepthReleaseStretch = 0.6f; // instantaneous-depth-driven tau stretch
    static constexpr float kPeakHoldDecaySeconds = 1.5f;

    // Fix-Man dual-stage blend (the novel piece - see NovaVelvetOpticalCell).
    static constexpr float kFixManDurationRefSeconds = 0.6f;
    static constexpr float kFixManActiveGrThresholdDb = 0.3f;
    static constexpr float kFixManDurationHangSeconds = 0.15f;
    static constexpr float kFixManBlendSmoothSeconds = 0.35f; // continuous-crossfade tau, never a hard switch
    static constexpr float kFixManPeakWeight = 0.35f;
    static constexpr float kFixManDurationWeight = 0.35f;
    static constexpr float kFixManIntegratedWeight = 0.20f;
    static constexpr float kFixManEnergyWeight = 0.10f; // sums to 1.0
    static constexpr float kFixManAttackInfluenceRange = 0.5f;
    static constexpr float kModeTransitionSmoothSeconds = 0.04f; // Time Select click-free crossfade

    // Tube push-pull stage.
    static constexpr float kTubeEvenHarmonicBiasBaseline = 0.18f; // restrained - push-pull default
    static constexpr float kTubeLevelDependentBiasGrowth = 0.12f; // bias grows a bit with level
    static constexpr float kTubeDriveRangeMax = 2.5f; // Tube Drive 0-1 -> 0..this
    static constexpr float kTubeGainCouplingMax = 0.3f; // GAIN's small coupling into tube drive
    static constexpr float kTubeDriveMakeupDbAtMax = -2.5f; // deliberately PARTIAL compensation only

    // Input / output transformers.
    static constexpr float kInputTransformerBaselineDrive = 0.12f; // far below Nova Heat's default
    static constexpr float kInputTransformerLevelCoupling = 0.20f;
    static constexpr float kInputTransformerLowSplitHz = 100.0f;
    static constexpr float kOutputTransformerBaselineDrive = 0.20f;
    static constexpr float kOutputTransformerLevelCoupling = 0.30f;
    static constexpr float kOutputTransformerLowSplitHz = 150.0f;
    static constexpr float kOutputTransformerHfSoftenHz = 11000.0f;
    static constexpr float kOutputTransformerLfDrive = 0.15f;

    // Level envelopes driving level-coupled transformer drive.
    static constexpr float kLevelEnvelopeAttackSeconds = 0.005f;
    static constexpr float kLevelEnvelopeReleaseSeconds = 0.200f;

    // Oversampling: Realtime = 2x, High Quality = 4x (spec's own two-option
    // request - simpler than this catalog's usual 4-option Off/2x/4x/8x).
    static constexpr int kOversamplingStagesRealtime = 1; // 2x
    static constexpr int kOversamplingStagesHighQuality = 2; // 4x

    // Block-rate smoothing (mirrors NovaGlowEngine's kBlockSmoothSeconds idiom).
    static constexpr float kParamSmoothSeconds = 0.02f;
    static constexpr float kBypassSmoothSeconds = 0.03f;
    static constexpr float kBlockSmoothSeconds = 0.03f;
};

} // namespace nova::velvet
