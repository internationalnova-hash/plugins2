#include "NovaResonanceControl.h"
#include <cmath>

namespace
{
    struct BandDef { double freq; double probeQ; };

    // Same 7 crossover-derived centers as NovaSpectralAnalyzer, for
    // consistency between what the analysis engine measures and what this
    // stage can act on.
    const std::array<BandDef, NovaResonanceControl::kNumBands> kBands { {
        { 60.0,   0.707 }, { 122.0, 0.64 }, { 354.0, 1.42 }, { 1000.0, 0.67 },
        { 2828.0, 1.41 }, { 5657.0, 1.41 }, { 12000.0, 0.8 },
    } };

    constexpr double kCutQ = 1.2; // fixed moderate width for the suppression bell

    void designBandpass (double fs, double f0, double q,
                         float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cosw0 = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double bb0 = alpha, bb1 = 0.0, bb2 = -alpha;
        const double aa0 = 1.0 + alpha, aa1 = -2.0 * cosw0, aa2 = 1.0 - alpha;
        b0 = (float) (bb0 / aa0); b1 = (float) (bb1 / aa0); b2 = (float) (bb2 / aa0);
        a1 = (float) (aa1 / aa0); a2 = (float) (aa2 / aa0);
    }

    void designPeak (double fs, double f0, double gainDb, double q,
                     float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cosw0 = std::cos (w0);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double bb0 = 1.0 + alpha * A, bb1 = -2.0 * cosw0, bb2 = 1.0 - alpha * A;
        const double aa0 = 1.0 + alpha / A, aa1 = -2.0 * cosw0, aa2 = 1.0 - alpha / A;
        b0 = (float) (bb0 / aa0); b1 = (float) (bb1 / aa0); b2 = (float) (bb2 / aa0);
        a1 = (float) (aa1 / aa0); a2 = (float) (aa2 / aa0);
    }
}

void NovaResonanceControl::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    for (size_t i = 0; i < kNumBands; ++i)
    {
        float b0, b1, b2, a1, a2;
        designBandpass (sampleRate, kBands[i].freq, kBands[i].probeQ, b0, b1, b2, a1, a2);
        probeL[i] = { b0, b1, b2, a1, a2, 0.f, 0.f };
        probeR[i] = { b0, b1, b2, a1, a2, 0.f, 0.f };
    }

    // Fast envelope: ~5ms attack / ~80ms release (tracks current band
    // energy). A second, slower-tracking stage (see process()) learns the
    // band's typical level so reduction only engages when something
    // stands out above the band's own trend, not just whenever it's loud.
    envAttackCoeff  = (float) std::exp (-1.0 / (0.005 * sampleRate));
    envReleaseCoeff = (float) std::exp (-1.0 / (0.080 * sampleRate));

    reset();
    setPlan (plan);
}

void NovaResonanceControl::reset() noexcept
{
    for (auto& f : probeL) f.reset();
    for (auto& f : probeR) f.reset();
    for (auto& f : cutL) f.reset();
    for (auto& f : cutR) f.reset();
    envelope.fill (0.f);
    currentReductionDb.fill (0.f);
    slowTrend.fill (0.f);
}

void NovaResonanceControl::setPlan (const NovaMasteringPlan::ResonanceControl& p) noexcept
{
    plan = p;
}

void NovaResonanceControl::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! plan.active) return;

    const int nCh = buffer.getNumChannels();
    const int N   = buffer.getNumSamples();
    if (nCh == 0 || N == 0) return;

    float* l = buffer.getWritePointer (0);
    float* r = nCh > 1 ? buffer.getWritePointer (1) : l;

    // Conservative by construction: max possible cut scales with
    // plan.amount, capped well short of aggressive (spec section 14:
    // "processing must be conservative").
    const float maxReductionDb = 3.f + plan.amount * 3.f;

    // ---- Detection: probe each band, update envelope ----
    for (size_t b = 0; b < kNumBands; ++b)
    {
        for (int i = 0; i < N; ++i)
        {
            const float pl = probeL[b].processSample (l[i]);
            const float pr = nCh > 1 ? probeR[b].processSample (r[i]) : pl;
            const float mag = juce::jmax (std::abs (pl), std::abs (pr));

            if (mag > envelope[b])
                envelope[b] = envelope[b] * envAttackCoeff + mag * (1.f - envAttackCoeff);
            else
                envelope[b] = envelope[b] * envReleaseCoeff + mag * (1.f - envReleaseCoeff);
        }
    }

    // Target reduction: compare the fast envelope against a slow trend of
    // itself (a one-pole "average of the average", ~1s time constant) -
    // only bands standing out above their own recent trend get reduced.
    for (size_t b = 0; b < kNumBands; ++b)
    {
        const float trendCoeff = 0.9995f; // ~1s-ish at typical block rates via repeated per-block updates
        slowTrend[b] = slowTrend[b] * trendCoeff + envelope[b] * (1.f - trendCoeff);

        const float envDb   = juce::Decibels::gainToDecibels (envelope[b], -100.f);
        const float trendDb = juce::Decibels::gainToDecibels (slowTrend[b], -100.f);
        const float excessDb = envDb - trendDb;

        const float targetReductionDb = juce::jlimit (0.f, maxReductionDb, excessDb * plan.amount * 0.6f);

        // Smooth toward target (fast-ish onset, slower release) so the
        // cut filter's coefficients don't jump abruptly block to block.
        if (targetReductionDb > currentReductionDb[b])
            currentReductionDb[b] += (targetReductionDb - currentReductionDb[b]) * 0.3f;
        else
            currentReductionDb[b] += (targetReductionDb - currentReductionDb[b]) * 0.05f;

        float b0, b1, b2, a1, a2;
        designPeak (sampleRate, kBands[b].freq, -currentReductionDb[b], kCutQ, b0, b1, b2, a1, a2);
        cutL[b].b0 = b0; cutL[b].b1 = b1; cutL[b].b2 = b2; cutL[b].a1 = a1; cutL[b].a2 = a2;
        cutR[b].b0 = b0; cutR[b].b1 = b1; cutR[b].b2 = b2; cutR[b].a1 = a1; cutR[b].a2 = a2;
    }

    // ---- Suppression: cascade all 7 dynamic cut filters on the real signal ----
    for (int i = 0; i < N; ++i)
    {
        float xl = l[i], xr = r[i];
        for (size_t b = 0; b < kNumBands; ++b)
        {
            xl = cutL[b].processSample (xl);
            if (nCh > 1) xr = cutR[b].processSample (xr);
        }
        l[i] = xl;
        if (nCh > 1) r[i] = xr;
    }
}
