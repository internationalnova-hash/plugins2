#include "NovaPunchClipSafe.h"
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

    void NovaPunchClipSafe::prepare (double sampleRate) noexcept
    {
        releaseAlpha = alphaFor (0.020, sampleRate);
        reset();
    }

    void NovaPunchClipSafe::reset() noexcept
    {
        gainEnvelope = 1.0f;
    }

    float NovaPunchClipSafe::processSample (float input) noexcept
    {
        if (! enabled)
            return input;

        const float absIn = std::abs (input);
        const float neededGain = absIn > kCeiling ? kCeiling / absIn : 1.0f;

        if (neededGain < gainEnvelope)
            gainEnvelope = neededGain; // instant attack - see class comment
        else
            gainEnvelope += releaseAlpha * (neededGain - gainEnvelope);

        return input * gainEnvelope;
    }

    float NovaPunchClipSafe::getCurrentGainReductionDb() const noexcept
    {
        return gainToDb (gainEnvelope);
    }
}
