#include "NovaRideSidechainAnalyzer.h"

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

    void NovaRideSidechainAnalyzer::prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        envCoeff = onePoleCoeff (sampleRate, kSidechainEnvMs);
        presenceAttackCoeff = onePoleCoeffSeconds (sampleRate, kSidechainPresenceAttackSec);
        presenceReleaseCoeff = onePoleCoeffSeconds (sampleRate, kSidechainPresenceReleaseSec);
        reset();
    }

    void NovaRideSidechainAnalyzer::reset() noexcept
    {
        envLin = 0.0f;
        presence = 0.0f;
    }

    NovaRideSidechainAnalyzer::Result NovaRideSidechainAnalyzer::processSample (float monoSample, bool busConnected) noexcept
    {
        if (! std::isfinite (monoSample))
            monoSample = 0.0f;

        const float mag = std::abs (monoSample);
        envLin = envCoeff * envLin + (1.0f - envCoeff) * mag;

        const float levelDb = juce::Decibels::gainToDecibels (envLin, kMinDb);
        const bool hasRealSignal = busConnected && levelDb > kSidechainSilenceThresholdDb;

        const float target = hasRealSignal ? 1.0f : 0.0f;
        const float coeff = (target > presence) ? presenceAttackCoeff : presenceReleaseCoeff;
        presence = coeff * presence + (1.0f - coeff) * target;

        Result result;
        result.musicLevelDb = levelDb;
        result.sidechainPresence = presence;
        return result;
    }
}
