#include "NovaLimiter.h"
#include <cmath>

namespace
{
    // Ported verbatim from Nova Master (PluginProcessor.cpp:29-45, 54-60).
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

    // See NovaSoftClipper.cpp for why this is gated to samples at/above
    // the ceiling rather than applied unconditionally (the raw curve is
    // not identity below the ceiling - it's a consistent ~1.2x boost).
    float applySoftCeilingCurve (float sample, float ceilingLinear) noexcept
    {
        const auto safeCeiling = juce::jmax (0.05f, ceilingLinear);
        const auto normalised = sample / safeCeiling;
        const auto shaped = std::tanh (normalised * 0.82f) / std::tanh (0.82f);
        return shaped * safeCeiling;
    }

    float applySoftCeiling (float sample, float ceilingLinear) noexcept
    {
        return std::abs (sample) >= ceilingLinear ? applySoftCeilingCurve (sample, ceilingLinear) : sample;
    }

    constexpr float kLimiterRatio  = 3.0f;
    constexpr float kLimiterKneeDb = 3.0f;
    constexpr float kAttackRate    = 0.32f;
}

void NovaLimiter::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    preparedNumChannels = (int) spec.numChannels;

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        spec.numChannels, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
    oversampler->initProcessing (spec.maximumBlockSize);

    reset();
    setPlan (plan);
}

void NovaLimiter::reset() noexcept
{
    gainReductionDb = 0.f;
    if (oversampler) oversampler->reset();
}

void NovaLimiter::setPlan (const NovaMasteringPlan::Limiting& p) noexcept
{
    plan = p;
    const float releaseMs = juce::jmax (1.f, plan.releaseMs);
    releaseCoeffRate = juce::jlimit (0.0005f, 0.05f, 1.f / (releaseMs * 0.001f * (float) sampleRate) * 12.f);
}

void NovaLimiter::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! plan.active) return;

    const int nCh = buffer.getNumChannels();
    const int N   = buffer.getNumSamples();
    if (nCh == 0 || N == 0) return;

    float* l = buffer.getWritePointer (0);
    float* r = nCh > 1 ? buffer.getWritePointer (1) : l;

    const float thresholdLinear = juce::Decibels::decibelsToGain (plan.thresholdDb);
    const float ceilingLinear   = juce::Decibels::decibelsToGain (plan.truePeakCeilDb);

    // 1) Sample-domain gain-reduction limiter, Nova Master's proven pattern.
    for (int i = 0; i < N; ++i)
    {
        const float peak = juce::jmax (std::abs (l[i]), std::abs (r[i]));
        float targetGrDb = 0.f;

        if (peak > thresholdLinear)
        {
            const float overDb = juce::Decibels::gainToDecibels (peak / thresholdLinear, 0.f);
            targetGrDb = juce::jlimit (0.f, 24.f, computeGainReductionDb (overDb, kLimiterRatio, kLimiterKneeDb));
        }

        if (targetGrDb > gainReductionDb)
            gainReductionDb += (targetGrDb - gainReductionDb) * kAttackRate;
        else
            gainReductionDb += (targetGrDb - gainReductionDb) * releaseCoeffRate;

        const float gain = std::isfinite (gainReductionDb) ? juce::Decibels::decibelsToGain (-gainReductionDb) : 0.f;
        l[i] *= gain;
        if (nCh > 1) r[i] *= gain;
    }

    // 2) True-peak verification: oversample the gain-reduced result purely
    // to measure it (discarded afterwards, not used as output - same
    // measurement-only pattern as NovaLoudnessAnalyzer's true peak). If
    // inter-sample peaks still exceed the ceiling, apply one broadband
    // corrective trim - exact and safe since oversampling is linear, so
    // scaling the input scales the true peak by the same factor.
    if (oversampler != nullptr)
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                            (size_t) juce::jmin (nCh, preparedNumChannels), (size_t) N);
        auto upBlock = oversampler->processSamplesUp (block);

        float truePeak = 0.f;
        for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch)
        {
            const float* d = upBlock.getChannelPointer (ch);
            for (size_t i = 0; i < upBlock.getNumSamples(); ++i)
                truePeak = juce::jmax (truePeak, std::abs (d[i]));
        }

        if (truePeak > ceilingLinear && truePeak > 1e-9f)
        {
            const float correction = ceilingLinear / truePeak;
            for (int ch = 0; ch < nCh; ++ch)
                juce::FloatVectorOperations::multiply (buffer.getWritePointer (ch), correction, N);
        }
    }

    // 3) Soft ceiling safety net, ported from Nova Master.
    for (int i = 0; i < N; ++i)
    {
        l[i] = applySoftCeiling (l[i], ceilingLinear);
        if (nCh > 1) r[i] = applySoftCeiling (r[i], ceilingLinear);
    }
}
