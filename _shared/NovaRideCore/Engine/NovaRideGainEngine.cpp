#include "NovaRideGainEngine.h"

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

        float sensitivityMultiplier (float sensitivityNorm) noexcept
        {
            return sensitivityNorm >= 0.0f
                       ? (kSensitivityThresholdMultiplierAtMin
                          + (kSensitivityThresholdMultiplierAtMax - kSensitivityThresholdMultiplierAtMin) * sensitivityNorm)
                       : kSensitivityThresholdMultiplierAtMin;
        }
    }

    void NovaRideGainEngine::prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        setParams (kNaturalMode, 0.5f, 0.5f, 6.0f);
        reset();
    }

    void NovaRideGainEngine::reset() noexcept
    {
        correctionDb = 0.0f;
        slewedLinearGain = 1.0f;
    }

    void NovaRideGainEngine::setParams (const ModeTimings& mode, float speedNorm, float sensitivityNorm, float rangeDb) noexcept
    {
        speedNorm = juce::jlimit (0.0f, 1.0f, speedNorm);
        sensitivityNorm = juce::jlimit (0.0f, 1.0f, sensitivityNorm);

        const float attackMs = lerp (mode.attackSlowMs, mode.attackFastMs, speedNorm);
        const float releaseMs = lerp (mode.releaseSlowMs, mode.releaseFastMs, speedNorm);
        const float gainSlewMs = lerp (mode.gainSlewSlowMs, mode.gainSlewFastMs, speedNorm);

        attackCoeff = onePoleCoeff (sampleRate, attackMs);
        releaseCoeff = onePoleCoeff (sampleRate, releaseMs);
        gainSlewCoeff = onePoleCoeff (sampleRate, gainSlewMs);
        returnToNeutralCoeff = onePoleCoeffSeconds (sampleRate, mode.returnToNeutralSeconds);

        deadBandDbEff = mode.deadBandDb * sensitivityMultiplier (sensitivityNorm);
        rangeDbEff = juce::jlimit (0.1f, kMaxCorrectionSafetyDb, std::abs (rangeDb));
    }

    float NovaRideGainEngine::process (float detectedLevelDb, bool gateOpen, bool fastEnvRising,
                                       float breathLikelihood, float effectiveTargetDb) noexcept
    {
        float targetForSmoothing;
        float coeff;

        if (gateOpen)
        {
            const float error = detectedLevelDb - effectiveTargetDb;
            float errorBeyondBand = 0.0f;
            if (error > deadBandDbEff)
                errorBeyondBand = error - deadBandDbEff;
            else if (error < -deadBandDbEff)
                errorBeyondBand = error + deadBandDbEff;

            float desiredCorrectionDb = -errorBeyondBand;
            desiredCorrectionDb = juce::jlimit (-rangeDbEff, rangeDbEff, desiredCorrectionDb);

            const bool isRaising = desiredCorrectionDb > correctionDb;

            if (isRaising && ! fastEnvRising)
            {
                // Real signal energy is not currently climbing (we may be
                // mid-decay, or in the gate's hold-extension window after
                // signal has already stopped). Chasing a "raise" here would
                // be reacting to a stale/decaying reading, not real material
                // - hold flat instead (still allowed to reduce, see the
                // release branch below via targetForSmoothing==current).
                targetForSmoothing = correctionDb;
                coeff = 0.0f;
            }
            else
            {
                // A raise-in-progress gets damped while breathLikelihood is
                // high (quiet, short-duration events) - reductions are never
                // damped, only the act of pushing a quiet moment louder.
                if (isRaising && breathLikelihood > 0.0f)
                    desiredCorrectionDb = correctionDb + (desiredCorrectionDb - correctionDb) * (1.0f - breathLikelihood * kBreathRaiseDamping);

                targetForSmoothing = desiredCorrectionDb;
                coeff = isRaising ? attackCoeff : releaseCoeff;
            }
        }
        else
        {
            // Confirmed silence: never chase a target - only ever relax
            // toward 0dB. This is what makes invariant #2 (no runaway
            // positive gain during silence) true by construction.
            targetForSmoothing = 0.0f;
            coeff = returnToNeutralCoeff;
        }

        correctionDb = coeff * correctionDb + (1.0f - coeff) * targetForSmoothing;

        // Defensive re-clamp (invariant #1) plus an absolute safety ceiling
        // independent of whatever Range is currently set to.
        correctionDb = juce::jlimit (-rangeDbEff, rangeDbEff, correctionDb);
        correctionDb = juce::jlimit (-kMaxCorrectionSafetyDb, kMaxCorrectionSafetyDb, correctionDb);
        if (! std::isfinite (correctionDb))
            correctionDb = 0.0f;

        const float targetLinearGain = juce::Decibels::decibelsToGain (correctionDb, -100.0f);
        slewedLinearGain = gainSlewCoeff * slewedLinearGain + (1.0f - gainSlewCoeff) * targetLinearGain;
        if (! std::isfinite (slewedLinearGain))
            slewedLinearGain = 1.0f;

        return slewedLinearGain;
    }
}
