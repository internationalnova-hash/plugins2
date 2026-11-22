#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace nova::glow
{

// The heart of Nova Glow: a program-dependent optical gain-reduction cell.
// Adapts NovaVariableMuDetector's multi-pole-blend idiom (see
// _shared/NovaRoyal670Core/DSP/NovaVariableMuDetector.cpp) into a
// continuous, self-deepening two-stage release rather than 6 discrete
// modes - see the design note on cellCharge below for why this makes the
// cell genuinely program-dependent (proven by
// NovaGlowDetectorValidation's differential-recovery test: a short
// transient and a long sustained signal at the SAME peak GR must NOT
// recover identically).
//
// Not a fixed attack/release compressor: attack itself is program-
// dependent (bigger GR jumps attack faster), and release blends a fast
// pole against a slow pole whose OWN time constant stretches the longer/
// harder the cell has been driven - an "optical memory" that deepens with
// sustained use and drains back down during quiet passages, exactly the
// LA-2A-workflow behavior the spec calls for (without claiming circuit
// accuracy).
//
// All constants are named and centralized here for by-ear tuning, per the
// spec's explicit instruction. All coefficients use the
// exp(-1/(tau*sampleRate)) idiom, so behavior is sample-rate independent
// from 44.1kHz to 192kHz.
class NovaOpticalGainCell
{
public:
    NovaOpticalGainCell() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // memoryDepth01: how much the slow/charged tail is allowed to show
    // through at full charge - GLOW's macro raises this (more Glow = more
    // audible optical memory). Independent of the detector's own
    // Compress/Limit or Analog Mode state, which the caller has already
    // folded into targetGrDb by the time it reaches here.
    void setMemoryDepth (float memoryDepth01) noexcept { memoryDepth = juce::jlimit (0.0f, 1.0f, memoryDepth01); }

    // attackSoftening01: gently widens the attack-time range (GLOW's
    // "transient softening" contribution) - deliberately capped low so it
    // never comes close to fully defeating transients.
    void setAttackSoftening (float attackSoftening01) noexcept { attackSoftening = juce::jlimit (0.0f, 1.0f, attackSoftening01); }

    // Runs one sample of the cell against a freshly-computed target GR (dB,
    // >= 0) from the detector/knee, and returns the resulting smoothed
    // gain-reduction in dB (always >= 0).
    float process (float targetGrDb) noexcept;

    float getGainReductionDb() const noexcept { return gainReductionDb; }
    float getCellCharge() const noexcept { return cellCharge; }

private:
    // Attack: program-dependent, ~10ms nominal, faster for bigger jumps.
    static constexpr float kAttackFastSeconds = 0.003f;
    static constexpr float kAttackSlowSeconds = 0.015f;
    static constexpr float kAttackReferenceDb = 10.0f; // GR jump size that reaches the fast end of the range

    // cellCharge: slow "how hard/long has this been driven" accumulator.
    static constexpr float kCellChargeSeconds = 0.9f;
    static constexpr float kDeepReductionReferenceDb = 14.0f; // same normalization idiom as NovaVariableMuDetector

    // Two release poles.
    static constexpr float kFastReleaseSeconds = 0.06f;
    static constexpr float kSlowReleaseBaseSeconds = 0.9f;
    static constexpr float kSlowTailDeepenFactor = 2.0f; // effective slow tau roughly triples as cellCharge -> 1

    double sampleRate = 44100.0;

    float envelopeGr = 0.0f;
    float fastPoleGr = 0.0f;
    float slowPoleGr = 0.0f;
    float cellCharge = 0.0f;

    float chargeCoeff = 0.0f;
    float fastReleaseCoeff = 0.0f;

    float memoryDepth = 1.0f;
    float attackSoftening = 0.0f;

    float gainReductionDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaOpticalGainCell)
};

} // namespace nova::glow
