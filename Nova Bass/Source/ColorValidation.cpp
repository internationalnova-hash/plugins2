// Headless validation harness for NovaColorEngine (spec section 10, 21).
// Drives the CLEAN/WARM/GRITTY color engine directly with a synthetic
// sine input and measures: near-zero-drive is a near-passthrough, GRITTY
// adds more harmonic content than CLEAN at the same drive, and the two
// modes produce genuinely different output at the same drive setting (not
// just a louder/quieter version of the same curve).

#include "../../_shared/NovaBassCore/Color/NovaColorEngine.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdio>

using namespace nova::bass;

static int failures = 0;

static void check (bool condition, const char* description)
{
    if (condition) std::printf ("  PASS: %s\n", description);
    else { std::printf ("  FAIL: %s\n", description); ++failures; }
}

static juce::AudioBuffer<float> makeSineBuffer (float freqHz, double sampleRate, int numSamples)
{
    juce::AudioBuffer<float> buf (2, numSamples);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            d[i] = 0.5f * std::sin (juce::MathConstants<float>::twoPi * freqHz * (float) i / (float) sampleRate);
    }
    return buf;
}

// RMS of the component of `output` NOT explained by a best-fit linear
// scaling of `input` - a proxy for added harmonic/nonlinear energy that
// doesn't require an FFT.
static float harmonicResidualRms (const juce::AudioBuffer<float>& input, const juce::AudioBuffer<float>& output)
{
    const int n = input.getNumSamples();
    const float* in = input.getReadPointer (0);
    const float* out = output.getReadPointer (0);

    double dotIO = 0.0, dotII = 0.0;
    for (int i = 0; i < n; ++i) { dotIO += (double) in[i] * out[i]; dotII += (double) in[i] * in[i]; }
    const float scale = dotII > 1.0e-12 ? (float) (dotIO / dotII) : 0.0f;

    double sumSq = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float residual = out[i] - scale * in[i];
        sumSq += (double) residual * residual;
    }
    return (float) std::sqrt (sumSq / n);
}

static void runColorEngine (NovaColorEngine& engine, NovaColorMode mode, float drive01,
                             const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output)
{
    output.makeCopyOf (input);
    engine.setParams (mode, drive01);
    engine.process (output);
}

int main()
{
    std::printf ("=== NovaColorEngine Validation ===\n");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 4096;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2 };

    const auto input = makeSineBuffer (110.0f, sampleRate, blockSize);

    // -- Near-zero drive is a near-passthrough --
    {
        NovaColorEngine engine;
        engine.prepare (spec);
        juce::AudioBuffer<float> output;
        runColorEngine (engine, NovaColorMode::Clean, 0.0f, input, output);

        double maxDiff = 0.0;
        for (int i = 0; i < blockSize; ++i)
            maxDiff = juce::jmax (maxDiff, (double) std::abs (output.getSample (0, i) - input.getSample (0, i)));

        check (maxDiff < 0.001, "drive01=0 leaves the signal effectively unmodified (early-out gate)");
    }

    // -- GRITTY adds more harmonic content than CLEAN at the same drive --
    {
        NovaColorEngine cleanEngine, grittyEngine;
        cleanEngine.prepare (spec);
        grittyEngine.prepare (spec);

        juce::AudioBuffer<float> cleanOut, grittyOut;
        runColorEngine (cleanEngine, NovaColorMode::Clean, 0.7f, input, cleanOut);
        runColorEngine (grittyEngine, NovaColorMode::Gritty, 0.7f, input, grittyOut);

        const float cleanHarmonics = harmonicResidualRms (input, cleanOut);
        const float grittyHarmonics = harmonicResidualRms (input, grittyOut);

        std::printf ("     CLEAN harmonic residual RMS: %.5f | GRITTY: %.5f\n", cleanHarmonics, grittyHarmonics);
        check (grittyHarmonics > cleanHarmonics, "GRITTY produces more added harmonic content than CLEAN at the same drive");
    }

    // -- Same drive, different mode -> genuinely different output --
    {
        NovaColorEngine warmEngine, grittyEngine;
        warmEngine.prepare (spec);
        grittyEngine.prepare (spec);

        juce::AudioBuffer<float> warmOut, grittyOut;
        runColorEngine (warmEngine, NovaColorMode::Warm, 0.5f, input, warmOut);
        runColorEngine (grittyEngine, NovaColorMode::Gritty, 0.5f, input, grittyOut);

        double sumAbsDiff = 0.0;
        for (int i = 0; i < blockSize; ++i)
            sumAbsDiff += std::abs (warmOut.getSample (0, i) - grittyOut.getSample (0, i));

        check (sumAbsDiff / blockSize > 0.001, "WARM and GRITTY at the same DRIVE produce measurably different output");
    }

    // -- Finite output across the full mode x drive grid --
    {
        bool allFinite = true;
        const NovaColorMode modes[] = { NovaColorMode::Clean, NovaColorMode::Warm, NovaColorMode::Gritty };
        for (auto mode : modes)
        {
            for (float drive = 0.0f; drive <= 1.0f; drive += 0.25f)
            {
                NovaColorEngine engine;
                engine.prepare (spec);
                juce::AudioBuffer<float> output;
                runColorEngine (engine, mode, drive, input, output);
                for (int ch = 0; ch < output.getNumChannels(); ++ch)
                    for (int i = 0; i < output.getNumSamples(); ++i)
                        if (! std::isfinite (output.getSample (ch, i)))
                            allFinite = false;
            }
        }
        check (allFinite, "output is finite across every mode x drive combination tested");
    }

    std::printf ("=== %s (%d failure%s) ===\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
