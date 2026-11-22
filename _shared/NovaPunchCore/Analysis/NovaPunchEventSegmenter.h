#pragma once

// Adaptive ATTACK -> DECAY -> SUSTAIN region tracking for a single mono
// detection stream (no reusable equivalent exists in the catalog - built
// from scratch). Deliberately does NOT hardcode region durations (spec
// section 4 is explicit: "do NOT hardcode 'every transient lasts exactly
// 20ms'") - ATTACK ends when the medium-timescale envelope actually starts
// falling from its own just-observed peak, DECAY ends when that envelope
// actually stabilizes (or a generous timeout elapses, so a very slowly-
// decaying pad doesn't get stuck mid-transition forever).
//
// Outputs smoothly-crossfaded attackBlend/bodyBlend weights rather than a
// hard region switch - PUNCH acts mostly during ATTACK (tapering into
// early DECAY), BODY acts mostly during DECAY/SUSTAIN - so the gain
// shaper never has to jump discontinuously between them.
namespace nova::punch
{
    class NovaPunchEventSegmenter
    {
    public:
        NovaPunchEventSegmenter() = default;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        enum class Region { Idle, Attack, Decay, Sustain };

        struct Result
        {
            Region region = Region::Idle;
            float attackBlend = 0.0f; // 0..1, smoothed
            float bodyBlend = 0.0f;   // 0..1, smoothed
            float eventStrength = 0.0f; // the triggering onset's strength, held through the whole event
            bool  isPeakSample = false; // true for one sample: the detected peak instant (drives the UI's PEAK marker)
        };

        // mediumEnvelope: NovaPunchTransientDetector's medium-timescale
        // envelope (via the shared NovaPunchEnvelopeFollower) - traces an
        // individual event's own shape, unlike the fast (onset-only) or
        // slow (program-level) tiers.
        Result processSample (bool onsetTriggered, float onsetStrength, float mediumEnvelope) noexcept;

    private:
        double sr = 44100.0;

        Region state = Region::Idle;
        float peakEnvelopeSoFar = 0.0f;
        float currentEventStrength = 0.0f;
        int samplesSinceOnset = 0;
        int samplesSinceDecayStart = 0;
        int stableSampleCount = 0;

        float attackBlendSmoothed = 0.0f, bodyBlendSmoothed = 1.0f;
        float blendSmoothAlpha = 1.0f;

        float prevMediumEnv = 0.0f;

        int minAttackSamples = 0;
        int maxAttackSamples = 0;
        int maxDecaySamples = 0;
        int stableSamplesToSustain = 0; // set from sampleRate in prepare()
        static constexpr float kDecayStartFallFraction = 0.85f; // envelope fell below this fraction of its own peak -> DECAY
        static constexpr float kStabilitySlope = 0.002f;        // |delta| below this, per sample, counts as "settling"
                                                                  // (loose enough to tolerate a real oscillating
                                                                  // waveform's own irreducible envelope-follower
                                                                  // ripple, which never reads back perfectly flat)
    };
}
