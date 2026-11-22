#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "../NovaRideConstants.h"

namespace nova::ride
{
    // Multi-timescale vocal-activity detector: three one-pole envelopes on
    // |sample| (fast/medium/slow), an adaptive noise floor relative to a
    // recently-tracked peak (not a fixed dB threshold - works across
    // different gain staging), and a hysteresis+hold-time activity gate
    // (same pattern as Nova Console's processGate()). Also produces a
    // "breath likelihood" signal - low-level and short-duration relative to
    // sustained phrase activity - that NovaRideGainEngine uses to damp the
    // *raise* response without hard-classifying anything.
    //
    // No allocations, no locks - safe to call processSample() from the audio
    // thread. setParams() is cheap (a handful of float multiplies) and is
    // expected to be called at most once per block from the same thread.
    class NovaRideDetector
    {
    public:
        NovaRideDetector() = default;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;
        void setParams (const ModeTimings& mode, float speedNorm, float sensitivityNorm) noexcept;

        struct Result
        {
            float levelDb = kMinDb;          // medium-envelope level, in dB
            bool  gateOpen = false;          // true while the activity gate considers this real signal (hold-extended)
            bool  aboveOpenThreshold = false; // true only while the RAW (medium-envelope) level currently clears the
                                              // open threshold, i.e. NOT hold-extended
            bool  fastEnvRising = false;     // true only when the ~3ms fast envelope is flat or increasing this
                                              // sample, i.e. real signal energy is not actively falling right now.
                                              // Reacts within a single sample to a hard cutoff to silence, unlike
                                              // aboveOpenThreshold (which lags behind the slower medium envelope) -
                                              // see NovaRideGainEngine for why this matters.
            float activityConfidence = 0.0f; // 0..1
            float breathLikelihood = 0.0f;   // 0..1, smoothed
        };

        Result processSample (float monoSample) noexcept;

    private:
        static float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
        static float sensitivityMultiplier (float sensitivityNorm) noexcept
        {
            return lerp (kSensitivityThresholdMultiplierAtMin, kSensitivityThresholdMultiplierAtMax, sensitivityNorm);
        }

        double sampleRate = 44100.0;

        // Envelopes on |sample|, linear domain.
        float fastEnvLin = 0.0f, mediumEnvLin = 0.0f, slowEnvLin = 0.0f;
        float fastCoeff = 0.0f, mediumCoeff = 0.0f, slowCoeff = 0.0f;

        // Adaptive noise floor.
        float recentPeakLin = 0.0f;
        float peakDecayCoeff = 0.0f;

        // Activity gate (hysteresis + hold).
        bool gateOpenState = false;
        int  holdCounter = 0;
        int  holdSamples = 0;
        int  sustainedActiveSamples = 0;
        int  breathConfirmSamples = 0;

        // Smoothed output.
        float breathLikelihoodSmoothed = 0.0f;
        float breathSmoothCoeff = 0.0f;

        // Effective (Mode/Speed/Sensitivity-scaled) parameters, set by setParams().
        float gateHysteresisDbEff = kNaturalMode.gateHysteresisDb;
        float gateOpenMarginDbEff = kGateOpenMarginDbAtMinSensitivity;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaRideDetector)
    };
}
