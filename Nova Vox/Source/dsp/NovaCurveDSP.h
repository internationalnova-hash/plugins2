#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

// NovaCurveDSP — simple 3-band shelving/peak EQ for the Nova Vox Curve module.
// Low shelf at 200 Hz, mid bell at 1200 Hz, high shelf at 7 kHz.
// Mode presets define default gain targets; amount scales them (0-100).
// No APVTS, no UI dependencies. Lifecycle: prepare → setParameters → process.

struct NovaCurveParameters
{
    bool  curveOn   = true;
    float amount    = 50.0f;   // 0..100 — scales mode curve gains
    int   mode      = 0;       // 0=Flat 1=Bright 2=Warm 3=Air
    float lowDb     = 0.0f;    // -12..+12 advanced override
    float midDb     = 0.0f;    // -12..+12
    float highDb    = 0.0f;    // -12..+12
};

class NovaCurveDSP
{
public:
    NovaCurveDSP() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;
        juce::dsp::ProcessSpec mono { spec.sampleRate, spec.maximumBlockSize, 1 };
        for (int ch = 0; ch < 2; ++ch)
        {
            lowShelf[ch].prepare (mono);
            midPeak[ch].prepare (mono);
            highShelf[ch].prepare (mono);
        }
        reset();
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            lowShelf[ch].reset();
            midPeak[ch].reset();
            highShelf[ch].reset();
        }
    }

    void setParameters (const NovaCurveParameters& p) noexcept { params = p; }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        if (!params.curveOn) return;

        const float scale = params.amount / 100.0f;

        // Mode preset gains (dB) — Flat / Bright / Warm / Air
        static constexpr float kLow[4]  = { 0.0f,  -1.0f,  2.5f, -0.5f };
        static constexpr float kMid[4]  = { 0.0f,   0.5f, -0.5f, -0.5f };
        static constexpr float kHigh[4] = { 0.0f,   3.5f, -2.0f,  5.0f };

        const int m = juce::jlimit (0, 3, params.mode);
        const float lowDb  = kLow[m]  * scale + params.lowDb;
        const float midDb  = kMid[m]  * scale + params.midDb;
        const float highDb = kHigh[m] * scale + params.highDb;

        updateCoefficients (lowDb, midDb, highDb);

        const int N  = buffer.getNumSamples();
        const int ch = juce::jmin (2, buffer.getNumChannels());
        for (int c = 0; c < ch; ++c)
        {
            auto* data = buffer.getWritePointer (c);
            for (int i = 0; i < N; ++i)
            {
                data[i] = lowShelf[c].processSample (data[i]);
                data[i] = midPeak[c].processSample (data[i]);
                data[i] = highShelf[c].processSample (data[i]);
            }
        }
    }

private:
    void updateCoefficients (float lowDb, float midDb, float highDb)
    {
        if (std::abs (lowDb  - lastLow)  < 0.01f &&
            std::abs (midDb  - lastMid)  < 0.01f &&
            std::abs (highDb - lastHigh) < 0.01f)
            return;

        lastLow  = lowDb;
        lastMid  = midDb;
        lastHigh = highDb;

        const float Q = 0.707f;
        *lowShelf[0].coefficients  = *juce::dsp::IIR::Coefficients<float>::makeLowShelf  ((float)sr, 200.0f,   Q, juce::Decibels::decibelsToGain (lowDb));
        *midPeak[0].coefficients   = *juce::dsp::IIR::Coefficients<float>::makePeakFilter ((float)sr, 1200.0f,  Q, juce::Decibels::decibelsToGain (midDb));
        *highShelf[0].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf  ((float)sr, 7000.0f,  Q, juce::Decibels::decibelsToGain (highDb));
        *lowShelf[1].coefficients  = *lowShelf[0].coefficients;
        *midPeak[1].coefficients   = *midPeak[0].coefficients;
        *highShelf[1].coefficients = *highShelf[0].coefficients;
    }

    double sr = 44100.0;
    NovaCurveParameters params;

    juce::dsp::IIR::Filter<float> lowShelf[2];
    juce::dsp::IIR::Filter<float> midPeak[2];
    juce::dsp::IIR::Filter<float> highShelf[2];

    float lastLow = 999.f, lastMid = 999.f, lastHigh = 999.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaCurveDSP)
};
