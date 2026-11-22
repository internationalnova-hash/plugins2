#pragma once

// Transparent peak containment for overs Punch processing itself can
// create - explicitly NOT a mastering limiter (spec section 9). Zero
// added latency by design (no lookahead delay line): gain reduction
// snaps INSTANTLY (same sample, no smoothing) whenever the input needs
// more reduction than currently applied, and only the RELEASE (letting
// go once the peak has passed) is smoothed (~20ms) to avoid an audible
// gain-jump/pump on the way back up.
//
// An earlier version smoothed the attack too (with a very short but
// still finite time constant, ~0.03ms) on the theory that "fast enough"
// would be indistinguishable from instant. Measured directly against
// real snare material, it wasn't: a one-pole smoother's output on the
// very sample a real (not synthetically-ramped) transient first crosses
// the ceiling is `input * (prevGain + attackAlpha*(neededGain-prevGain))`
// - structurally under-corrected on that exact sample no matter how
// short the time constant, since it blends toward the target rather
// than reaching it. That produced a real, measured 1.057 peak (0.98
// ceiling) on real snare material despite passing every synthetic
// clip-safe test. A true instant attack has no such gap: on the sample
// that needs it, gainEnvelope is set directly to neededGain, so
// output = input * (kCeiling/|input|) = exactly +-kCeiling, never more.
namespace nova::punch
{
    class NovaPunchClipSafe
    {
    public:
        NovaPunchClipSafe() = default;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;
        void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

        float processSample (float input) noexcept;

        float getCurrentGainReductionDb() const noexcept;

    private:
        bool enabled = true;
        float gainEnvelope = 1.0f; // 1 = no reduction
        float releaseAlpha = 1.0f;

        static constexpr float kCeiling = 0.98f; // just under 0dBFS
    };
}
