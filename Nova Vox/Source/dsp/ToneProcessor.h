#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

// Nova Console-inspired saturation and harmonic coloring.
// Modes apply different saturation characters; amount scales drive depth.
class ToneProcessor
{
public:
    enum Mode { WarmConsole = 0, Tube = 1, Air = 2, Vintage = 3 };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        dc[0] = dc[1] = 0.f;
        reset();
    }

    void reset()
    {
        dc[0] = dc[1] = 0.f;
        prevL = prevR = 0.f;
    }

    void setAmount   (float a) noexcept { amount    = juce::jlimit (0.f, 1.f, a); }
    void setMode     (int   m) noexcept { mode      = juce::jlimit (0, 3, m);      }
    void setDrive    (float d) noexcept { drive     = juce::jlimit (0.f, 1.f, d); }
    void setBody     (float b) noexcept { bodyDb    = juce::jlimit (-12.f, 12.f, b); }
    void setPresence (float p) noexcept { presenceDb = juce::jlimit (-12.f, 12.f, p); }
    void setOutputGain (float o) noexcept { outputDb = juce::jlimit (-12.f, 12.f, o); }

    void process (float* L, float* R, int N)
    {
        const float combinedDrive = amount * 0.85f + drive * 0.15f;
        if (combinedDrive < 0.001f && std::abs (bodyDb) < 0.1f &&
            std::abs (presenceDb) < 0.1f && std::abs (outputDb) < 0.1f)
            return;

        const float outGain = juce::Decibels::decibelsToGain (outputDb);

        for (int i = 0; i < N; ++i)
        {
            float l = L[i];
            float r = R[i];

            if (combinedDrive > 0.001f)
            {
                l = saturate (l, combinedDrive);
                r = saturate (r, combinedDrive);
            }

            // DC block (one-pole highpass at ~10Hz)
            const float dcCoeff = 1.f - (juce::MathConstants<float>::twoPi * 10.f / (float)sampleRate);
            dc[0] = l + dcCoeff * (dc[0] - l); l = l - dc[0];
            dc[1] = r + dcCoeff * (dc[1] - r); r = r - dc[1];

            L[i] = l * outGain;
            R[i] = r * outGain;
        }
    }

private:
    float saturate (float x, float drive) const noexcept
    {
        const float d = drive * 2.5f + 0.01f;
        switch (mode)
        {
            case WarmConsole:
            {
                // Soft even-harmonic saturation (console warmth)
                const float driven = x * (1.f + d * 0.6f);
                return driven / (1.f + std::abs (driven) * 0.5f);
            }
            case Tube:
            {
                // Asymmetric tube-style (2nd + 3rd harmonic)
                const float driven = x * (1.f + d);
                if (driven >= 0.f)
                    return std::tanh (driven * 1.3f) / std::tanh (1.3f + d * 0.3f);
                else
                    return std::tanh (driven * 0.85f) / std::tanh (0.85f + d * 0.2f);
            }
            case Air:
            {
                // Bright saturation - slightly harder clip with HF character
                const float driven = x * (1.f + d * 1.4f);
                return std::tanh (driven) / (1.f + d * 0.1f);
            }
            case Vintage:
            {
                // Soft knee with mild odd harmonics
                const float driven = x * (1.f + d * 0.8f);
                const float shaped = 1.5f * driven - 0.5f * driven * driven * driven;
                return juce::jlimit (-1.f, 1.f, shaped);
            }
            default: return x;
        }
    }

    double sampleRate = 44100.0;
    float  amount = 0.f, drive = 0.f;
    float  bodyDb = 0.f, presenceDb = 0.f, outputDb = 0.f;
    float  dc[2] = {};
    float  prevL = 0.f, prevR = 0.f;
    int    mode = WarmConsole;
};
