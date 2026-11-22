#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace nova::velvet
{

// Small attack/release envelope follower driving the transformer stages'
// and tube stage's level-coupled drive - a genuinely new component
// (neither NovaTransformerStage nor NovaTubeStage in this catalog's prior
// art has level-coupled drive at all; both take a single static drive
// value per block). Header-only: trivial enough not to warrant a .cpp.
class NovaVelvetLevelEnvelope
{
public:
    void prepare (double sampleRate, float attackSeconds, float releaseSeconds) noexcept
    {
        attackCoeff = std::exp (-1.0f / (attackSeconds * (float) sampleRate));
        releaseCoeff = std::exp (-1.0f / (releaseSeconds * (float) sampleRate));
        reset();
    }

    void reset() noexcept { level = 0.0f; }

    float processSample (float x) noexcept
    {
        const float rectified = std::abs (x);
        const float coeff = rectified > level ? attackCoeff : releaseCoeff;
        level = coeff * level + (1.0f - coeff) * rectified;
        if (! std::isfinite (level)) level = 0.0f;
        return level;
    }

    // 0-1-ish normalized reading (1.0 == 0dBFS full-scale amplitude).
    float get01() const noexcept { return juce::jlimit (0.0f, 1.0f, level); }

private:
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float level = 0.0f;
};

} // namespace nova::velvet
