#pragma once

#include "../Analysis/NovaPunchEventSegmenter.h"

// Turns PUNCH/BODY parameters into actual per-sample gain, gated by the
// event segmenter's attack/body blend AND scaled by the detector's own
// transientStrength - spec is explicit that this must NOT be a blind
// static gain: "a strong snare transient and a soft vocal consonant
// should not receive identical behavior." No reusable equivalent exists
// in the catalog (every existing dynamics gain stage, e.g. NovaLevelDSP,
// derives gain from a single compressor-style detector, not a segmented
// attack/body split).
//
// PUNCH > 50 increases attack-region gain, < 50 decreases it. BODY > 50
// increases decay/sustain-region gain, < 50 decreases it. Both taper via
// the segmenter's smoothed blend weights, so there is never a hard,
// clicking jump between "punch mode" and "body mode" on the same signal.
namespace nova::punch
{
    class NovaPunchGainShaper
    {
    public:
        NovaPunchGainShaper() = default;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        struct Result
        {
            float punchGainLinear = 1.0f;
            float bodyGainLinear = 1.0f;
            float attackGainDb = 0.0f; // real, matches punchGainLinear - telemetry reads this directly, never invents a number
            float decayGainDb = 0.0f;
            float sustainGainDb = 0.0f;
        };

        // punchParam0to100/bodyParam0to100: raw APVTS values, 50 = neutral.
        // region: only used to route the real applied gain into the
        // matching one of attackGainDb/decayGainDb/sustainGainDb for the
        // UI's three telemetry readouts - never invents a value for the
        // two that aren't currently active, just holds them at their last
        // real reading until that region is active again.
        Result processSample (float punchParam0to100, float bodyParam0to100,
                               float attackBlend, float bodyBlend, float eventStrength,
                               NovaPunchEventSegmenter::Region region) noexcept;

        static constexpr float kMaxPunchDb = 10.0f;
        static constexpr float kMaxBodyDb = 8.0f;

    private:
        float punchGainDbSmoothed = 0.0f, bodyGainDbSmoothed = 0.0f;
        float gainSmoothAlpha = 1.0f;
        float lastAttackDb = 0.0f, lastDecayDb = 0.0f, lastSustainDb = 0.0f;
    };
}
