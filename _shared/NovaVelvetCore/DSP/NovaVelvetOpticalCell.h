#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace nova::velvet
{

// The centerpiece of Nova Velvet: a program-dependent optical gain-
// reduction cell, explicitly NOT juce::dsp::Compressor. Adapts the
// two-pole-plus-charge-accumulator SHAPE proven in this catalog's
// NovaOpticalGainCell (_shared/NovaGlowCore/DSP/) and NovaVariableMuDetector
// (_shared/NovaRoyal670Core/DSP/), but is architecturally new: neither of
// those classes has a discrete timing-mode selector, and both do fully
// automatic program-dependence blending rather than a genuine manual
// dual-stage crossfade.
//
// Time Select has three modes:
//   Fixed  - hardcoded ~1ms attack / ~50ms release, immune to the manual
//            Attack/Release knobs entirely.
//   Manual - the knob values are the primary timing targets, still shaped
//            by jump-size-normalized attack softening (never a bare
//            exponential coefficient).
//   FixMan - NOT a static midpoint of Fixed and Manual. A single internal
//            modeBlend in [0,1] is applied uniformly to both the attack law
//            and a two-persistent-pole release system; in FixMan mode that
//            blend is `historyDependentBlend`, a LIVE, continuously-
//            smoothed combination of peak-GR-hold, true elapsed duration
//            under compression, an integrated-GR "cellCharge" accumulator,
//            and the detector's own recent energy. Short events keep the
//            blend near 0 (fast/Fixed-like recovery); sustained events
//            drive it toward 1 (slow/Manual-like recovery) DURING the
//            event, not after a fixed delay - this is what makes a short
//            20-50ms transient and a sustained 1-3s event at matched peak
//            GR recover on measurably different trajectories even in
//            FixMan, satisfying the spec's mandatory program-dependent-
//            release requirement without ever being a simple average.
//
// All state is bounded one-pole IIR with clamp01/isfinite guards -
// deterministic, no randomness, no instability risk.
class NovaVelvetOpticalCell
{
public:
    enum class TimeSelectMode { Fixed, FixMan, Manual };

    NovaVelvetOpticalCell() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setTimeSelectMode (TimeSelectMode mode) noexcept { timeSelectMode = mode; }
    void setManualAttackSeconds (float seconds) noexcept;
    void setManualReleaseSeconds (float seconds) noexcept;

    // detectorEnergy01: from NovaVelvetDetector::getRecentEnergy01(), used
    // only by the FixMan blend (not by Fixed or Manual timing at all).
    float process (float targetGrDb, float detectorEnergy01) noexcept;

    float getGainReductionDb() const noexcept { return gainReductionDb; }
    float getCellCharge() const noexcept { return cellCharge; }
    float getHistoryBlend() const noexcept { return historyDependentBlend; } // validation hook

private:
    double sampleRate = 44100.0;

    TimeSelectMode timeSelectMode = TimeSelectMode::FixMan;
    float manualAttackSeconds = 0.010f;
    float manualReleaseSeconds = 0.300f;

    // Shared attack envelope (both release poles chase this).
    float envelopeGr = 0.0f;

    // Two persistent release poles - always both running so mode switches
    // are click-free (never reset/skipped by mode).
    float fastPoleGr = 0.0f;
    float slowPoleGr = 0.0f;

    // Program-dependence state (FixMan's historyDependentBlend inputs).
    float cellCharge = 0.0f; // leaky integral of envelopeGr/kDeepReductionReferenceDb
    float peakGrHold = 0.0f; // instant-attack/slow-decay peak-and-hold of envelopeGr
    float durationSeconds = 0.0f; // true elapsed time above the active-GR threshold
    float historyDependentBlend = 0.0f; // live, smoothed FixMan blend weight

    // Pre-baked coefficients.
    float chargeCoeff = 0.0f;
    float fastReleaseCoeff = 0.0f;
    float peakHoldDecayCoeff = 0.0f;
    float durationHangCoeff = 0.0f;
    float historyBlendSmoothCoeff = 0.0f;

    float gainReductionDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVelvetOpticalCell)
};

} // namespace nova::velvet
