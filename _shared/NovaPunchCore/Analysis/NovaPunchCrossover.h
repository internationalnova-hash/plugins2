#pragma once

#include <juce_dsp/juce_dsp.h>

// Phase-coherent 3-band Linkwitz-Riley crossover - confirmed via catalog
// research that nothing like this exists anywhere in plugins2 (the
// closest relative, NovaResonanceControl, explicitly documents why it
// avoided building one: "a parallel band-split + recombine... would need
// a complementary crossover design to avoid coloring the signal"). This
// is that complementary design, built for real: two LR4 (4th-order
// Linkwitz-Riley, i.e. two cascaded 2nd-order Butterworth stages at the
// same cutoff) splits in series, so low+mid+high sums back to a flat,
// uncolored copy of the input at unity gain.
//
// This backs FOCUS (weights which band's activity drives detection/
// shaping - prevents e.g. a Low-focused Punch boost from also grabbing
// hi-hat bleed) and stands in for "spectral flux" analysis cheaply: the
// per-band energy this produces is enough to detect frequency-localized
// transient activity without a separate FFT/STFT path and its added
// latency.
//
// Crossover points are fixed (not user-adjustable - FOCUS selects a
// WEIGHTING over these three fixed bands, it doesn't move them).
//
// Real-time-safe: filters sized/coefficients computed once in prepare()
// (message thread); processSample() only runs the filters.
namespace nova::punch
{
    class NovaPunchCrossover
    {
    public:
        NovaPunchCrossover() = default;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        struct Bands
        {
            float low = 0.0f, mid = 0.0f, high = 0.0f;
        };

        Bands processSample (float input) noexcept;

        static constexpr float kLowMidHz = 180.0f;
        static constexpr float kMidHighHz = 2800.0f;

    private:
        double sr = 44100.0;

        // Stage 1: split input into low (LP4 @ kLowMidHz) and
        // highPass1 (HP4 @ kLowMidHz) - each "4" is two cascaded 2nd-
        // order Butterworth stages at the same cutoff (the LR4 recipe).
        juce::dsp::IIR::Filter<float> lowStageA, lowStageB;
        juce::dsp::IIR::Filter<float> highPass1StageA, highPass1StageB;

        // Stage 2: split highPass1 into mid (LP4 @ kMidHighHz) and
        // high (HP4 @ kMidHighHz).
        juce::dsp::IIR::Filter<float> midStageA, midStageB;
        juce::dsp::IIR::Filter<float> highStageA, highStageB;
    };
}
