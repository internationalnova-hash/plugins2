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

    look  = juce::jmax (16, (int) std::lround (0.0015 * sampleRate));   // 1.5 ms look-ahead
    delay = look + kIspTaps / 2;                                     // + the true-peak estimator's reach
    hold  = delay + 1;
    const int nch = juce::jmax (2, (int) spec.numChannels);
    delayRing.assign ((size_t) nch, std::vector<float> ((size_t) delay, 0.f));
    ispHist.assign ((size_t) nch, std::array<float, kIspTaps> {});
    holdRing.assign ((size_t) hold, 1.f);
    boxRing.assign ((size_t) look, 1.f);
    // windowed-sinc taps for the points 1/4, 1/2, 3/4 between the two middle samples
    for (int f = 0; f < 3; ++f)
        for (int k = 0; k < kIspTaps; ++k)
        {
            const double t = (kIspTaps / 2 - 1) + (f + 1) * 0.25 - k;
            const double sinc = std::abs (t) < 1e-9 ? 1.0 : std::sin (juce::MathConstants<double>::pi * t) / (juce::MathConstants<double>::pi * t);
            const double w = 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * t / (kIspTaps / 2 + 0.5));
            ispCoef[(size_t) f][(size_t) k] = (float) (sinc * w);
        }

    reset();
    setPlan (plan);
}

void NovaLimiter::reset() noexcept
{
    gainReductionDb = 0.f;
    for (auto& d : delayRing) std::fill (d.begin(), d.end(), 0.f);
    for (auto& h : ispHist) h.fill (0.f);
    std::fill (holdRing.begin(), holdRing.end(), 1.f);
    std::fill (boxRing.begin(), boxRing.end(), 1.f);
    boxSum = (double) boxRing.size();
    env = 1.f;
    delayPos = holdPos = boxPos = ispPos = 0;
    sinceResum = 0;
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
    const int nCh = buffer.getNumChannels();
    const int N   = buffer.getNumSamples();
    if (nCh == 0 || N == 0) return;

    if (plan.active && plan.brickwall) { processLookAhead (buffer); return; }

    delayOnly (buffer);           // same latency in every mode (reported to the host)
    if (! plan.active) return;

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

void NovaLimiter::delayOnly (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), (int) delayRing.size());
    const int N = buffer.getNumSamples();
    int pos = delayPos;
    for (int i = 0; i < N; ++i)
    {
        for (int c = 0; c < nCh; ++c)
        {
            auto& d = delayRing[(size_t) c];
            float* x = buffer.getWritePointer (c);
            const float out = d[(size_t) pos];
            d[(size_t) pos] = x[i];
            x[i] = out;
        }
        pos = (pos + 1) % delay;
    }
    delayPos = pos;
}

// Look-ahead true-peak limiter (plans made since the loudness fix):
// per sample, the peak (sample and estimated inter-sample) sets the gain this
// moment needs; that's held across the look-ahead window, released smoothly,
// then box-averaged over the window, and the audio is delayed so the gain is
// fully down before each peak arrives. Peaks land at the ceiling, not over it,
// with no block-size dependence.
void NovaLimiter::processLookAhead (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), (int) delayRing.size());
    const int N = buffer.getNumSamples();
    const float ceiling = juce::Decibels::decibelsToGain (plan.truePeakCeilDb);
    const float thr = ceiling * juce::Decibels::decibelsToGain (-0.2f);     // margin for the estimator
    const float rel = 1.f - std::exp (-1.f / (juce::jmax (5.f, plan.releaseMs) * 0.001f * (float) sampleRate));
    float minGain = 1.f;

    for (int i = 0; i < N; ++i)
    {
        float pk = 0.f;
        for (int c = 0; c < nCh; ++c)
        {
            const float x = buffer.getReadPointer (c)[i];
            auto& h = ispHist[(size_t) c];
            h[(size_t) ispPos] = x;
            pk = juce::jmax (pk, std::abs (x));
            for (int f = 0; f < 3; ++f)
            {
                float v = 0.f;
                for (int k = 0; k < kIspTaps; ++k)
                    v += ispCoef[(size_t) f][(size_t) k] * h[(size_t) ((ispPos + 1 + k) % kIspTaps)];
                pk = juce::jmax (pk, std::abs (v));
            }
        }
        ispPos = (ispPos + 1) % kIspTaps;

        holdRing[(size_t) holdPos] = pk > thr ? thr / pk : 1.f;
        holdPos = (holdPos + 1) % hold;
        float held = 1.f;
        for (float g : holdRing) held = juce::jmin (held, g);

        env = held < env ? held : env + (held - env) * rel;

        boxSum += (double) env - (double) boxRing[(size_t) boxPos];
        boxRing[(size_t) boxPos] = env;
        boxPos = (boxPos + 1) % look;
        if (++sinceResum >= 8192)
        {
            sinceResum = 0;
            boxSum = 0.0;
            for (float g : boxRing) boxSum += g;
        }
        const float g = juce::jmin (1.f, (float) (boxSum / (double) look));
        minGain = juce::jmin (minGain, g);

        for (int c = 0; c < nCh; ++c)
        {
            auto& d = delayRing[(size_t) c];
            float* x = buffer.getWritePointer (c);
            const float out = d[(size_t) delayPos] * g;
            d[(size_t) delayPos] = x[i];
            // last-resort sample clamp (the look-ahead already keeps peaks under it)
            x[i] = std::isfinite (out) ? juce::jlimit (-ceiling, ceiling, out) : 0.f;
        }
        delayPos = (delayPos + 1) % delay;
    }
    gainReductionDb = -juce::Decibels::gainToDecibels (minGain, -60.f);
}
