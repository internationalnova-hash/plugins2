// Headless validation of NovaOversampledColorStage (NOVA_7EVEN_PLAN.md
// section 5.6 / 8.1 step 3).

#include "DSP/NovaOversampledColorStage.h"
#include "DSP/NovaTransformerStage.h"
#include "DSP/NovaClassAAmp.h"
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

    // Not rounded to the nearest integer bin - see EQValidation.cpp for
    // why (single-frequency coherent probe; rounding causes spectral
    // leakage for frequencies not aligned to this window's bin spacing).
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
}

int main()
{
    std::cout << "\n[Oversampling] Latency reporting\n";
    {
        NovaOversampledColorStage stage;
        juce::dsp::ProcessSpec spec { 48000.0, 512, 1 };
        stage.prepare (spec, 1); // 2x

        const int latency = stage.getLatencySamples();
        std::cout << "    2x oversampling latency: " << latency << " samples\n";
        check (latency > 0, "2x oversampling reports positive latency");
        check (latency < 200, "2x oversampling latency stays under 200 samples (sane for a halfband FIR pair)");
    }

    std::cout << "\n[Oversampling] Aliasing content above the base Nyquist is measurably reduced vs. non-oversampled\n";
    {
        // Drive a 10kHz tone hard enough (Hot mode, high preamp gain) that
        // the nonlinearity generates a harmonic (3rd, at 30kHz) above the
        // 48kHz base rate's 24kHz Nyquist. Without oversampling, a
        // discrete-time nonlinearity computing that content at only 48kHz
        // can't represent 30kHz - it folds/aliases immediately down to
        // |48000-30000| = 18000Hz. Oversampling computes the nonlinearity
        // at 96kHz (Nyquist 48kHz, comfortably above 30kHz - no fold),
        // then the halfband decimation filter removes content above the
        // final 24kHz Nyquist before downsampling - so the 18kHz alias bin
        // should show measurably less energy in the oversampled path.
        const double sampleRate = 48000.0;
        const int numSamples = 48000;
        const double fundamentalHz = 10000.0;
        const double aliasProbeHz = 18000.0;
        const float amplitude = 0.5f;
        const float preampGain = 9.0f;

        std::vector<float> input (numSamples);
        for (int i = 0; i < numSamples; ++i)
            input[i] = amplitude * std::sin (2.0 * juce::MathConstants<double>::pi * fundamentalHz * (double) i / sampleRate);

        // Non-oversampled reference: run the two stages directly at 48kHz.
        std::vector<float> outNonOS = input;
        {
            NovaTransformerStage transformer;
            NovaClassAAmp classA;
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) numSamples, 1 };
            transformer.prepare (spec);
            classA.prepare (spec);
            classA.setMode (NovaCharacterMode::Hot);
            classA.setPreampGain (preampGain);

            juce::AudioBuffer<float> buffer (1, numSamples);
            buffer.copyFrom (0, 0, outNonOS.data(), numSamples);
            transformer.process (buffer);
            classA.process (buffer);
            std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples, outNonOS.begin());
        }

        // Oversampled path: same stages, same settings, wrapped in 2x OS.
        std::vector<float> outOS = input;
        {
            NovaOversampledColorStage stage;
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) numSamples, 1 };
            stage.prepare (spec, 1);
            stage.setMode (NovaCharacterMode::Hot);
            stage.setPreampGain (preampGain);

            juce::AudioBuffer<float> buffer (1, numSamples);
            buffer.copyFrom (0, 0, outOS.data(), numSamples);
            stage.process (buffer);
            std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples, outOS.begin());
        }

        const float aliasNonOS = goertzelMagnitude (outNonOS.data(), numSamples, sampleRate, aliasProbeHz);
        const float aliasOS = goertzelMagnitude (outOS.data(), numSamples, sampleRate, aliasProbeHz);

        std::cout << "    alias energy at 18kHz - non-oversampled: " << aliasNonOS << ", oversampled: " << aliasOS << "\n";
        check (aliasOS < aliasNonOS * 0.5f, "oversampled path shows at least 2x less energy at the 18kHz alias bin than the non-oversampled path");
    }

    std::cout << "\n[Oversampling] Correct, finite, bounded behavior across the full supported sample-rate range\n";
    {
        const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
        bool allFinite = true, allBounded = true;
        std::vector<int> latencies;

        for (double sr : rates)
        {
            NovaOversampledColorStage stage;
            const int blockSize = 512;
            juce::dsp::ProcessSpec spec { sr, (juce::uint32) blockSize, 1 };
            stage.prepare (spec, 1);
            stage.setMode (NovaCharacterMode::Hot);
            stage.setPreampGain (8.0f);

            latencies.push_back (stage.getLatencySamples());

            juce::AudioBuffer<float> buffer (1, blockSize);
            for (int block = 0; block < 20; ++block)
            {
                auto* w = buffer.getWritePointer (0);
                for (int i = 0; i < blockSize; ++i)
                {
                    const int n = block * blockSize + i;
                    w[i] = 0.6f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * (double) n / sr);
                }
                stage.process (buffer);

                for (int i = 0; i < blockSize; ++i)
                {
                    const float v = buffer.getSample (0, i);
                    if (! std::isfinite (v)) allFinite = false;
                    if (std::abs (v) > 3.0f) allBounded = false;
                }
            }
        }

        std::cout << "    latencies (44.1/48/88.2/96/192k): ";
        for (int l : latencies) std::cout << l << " ";
        std::cout << "\n";

        check (allFinite, "no NaN/Inf at any of the 5 supported sample rates");
        check (allBounded, "output stays bounded (|y| <= 3.0) at all 5 supported sample rates");

        // Latency in SAMPLES is a property of the halfband FIR's fixed tap
        // count, not of the sample rate - should stay in the same
        // ballpark across all 5 rates, not swing wildly.
        int maxLatency = 0, minLatency = latencies[0];
        for (int l : latencies) { maxLatency = juce::jmax (maxLatency, l); minLatency = juce::jmin (minLatency, l); }
        check (maxLatency <= minLatency * 3, "latency-in-samples stays within a 3x band across all 5 sample rates (min="
                + juce::String (minLatency) + ", max=" + juce::String (maxLatency) + ")");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL OVERSAMPLING VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
