#include "NovaPunchEventSegmenter.h"
#include <cmath>

namespace nova::punch
{
    namespace
    {
        float alphaFor (double seconds, double sampleRate) noexcept
        {
            return 1.0f - (float) std::exp (-1.0 / (seconds * sampleRate));
        }
    }

    void NovaPunchEventSegmenter::prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        minAttackSamples = (int) (0.002 * sr);   // ~2ms - let the attack actually develop before allowing a DECAY transition
        maxAttackSamples = (int) (0.4 * sr);      // 400ms timeout - a sustained tone's plateau never actually falls
                                                   // kDecayStartFallFraction below its own running peak (its
                                                   // envelope-follower ripple is well under 15%), so without this
                                                   // escape hatch a held note would stay in ATTACK forever and never
                                                   // reach DECAY/SUSTAIN at all; 400ms is far longer than any real
                                                   // transient's attack phase, so it never fires on genuine hits
        maxDecaySamples = (int) (0.3 * sr);       // 300ms timeout - a sustained tone's own envelope-follower ripple
                                                   // (a real oscillating waveform never reads back perfectly flat)
                                                   // can keep it from ever measuring as "stable" by slope alone;
                                                   // this timeout is what actually settles it into SUSTAIN in that
                                                   // case, generous enough for real instrument decay times without
                                                   // leaving BODY unreachable indefinitely
        stableSamplesToSustain = (int) (0.030 * sr); // ~30ms of settled envelope before calling it SUSTAIN
        blendSmoothAlpha = alphaFor (0.008, sr);  // ~8ms - smooth enough to avoid clicks, fast enough to feel responsive
        reset();
    }

    void NovaPunchEventSegmenter::reset() noexcept
    {
        state = Region::Idle;
        peakEnvelopeSoFar = 0.0f;
        currentEventStrength = 0.0f;
        samplesSinceOnset = 0;
        samplesSinceDecayStart = 0;
        stableSampleCount = 0;
        attackBlendSmoothed = 0.0f;
        bodyBlendSmoothed = 1.0f;
        prevMediumEnv = 0.0f;
    }

    NovaPunchEventSegmenter::Result NovaPunchEventSegmenter::processSample (bool onsetTriggered, float onsetStrength, float mediumEnvelope) noexcept
    {
        Result result;
        bool isPeakSample = false;

        if (onsetTriggered)
        {
            state = Region::Attack;
            currentEventStrength = onsetStrength;
            peakEnvelopeSoFar = mediumEnvelope;
            samplesSinceOnset = 0;
            samplesSinceDecayStart = 0;
            stableSampleCount = 0;

            // Snap (don't smooth) at the onset instant: a genuinely NEW
            // event deserves an immediate, full attackBlend regardless of
            // whatever blend the PREVIOUS event's tail (or resting Idle/
            // Sustain, which targets bodyBlend=1) left behind - measured
            // directly: without this snap, an isolated hit preceded by
            // silence showed real Body-gain bleeding into its own attack
            // window while the smoother was still catching up from its
            // resting bodyBlend=1 starting point.
            attackBlendSmoothed = 1.0f;
            bodyBlendSmoothed = 0.0f;
        }

        switch (state)
        {
            case Region::Idle:
                break;

            case Region::Attack:
            {
                ++samplesSinceOnset;
                if (mediumEnvelope > peakEnvelopeSoFar)
                    peakEnvelopeSoFar = mediumEnvelope;
                else if (samplesSinceOnset > minAttackSamples
                         && mediumEnvelope < peakEnvelopeSoFar * kDecayStartFallFraction)
                {
                    state = Region::Decay;
                    samplesSinceDecayStart = 0;
                    isPeakSample = true;
                }
                else if (samplesSinceOnset > maxAttackSamples)
                {
                    // Never fell far enough below its own peak to read as a
                    // real decay (a held/sustained note plateaus instead) -
                    // treat the plateau itself as the peak and hand off to
                    // DECAY, whose own stability check/timeout will settle
                    // it into SUSTAIN.
                    state = Region::Decay;
                    samplesSinceDecayStart = 0;
                    isPeakSample = true;
                }
                break;
            }

            case Region::Decay:
            {
                ++samplesSinceDecayStart;
                const float slope = std::abs (mediumEnvelope - prevMediumEnv);
                if (slope < kStabilitySlope)
                    ++stableSampleCount;
                else
                    stableSampleCount = 0;

                if (stableSampleCount > stableSamplesToSustain || samplesSinceDecayStart > maxDecaySamples)
                    state = Region::Sustain;
                break;
            }

            case Region::Sustain:
                // Persists until the next onset resets to Attack - a
                // continuously-held note (spec's "long sustained body"
                // examples) simply stays here.
                break;
        }

        prevMediumEnv = mediumEnvelope;

        const float attackTarget = state == Region::Attack ? 1.0f : (state == Region::Decay ? 0.3f : 0.0f);
        const float bodyTarget = 1.0f - attackTarget;

        attackBlendSmoothed += blendSmoothAlpha * (attackTarget - attackBlendSmoothed);
        bodyBlendSmoothed += blendSmoothAlpha * (bodyTarget - bodyBlendSmoothed);

        result.region = state;
        result.attackBlend = attackBlendSmoothed;
        result.bodyBlend = bodyBlendSmoothed;
        result.eventStrength = currentEventStrength;
        result.isPeakSample = isPeakSample;
        return result;
    }
}
