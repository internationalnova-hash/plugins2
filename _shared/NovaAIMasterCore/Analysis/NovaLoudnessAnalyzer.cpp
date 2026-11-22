#include "NovaLoudnessAnalyzer.h"
#include <algorithm>
#include <cmath>

namespace
{
    // Standard RBJ biquad cookbook formulas, used to derive BS.1770's
    // K-weighting stages at any sample rate from the documented analog
    // prototype (f0/Q/gain), rather than only the commonly-quoted 48kHz
    // coefficient table.
    void designHighShelf (double fs, double f0, double q, double gainDb,
                          float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cosw0 = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double sqrtA = std::sqrt (A);

        const double bb0 =    A * ( (A + 1) + (A - 1) * cosw0 + 2 * sqrtA * alpha );
        const double bb1 = -2 * A * ( (A - 1) + (A + 1) * cosw0 );
        const double bb2 =    A * ( (A + 1) + (A - 1) * cosw0 - 2 * sqrtA * alpha );
        const double aa0 =        (A + 1) - (A - 1) * cosw0 + 2 * sqrtA * alpha;
        const double aa1 =    2 * ( (A - 1) - (A + 1) * cosw0 );
        const double aa2 =        (A + 1) - (A - 1) * cosw0 - 2 * sqrtA * alpha;

        b0 = (float) (bb0 / aa0); b1 = (float) (bb1 / aa0); b2 = (float) (bb2 / aa0);
        a1 = (float) (aa1 / aa0); a2 = (float) (aa2 / aa0);
    }

    void designHighPass (double fs, double f0, double q,
                         float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cosw0 = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);

        const double bb0 = (1.0 + cosw0) / 2.0;
        const double bb1 = -(1.0 + cosw0);
        const double bb2 = (1.0 + cosw0) / 2.0;
        const double aa0 = 1.0 + alpha;
        const double aa1 = -2.0 * cosw0;
        const double aa2 = 1.0 - alpha;

        b0 = (float) (bb0 / aa0); b1 = (float) (bb1 / aa0); b2 = (float) (bb2 / aa0);
        a1 = (float) (aa1 / aa0); a2 = (float) (aa2 / aa0);
    }

    // Linear-interpolated percentile of a sorted vector, 0-100 scale.
    float percentileOfSorted (const std::vector<float>& sorted, float pct) noexcept
    {
        if (sorted.empty()) return -70.f;
        if (sorted.size() == 1) return sorted[0];

        const double pos = (pct / 100.0) * (double) (sorted.size() - 1);
        const size_t lo = (size_t) std::floor (pos);
        const size_t hi = std::min (lo + 1, sorted.size() - 1);
        const double frac = pos - (double) lo;
        return (float) (sorted[lo] * (1.0 - frac) + sorted[hi] * frac);
    }
}

void NovaLoudnessAnalyzer::designKWeightFilter (KWeightFilter& f) const
{
    // BS.1770-4 Annex 1 analog-prototype parameters.
    designHighShelf (sampleRate, 1681.9744509555319, 0.7071752369554193, 3.99984385397,
                     f.stage1.b0, f.stage1.b1, f.stage1.b2, f.stage1.a1, f.stage1.a2);
    designHighPass (sampleRate, 38.13547087613982, 0.5003270373238773,
                    f.stage2.b0, f.stage2.b1, f.stage2.b2, f.stage2.a1, f.stage2.a2);
}

void NovaLoudnessAnalyzer::prepare (double sr)
{
    sampleRate = sr;
    hopSamples = (int) std::round (sr * 0.1); // 100ms hop

    designKWeightFilter (kFilterL);
    designKWeightFilter (kFilterR);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
    oversampler->initProcessing (8192);

    reset();
}

void NovaLoudnessAnalyzer::reset() noexcept
{
    kFilterL.reset();
    kFilterR.reset();
    if (oversampler) oversampler->reset();

    hopSampleCount = 0;
    hopSumSqL = hopSumSqR = 0.0;
    hopMeanSquares.clear();

    integratedLufs  = -70.f;
    shortTermLufs   = -70.f;
    loudnessRangeLu = 0.f;
    truePeakDb      = -100.f;
    samplePeakDb    = -100.f;
}

void NovaLoudnessAnalyzer::process (const juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = buffer.getNumChannels();
    const int N   = buffer.getNumSamples();
    if (N == 0 || nCh == 0 || hopSamples <= 0) return;

    const float* l = buffer.getReadPointer (0);
    const float* r = nCh > 1 ? buffer.getReadPointer (1) : l;

    // Sample peak.
    const float peak = buffer.getMagnitude (0, N);
    if (peak > 0.f)
        samplePeakDb = juce::jmax (samplePeakDb, juce::Decibels::gainToDecibels (peak));

    // K-weighted loudness accumulation, 100ms hops.
    for (int i = 0; i < N; ++i)
    {
        const float kl = kFilterL.processSample (l[i]);
        const float kr = kFilterR.processSample (r[i]);
        hopSumSqL += (double) kl * (double) kl;
        hopSumSqR += (double) kr * (double) kr;
        ++hopSampleCount;

        if (hopSampleCount >= hopSamples)
        {
            hopMeanSquares.emplace_back ((float) (hopSumSqL / hopSampleCount),
                                         (float) (hopSumSqR / hopSampleCount));
            hopSampleCount = 0;
            hopSumSqL = hopSumSqR = 0.0;
        }
    }

    // Oversampled true peak, processed in fixed-size chunks matching the
    // oversampler's prepared block size (offline: input may be far larger
    // than that, e.g. a whole imported file in one call).
    if (oversampler != nullptr)
    {
        constexpr int kChunk = 8192;
        juce::AudioBuffer<float> scratch (nCh, kChunk);

        for (int start = 0; start < N; start += kChunk)
        {
            const int thisLen = juce::jmin (kChunk, N - start);
            for (int ch = 0; ch < nCh; ++ch)
                scratch.copyFrom (ch, 0, buffer, ch, start, thisLen);

            juce::dsp::AudioBlock<float> block (scratch);
            auto sub = block.getSubBlock (0, (size_t) thisLen);
            auto upBlock = oversampler->processSamplesUp (sub);

            for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch)
            {
                const float* d = upBlock.getChannelPointer (ch);
                for (size_t i = 0; i < upBlock.getNumSamples(); ++i)
                    truePeakDb = juce::jmax (truePeakDb, std::abs (d[i]));
            }
        }
    }
}

void NovaLoudnessAnalyzer::finalize() noexcept
{
    // truePeakDb currently holds a linear magnitude accumulated in
    // process() (reusing the member to avoid an extra field) - convert to
    // dB now, once, guarding silence.
    truePeakDb = truePeakDb > 0.f ? juce::Decibels::gainToDecibels (truePeakDb) : -100.f;

    if (hopMeanSquares.size() < 4)
        return; // not enough material for a single 400ms gating block

    // ---- Integrated loudness (BS.1770-4): 400ms blocks, 100ms hop (75% overlap) ----
    std::vector<float> blockLoudness;
    blockLoudness.reserve (hopMeanSquares.size());
    for (size_t i = 0; i + 4 <= hopMeanSquares.size(); ++i)
    {
        double msL = 0.0, msR = 0.0;
        for (size_t k = i; k < i + 4; ++k) { msL += hopMeanSquares[k].first; msR += hopMeanSquares[k].second; }
        msL *= 0.25; msR *= 0.25;
        const double sum = msL + msR;
        blockLoudness.push_back (sum > 1e-12 ? (float) (-0.691 + 10.0 * std::log10 (sum)) : -100.f);
    }

    auto energyMean = [] (const std::vector<float>& vals) -> double
    {
        if (vals.empty()) return 0.0;
        double sum = 0.0;
        for (float v : vals) sum += std::pow (10.0, (v + 0.691) / 10.0);
        return sum / (double) vals.size();
    };

    std::vector<float> gate1;
    for (float v : blockLoudness) if (v >= -70.f) gate1.push_back (v);

    if (! gate1.empty())
    {
        const double ungatedEnergy = energyMean (gate1);
        const float relativeThreshold = ungatedEnergy > 1e-12
            ? (float) (-0.691 + 10.0 * std::log10 (ungatedEnergy)) - 10.f : -70.f;

        std::vector<float> gate2;
        for (float v : gate1) if (v >= relativeThreshold) gate2.push_back (v);

        if (! gate2.empty())
        {
            const double finalEnergy = energyMean (gate2);
            integratedLufs = finalEnergy > 1e-12 ? (float) (-0.691 + 10.0 * std::log10 (finalEnergy)) : -70.f;
        }
    }

    // ---- Short-term loudness: 3s windows (30 hops), stepped by 1 hop ----
    std::vector<float> shortTermValues;
    if (hopMeanSquares.size() >= 30)
    {
        shortTermValues.reserve (hopMeanSquares.size() - 29);
        for (size_t i = 0; i + 30 <= hopMeanSquares.size(); ++i)
        {
            double msL = 0.0, msR = 0.0;
            for (size_t k = i; k < i + 30; ++k) { msL += hopMeanSquares[k].first; msR += hopMeanSquares[k].second; }
            msL /= 30.0; msR /= 30.0;
            const double sum = msL + msR;
            shortTermValues.push_back (sum > 1e-12 ? (float) (-0.691 + 10.0 * std::log10 (sum)) : -100.f);
        }

        shortTermLufs = -70.f;
        for (float v : shortTermValues) shortTermLufs = juce::jmax (shortTermLufs, v);
    }

    // ---- Loudness range (EBU Tech 3342): two-gate percentile method ----
    if (! shortTermValues.empty())
    {
        std::vector<float> lraGate1;
        for (float v : shortTermValues) if (v >= -70.f) lraGate1.push_back (v);

        if (! lraGate1.empty())
        {
            const double ungatedEnergy = energyMean (lraGate1);
            const float relativeThreshold = ungatedEnergy > 1e-12
                ? (float) (-0.691 + 10.0 * std::log10 (ungatedEnergy)) - 20.f : -70.f;

            std::vector<float> lraGate2;
            for (float v : lraGate1) if (v >= relativeThreshold) lraGate2.push_back (v);

            if (lraGate2.size() > 1)
            {
                std::sort (lraGate2.begin(), lraGate2.end());
                const float p10 = percentileOfSorted (lraGate2, 10.f);
                const float p95 = percentileOfSorted (lraGate2, 95.f);
                loudnessRangeLu = juce::jmax (0.f, p95 - p10);
            }
        }
    }
}
