#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Decision/NovaMasteringPlan.h"

// 4-band broad corrective EQ (low shelf, low-mid bell, high-mid bell, high
// shelf). RBJ-cookbook-derived coefficients (same technique as
// NovaLoudnessAnalyzer's K-weighting filter and NovaSpectralAnalyzer's
// band filters), correct at any sample rate.
class NovaCorrectiveEQ
{
public:
    NovaCorrectiveEQ() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;
    void setPlan (const NovaMasteringPlan::CorrectiveEQ& plan) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    struct Biquad
    {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
        float z1 = 0.f, z2 = 0.f;
        void reset() noexcept { z1 = z2 = 0.f; }
        inline float processSample (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x + z2 - a1 * y;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    enum { LowShelf = 0, LowMidBell, HighMidBell, HighShelf, kNumBands };

    double sampleRate = 44100.0;
    std::array<Biquad, kNumBands> filtersL, filtersR;

    NovaMasteringPlan::CorrectiveEQ plan;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaCorrectiveEQ)
};
