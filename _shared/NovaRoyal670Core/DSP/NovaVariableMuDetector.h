#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace nova::royal670
{

// Detector + 6-mode program-dependent time-constant network. This is an
// original design (not a circuit port - see the catalog-wide instruction
// to describe Royal 670 as "Fairchild 670-inspired" only, never as
// measured/validated circuit accuracy) built to satisfy one explicit
// requirement: a single fixed release coefficient per mode was ruled out,
// so each mode instead blends up to three parallel one-pole followers on
// the target gain-reduction value - a fast pole, a slow (nominal) pole,
// and, for the three longest modes, an extra-slow pole for a deepening
// multi-stage tail. The blend weight is driven by a slow-moving
// "program dependence" estimate (how much/how long the material has been
// pushing GR), so brief transient hits recover faster than sustained
// heavy reduction even within a single mode - the actual "program
// dependent" behavior the spec asks for, not just six different but
// still-fixed numbers.
//
// AC Threshold is the conventional detector-sensitivity/onset threshold.
// DC Threshold ("Behavior") is deliberately NOT a second ordinary
// threshold - it biases the knee width and the effective ratio-to-
// limiting slope, shaping compression *character* (soft/gentle vs.
// hard/limiting) rather than adding another level comparison.
class NovaVariableMuDetector
{
public:
    NovaVariableMuDetector() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // timeConstantMode: 1-6, matching the UI's 0.1/0.2/0.6/1.2/2.0/4.0s
    // positions. sidechainHpfHz: pre-detector HPF cutoff (audio path is
    // never filtered, only the detector's sidechain tap).
    void setParams (float acThresholdDb, float dcThresholdDb, int timeConstantMode, float sidechainHpfHz) noexcept;

    // Convenience: HPF + rectify this channel's own signal, then run the
    // full detector on it. Use the split computeSidechainMagnitude() /
    // applyMagnitude() pair instead when the caller needs to blend this
    // channel's own magnitude against another channel's (e.g. the engine's
    // Link control, which continuously blends toward a shared/linked
    // magnitude across channels without needing a second detector
    // instance).
    float processSample (float sidechainSample) noexcept;

    // HPFs and rectifies sidechainSample, updating this detector's own HPF
    // state, and returns the resulting magnitude - does NOT touch envelope
    // or gain-reduction state.
    float computeSidechainMagnitude (float sidechainSample) noexcept;

    // Runs envelope tracking, threshold/knee, and the 6-mode pole blend on
    // an already-computed magnitude (e.g. from computeSidechainMagnitude(),
    // or a Link-blended combination of two channels' magnitudes). Updates
    // and returns gainReductionDb.
    float applyMagnitude (float magnitude) noexcept;

    // The signed, HPF'd sidechain sample from the most recent
    // computeSidechainMagnitude() call - lets the engine implement
    // Sidechain Listen (auditioning what the detector actually hears)
    // without needing a second HPF pass.
    float getLastHpfedSignal() const noexcept { return lastHpfedSignal; }

    float getGainReductionDb() const noexcept { return gainReductionDb; }

private:
    static constexpr float kBaseRatio = 12.0f;
    static constexpr float kBaseKneeDb = 6.0f;
    static constexpr float kAttackSeconds = 0.003f; // fixed ~3ms instantaneous attack, all modes
    static constexpr float kProgramDependenceSeconds = 0.7f;
    static constexpr float kDeepReductionReferenceDb = 14.0f;

    double sampleRate = 44100.0;

    // Sidechain HPF (one-pole).
    float hpfCoeff = 0.0f;
    float hpfState = 0.0f;
    float hpfHz = 20.0f;

    float acThresholdDb = 0.0f;
    float dcThresholdDb = 0.0f;
    int mode = 4;

    float envelope = 0.0f;

    float attackCoeff = 0.0f;
    float fastCoeff = 0.0f, slowCoeff = 0.0f, extraSlowCoeff = 0.0f;
    float fastGr = 0.0f, slowGr = 0.0f, extraSlowGr = 0.0f;

    float programDependenceCoeff = 0.0f;
    float programDependence = 0.0f;

    float gainReductionDb = 0.0f;
    float lastHpfedSignal = 0.0f;

    void updateModeCoefficients() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVariableMuDetector)
};

} // namespace nova::royal670
