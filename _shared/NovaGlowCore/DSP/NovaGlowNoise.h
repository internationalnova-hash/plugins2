#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

namespace nova::glow
{

// Optional modeled analog noise - filtered white noise (mild low-cut +
// high-shelf tilt so it reads as amplifier hiss rather than flat white
// noise), scaled by knob^2 (musical: quiet at low settings, still
// restrained at max). Zero at knob=0. Adapted from
// _shared/NovaTapeCore/DSP/NovaTapeNoise.h, with one deliberate change:
// juce::Random is seeded from a FIXED constant, not setSeedRandomly(), so
// two offline bounces of identical automation produce bit-identical
// output (spec's "deterministic during offline rendering" requirement -
// no other Nova Core noise generator has solved this before).
class NovaGlowNoise
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        random.setSeed (kFixedSeed);
        auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 300.0f, 0.5f);
        auto shelf = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 4500.0f, 0.6f, 1.6f);
        *hpL.coefficients = *hp; *hpR.coefficients = *hp;
        *shelfL.coefficients = *shelf; *shelfR.coefficients = *shelf;
        reset();
    }

    void reset() noexcept
    {
        hpL.reset(); hpR.reset(); shelfL.reset(); shelfR.reset();
    }

    // noise01: 0-1 (from the 0-100 NOISE parameter). noiseFloorScale: the
    // active Analog Mode's noise-floor multiplier (Clean quieter, Hot
    // louder).
    void setLevel (float noise01, float noiseFloorScale) noexcept
    {
        const float knob = juce::jlimit (0.0f, 1.0f, noise01);
        targetGain = knob * knob * 0.015f * juce::jmax (0.0f, noiseFloorScale);
    }

    void processAdd (float* dataL, float* dataR, int numSamples) noexcept
    {
        if (targetGain <= 0.0f) return;
        for (int i = 0; i < numSamples; ++i)
        {
            const float nL = shelfL.processSample (hpL.processSample (random.nextFloat() * 2.0f - 1.0f));
            const float nR = shelfR.processSample (hpR.processSample (random.nextFloat() * 2.0f - 1.0f));
            dataL[i] += nL * targetGain;
            dataR[i] += nR * targetGain;
        }
    }

private:
    static constexpr juce::int64 kFixedSeed = 0x4E6F76614F70; // "NovaOp" - fixed, deterministic offline renders

    double sr = 44100.0;
    juce::Random random;
    juce::dsp::IIR::Filter<float> hpL, hpR, shelfL, shelfR;
    float targetGain = 0.0f;
};

} // namespace nova::glow
