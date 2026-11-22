#include "NovaPunchEnvelopeFollower.h"
#include <cmath>

namespace nova::punch
{
    float NovaPunchEnvelopeFollower::alphaFor (double seconds, double sampleRate) noexcept
    {
        return 1.0f - (float) std::exp (-1.0 / (seconds * sampleRate));
    }

    void NovaPunchEnvelopeFollower::prepare (double sampleRate) noexcept
    {
        sr = sampleRate;

        // Fast: ~1ms attack / ~10ms release - tracks true peaks almost
        // instantly, decays fast enough to be ready for the next event.
        fastAttackAlpha = alphaFor (0.001, sr);
        fastReleaseAlpha = alphaFor (0.010, sr);

        // Medium: ~10ms attack / ~80ms release - traces the shape of an
        // individual event (attack through early decay).
        mediumAttackAlpha = alphaFor (0.010, sr);
        mediumReleaseAlpha = alphaFor (0.080, sr);

        // Slow: ~50ms attack / ~60ms release. The ATTACK stays slow (this
        // is still meaningfully slower to respond to a fresh rise than
        // the fast/medium tiers, so it still functions as a genuine
        // "local level" reference, not just a copy of medium) - only the
        // RELEASE was sped up, and substantially: originally 500ms, which
        // measurably suppressed detection of a realistic dense pattern
        // (repeat hits ~150ms apart, an ordinary 16th-note spacing) by
        // never letting the fast/slow RATIO cue recover between hits,
        // confirmed directly by tracing per-hit peak probability (0.60
        // falling to 0.09 by the 8th identical hit at the old release
        // time). 60ms release recovers ~92% within 150ms (3 time
        // constants), so ratio is free to fully re-engage for each
        // separate hit in a dense passage instead of ratcheting upward
        // across the whole passage.
        slowAttackAlpha = alphaFor (0.050, sr);
        slowReleaseAlpha = alphaFor (0.060, sr);

        reset();
    }

    void NovaPunchEnvelopeFollower::reset() noexcept
    {
        fastEnv = mediumEnv = slowEnv = 0.0f;
    }

    void NovaPunchEnvelopeFollower::processSample (float input) noexcept
    {
        const float rectified = std::abs (input);

        const float fa = rectified > fastEnv ? fastAttackAlpha : fastReleaseAlpha;
        fastEnv += fa * (rectified - fastEnv);

        const float ma = rectified > mediumEnv ? mediumAttackAlpha : mediumReleaseAlpha;
        mediumEnv += ma * (rectified - mediumEnv);

        const float sa = rectified > slowEnv ? slowAttackAlpha : slowReleaseAlpha;
        slowEnv += sa * (rectified - slowEnv);
    }
}
