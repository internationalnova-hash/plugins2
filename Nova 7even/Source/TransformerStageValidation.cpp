// Headless validation of NovaTransformerStage (NOVA_7EVEN_PLAN.md section
// 5.2 / 8.1 step 1).

#include "DSP/NovaTransformerStage.h"
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

    // Goertzel magnitude at a known frequency - exact for arbitrary block
    // lengths, no windowing/leakage concerns the way an FFT bin would have.
    // Not rounded to the nearest integer bin - see EQValidation.cpp for
    // why (single-frequency coherent probe, not bin-by-bin analysis;
    // rounding causes spectral leakage for frequencies that don't happen
    // to be exact multiples of this window's bin spacing).
    float goertzelMagnitude (const std::vector<float>& samples, double sampleRate, double targetFreqHz)
    {
        const int N = (int) samples.size();
        const double k = (targetFreqHz * (double) N) / sampleRate;
        const double omega = (2.0 * juce::MathConstants<double>::pi * k) / (double) N;
        const double coeff = 2.0 * std::cos (omega);

        double s0 = 0.0, s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < N; ++i)
        {
            s0 = (double) samples[(size_t) i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        const double realPart = s1 - s2 * std::cos (omega);
        const double imagPart = s2 * std::sin (omega);
        return (float) (2.0 * std::sqrt (realPart * realPart + imagPart * imagPart) / (double) N);
    }

    std::vector<float> renderSine (NovaTransformerStage& stage, double sampleRate, double freqHz, float amplitude, int numSamples)
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) numSamples, 1 };
        stage.prepare (spec);

        juce::AudioBuffer<float> buffer (1, numSamples);
        auto* w = buffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            w[i] = amplitude * std::sin (2.0 * juce::MathConstants<double>::pi * freqHz * (double) i / sampleRate);

        stage.process (buffer);

        return std::vector<float> (buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples);
    }

    // THD = sqrt(sum of harmonic^2) / fundamental, harmonics 2..6.
    float measureThd (const std::vector<float>& out, double sampleRate, double fundamentalHz)
    {
        const float fundamentalMag = goertzelMagnitude (out, sampleRate, fundamentalHz);
        if (fundamentalMag < 1.0e-9f) return 0.0f;

        double harmonicEnergy = 0.0;
        for (int h = 2; h <= 6; ++h)
        {
            const float mag = goertzelMagnitude (out, sampleRate, fundamentalHz * h);
            harmonicEnergy += (double) mag * (double) mag;
        }
        return (float) (std::sqrt (harmonicEnergy) / (double) fundamentalMag);
    }
}

int main()
{
    const double sampleRate = 48000.0;
    const int numSamples = 48000; // 1 second - long enough for tight Goertzel bins

    std::cout << "\n[TransformerStage] Silence in, silence out\n";
    {
        NovaTransformerStage stage;
        juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };
        stage.prepare (spec);

        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();
        stage.process (buffer);

        float maxAbs = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                maxAbs = juce::jmax (maxAbs, std::abs (buffer.getSample (ch, i)));

        check (maxAbs < 1.0e-6f, "silence in produces silence out, max abs = " + juce::String (maxAbs, 8));
    }

    std::cout << "\n[TransformerStage] DC input stays bounded and finite\n";
    {
        NovaTransformerStage stage;
        stage.setDriveLevel (1.0f);
        juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
        stage.prepare (spec);

        juce::AudioBuffer<float> buffer (1, 512);
        bool bounded = true, finite = true;
        for (int block = 0; block < 200; ++block)
        {
            buffer.clear();
            auto* w = buffer.getWritePointer (0);
            for (int i = 0; i < 512; ++i) w[i] = 0.95f;
            stage.process (buffer);

            for (int i = 0; i < 512; ++i)
            {
                const float v = buffer.getSample (0, i);
                if (! std::isfinite (v)) finite = false;
                if (std::abs (v) > 3.0f) bounded = false;
            }
        }
        check (finite, "DC input never produces NaN/Inf over 200 blocks");
        check (bounded, "DC input stays bounded (|y| <= 3.0) over 200 blocks");
    }

    std::cout << "\n[TransformerStage] Near-linear at low signal level\n";
    {
        NovaTransformerStage stage;
        stage.setDriveLevel (0.0f);
        auto out = renderSine (stage, sampleRate, 300.0, 0.01f, numSamples);
        const float thd = measureThd (out, sampleRate, 300.0);
        check (thd < 0.02f, "THD stays under 2% at 0.01 amplitude / driveLevel=0, got " + juce::String (thd * 100.0f, 3) + "%");
    }

    std::cout << "\n[TransformerStage] Harmonic content increases with input level\n";
    {
        NovaTransformerStage stageLow;
        stageLow.setDriveLevel (0.5f);
        auto outLow = renderSine (stageLow, sampleRate, 300.0, 0.05f, numSamples);
        const float thdLow = measureThd (outLow, sampleRate, 300.0);

        NovaTransformerStage stageHigh;
        stageHigh.setDriveLevel (0.5f);
        auto outHigh = renderSine (stageHigh, sampleRate, 300.0, 0.9f, numSamples);
        const float thdHigh = measureThd (outHigh, sampleRate, 300.0);

        std::cout << "    THD at amplitude 0.05: " << (thdLow * 100.0f) << "%, at 0.9: " << (thdHigh * 100.0f) << "%\n";
        check (thdHigh > thdLow, "THD at high amplitude exceeds THD at low amplitude");
    }

    std::cout << "\n[TransformerStage] Harmonic content increases with driveLevel at fixed amplitude\n";
    {
        NovaTransformerStage stageLowDrive;
        stageLowDrive.setDriveLevel (0.0f);
        auto outLowDrive = renderSine (stageLowDrive, sampleRate, 300.0, 0.6f, numSamples);
        const float thdLowDrive = measureThd (outLowDrive, sampleRate, 300.0);

        NovaTransformerStage stageHighDrive;
        stageHighDrive.setDriveLevel (1.0f);
        auto outHighDrive = renderSine (stageHighDrive, sampleRate, 300.0, 0.6f, numSamples);
        const float thdHighDrive = measureThd (outHighDrive, sampleRate, 300.0);

        std::cout << "    THD at driveLevel=0: " << (thdLowDrive * 100.0f) << "%, at driveLevel=1: " << (thdHighDrive * 100.0f) << "%\n";
        check (thdHighDrive > thdLowDrive, "THD at driveLevel=1 exceeds THD at driveLevel=0, at matched amplitude");
    }

    std::cout << "\n[TransformerStage] No NaN/Inf at extreme drive\n";
    {
        NovaTransformerStage stage;
        stage.setDriveLevel (1.0f);
        auto out = renderSine (stage, sampleRate, 300.0, 4.0f, numSamples); // well beyond normal headroom
        bool finite = true;
        for (float v : out) if (! std::isfinite (v)) finite = false;
        check (finite, "extreme-amplitude, max-drive input produces no NaN/Inf");
    }

    std::cout << "\n[TransformerStage] Left/right channels processed independently (no crosstalk)\n";
    {
        NovaTransformerStage stage;
        stage.setDriveLevel (0.8f);
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) numSamples, 2 };
        stage.prepare (spec);

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();
        auto* left = buffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            left[i] = 0.8f * std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * (double) i / sampleRate);
        // Right channel stays silent.

        stage.process (buffer);

        float rightMaxAbs = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            rightMaxAbs = juce::jmax (rightMaxAbs, std::abs (buffer.getSample (1, i)));

        check (rightMaxAbs < 1.0e-6f, "silent right channel stays silent while left channel is driven, max abs = " + juce::String (rightMaxAbs, 8));
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL TRANSFORMER STAGE VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
