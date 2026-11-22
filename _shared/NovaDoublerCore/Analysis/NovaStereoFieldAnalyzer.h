#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

// Continuously-updated stereo-field metrics on the real, final post-mix
// output - L/R RMS, mid/side RMS, correlation, and a derived 0-100%
// spread metric. Feeds the UI's single live stereo visualization (spec:
// real signal, never faked/simulated). Runs entirely on the audio thread
// via leaky integrators (same sample-rate-correct technique as
// NovaOrganicModulator) - no allocation, no locks; the caller snapshots
// the getters into std::atomic<float> fields once per block for the
// editor's timer to read.
class NovaStereoFieldAnalyzer
{
public:
    NovaStereoFieldAnalyzer() = default;

    void prepare (double sampleRate) noexcept
    {
        // Meter-ballistics-style smoothing: responsive enough to track
        // real playing dynamics, slow enough not to flicker sample-to-sample.
        constexpr float tau = 0.15f;
        leak = std::exp (-1.0f / (tau * (float) sampleRate));
        reset();
    }

    void reset() noexcept
    {
        runL2 = 0.0f;
        runR2 = 0.0f;
        runLR = 0.0f;
        runMid2 = 0.0f;
        runSide2 = 0.0f;
    }

    void processSample (float l, float r) noexcept
    {
        const float mid = 0.5f * (l + r);
        const float side = 0.5f * (l - r);

        runL2 = runL2 * leak + (1.0f - leak) * (l * l);
        runR2 = runR2 * leak + (1.0f - leak) * (r * r);
        runLR = runLR * leak + (1.0f - leak) * (l * r);
        runMid2 = runMid2 * leak + (1.0f - leak) * (mid * mid);
        runSide2 = runSide2 * leak + (1.0f - leak) * (side * side);
    }

    float getLeftRms() const noexcept { return std::sqrt (juce::jmax (0.0f, runL2)); }
    float getRightRms() const noexcept { return std::sqrt (juce::jmax (0.0f, runR2)); }
    float getMidRms() const noexcept { return std::sqrt (juce::jmax (0.0f, runMid2)); }
    float getSideRms() const noexcept { return std::sqrt (juce::jmax (0.0f, runSide2)); }

    // -1 (fully out-of-phase) .. 0 (decorrelated) .. +1 (mono/identical).
    float getCorrelation() const noexcept
    {
        const float denom = std::sqrt (juce::jmax (0.0f, runL2) * juce::jmax (0.0f, runR2));
        if (denom < 1.0e-9f) return 1.0f; // silence reads as mono/coherent, not undefined
        return juce::jlimit (-1.0f, 1.0f, runLR / denom);
    }

    // 0% (mono/identical) .. 50% (decorrelated) .. 100% (fully out-of-phase).
    float getSpread() const noexcept
    {
        return juce::jlimit (0.0f, 100.0f, (1.0f - getCorrelation()) * 50.0f);
    }

private:
    float leak = 0.999f;
    float runL2 = 0.0f, runR2 = 0.0f, runLR = 0.0f, runMid2 = 0.0f, runSide2 = 0.0f;
};
