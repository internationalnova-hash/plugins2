#pragma once

#include "NovaPitchTypes.h"
#include "Analysis/NovaPitchDetector.h"
#include "Correction/NovaScaleQuantizer.h"
#include "DSP/NovaPitchSynchronousShifter.h"
#include "DSP/NovaFormantCorrector.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// Top-level facade tying the whole pitch-correction chain together - the
// only class PluginProcessor talks to for audio. Per-sample flow:
//   mono-sum -> detector -> quantizer -> formant pre (L/R) ->
//   pitch-synchronous shifter (L/R) -> formant post (L/R) ->
//   transient-hold blend -> dry/wet mix -> output gain
//
// setParams() is real-time-safe (no allocation) and reads APVTS percent-
// scale values once per block, matching NovaDoublerEngine's convention.
// lowLatencyMode is the one exception: changing it resizes the detector's
// analysis window (heap-allocates), so it goes through
// reconfigureIfNeeded() instead - message-thread-only, mirrors Nova
// Seven's proven oversampling-reconfigure pattern (NovaOversampledColorStage).
namespace nova::pitch
{
    class NovaPitchEngine
    {
    public:
        NovaPitchEngine() = default;

        void prepare (double sampleRate, int maxBlockSize, bool lowLatencyMode) noexcept;
        void reset() noexcept;

        // Message-thread-only - never call from process()/setParams(), and
        // never from the audio thread. See class comment.
        void reconfigureIfNeeded (double sampleRate, bool desiredLowLatencyMode) noexcept;

        struct Params
        {
            int   keyPitchClass = 0;
            Scale scale = Scale::Chromatic;
            float retuneSpeedPercent = 50.0f;          // APVTS "tolerance": 0=fast/hard, 100=slow/natural
            float amountPercent = 100.0f;               // APVTS "amount"
            float confidenceThresholdPercent = 70.0f;   // APVTS "confidenceThreshold"
            float vibratoPreservePercent = 0.0f;        // APVTS "vibrato"
            float vibratoRateHz = 5.0f;                 // APVTS "vibratoRate"
            float formantAmountPercent = 100.0f;        // APVTS "formant"
            float formantWarpPercent = 0.0f;            // APVTS "formantWarp", -100..100
            float rangePercent = 100.0f;                // APVTS "range"
            float mixPercent = 100.0f;                  // APVTS "mix"
            float outputGainDb = 0.0f;                  // APVTS "outputGain"
            float transientsPercent = 30.0f;             // APVTS "transients"
            bool  bypassed = false;                      // APVTS "bypass"
        };

        void setParams (const Params& p) noexcept;

        void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

        float getDetectedHz() const noexcept { return lastDetectedHz; }
        float getCorrectedHz() const noexcept { return lastCorrectedHz; }
        float getConfidence() const noexcept { return lastConfidence; }
        int   getLatencySamples() const noexcept { return latencySamples; }

        float getShifterRatioDebug() const noexcept { return shifterL.getCurrentRatioDebug(); }
        bool  isVoicedDebug() const noexcept { return detector.isVoiced(); }

        // Forensic instrumentation only - see NovaPitchSynchronousShifter's
        // own comment on onGrainCaptured. Forwards to the LEFT channel's
        // shifter only (sufficient for diagnostics on a mono-sourced
        // stereo file, where L/R behave identically).
        void setGrainCaptureDebugCallback (std::function<void (const NovaPitchSynchronousShifter::CaptureDebugInfo&)> cb) noexcept
        {
            shifterL.onGrainCaptured = std::move (cb);
        }

    private:
        NovaPitchDetector detector;
        NovaScaleQuantizer quantizer;
        NovaPitchSynchronousShifter shifterL, shifterR;
        NovaFormantCorrector formantL, formantR;

        std::vector<float> monoScratch;
        std::vector<float> dryDelayL, dryDelayR;
        int dryDelayWrite = 0;
        int latencySamples = 0;

        bool appliedLowLatencyMode = false;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed, outputGainSmoothed,
            transientsSmoothed, formantAmountSmoothed, formantWarpSmoothed, bypassSmoothed;

        // Simple fast/slow envelope-follower onset detector, drives the
        // TRANSIENTS control's attack-hold (briefly blends toward the dry
        // signal during a detected consonant/attack transient, instead of
        // pitch-correcting the noisy/inharmonic onset).
        float fastEnv = 0.0f, slowEnv = 0.0f;
        float fastEnvAlpha = 0.0f, slowEnvAlpha = 0.0f;

        float lastDetectedHz = 0.0f;
        float lastCorrectedHz = 0.0f;
        float lastConfidence = 0.0f;

        // Shift ratio releases toward unity (1:1, i.e. "stop actively
        // shifting") while the detector is unvoiced, instead of holding
        // whatever correction ratio was last active indefinitely - see
        // process() for why.
        float releasedOutHz = 0.0f;
        float releaseAlpha = 0.0f; // one-pole coeff, ~120ms time constant
    };
}
