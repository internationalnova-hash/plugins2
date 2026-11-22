#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "../NovaRideConstants.h"

namespace nova::ride
{
    // Tracks the instrumental/music sidechain's short-term loudness and
    // whether it's actually present (bus connected AND carrying real
    // signal, not just silence). sidechainPresence ramps smoothly rather
    // than switching hard, so NovaRideGainEngine can blend gracefully back
    // to absolute-Target behavior when the sidechain drops out or was never
    // connected - never a hard cliff.
    class NovaRideSidechainAnalyzer
    {
    public:
        NovaRideSidechainAnalyzer() = default;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        struct Result
        {
            float musicLevelDb = kMinDb;
            float sidechainPresence = 0.0f; // 0..1, smoothed
        };

        // busConnected: whether the host has actually wired a sidechain
        // source in (isBusesLayoutSupported/isEnabled at the processor
        // level) - if false, presence always relaxes to 0 regardless of
        // monoSample.
        Result processSample (float monoSample, bool busConnected) noexcept;

    private:
        double sampleRate = 44100.0;

        float envLin = 0.0f;
        float envCoeff = 0.0f;

        float presence = 0.0f;
        float presenceAttackCoeff = 0.0f;
        float presenceReleaseCoeff = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaRideSidechainAnalyzer)
    };
}
