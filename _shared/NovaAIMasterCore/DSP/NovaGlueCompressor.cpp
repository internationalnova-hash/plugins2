#include "NovaGlueCompressor.h"
#include <cmath>

namespace
{
    // Ported verbatim from Nova Master (PluginProcessor.cpp:29-45).
    float computeGainReductionDb (float overDb, float ratio, float kneeDb) noexcept
    {
        if (overDb <= 0.0f) return 0.0f;

        const auto slope = 1.0f - (1.0f / juce::jmax (1.0f, ratio));
        if (kneeDb <= 0.0f) return slope * overDb;

        const auto kneeStart = -0.5f * kneeDb;
        const auto kneeEnd = 0.5f * kneeDb;

        if (overDb <= kneeStart) return 0.0f;
        if (overDb < kneeEnd)
        {
            const auto x = overDb - kneeStart;
            return slope * (x * x) / (2.0f * kneeDb);
        }
        return slope * overDb;
    }
}

void NovaGlueCompressor::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    reset();
    setPlan (plan);
}

void NovaGlueCompressor::reset() noexcept
{
    envelope = 0.f;
    gainReductionDb = 0.f;
}

void NovaGlueCompressor::setPlan (const NovaMasteringPlan::Compression& p) noexcept
{
    plan = p;
    if (sampleRate <= 0.0) return;

    const float attackMs  = juce::jmax (0.1f, plan.attackMs);
    const float releaseMs = juce::jmax (1.0f, plan.releaseMs);
    attackCoeff  = std::exp (-1.0f / (0.001f * attackMs  * (float) sampleRate));
    releaseCoeff = std::exp (-1.0f / (0.001f * releaseMs * (float) sampleRate));
}

void NovaGlueCompressor::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! plan.active) return;

    const int nCh = buffer.getNumChannels();
    const int N   = buffer.getNumSamples();
    if (nCh == 0 || N == 0) return;

    float* l = buffer.getWritePointer (0);
    float* r = nCh > 1 ? buffer.getWritePointer (1) : l;
    const float makeupGain = juce::Decibels::decibelsToGain (plan.makeupDb);

    for (int i = 0; i < N; ++i)
    {
        const float detector = juce::jmax (std::abs (l[i]), std::abs (r[i])) + 1.0e-6f;
        if (detector > envelope)
            envelope = (attackCoeff * envelope) + ((1.f - attackCoeff) * detector);
        else
            envelope = (releaseCoeff * envelope) + ((1.f - releaseCoeff) * detector);

        const float detectorDb = juce::Decibels::gainToDecibels (envelope, -160.f);
        const float targetGrDb = computeGainReductionDb (detectorDb - plan.thresholdDb, plan.ratio, kKneeDb);

        // Asymmetric smoothing of the GR value itself (not just the
        // detector envelope) - faster onset, slower release, ported from
        // Nova Master's exact 0.14/0.025 rates.
        if (targetGrDb > gainReductionDb)
            gainReductionDb += (targetGrDb - gainReductionDb) * 0.14f;
        else
            gainReductionDb += (targetGrDb - gainReductionDb) * 0.025f;

        const float gain = juce::Decibels::decibelsToGain (-gainReductionDb) * makeupGain;
        l[i] *= gain;
        if (nCh > 1) r[i] *= gain;
    }
}
