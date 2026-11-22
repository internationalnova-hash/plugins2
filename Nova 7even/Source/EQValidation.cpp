// Headless validation of NovaSevenEQ (NOVA_7EVEN_PLAN.md section 5.5 /
// 8.2). For every stepped HPF/Low/Mid choice, measures the actual
// filter's response at its labeled frequency and asserts it matches what
// the UI label implies - the plan's own explicit gate: "never ship a UI
// label that doesn't match the measured frequency."

#include "DSP/NovaSevenEQ.h"
#include <juce_core/juce_core.h>
#include <iostream>
#include <cmath>
#include <vector>

using namespace nova::seven;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    // Deliberately NOT rounding k to the nearest integer bin: this is used
    // as a single-frequency coherent probe (a direct DFT-at-one-frequency
    // evaluation), not a bin-by-bin spectrum analysis. Rounding to the
    // nearest bin was an earlier bug here - for window lengths that don't
    // make every test frequency an exact multiple of the bin spacing
    // (e.g. 110Hz against a 41895-sample window at 44.1kHz sits almost
    // exactly halfway between bins 104 and 105), snapping to the nearest
    // bin causes severe spectral leakage and a badly wrong magnitude
    // reading (confirmed: measured 2.08dB where JUCE's own
    // getMagnitudeForFrequency() analytically gives 6.00dB for the same
    // coefficients). Using the exact (non-integer) target frequency
    // avoids this entirely.
    float goertzelMagnitude (const float* samples, int N, double sampleRate, double targetFreqHz)
    {
        const double k = (targetFreqHz * (double) N) / sampleRate;
        const double omega = (2.0 * juce::MathConstants<double>::pi * k) / (double) N;
        const double coeff = 2.0 * std::cos (omega);

        double s0 = 0.0, s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < N; ++i)
        {
            s0 = (double) samples[i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        const double realPart = s1 - s2 * std::cos (omega);
        const double imagPart = s2 * std::sin (omega);
        return (float) (2.0 * std::sqrt (realPart * realPart + imagPart * imagPart) / (double) N);
    }

    // Measures steady-state gain in dB at freqHz, discarding a settling
    // prefix so the filter's transient response doesn't pollute the
    // reading.
    float measureGainDb (NovaSevenEQ& eq, double sampleRate, double freqHz, float inputAmplitude)
    {
        const int totalSamples = (int) (sampleRate * 1.0);
        const int settleSamples = (int) (sampleRate * 0.05);

        juce::AudioBuffer<float> buffer (1, totalSamples);
        auto* w = buffer.getWritePointer (0);
        for (int i = 0; i < totalSamples; ++i)
            w[i] = inputAmplitude * std::sin (2.0 * juce::MathConstants<double>::pi * freqHz * (double) i / sampleRate);

        eq.process (buffer);

        const int measureSamples = totalSamples - settleSamples;
        const float outputMag = goertzelMagnitude (buffer.getReadPointer (0) + settleSamples, measureSamples, sampleRate, freqHz);

        return juce::Decibels::gainToDecibels (outputMag / inputAmplitude);
    }

    void testAtSampleRate (double sampleRate)
    {
        std::cout << "\n=== Sample rate: " << sampleRate << "Hz ===\n";

        std::cout << "\n[EQ] HPF -3dB corner lands at the labeled frequency (Butterworth Q)\n";
        for (int i = 0; i < (int) NovaSevenEQ::hpfFreqChoices.size(); ++i)
        {
            const float freq = NovaSevenEQ::hpfFreqChoices[(size_t) i];

            NovaSevenEQ eq;
            juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
            eq.prepare (spec);
            eq.setHpfFreqIndex (i + 1);
            eq.setLowGainDb (0.0f);
            eq.setMidGainDb (0.0f);
            eq.setHighShelfGainDb (0.0f);

            const float gainDb = measureGainDb (eq, sampleRate, freq, 0.5f);
            std::cout << "    HPF " << freq << "Hz: measured " << gainDb << " dB at cutoff (expect ~-3.01dB)\n";
            check (std::abs (gainDb - (-3.01f)) < 1.0f, "HPF " + juce::String (freq, 0) + "Hz gain at cutoff within 1dB of -3.01dB, got " + juce::String (gainDb, 2) + "dB");
        }

        std::cout << "\n[EQ] Low shelf midpoint gain lands at the labeled frequency\n";
        for (int i = 0; i < (int) NovaSevenEQ::lowFreqChoices.size(); ++i)
        {
            const float freq = NovaSevenEQ::lowFreqChoices[(size_t) i];

            NovaSevenEQ eq;
            juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
            eq.prepare (spec);
            eq.setLowFreqIndex (i);
            eq.setLowGainDb (12.0f);
            eq.setMidGainDb (0.0f);
            eq.setHighShelfGainDb (0.0f);

            const float gainDb = measureGainDb (eq, sampleRate, freq, 0.5f);
            std::cout << "    Low " << freq << "Hz: measured " << gainDb << " dB at shelf freq (expect ~+6dB, half of +12dB boost)\n";
            check (std::abs (gainDb - 6.0f) < 1.5f, "Low shelf " + juce::String (freq, 0) + "Hz gain at labeled freq within 1.5dB of +6dB, got " + juce::String (gainDb, 2) + "dB");
        }

        std::cout << "\n[EQ] Mid bell peak gain lands at the labeled frequency\n";
        for (int i = 0; i < (int) NovaSevenEQ::midFreqChoices.size(); ++i)
        {
            const float freq = NovaSevenEQ::midFreqChoices[(size_t) i];

            NovaSevenEQ eq;
            juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
            eq.prepare (spec);
            eq.setMidFreqIndex (i);
            eq.setLowGainDb (0.0f);
            eq.setMidGainDb (12.0f);
            eq.setHighShelfGainDb (0.0f);

            const float gainDb = measureGainDb (eq, sampleRate, freq, 0.5f);
            std::cout << "    Mid " << freq << "Hz: measured " << gainDb << " dB at bell center (expect ~+12dB, full boost)\n";
            check (std::abs (gainDb - 12.0f) < 1.0f, "Mid bell " + juce::String (freq, 0) + "Hz gain at labeled freq within 1dB of +12dB, got " + juce::String (gainDb, 2) + "dB");
        }

        std::cout << "\n[EQ] Fixed 12k high shelf midpoint gain\n";
        {
            NovaSevenEQ eq;
            juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
            eq.prepare (spec);
            eq.setLowGainDb (0.0f);
            eq.setMidGainDb (0.0f);
            eq.setHighShelfGainDb (12.0f);

            const float gainDb = measureGainDb (eq, sampleRate, NovaSevenEQ::highShelfFreqHz, 0.5f);
            std::cout << "    High shelf 12kHz: measured " << gainDb << " dB (expect ~+6dB, half of +12dB boost)\n";
            check (std::abs (gainDb - 6.0f) < 1.5f, "High shelf 12kHz gain within 1.5dB of +6dB, got " + juce::String (gainDb, 2) + "dB");
        }
    }
}

int main()
{
    std::cout << "\n[EQ] Silence in, silence out\n";
    {
        NovaSevenEQ eq;
        juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
        eq.prepare (spec);
        eq.setHpfFreqIndex (3);
        eq.setLowGainDb (8.0f);
        eq.setMidGainDb (-6.0f);
        eq.setHighShelfGainDb (4.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();
        eq.process (buffer);

        float maxAbs = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                maxAbs = juce::jmax (maxAbs, std::abs (buffer.getSample (ch, i)));

        check (maxAbs < 1.0e-5f, "silence in produces silence out even with all bands active, max abs = " + juce::String (maxAbs, 8));
    }

    // Plan section 8.2: must run at at least 44.1kHz and 96kHz (bilinear-
    // transform-derived coefficients shift slightly with sample rate at
    // fixed Hz targets).
    testAtSampleRate (44100.0);
    testAtSampleRate (96000.0);

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL EQ VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
