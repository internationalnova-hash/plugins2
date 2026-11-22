#pragma once

// Centralized, by-ear-tunable constants for Nova Ride's detection and gain-ride
// engine. Nothing DSP-behavioral should be a magic number anywhere else in
// NovaRideCore - if a behavior needs tuning later, it should be a change to
// this file only.

namespace nova::ride
{
    // ------------------------------------------------------------------
    // Per-Mode timing/threshold table. SPEED (0..1) linearly interpolates
    // between each field's "Slow" and "Fast" endpoint within whichever Mode
    // is active - Mode picks the character, Speed picks where in that
    // character's range you sit. Values are starting points for real-vocal
    // listening tests, not final.
    // ------------------------------------------------------------------
    struct ModeTimings
    {
        float mediumEnvMs;          // "current level" window compared against Target
        float slowEnvMs;            // phrase-length reference window (sustained-activity sense)

        float attackFastMs;         // raising a quiet phrase - fast end (high Speed)
        float attackSlowMs;         // raising a quiet phrase - slow end (low Speed)
        float releaseFastMs;        // reducing a loud phrase - fast end
        float releaseSlowMs;        // reducing a loud phrase - slow end

        float holdFastMs;           // gap hold before relaxing toward neutral - fast end
        float holdSlowMs;           // gap hold before relaxing toward neutral - slow end

        float gainSlewFastMs;       // final anti-zipper linear-gain slew - fast end
        float gainSlewSlowMs;       // final anti-zipper linear-gain slew - slow end

        float deadBandDb;           // hysteresis dead-band around Target (no correction inside this)
        float gateHysteresisDb;     // activity-gate open/close hysteresis band
        float returnToNeutralSeconds; // time constant relaxing held correction to 0dB during silence
    };

    // Natural: widest dead-band/hysteresis, slowest everything - preserves
    // phrase dynamics, avoids micro-riding.
    inline constexpr ModeTimings kNaturalMode {
        300.f, 2500.f,
        120.f, 600.f,
        180.f, 900.f,
        300.f, 700.f,
        8.f, 25.f,
        1.5f, 2.5f,
        4.0f
    };

    // Tight: smallest dead-band, mid-fast timing - consistent modern level,
    // small variance.
    inline constexpr ModeTimings kTightMode {
        200.f, 1500.f,
        60.f, 300.f,
        90.f, 450.f,
        150.f, 400.f,
        4.f, 15.f,
        0.75f, 1.75f,
        2.5f
    };

    // Live: shortest detection window, fastest hold/attack - fastest useful
    // response for uncontrolled/live performances.
    inline constexpr ModeTimings kLiveMode {
        120.f, 900.f,
        25.f, 140.f,
        40.f, 220.f,
        60.f, 220.f,
        3.f, 10.f,
        1.0f, 1.25f,
        1.5f
    };

    inline constexpr const ModeTimings& modeTimingsFor (int modeIndex) noexcept
    {
        return modeIndex == 1 ? kTightMode : (modeIndex == 2 ? kLiveMode : kNaturalMode);
    }

    // ------------------------------------------------------------------
    // Detector constants (not Mode-dependent)
    // ------------------------------------------------------------------
    inline constexpr float kFastEnvMs             = 3.0f;   // near-instant envelope, feeds the noise-floor tracker
    inline constexpr float kNoiseFloorFactor       = 0.125f; // noise floor = this * recent peak envelope (~18dB below)
    inline constexpr float kNoiseFloorMinLinear    = 0.0006f; // absolute floor so silence doesn't chase a zero noise floor
    inline constexpr float kPeakEnvelopeDecaySec   = 2.0f;   // "recent peak" tracking window for the noise floor

    inline constexpr float kBreathConfirmMs        = 200.0f; // below this sustained duration, a low-level event may be a breath
    inline constexpr float kBreathLevelMarginDb     = 30.0f;  // within this many dB of the gate-open threshold, breath-likelihood can ramp up -
                                                                // wide on purpose: duration is the primary breath signal, this just excludes
                                                                // genuinely loud phrases from ever being flagged
    inline constexpr float kBreathRaiseDamping      = 0.65f;  // how much a full-confidence breath damps the *raise* response only
    inline constexpr float kBreathSmoothSeconds     = 0.05f;  // smoothing time for the breathLikelihood signal itself

    // Sensitivity (0..1) scales dead-band/hysteresis/gate-open-margin. Lower
    // multiplier = more sensitive (reacts to smaller level changes).
    inline constexpr float kSensitivityThresholdMultiplierAtMin = 1.6f; // sensitivity = 0%
    inline constexpr float kSensitivityThresholdMultiplierAtMax = 0.5f; // sensitivity = 100%
    inline constexpr float kGateOpenMarginDbAtMinSensitivity    = 12.0f; // dB above noise floor required to open gate
    inline constexpr float kGateOpenMarginDbAtMaxSensitivity    = 4.0f;

    // ------------------------------------------------------------------
    // Sidechain analyzer constants
    // ------------------------------------------------------------------
    inline constexpr float kSidechainEnvMs            = 400.0f;  // short-term loudness window for the music bus
    inline constexpr float kSidechainPresenceAttackSec = 0.15f;  // how fast "sidechain has real signal" ramps up
    inline constexpr float kSidechainPresenceReleaseSec = 1.5f;  // how slowly it ramps back down when the bus goes quiet/disconnected
    inline constexpr float kSidechainSilenceThresholdDb = -50.0f; // below this, the sidechain counts as "no real signal"

    // ------------------------------------------------------------------
    // Meter ballistics (IN/OUT telemetry, audio-thread one-pole smoothing
    // before the atomic store - same idiom as Nova Console)
    // ------------------------------------------------------------------
    inline constexpr float kMeterRiseCoeff = 0.16f;
    inline constexpr float kMeterHoldCoeff = 0.84f;

    // ------------------------------------------------------------------
    // Safety
    // ------------------------------------------------------------------
    inline constexpr float kMinDb  = -100.0f; // floor used when converting silence to a dB reading
    inline constexpr float kMaxCorrectionSafetyDb = 24.0f; // absolute ceiling regardless of Range setting (defensive, Range itself is always <= this)
}
