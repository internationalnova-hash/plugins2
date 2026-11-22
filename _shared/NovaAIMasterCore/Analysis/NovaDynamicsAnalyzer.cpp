#include "NovaDynamicsAnalyzer.h"
#include <algorithm>
#include <cmath>

void NovaDynamicsAnalyzer::prepare (double sr)
{
    sampleRate = sr;

    // Transient envelope time constants (technique ported from Nova Apex's
    // transientWeight(): a very fast follower vs. a moderately slow one,
    // strength derived from their ratio). Apex hardcodes coefficients
    // (~0.9 fast / ~0.9995 slow) tuned for a specific sample rate; here the
    // equivalent time constants (~0.2ms fast, ~45ms slow) are derived from
    // those and then converted to per-sample coefficients for the actual
    // sample rate, so the detector behaves consistently at 44.1/48/96kHz.
    const double preSmoothTauSeconds = 0.003;
    const double fastTauSeconds = 0.0002;
    const double slowTauSeconds = 0.045;
    preSmoothCoeff = (float) std::exp (-1.0 / (preSmoothTauSeconds * sr));
    fastEnvCoeff = (float) std::exp (-1.0 / (fastTauSeconds * sr));
    slowEnvCoeff = (float) std::exp (-1.0 / (slowTauSeconds * sr));

    microWindowSamples = juce::jmax (1, (int) std::round (sr * 0.05));  // 50ms
    macroWindowSamples = juce::jmax (1, (int) std::round (sr * 3.0));   // 3s

    reset();
}

void NovaDynamicsAnalyzer::reset() noexcept
{
    crestWindowSamples   = 0;
    crestPeakAccum       = 0.f;
    crestSquaredSumAccum = 0.0;

    preSmoothState = 0.f;
    fastEnv = slowEnv = 0.f;
    aboveOnsetThreshold = false;
    onsetCount = 0;
    totalSamplesProcessed = 0;
    transientStrengthRunningMax = 0.f;

    microWindowCount = 0;
    microSumSq = 0.0;
    microRmsDbSeries.clear();

    macroWindowCount = 0;
    macroSumSq = 0.0;
    macroRmsDbSeries.clear();

    crestFactorDb     = 0.f;
    macroDynamicRange = 0.f;
    microDynamics     = 0.f;
    transientDensity  = 0.f;
    transientStrength = 0.f;
    compressionAmount = 0.f;
}

void NovaDynamicsAnalyzer::process (const juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = buffer.getNumChannels();
    const int N   = buffer.getNumSamples();
    if (nCh == 0 || N == 0) return;

    const float* l = buffer.getReadPointer (0);
    const float* r = nCh > 1 ? buffer.getReadPointer (1) : l;

    float runningCrestDb = crestFactorDb;

    for (int i = 0; i < N; ++i)
    {
        const float sampleAbs = juce::jmax (std::abs (l[i]), std::abs (r[i]));
        const float sampleSq  = ((l[i] * l[i]) + (r[i] * r[i])) * 0.5f;

        // Crest factor, ported from Nova Master (PluginProcessor.cpp:527-569).
        crestPeakAccum = juce::jmax (crestPeakAccum, sampleAbs);
        crestSquaredSumAccum += (double) sampleAbs * (double) sampleAbs;
        ++crestWindowSamples;
        if (crestWindowSamples >= kCrestWindowSize)
        {
            const float windowRms = (float) std::sqrt (crestSquaredSumAccum / (double) crestWindowSamples);
            const float crestDb = windowRms > 1e-9f
                                     ? juce::Decibels::gainToDecibels (crestPeakAccum / windowRms, 0.f)
                                     : 0.f;
            runningCrestDb = (runningCrestDb * 0.7f) + (juce::jlimit (0.f, 24.f, crestDb) * 0.3f);
            crestWindowSamples = 0;
            crestPeakAccum = 0.f;
            crestSquaredSumAccum = 0.0;
        }

        // Transient detection: fast/slow envelope ratio (Nova Apex
        // technique) fed by a short pre-smoothed energy estimate rather
        // than the raw sample, so isolated-sample noise jitter can't
        // register as an "onset" on its own.
        preSmoothState = preSmoothState * preSmoothCoeff + sampleAbs * (1.f - preSmoothCoeff);
        const float smoothedAbs = preSmoothState;
        fastEnv = juce::jmax (smoothedAbs, fastEnv * fastEnvCoeff + smoothedAbs * (1.f - fastEnvCoeff));
        slowEnv = slowEnv * slowEnvCoeff + smoothedAbs * (1.f - slowEnvCoeff);
        const float ratio = slowEnv > 1e-6f ? (fastEnv / slowEnv) : 1.f;
        const float instStrength = juce::jlimit (0.f, 1.f, (ratio - 1.4f) / 2.f);
        transientStrengthRunningMax = juce::jmax (transientStrengthRunningMax * 0.9999f, instStrength);

        constexpr float kOnsetThreshold = 0.35f;
        if (instStrength >= kOnsetThreshold && ! aboveOnsetThreshold) { ++onsetCount; aboveOnsetThreshold = true; }
        else if (instStrength < kOnsetThreshold * 0.6f) aboveOnsetThreshold = false;

        // Micro dynamics: 50ms RMS series.
        microSumSq += (double) sampleSq;
        if (++microWindowCount >= microWindowSamples)
        {
            const float rms = (float) std::sqrt (microSumSq / (double) microWindowCount);
            microRmsDbSeries.push_back (rms > 1e-9f ? juce::Decibels::gainToDecibels (rms) : -100.f);
            microWindowCount = 0;
            microSumSq = 0.0;
        }

        // Macro dynamic range: 3s RMS series.
        macroSumSq += (double) sampleSq;
        if (++macroWindowCount >= macroWindowSamples)
        {
            const float rms = (float) std::sqrt (macroSumSq / (double) macroWindowCount);
            macroRmsDbSeries.push_back (rms > 1e-9f ? juce::Decibels::gainToDecibels (rms) : -100.f);
            macroWindowCount = 0;
            macroSumSq = 0.0;
        }

        ++totalSamplesProcessed;
    }

    crestFactorDb = runningCrestDb;
    compressionAmount = juce::jlimit (0.f, 1.f, 1.f - (crestFactorDb / 18.f));
    transientStrength = transientStrengthRunningMax;
}

void NovaDynamicsAnalyzer::finalize() noexcept
{
    const double durationSeconds = sampleRate > 0.0 ? (double) totalSamplesProcessed / sampleRate : 0.0;
    transientDensity = durationSeconds > 0.0
        ? juce::jlimit (0.f, 1.f, (float) (onsetCount / durationSeconds) / 8.f)
        : 0.f;

    if (microRmsDbSeries.size() > 1)
    {
        double mean = 0.0;
        for (float v : microRmsDbSeries) mean += v;
        mean /= (double) microRmsDbSeries.size();

        double variance = 0.0;
        for (float v : microRmsDbSeries) variance += (v - mean) * (v - mean);
        variance /= (double) microRmsDbSeries.size();

        const float stdDevDb = (float) std::sqrt (variance);
        microDynamics = juce::jlimit (0.f, 1.f, stdDevDb / 12.f);
    }

    if (macroRmsDbSeries.size() > 1)
    {
        std::vector<float> sorted = macroRmsDbSeries;
        std::sort (sorted.begin(), sorted.end());
        const auto p10Index = (size_t) (0.10 * (double) (sorted.size() - 1));
        const auto p90Index = (size_t) (0.90 * (double) (sorted.size() - 1));
        const float spreadDb = sorted[p90Index] - sorted[p10Index];
        macroDynamicRange = juce::jlimit (0.f, 1.f, spreadDb / 20.f);
    }
}
