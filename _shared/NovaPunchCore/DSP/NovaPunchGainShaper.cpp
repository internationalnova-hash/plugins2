#include "NovaPunchGainShaper.h"
#include "../NovaPunchTypes.h"
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

    void NovaPunchGainShaper::prepare (double sampleRate) noexcept
    {
        gainSmoothAlpha = alphaFor (0.003, sampleRate); // ~3ms - fast enough to feel immediate, slow enough to avoid zipper noise
        reset();
    }

    void NovaPunchGainShaper::reset() noexcept
    {
        punchGainDbSmoothed = 0.0f;
        bodyGainDbSmoothed = 0.0f;
        lastAttackDb = lastDecayDb = lastSustainDb = 0.0f;
    }

    NovaPunchGainShaper::Result NovaPunchGainShaper::processSample (float punchParam0to100, float bodyParam0to100,
                                                                      float attackBlend, float bodyBlend, float eventStrength,
                                                                      NovaPunchEventSegmenter::Region region) noexcept
    {
        const float punchNorm = (punchParam0to100 - 50.0f) / 50.0f;   // -1..1
        const float bodyNorm = (bodyParam0to100 - 50.0f) / 50.0f;     // -1..1

        // Scaled by the triggering event's own detected strength - a weak
        // onset gets proportionally gentler treatment than a strong one,
        // per spec's explicit requirement (never apply the same gain
        // envelope blindly to every event).
        const float punchTargetDb = punchNorm * kMaxPunchDb * attackBlend * (0.3f + 0.7f * eventStrength);
        const float bodyTargetDb = bodyNorm * kMaxBodyDb * bodyBlend * (0.5f + 0.5f * eventStrength);

        punchGainDbSmoothed += gainSmoothAlpha * (punchTargetDb - punchGainDbSmoothed);
        bodyGainDbSmoothed += gainSmoothAlpha * (bodyTargetDb - bodyGainDbSmoothed);

        Result result;
        result.punchGainLinear = dbToGain (punchGainDbSmoothed);
        result.bodyGainLinear = dbToGain (bodyGainDbSmoothed);

        // Route the real applied gain into whichever of the three
        // readouts matches the CURRENT region; the other two hold their
        // last real value rather than showing a fabricated number.
        if (region == NovaPunchEventSegmenter::Region::Attack)
            lastAttackDb = punchGainDbSmoothed;
        else if (region == NovaPunchEventSegmenter::Region::Decay)
            lastDecayDb = bodyGainDbSmoothed;
        else if (region == NovaPunchEventSegmenter::Region::Sustain)
            lastSustainDb = bodyGainDbSmoothed;

        result.attackGainDb = lastAttackDb;
        result.decayGainDb = lastDecayDb;
        result.sustainGainDb = lastSustainDb;
        return result;
    }
}
