#include "NovaRideDetector.h"

namespace nova::ride
{
    namespace
    {
        float onePoleCoeff (double sampleRate, float timeMs) noexcept
        {
            return std::exp (-1.0f / (0.001f * juce::jmax (0.01f, timeMs) * static_cast<float> (sampleRate)));
        }

        float onePoleCoeffSeconds (double sampleRate, float timeSeconds) noexcept
        {
            return std::exp (-1.0f / (juce::jmax (0.001f, timeSeconds) * static_cast<float> (sampleRate)));
        }
    }

    void NovaRideDetector::prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        fastCoeff = onePoleCoeff (sampleRate, kFastEnvMs);
        peakDecayCoeff = onePoleCoeffSeconds (sampleRate, kPeakEnvelopeDecaySec);
        breathSmoothCoeff = onePoleCoeffSeconds (sampleRate, kBreathSmoothSeconds);
        breathConfirmSamples = static_cast<int> ((kBreathConfirmMs * 0.001f) * static_cast<float> (sampleRate));
        setParams (kNaturalMode, 0.5f, 0.5f);
        reset();
    }

    void NovaRideDetector::reset() noexcept
    {
        fastEnvLin = mediumEnvLin = slowEnvLin = 0.0f;
        recentPeakLin = 0.0f;
        gateOpenState = false;
        holdCounter = 0;
        sustainedActiveSamples = 0;
        breathLikelihoodSmoothed = 0.0f;
    }

    void NovaRideDetector::setParams (const ModeTimings& mode, float speedNorm, float sensitivityNorm) noexcept
    {
        speedNorm = juce::jlimit (0.0f, 1.0f, speedNorm);
        sensitivityNorm = juce::jlimit (0.0f, 1.0f, sensitivityNorm);

        mediumCoeff = onePoleCoeff (sampleRate, mode.mediumEnvMs);
        slowCoeff = onePoleCoeff (sampleRate, mode.slowEnvMs);

        const float holdMs = lerp (mode.holdSlowMs, mode.holdFastMs, speedNorm);
        holdSamples = static_cast<int> ((holdMs * 0.001f) * static_cast<float> (sampleRate));

        const float sensMul = sensitivityMultiplier (sensitivityNorm);
        gateHysteresisDbEff = mode.gateHysteresisDb * sensMul;
        gateOpenMarginDbEff = lerp (kGateOpenMarginDbAtMinSensitivity, kGateOpenMarginDbAtMaxSensitivity, sensitivityNorm);
    }

    NovaRideDetector::Result NovaRideDetector::processSample (float monoSample) noexcept
    {
        if (! std::isfinite (monoSample))
            monoSample = 0.0f;

        const float mag = std::abs (monoSample);

        const float previousFastEnvLin = fastEnvLin;
        fastEnvLin = fastCoeff * fastEnvLin + (1.0f - fastCoeff) * mag;

        mediumEnvLin = mediumCoeff * mediumEnvLin + (1.0f - mediumCoeff) * mag;
        slowEnvLin = slowCoeff * slowEnvLin + (1.0f - slowCoeff) * mag;

        // Adaptive "recent peak" tracker: instant-attack, slow decay.
        recentPeakLin = juce::jmax (fastEnvLin, recentPeakLin * peakDecayCoeff);

        const float noiseFloorLin = juce::jmax (kNoiseFloorMinLinear, kNoiseFloorFactor * recentPeakLin);

        // "Rising" alone isn't enough: once fastEnvLin has fully decayed to
        // (bit-exact) 0 during true silence, coeff*0 + (1-coeff)*0 == 0, so
        // the plain derivative check would read as "flat/rising" again and
        // let a stale raise resume mid-hold-window. Requiring real energy
        // above the (adaptive) noise floor as well means this stays false
        // for good once genuine silence has fully drained the envelope.
        const bool fastEnvRising = (fastEnvLin >= previousFastEnvLin) && (fastEnvLin > noiseFloorLin);
        const float noiseFloorDb = juce::Decibels::gainToDecibels (noiseFloorLin, kMinDb);
        const float levelDb = juce::Decibels::gainToDecibels (mediumEnvLin, kMinDb);

        const float openThresholdDb = noiseFloorDb + gateOpenMarginDbEff;
        const float closeThresholdDb = openThresholdDb - gateHysteresisDbEff;

        const bool aboveOpen = levelDb > openThresholdDb;
        const bool belowClose = levelDb < closeThresholdDb;

        if (! gateOpenState)
        {
            if (aboveOpen)
            {
                gateOpenState = true;
                holdCounter = holdSamples;
                sustainedActiveSamples = 0;
            }
        }
        else
        {
            if (! belowClose)
            {
                holdCounter = holdSamples;
                ++sustainedActiveSamples;
            }
            else if (holdCounter > 0)
            {
                --holdCounter;
                ++sustainedActiveSamples;
            }
            else
            {
                gateOpenState = false;
                sustainedActiveSamples = 0;
            }
        }

        const bool withinBreathLevel = levelDb < (openThresholdDb + kBreathLevelMarginDb);
        const bool withinBreathDuration = sustainedActiveSamples < breathConfirmSamples;
        const float breathTarget = (gateOpenState && withinBreathLevel && withinBreathDuration) ? 1.0f : 0.0f;
        breathLikelihoodSmoothed = breathSmoothCoeff * breathLikelihoodSmoothed + (1.0f - breathSmoothCoeff) * breathTarget;

        Result result;
        result.levelDb = levelDb;
        result.gateOpen = gateOpenState;
        result.aboveOpenThreshold = aboveOpen;
        result.fastEnvRising = fastEnvRising;
        result.activityConfidence = gateOpenState ? 1.0f : 0.0f;
        result.breathLikelihood = breathLikelihoodSmoothed;
        return result;
    }
}
