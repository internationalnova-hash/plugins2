#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

// Offline 7-band spectral energy analyzer + tilt/harshness/resonance
// scoring. See spec section 6. Band scheme: sub, bass, lowMid, mid,
// upperMid, presence, high ("air"), split via a bank of overlapping
// bandpass/shelf IIR filters (same broad-strokes approach as a graphic
// EQ's band split - not a perfectly disjoint partition, appropriate for a
// relative energy-balance measurement rather than surgical EQ).
//
// Harshness/resonance are derived from the 7-band energy distribution
// itself (band-count limited, unlike Nova Silk's 24-band probe scaffold -
// good enough for v0.1's broad-strokes corrective EQ; revisit with finer
// resolution if listening tests show it's missing real problems).
class NovaSpectralAnalyzer
{
public:
    enum Band { Sub = 0, Bass, LowMid, Mid, UpperMid, Presence, High, kNumBands };

    NovaSpectralAnalyzer() = default;

    void prepare (double sampleRate);
    void reset() noexcept;
    void process (const juce::AudioBuffer<float>& buffer) noexcept;
    void finalize() noexcept;

    float getBandEnergy (Band b) const noexcept { return bandEnergy[(size_t) b]; }
    float getSpectralTilt() const noexcept      { return spectralTilt; }
    float getHarshness() const noexcept         { return harshness; }
    float getResonanceScore() const noexcept    { return resonanceScore; }

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

    double sampleRate = 44100.0;

    std::array<Biquad, kNumBands> filtersL, filtersR;
    std::array<double, kNumBands> sumSq {};
    long long samplesProcessed = 0;

    // Each band filter has a different Hz-bandwidth (narrower near the low
    // end, wider near the top under fixed-Q design), so even perfectly
    // flat white noise doesn't split evenly across bands - a naive
    // uniform-1/(kNumBands) baseline would read flat material as
    // artificially "harsh" or "resonant". Calibrated once in prepare() by
    // running a deterministic noise probe through throwaway filter copies
    // and recording each band's neutral energy share.
    std::array<float, kNumBands> calibratedNeutralShare {};

    std::array<float, kNumBands> bandEnergy {};
    float spectralTilt   = 0.f;
    float harshness      = 0.f;
    float resonanceScore = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaSpectralAnalyzer)
};
