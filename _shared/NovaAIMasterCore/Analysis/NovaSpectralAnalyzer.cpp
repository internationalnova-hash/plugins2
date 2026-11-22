#include "NovaSpectralAnalyzer.h"
#include <cmath>

namespace
{
    struct BandDef { double freq; double q; enum { LowPass, BandPass, HighPass } type; };

    // Crossover points at 60/250/500/2000/4000/8000 Hz define the 7 bands
    // (sub/bass/lowMid/mid/upperMid/presence/high). Inner bands use a
    // bandpass centered at the geometric mean of their edges, with Q set
    // from center/bandwidth - the same broad-strokes graphic-EQ-style
    // split used throughout this codebase's other analyzers.
    const std::array<BandDef, NovaSpectralAnalyzer::kNumBands> kBands { {
        { 60.0,   0.707, BandDef::LowPass  },              // Sub
        { 122.0,  0.64,  BandDef::BandPass },               // Bass    (60-250)
        { 354.0,  1.42,  BandDef::BandPass },               // LowMid  (250-500)
        { 1000.0, 0.67,  BandDef::BandPass },               // Mid     (500-2000)
        { 2828.0, 1.41,  BandDef::BandPass },               // UpperMid(2000-4000)
        { 5657.0, 1.41,  BandDef::BandPass },               // Presence(4000-8000)
        { 8000.0, 0.707, BandDef::HighPass },                // High/Air
    } };

    void designBiquad (const BandDef& def, double fs,
                       float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * def.freq / fs;
        const double cosw0 = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * def.q);

        double bb0, bb1, bb2, aa0, aa1, aa2;

        if (def.type == BandDef::LowPass)
        {
            bb0 = (1.0 - cosw0) / 2.0; bb1 = 1.0 - cosw0; bb2 = (1.0 - cosw0) / 2.0;
        }
        else if (def.type == BandDef::HighPass)
        {
            bb0 = (1.0 + cosw0) / 2.0; bb1 = -(1.0 + cosw0); bb2 = (1.0 + cosw0) / 2.0;
        }
        else // BandPass, constant 0dB peak gain
        {
            bb0 = alpha; bb1 = 0.0; bb2 = -alpha;
        }

        aa0 = 1.0 + alpha; aa1 = -2.0 * cosw0; aa2 = 1.0 - alpha;

        b0 = (float) (bb0 / aa0); b1 = (float) (bb1 / aa0); b2 = (float) (bb2 / aa0);
        a1 = (float) (aa1 / aa0); a2 = (float) (aa2 / aa0);
    }
}

void NovaSpectralAnalyzer::prepare (double sr)
{
    sampleRate = sr;

    for (size_t i = 0; i < kNumBands; ++i)
    {
        float b0, b1, b2, a1, a2;
        designBiquad (kBands[i], sr, b0, b1, b2, a1, a2);
        filtersL[i] = { b0, b1, b2, a1, a2, 0.f, 0.f };
        filtersR[i] = { b0, b1, b2, a1, a2, 0.f, 0.f };
    }

    // Calibration: run deterministic white noise through throwaway filter
    // copies (own state, doesn't touch filtersL/filtersR) to learn each
    // band's "neutral" energy share - narrower low-frequency bands pass
    // less broadband noise power than wider high-frequency ones purely
    // from bandwidth, independent of anything about a real signal's
    // actual spectral shape. Harshness/resonance below compare against
    // this baseline instead of assuming an even 1/kNumBands split.
    {
        std::array<Biquad, kNumBands> calFilters;
        for (size_t i = 0; i < kNumBands; ++i)
            calFilters[i] = filtersL[i]; // same coefficients, fresh z1/z2 (0)

        std::array<double, kNumBands> calSumSq {};
        juce::Random rng (0x4E4F5641); // fixed seed ("NOVA" in hex) - calibration must be deterministic
        const int calSamples = (int) (sr * 2.0);
        double calTotal = 0.0;

        for (int i = 0; i < calSamples; ++i)
        {
            const float noise = rng.nextFloat() * 2.f - 1.f;
            for (size_t b = 0; b < kNumBands; ++b)
            {
                const float y = calFilters[b].processSample (noise);
                calSumSq[b] += (double) y * (double) y;
            }
        }
        for (size_t b = 0; b < kNumBands; ++b) calTotal += calSumSq[b];

        if (calTotal > 1e-12)
            for (size_t b = 0; b < kNumBands; ++b)
                calibratedNeutralShare[b] = (float) (calSumSq[b] / calTotal);
        else
            calibratedNeutralShare.fill (1.f / (float) kNumBands);
    }

    reset();
}

void NovaSpectralAnalyzer::reset() noexcept
{
    for (auto& f : filtersL) f.reset();
    for (auto& f : filtersR) f.reset();
    sumSq.fill (0.0);
    samplesProcessed = 0;

    bandEnergy.fill (1.f / (float) kNumBands);
    spectralTilt   = 0.f;
    harshness      = 0.f;
    resonanceScore = 0.f;
}

void NovaSpectralAnalyzer::process (const juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = buffer.getNumChannels();
    const int N   = buffer.getNumSamples();
    if (nCh == 0 || N == 0) return;

    const float* l = buffer.getReadPointer (0);
    const float* r = nCh > 1 ? buffer.getReadPointer (1) : l;

    for (int i = 0; i < N; ++i)
    {
        for (size_t b = 0; b < kNumBands; ++b)
        {
            const float yl = filtersL[b].processSample (l[i]);
            const float yr = filtersR[b].processSample (r[i]);
            sumSq[b] += 0.5 * ((double) yl * yl + (double) yr * yr);
        }
        ++samplesProcessed;
    }
}

void NovaSpectralAnalyzer::finalize() noexcept
{
    if (samplesProcessed == 0) return;

    double total = 0.0;
    std::array<double, kNumBands> meanSq {};
    for (size_t b = 0; b < kNumBands; ++b)
    {
        meanSq[b] = sumSq[b] / (double) samplesProcessed;
        total += meanSq[b];
    }

    if (total < 1e-12) return; // silence - keep flat defaults from reset()

    for (size_t b = 0; b < kNumBands; ++b)
        bandEnergy[b] = (float) (meanSq[b] / total);

    // Spectral tilt: linear regression of per-band dB energy against
    // log2(center frequency), normalized to roughly [-1, 1].
    {
        double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumXX = 0.0;
        for (size_t b = 0; b < kNumBands; ++b)
        {
            const double x = std::log2 (kBands[b].freq);
            const double y = meanSq[b] > 1e-12 ? 10.0 * std::log10 (meanSq[b]) : -100.0;
            sumX += x; sumY += y; sumXY += x * y; sumXX += x * x;
        }
        const double n = (double) kNumBands;
        const double denom = n * sumXX - sumX * sumX;
        const double slope = std::abs (denom) > 1e-9 ? (n * sumXY - sumX * sumY) / denom : 0.0;
        spectralTilt = juce::jlimit (-1.f, 1.f, (float) (slope / 6.0));
    }

    // Harshness: excess energy share in upperMid+presence (~2-8kHz) vs.
    // each band's own calibrated neutral (flat-source) baseline, not a
    // naive even 1/kNumBands split - see calibratedNeutralShare comment.
    {
        const float actualShare = bandEnergy[UpperMid] + bandEnergy[Presence];
        const float neutralShare = calibratedNeutralShare[UpperMid] + calibratedNeutralShare[Presence];
        harshness = neutralShare > 1e-9f
            ? juce::jlimit (0.f, 1.f, (actualShare - neutralShare) / neutralShare)
            : 0.f;
    }

    // Resonance score: spectral flatness (geometric/arithmetic mean) of
    // each band's energy NORMALIZED by its calibrated neutral share first
    // - a genuinely flat-spectrum source reads as flatness ~1 (resonance
    // ~0) regardless of the filter bank's own uneven bandwidths, so this
    // only picks up real peakiness in the source. Coarser than true
    // narrow-band resonance detection (see class comment).
    {
        double logSum = 0.0;
        double arithMean = 0.0;
        for (size_t b = 0; b < kNumBands; ++b)
        {
            const double normalized = calibratedNeutralShare[b] > 1e-9f
                ? (double) bandEnergy[b] / (double) calibratedNeutralShare[b] : 1.0;
            const double v = juce::jmax (1e-9, normalized);
            logSum += std::log (v);
            arithMean += v;
        }
        arithMean /= (double) kNumBands;
        const double geoMean = std::exp (logSum / (double) kNumBands);
        const double flatness = arithMean > 1e-12 ? juce::jlimit (0.0, 1.0, geoMean / arithMean) : 1.0;
        resonanceScore = juce::jlimit (0.f, 1.f, (float) (1.0 - flatness));
    }
}
