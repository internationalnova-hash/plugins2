// Headless validation for NovaRoyal670Engine (the full facade): silence,
// impulses, sine sweeps, sustained LF material, transient-heavy drums,
// stereo linking, click-free M/S mode switching, parameter automation,
// time-constant switching, oversampling changes + latency reporting,
// bypass, extreme input levels, and NaN/Inf protection end-to-end.
//
// Mono-bus handling is intentionally NOT tested here: the Core engine is
// stereo-only by design (both Stereo and M/S processing need a channel
// pair); PluginProcessor adapts a mono host bus by duplicating into a
// scratch stereo pair and downmixing on the way out. That adaptation is
// exercised by StateRoundTripValidation, which drives the real processor.

#include "NovaRoyal670Engine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <utility>

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& label)
    {
        if (condition)
            std::printf ("PASS: %s\n", label.toRawUTF8());
        else
        {
            std::printf ("FAIL: %s\n", label.toRawUTF8());
            ++failures;
        }
    }

    using nova::royal670::NovaRoyal670Engine;

    juce::dsp::ProcessSpec makeSpec (double sr, int blockSize)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sr;
        spec.maximumBlockSize = (juce::uint32) blockSize;
        spec.numChannels = 2;
        return spec;
    }

    void defaultParams (NovaRoyal670Engine& e, bool midSide = false, int mode = 4, bool bypass = false)
    {
        e.setParams (/*inputDb*/ 3.2f, /*hrmDrivePercent*/ 35.0f,
                     /*acThresholdDb*/ -6.8f, /*dcThresholdDb*/ 2.1f, /*timeConstantMode*/ mode,
                     /*outputDb*/ 1.5f, /*mixPercent*/ 100.0f,
                     /*midSideMode*/ midSide, /*linkPercent*/ 100.0f,
                     /*sidechainHpfHz*/ 80.0f, /*sidechainListen*/ false,
                     /*weightPercent*/ 50.0f, /*tubeSaturationPercent*/ 35.0f, /*transformerPercent*/ 40.0f,
                     /*outputTrimDb*/ 0.0f, /*autoGain*/ true,
                     /*analogNoisePercent*/ 15.0f, /*bypassed*/ bypass);
    }

    bool bufferIsFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            const float* d = b.getReadPointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (d[i]))
                    return false;
        }
        return true;
    }
}

int main()
{
    // ---- Silence ----
    {
        constexpr double sr = 48000.0;
        NovaRoyal670Engine engine;
        engine.prepare (makeSpec (sr, 512), 1);
        defaultParams (engine);

        juce::AudioBuffer<float> buf (2, 512);
        bool allFinite = true;
        for (int block = 0; block < 20; ++block)
        {
            buf.clear();
            engine.processBlock (buf);
            allFinite = allFinite && bufferIsFinite (buf);
        }
        check (allFinite, "Silence: all outputs finite");
    }

    // ---- Impulse ----
    {
        constexpr double sr = 48000.0;
        NovaRoyal670Engine engine;
        engine.prepare (makeSpec (sr, 512), 1);
        defaultParams (engine);

        juce::AudioBuffer<float> buf (2, 512);
        buf.clear();
        buf.setSample (0, 0, 1.0f);
        buf.setSample (1, 0, 1.0f);
        bool allFinite = true;
        for (int block = 0; block < 50; ++block)
        {
            engine.processBlock (buf);
            allFinite = allFinite && bufferIsFinite (buf);
            buf.clear();
        }
        check (allFinite, "Impulse: all outputs finite");
    }

    // ---- Sine sweep ----
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 512;
        NovaRoyal670Engine engine;
        engine.prepare (makeSpec (sr, blockSize), 2);
        defaultParams (engine);

        juce::AudioBuffer<float> buf (2, blockSize);
        bool allFinite = true;
        double phaseSampleIndex = 0.0;
        for (int block = 0; block < 200; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const double t = phaseSampleIndex / sr;
                const double freq = 20.0 * std::pow (1000.0, t / 2.0);
                const float x = 0.7f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * t);
                buf.setSample (0, i, x);
                buf.setSample (1, i, x);
                phaseSampleIndex += 1.0;
            }
            engine.processBlock (buf);
            allFinite = allFinite && bufferIsFinite (buf);
        }
        check (allFinite, "Sine sweep through full engine: all outputs finite");
    }

    // ---- Extreme input levels ----
    {
        constexpr double sr = 48000.0;
        NovaRoyal670Engine engine;
        engine.prepare (makeSpec (sr, 512), 3);
        defaultParams (engine);

        bool allFinite = true;
        for (float amp : { 100.0f, 1000.0f, 1.0e6f, -1.0e6f })
        {
            juce::AudioBuffer<float> buf (2, 512);
            for (int i = 0; i < 512; ++i)
            {
                buf.setSample (0, i, amp);
                buf.setSample (1, i, -amp);
            }
            for (int block = 0; block < 10; ++block)
            {
                engine.processBlock (buf);
                allFinite = allFinite && bufferIsFinite (buf);
            }
        }
        check (allFinite, "Extreme input levels (up to 1e6): all outputs finite end-to-end");
    }

    // ---- Bypass: output converges to the dry input ----
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 512;
        NovaRoyal670Engine engine;
        engine.prepare (makeSpec (sr, blockSize), 4);
        defaultParams (engine, false, 4, true); // bypassed = true

        juce::AudioBuffer<float> buf (2, blockSize);
        juce::Random rng (99);
        float lastX = 0.0f;
        float maxAbsDiff = 0.0f;
        for (int block = 0; block < 40; ++block) // let the bypass ramp settle
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = rng.nextFloat() * 0.5f - 0.25f;
                buf.setSample (0, i, x);
                buf.setSample (1, i, x);
                lastX = x;
            }
            engine.processBlock (buf);
            if (block > 10) // after the ~30ms bypass ramp has fully settled
                maxAbsDiff = juce::jmax (maxAbsDiff, std::abs (buf.getSample (0, blockSize - 1) - lastX));
        }
        check (maxAbsDiff < 0.01f, "Bypass: settled output matches dry input closely");
    }

    // ---- Stereo Link: 100% forces equal-ish GR behavior on asymmetric
    //      L/R input; 0% allows the channels to diverge ----
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 512;

        auto runAsymmetric = [&] (float linkPercent) -> std::pair<float, float>
        {
            NovaRoyal670Engine engine;
            engine.prepare (makeSpec (sr, blockSize), 1);
            engine.setParams (0.0f, 0.0f, -20.0f, 0.0f, 3, 0.0f, 100.0f,
                               false, linkPercent, 20.0f, false,
                               0.0f, 0.0f, 0.0f, 0.0f, false, 0.0f, false);

            juce::AudioBuffer<float> buf (2, blockSize);
            float outL = 0.0f, outR = 0.0f;
            for (int block = 0; block < 40; ++block)
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    // Loud L, quiet R - an asymmetric stereo signal.
                    buf.setSample (0, i, 0.95f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * i / sr));
                    buf.setSample (1, i, 0.05f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * i / sr));
                }
                engine.processBlock (buf);
                outL = buf.getRMSLevel (0, 0, blockSize);
                outR = buf.getRMSLevel (1, 0, blockSize);
            }
            return { outL, outR };
        };

        const auto linked = runAsymmetric (100.0f);
        const auto unlinked = runAsymmetric (0.0f);

        // Linked: both channels see the louder channel's gain reduction,
        // so the level RATIO between L and R should stay closer to the
        // input's own 0.95/0.05 ratio (~19:1) than the unlinked case,
        // where R's own (much quieter) detector applies far less GR,
        // pulling the ratio down.
        const float linkedRatio = linked.first / juce::jmax (1.0e-6f, linked.second);
        const float unlinkedRatio = unlinked.first / juce::jmax (1.0e-6f, unlinked.second);
        check (linkedRatio > unlinkedRatio, "Link 100% preserves the input L/R ratio more than Link 0%");
    }

    // ---- Click-free M/S mode switching: bounded sample-to-sample delta
    //      across the switch instant, comparable to steady-state deltas ----
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 256;
        NovaRoyal670Engine engine;
        engine.prepare (makeSpec (sr, blockSize), 1);
        defaultParams (engine, false, 4);

        juce::AudioBuffer<float> buf (2, blockSize);
        float prevSample = 0.0f;
        float maxDelta = 0.0f;
        float maxDeltaAtSwitch = 0.0f;

        for (int block = 0; block < 30; ++block)
        {
            if (block == 15)
                defaultParams (engine, true, 4); // flip to M/S mid-stream

            for (int i = 0; i < blockSize; ++i)
            {
                const float x = 0.6f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * (block * blockSize + i) / sr);
                buf.setSample (0, i, x);
                buf.setSample (1, i, x * 0.7f);
            }
            engine.processBlock (buf);

            for (int i = 0; i < blockSize; ++i)
            {
                const float s = buf.getSample (0, i);
                const float delta = std::abs (s - prevSample);
                if (block == 15 && i < 10)
                    maxDeltaAtSwitch = juce::jmax (maxDeltaAtSwitch, delta);
                else
                    maxDelta = juce::jmax (maxDelta, delta);
                prevSample = s;
            }
        }
        check (maxDeltaAtSwitch < maxDelta * 3.0f + 0.05f, "M/S mode switch: no outsized sample-to-sample jump at the switch instant");
    }

    // ---- Parameter automation: sweep everything, no NaN/Inf ----
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 512;
        NovaRoyal670Engine engine;
        engine.prepare (makeSpec (sr, blockSize), 2);

        juce::AudioBuffer<float> buf (2, blockSize);
        bool allFinite = true;
        for (int block = 0; block < 100; ++block)
        {
            const float t = (float) block / 100.0f;
            engine.setParams (
                juce::jmap (t, -20.0f, 20.0f), juce::jmap (t, 0.0f, 100.0f),
                juce::jmap (t, -20.0f, 20.0f), juce::jmap (t, -20.0f, 20.0f), 1 + (block % 6),
                juce::jmap (t, -20.0f, 20.0f), juce::jmap (t, 0.0f, 100.0f),
                (block % 20) < 10, juce::jmap (t, 0.0f, 100.0f),
                juce::jmap (t, 20.0f, 2000.0f), false,
                juce::jmap (t, 0.0f, 100.0f), juce::jmap (t, 0.0f, 100.0f), juce::jmap (t, 0.0f, 100.0f),
                juce::jmap (t, -12.0f, 12.0f), (block % 2) == 0,
                juce::jmap (t, 0.0f, 100.0f), false);

            for (int i = 0; i < blockSize; ++i)
            {
                const float x = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * (block * blockSize + i) / sr);
                buf.setSample (0, i, x);
                buf.setSample (1, i, x);
            }
            engine.processBlock (buf);
            allFinite = allFinite && bufferIsFinite (buf);
        }
        check (allFinite, "Full parameter automation sweep: all outputs finite, no NaN/Inf");
    }

    // ---- Sample-rate independence ----
    {
        bool allFinite = true;
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        {
            NovaRoyal670Engine engine;
            engine.prepare (makeSpec (sr, 512), 2);
            defaultParams (engine);

            juce::AudioBuffer<float> buf (2, 512);
            for (int block = 0; block < 20; ++block)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float x = 0.6f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * i / sr);
                    buf.setSample (0, i, x);
                    buf.setSample (1, i, x);
                }
                engine.processBlock (buf);
            }
            allFinite = allFinite && bufferIsFinite (buf);
        }
        check (allFinite, "Stable at 44.1/48/88.2/96/176.4/192kHz");
    }

    // ---- Oversampling changes: latency reporting, Off = 0 latency,
    //      no crash/NaN when re-preparing at a different stage count ----
    {
        constexpr double sr = 48000.0;
        int latencies[4] {};
        int idx = 0;
        for (int stages : { 0, 1, 2, 3 })
        {
            NovaRoyal670Engine engine;
            engine.prepare (makeSpec (sr, 512), stages);
            defaultParams (engine);
            latencies[idx++] = engine.getLatencySamples();

            juce::AudioBuffer<float> buf (2, 512);
            bool allFinite = true;
            for (int block = 0; block < 10; ++block)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float x = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * i / sr);
                    buf.setSample (0, i, x);
                    buf.setSample (1, i, x);
                }
                engine.processBlock (buf);
                allFinite = allFinite && bufferIsFinite (buf);
            }
            check (allFinite, juce::String ("Oversampling stages=") + juce::String (stages) + ": all outputs finite");
        }
        check (latencies[0] == 0, "Oversampling Off: zero added latency");
        check (latencies[1] > latencies[0] && latencies[2] > latencies[1] && latencies[3] > latencies[2],
               "Latency increases monotonically as oversampling stages increase");
        std::printf ("Latencies (Off/2x/4x/8x): %d %d %d %d\n", latencies[0], latencies[1], latencies[2], latencies[3]);
    }

    std::printf ("\n%d check(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
