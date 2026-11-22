#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "../NovaRideConstants.h"

namespace nova::ride
{
    // Computes, clamps, and smooths the actual gain-ride correction from a
    // detected level vs. a target. Two invariants are enforced here and
    // covered by dedicated tests in NovaRideEngineValidation:
    //   1. |correction| never exceeds the current Range setting.
    //   2. Confirmed silence (gate closed) never produces a runaway
    //      *positive* correction - the target-chasing computation is
    //      bypassed entirely during silence in favour of relaxing toward
    //      0dB, so there is nothing to chase.
    //
    // No allocations, no locks - safe to call process() from the audio
    // thread. setParams() is cheap and expected at most once per block.
    class NovaRideGainEngine
    {
    public:
        NovaRideGainEngine() = default;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;
        void setParams (const ModeTimings& mode, float speedNorm, float sensitivityNorm, float rangeDb) noexcept;

        // Runs one sample of the ride computation and returns the linear
        // gain (post anti-zipper slew) ready to multiply directly into the
        // audio sample. fastEnvRising (from NovaRideDetector::Result) is
        // distinct from gateOpen: gateOpen stays true through the gate's
        // hold-extension window even after real signal has already stopped,
        // and even the RAW medium-envelope threshold check lags an abrupt
        // cutoff to silence by however long the medium envelope takes to
        // decay. fastEnvRising reacts within a single sample instead - it is
        // false the instant the ~3ms fast envelope stops climbing. Raising
        // correction is only ever pursued while fastEnvRising is true -
        // otherwise a decaying envelope (whether mid-hold-window or simply
        // mid-decay-but-still-above-threshold) would get chased upward,
        // which is exactly the runaway-positive-gain-on-silence failure
        // mode this engine must not have.
        float process (float detectedLevelDb, bool gateOpen, bool fastEnvRising,
                       float breathLikelihood, float effectiveTargetDb) noexcept;

        // The smoothed correction in dB, pre-slew - the musically-meaningful
        // "ride amount" value for UI telemetry.
        float getCorrectionDb() const noexcept { return correctionDb; }

    private:
        static float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

        double sampleRate = 44100.0;

        float correctionDb = 0.0f;       // smoothed, pre-slew
        float slewedLinearGain = 1.0f;   // post-slew, what actually multiplies into audio

        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        float returnToNeutralCoeff = 0.0f;
        float gainSlewCoeff = 0.0f;

        float deadBandDbEff = kNaturalMode.deadBandDb;
        float rangeDbEff = 6.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaRideGainEngine)
    };
}
