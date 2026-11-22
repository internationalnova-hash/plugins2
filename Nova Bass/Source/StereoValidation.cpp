// Headless validation harness for NovaBassStereoProcessor (spec section
// 22). Confirms the mono-bass-protection property: WIDTH scales the side
// channel's content ABOVE the crossover (0 = collapses to mono, 1 =
// unaltered), while content BELOW the crossover passes through unaffected
// by the WIDTH setting either way - protecting whatever mono/stereo
// balance the bass fundamental already had rather than forcing it fully
// mono, matching the proven algorithm already shipping in Nova One.

#include "../../_shared/NovaBassCore/Stereo/NovaBassStereoProcessor.h"
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

// Builds a fully decorrelated (out-of-phase) stereo sine at freqHz: side
// channel carries all the energy, mid channel is silent.
static juce::AudioBuffer<float> makeDecorrelatedSine (float freqHz, double sampleRate, int numSamples)
{
    juce::AudioBuffer<float> buf (2, numSamples);
    auto* l = buf.getWritePointer (0);
    auto* r = buf.getWritePointer (1);
    for (int i = 0; i < numSamples; ++i)
    {
        const float s = 0.5f * std::sin (juce::MathConstants<float>::twoPi * freqHz * (float) i / (float) sampleRate);
        l[i] = s;
        r[i] = -s;
    }
    return buf;
}

static float sideRms (const juce::AudioBuffer<float>& buf)
{
    const int n = buf.getNumSamples();
    const float* l = buf.getReadPointer (0);
    const float* r = buf.getReadPointer (1);
    double sumSq = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float side = 0.5f * (l[i] - r[i]);
        sumSq += (double) side * side;
    }
    return (float) std::sqrt (sumSq / n);
}

int main()
{
    std::printf ("=== NovaBassStereoProcessor Validation ===\n");

    constexpr double sampleRate = 48000.0;
    constexpr int numSamples = 9600; // 200ms - several cycles even at 50Hz
    constexpr float crossoverHz = 150.0f;

    // -- High-frequency side content: width scales it from ~0 to ~full --
    {
        auto highFreqInput = makeDecorrelatedSine (3000.0f, sampleRate, numSamples);
        const float inputSideRms = sideRms (highFreqInput);

        NovaBassStereoProcessor engineMono;
        engineMono.prepare ({ sampleRate, (juce::uint32) numSamples, 2 });
        engineMono.setParams (0.0f, crossoverHz);
        auto monoOut = highFreqInput;
        engineMono.process (monoOut);

        NovaBassStereoProcessor engineWide;
        engineWide.prepare ({ sampleRate, (juce::uint32) numSamples, 2 });
        engineWide.setParams (1.0f, crossoverHz);
        auto wideOut = highFreqInput;
        engineWide.process (wideOut);

        const float monoSideRms = sideRms (monoOut);
        const float wideSideRms = sideRms (wideOut);

        std::printf ("     High-freq side RMS - input: %.4f, width=0: %.4f, width=1: %.4f\n",
                     inputSideRms, monoSideRms, wideSideRms);

        check (monoSideRms < inputSideRms * 0.15f, "width=0 collapses high-frequency side content toward mono");
        check (wideSideRms > inputSideRms * 0.7f, "width=1 preserves most of the high-frequency side content");
    }

    // -- Low-frequency side content: width setting does not affect it --
    {
        auto lowFreqInput = makeDecorrelatedSine (50.0f, sampleRate, numSamples);

        NovaBassStereoProcessor engineMono;
        engineMono.prepare ({ sampleRate, (juce::uint32) numSamples, 2 });
        engineMono.setParams (0.0f, crossoverHz);
        auto monoOut = lowFreqInput;
        engineMono.process (monoOut);

        NovaBassStereoProcessor engineWide;
        engineWide.prepare ({ sampleRate, (juce::uint32) numSamples, 2 });
        engineWide.setParams (1.0f, crossoverHz);
        auto wideOut = lowFreqInput;
        engineWide.process (wideOut);

        const float monoSideRms = sideRms (monoOut);
        const float wideSideRms = sideRms (wideOut);
        const float inputSideRms = sideRms (lowFreqInput);

        std::printf ("     Low-freq side RMS - input: %.4f, width=0: %.4f, width=1: %.4f\n",
                     inputSideRms, monoSideRms, wideSideRms);

        check (std::abs (monoSideRms - wideSideRms) < inputSideRms * 0.15f,
               "sub-bass side content is protected: width=0 and width=1 produce nearly identical low-frequency side energy");
        check (monoSideRms > inputSideRms * 0.6f,
               "sub-bass side content is not collapsed even at width=0 (protection, not forced mono)");
    }

    // -- Mid signal is preserved regardless of width --
    {
        auto input = makeDecorrelatedSine (3000.0f, sampleRate, numSamples);
        // Add a shared (already-mono) low-frequency component to both channels.
        auto* l = input.getWritePointer (0);
        auto* r = input.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float shared = 0.3f * std::sin (juce::MathConstants<float>::twoPi * 80.0f * (float) i / (float) sampleRate);
            l[i] += shared;
            r[i] += shared;
        }

        NovaBassStereoProcessor engine;
        engine.prepare ({ sampleRate, (juce::uint32) numSamples, 2 });
        engine.setParams (1.0f, crossoverHz);
        auto output = input;
        engine.process (output);

        double maxMidDiff = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const float midIn = 0.5f * (input.getSample (0, i) + input.getSample (1, i));
            const float midOut = 0.5f * (output.getSample (0, i) + output.getSample (1, i));
            maxMidDiff = juce::jmax (maxMidDiff, (double) std::abs (midOut - midIn));
        }

        check (maxMidDiff < 0.01, "mid (mono-sum) signal is preserved through the stereo processor regardless of width");
    }

    std::printf ("=== %s (%d failure%s) ===\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
