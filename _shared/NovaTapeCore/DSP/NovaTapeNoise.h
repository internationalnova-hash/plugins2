#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

// Tape hiss - filtered white noise (mild low-cut + high-shelf tilt so it
// reads as "tape hiss" rather than flat white noise), scaled by the NOISE
// knob and the active TapeType's noiseCharacterGain. Zero at knob=0.
namespace nova::tape
{
    class NovaTapeNoise
    {
    public:
        void prepare (double sampleRate) noexcept
        {
            sr = sampleRate;
            random.setSeedRandomly();
            auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 400.0f, 0.5f);
            auto shelf = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 4000.0f, 0.6f, 1.8f);
            *hpL.coefficients = *hp; *hpR.coefficients = *hp;
            *shelfL.coefficients = *shelf; *shelfR.coefficients = *shelf;
            reset();
        }

        void reset() noexcept
        {
            hpL.reset(); hpR.reset(); shelfL.reset(); shelfR.reset();
        }

        void setLevel (float noiseKnob0to10, float typeCharacterGain, float ipsNoiseFloorGain) noexcept
        {
            const float knob = juce::jlimit (0.0f, 10.0f, noiseKnob0to10) / 10.0f;
            // Musically-scaled: quiet at low settings, still restrained at max.
            targetGain = knob * knob * 0.02f * typeCharacterGain * ipsNoiseFloorGain;
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
        double sr = 44100.0;
        juce::Random random;
        juce::dsp::IIR::Filter<float> hpL, hpR, shelfL, shelfR;
        float targetGain = 0.0f;
    };
}
