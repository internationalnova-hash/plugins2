#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// Offline dynamics analyzer: crest factor, macro/micro dynamic range,
// transient density/strength, derived "how compressed is this already"
// score. See spec section 6.
class NovaDynamicsAnalyzer
{
public:
    NovaDynamicsAnalyzer() = default;

    void prepare (double sampleRate);
    void reset() noexcept;
    void process (const juce::AudioBuffer<float>& buffer) noexcept;

    // Call once after all process() calls, before reading percentile-based
    // getters (macro dynamic range, micro dynamics).
    void finalize() noexcept;

    float getCrestFactorDb() const noexcept     { return crestFactorDb; }
    float getMacroDynamicRange() const noexcept { return macroDynamicRange; }
    float getMicroDynamics() const noexcept     { return microDynamics; }
    float getTransientDensity() const noexcept  { return transientDensity; }
    float getTransientStrength() const noexcept { return transientStrength; }
    float getCompressionAmount() const noexcept { return compressionAmount; }

private:
    double sampleRate = 44100.0;

    // ---- Crest factor (windowed peak/RMS, ported from Nova Master) ----
    static constexpr int kCrestWindowSize = 2048;
    int    crestWindowSamples = 0;
    float  crestPeakAccum = 0.f;
    double crestSquaredSumAccum = 0.0;

    // ---- Transient detection (technique from Nova Apex: fast/slow
    // envelope-follower ratio), time constants derived per-sample-rate ----
    // A short pre-smoothing stage feeds the ratio detector a rectified
    // short-duration energy estimate rather than the raw instantaneous
    // sample, so single-sample noise jitter can't trip it - onset
    // detection needs to track energy that persists a few ms, not one
    // sample. Apex's original use (a continuous per-sample weighting
    // factor, not discrete onset counting) never surfaced this.
    float preSmoothCoeff = 0.f;
    float preSmoothState = 0.f;
    float fastEnvCoeff = 0.f, slowEnvCoeff = 0.f;
    float fastEnv = 0.f, slowEnv = 0.f;
    bool  aboveOnsetThreshold = false;
    int   onsetCount = 0;
    long long totalSamplesProcessed = 0;
    float transientStrengthRunningMax = 0.f;

    // ---- Short-window RMS series (50ms) -> micro dynamics ----
    int    microWindowSamples = 0;
    int    microWindowCount = 0;
    double microSumSq = 0.0;
    std::vector<float> microRmsDbSeries;

    // ---- Long-window RMS series (3s) -> macro dynamic range ----
    int    macroWindowSamples = 0;
    int    macroWindowCount = 0;
    double macroSumSq = 0.0;
    std::vector<float> macroRmsDbSeries;

    float crestFactorDb     = 0.f;
    float macroDynamicRange = 0.f;
    float microDynamics     = 0.f;
    float transientDensity  = 0.f;
    float transientStrength = 0.f;
    float compressionAmount = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaDynamicsAnalyzer)
};
