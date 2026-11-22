#pragma once

// Multi-timescale envelope follower - the foundation the transient
// detector's fast/slow onset ratio is built on. No generic equivalent
// exists elsewhere in the catalog (every existing detector, e.g. Nova
// Royal 670's NovaVariableMuDetector or Nova Glow's NovaOpticalDetector,
// is a single envelope tightly coupled to its own gain-reduction model) -
// this one is deliberately plugin-agnostic.
//
// Three independently-tuned one-pole followers track the SAME rectified
// input at different speeds:
//   fast   - tracks true peaks almost instantly (onset detection needs
//            this to actually see a transient's leading edge, not a
//            smoothed-over version of it)
//   medium - a mid-scale reference, used for local event-shape tracking
//            (the event segmenter watches this to find the peak/decay
//            transition)
//   slow   - the "local program level" - sensitivity's adaptive threshold
//            is relative to THIS, not a fixed absolute number, so the
//            detector behaves consistently whether program material is
//            quiet or loud (spec's own explicit requirement)
//
// Real-time-safe: no allocation in prepare() beyond setting scalar
// coefficients; processSample() is pure arithmetic.
namespace nova::punch
{
    class NovaPunchEnvelopeFollower
    {
    public:
        NovaPunchEnvelopeFollower() = default;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        // Rectifies internally - callers pass the raw (signed) sample.
        void processSample (float input) noexcept;

        float getFast() const noexcept { return fastEnv; }
        float getMedium() const noexcept { return mediumEnv; }
        float getSlow() const noexcept { return slowEnv; }

    private:
        static float alphaFor (double seconds, double sampleRate) noexcept;

        double sr = 44100.0;

        float fastEnv = 0.0f, mediumEnv = 0.0f, slowEnv = 0.0f;

        // Separate attack/release per tier - fast attack so real peaks
        // aren't missed, release set per-tier to give each envelope its
        // intended time character rather than symmetric smoothing.
        float fastAttackAlpha = 1.0f, fastReleaseAlpha = 1.0f;
        float mediumAttackAlpha = 1.0f, mediumReleaseAlpha = 1.0f;
        float slowAttackAlpha = 1.0f, slowReleaseAlpha = 1.0f;
    };
}
