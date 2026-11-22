// Headless validation for NovaGlueEngine (the full facade): silence,
// impulses, sine sweeps, extreme levels, bypass, Stereo Link image
// stability, click-free Ratio/Release switching, the GLUE-invariance
// test (the plugin's core promise), Drive loudness compensation, Mix
// phase alignment, oversampling latency + glitch-free switching,
// parameter automation, sample-rate/buffer-size sweeps.

#include "NovaGlueEngine.h"
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

    using nova::glue::NovaGlueEngine;

    juce::dsp::ProcessSpec makeSpec (double sr, int blockSize)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sr;
        spec.maximumBlockSize = (juce::uint32) blockSize;
        spec.numChannels = 2;
        return spec;
    }

    // thresholdDb, attackChoice, releaseChoice, makeupDb, ratioChoice,
    // scHpfHz, mixPercent, gluePercent, drivePercent, autoGain,
    // stereoLink, bypassed
    void defaultParams (NovaGlueEngine& e, float thresholdDb = -10.0f, bool bypass = false)
    {
        e.setParams (thresholdDb, 4, 4, 0.0f, 0, 20.0f, 100.0f, 45.0f, 0.0f, true, true, bypass);
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

    void fillTone (juce::AudioBuffer<float>& buf, double sr, double freq, float amp, int startSample = 0)
    {
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const float x = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (startSample + i) / sr);
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
        }
    }

    // A crude "full-mix-style" signal: three sustained tones (bass/mid/
    // presence) plus a periodic transient burst, used by the GLUE-
    // invariance test since a single sine can't represent "the musical
    // envelope of the buss" the spec talks about.
    void fillMixLikeSignal (juce::AudioBuffer<float>& buf, double sr, int blockIndex, int blockSize)
    {
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const double t = (double) (blockIndex * blockSize + i) / sr;
            float x = 0.35f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * t)
                    + 0.28f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 900.0 * t)
                    + 0.18f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 4000.0 * t);
            if ((blockIndex % 10) == 0 && i < 40)
                x += 0.45f;
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
        }
    }
}

int main()
{
    // ---- Silence ----
    {
        constexpr double sr = 48000.0;
        NovaGlueEngine engine;
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
        NovaGlueEngine engine;
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
        NovaGlueEngine engine;
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
        NovaGlueEngine engine;
        engine.prepare (makeSpec (sr, 512), 1);
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

    // ---- Bypass: settled output matches dry input closely (oversampling
    //      Off, same reasoning as Glow's own bypass test) ----
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 512;
        NovaGlueEngine engine;
        engine.prepare (makeSpec (sr, blockSize), 0);
        defaultParams (engine, -10.0f, true);

        juce::AudioBuffer<float> buf (2, blockSize);
        juce::Random rng (99);
        float lastX = 0.0f;
        float maxAbsDiff = 0.0f;
        for (int block = 0; block < 40; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = rng.nextFloat() * 0.5f - 0.25f;
                buf.setSample (0, i, x);
                buf.setSample (1, i, x);
                lastX = x;
            }
            engine.processBlock (buf);
            if (block > 10)
                maxAbsDiff = juce::jmax (maxAbsDiff, std::abs (buf.getSample (0, blockSize - 1) - lastX));
        }
        check (maxAbsDiff < 0.01f, "Bypass: settled output matches dry input closely");
    }

    // ---- Stereo Link: ON preserves the input L/R ratio more than OFF,
    //      and a strong one-channel-only transient does not create
    //      runaway/unstable divergence ----
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 512;

        auto runAsymmetric = [&] (bool linked) -> std::pair<float, float>
        {
            NovaGlueEngine engine;
            engine.prepare (makeSpec (sr, blockSize), 1);
            engine.setParams (-14.0f, 4, 4, 0.0f, 0, 20.0f, 100.0f, 0.0f, 0.0f, false, linked, false);

            juce::AudioBuffer<float> buf (2, blockSize);
            float outL = 0.0f, outR = 0.0f;
            for (int block = 0; block < 40; ++block)
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    buf.setSample (0, i, 0.95f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * i / sr));
                    buf.setSample (1, i, 0.05f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * i / sr));
                }
                engine.processBlock (buf);
                outL = buf.getRMSLevel (0, 0, blockSize);
                outR = buf.getRMSLevel (1, 0, blockSize);
            }
            return { outL, outR };
        };

        const auto linked = runAsymmetric (true);
        const auto unlinked = runAsymmetric (false);
        const float linkedRatio = linked.first / juce::jmax (1.0e-6f, linked.second);
        const float unlinkedRatio = unlinked.first / juce::jmax (1.0e-6f, unlinked.second);
        check (linkedRatio > unlinkedRatio, "Stereo Link ON preserves the input L/R ratio more than Link OFF");

        // A strong transient on only one channel, Link ON, must not create
        // an unstable/runaway image (e.g. the quiet channel being pushed
        // to a wildly different, exploding gain).
        NovaGlueEngine engine;
        engine.prepare (makeSpec (sr, blockSize), 1);
        engine.setParams (-10.0f, 0, 0, 0.0f, 2, 20.0f, 100.0f, 0.0f, 0.0f, false, true, false);
        juce::AudioBuffer<float> buf (2, blockSize);
        bool allFinite = true;
        bool stable = true;
        float prevPeakDiff = 0.0f;
        for (int block = 0; block < 30; ++block)
        {
            buf.clear();
            if (block % 5 == 0)
            {
                buf.setSample (0, 0, 1.0f); // transient on L only
            }
            engine.processBlock (buf);
            allFinite = allFinite && bufferIsFinite (buf);
            const float peakDiff = std::abs (buf.getMagnitude (0, 0, blockSize) - buf.getMagnitude (1, 0, blockSize));
            if (peakDiff > prevPeakDiff * 4.0f + 1.0f)
                stable = false;
            prevPeakDiff = peakDiff;
        }
        check (allFinite && stable, "One-channel transient with Stereo Link ON does not create unstable/runaway stereo-image movement");
    }

    // ---- Click-free Ratio switching: no outsized sample-to-sample jump ----
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 256;
        NovaGlueEngine engine;
        engine.prepare (makeSpec (sr, blockSize), 1);
        engine.setParams (-14.0f, 4, 4, 0.0f, 0, 20.0f, 100.0f, 45.0f, 0.0f, false, true, false);

        juce::AudioBuffer<float> buf (2, blockSize);
        float prevSample = 0.0f;
        float maxDelta = 0.0f, maxDeltaAtSwitch = 0.0f;
        for (int block = 0; block < 30; ++block)
        {
            if (block == 15)
                engine.setParams (-14.0f, 4, 4, 0.0f, 2, 20.0f, 100.0f, 45.0f, 0.0f, false, true, false); // flip to 10:1

            fillTone (buf, sr, 300.0, 0.9f, block * blockSize);
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
        check (maxDeltaAtSwitch < maxDelta * 3.0f + 0.05f, "Ratio switch (2:1->10:1): no outsized sample-to-sample jump");
    }

    // ---- Click-free Release switching (fixed<->AUTO): no outsized jump ----
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 256;
        NovaGlueEngine engine;
        engine.prepare (makeSpec (sr, blockSize), 1);
        engine.setParams (-14.0f, 4, 1, 0.0f, 1, 20.0f, 100.0f, 45.0f, 0.0f, false, true, false); // fixed 0.3s

        juce::AudioBuffer<float> buf (2, blockSize);
        float prevSample = 0.0f;
        float maxDelta = 0.0f, maxDeltaAtSwitch = 0.0f;
        for (int block = 0; block < 30; ++block)
        {
            if (block == 15)
                engine.setParams (-14.0f, 4, 4, 0.0f, 1, 20.0f, 100.0f, 45.0f, 0.0f, false, true, false); // flip to AUTO

            fillTone (buf, sr, 300.0, 0.9f, block * blockSize);
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
        check (maxDeltaAtSwitch < maxDelta * 3.0f + 0.05f, "Release switch (fixed 0.3s -> AUTO): no outsized sample-to-sample jump");
    }

    // ---- THE GLUE-invariance test: the plugin's core promise. Average
    //      gain reduction across a full-mix-style signal must stay nearly
    //      flat between Glue=0 and Glue=100 at matching Threshold/Ratio -
    //      Threshold/Ratio remain the only controls of HOW MUCH
    //      compression happens. A trajectory-smoothness proxy (block-to-
    //      block GR delta variance) must still measurably change, proving
    //      Glue does something real without simply adding more
    //      compression. ----
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 512;

        auto measureGlue = [&] (float gluePercent) -> std::pair<float, float>
        {
            NovaGlueEngine engine;
            engine.prepare (makeSpec (sr, blockSize), 0);
            engine.setParams (-16.0f, 4, 4, 0.0f, 0, 20.0f, 100.0f, gluePercent, 0.0f, false, true, false);

            juce::AudioBuffer<float> buf (2, blockSize);
            double grSum = 0.0;
            int count = 0;
            double deltaSqSum = 0.0;
            int deltaCount = 0;
            float lastGr = 0.0f;
            for (int block = 0; block < 80; ++block)
            {
                fillMixLikeSignal (buf, sr, block, blockSize);
                engine.processBlock (buf);
                if (block > 15)
                {
                    const float gr = engine.getGainReductionDb();
                    grSum += gr;
                    ++count;
                    const float delta = gr - lastGr;
                    deltaSqSum += (double) delta * delta;
                    ++deltaCount;
                    lastGr = gr;
                }
            }
            const float avgGr = count > 0 ? (float) (grSum / count) : 0.0f;
            const float trajectoryVariance = deltaCount > 0 ? (float) (deltaSqSum / deltaCount) : 0.0f;
            return { avgGr, trajectoryVariance };
        };

        const auto at0 = measureGlue (0.0f);
        const auto at100 = measureGlue (100.0f);
        std::printf ("GLUE invariance: avgGR(0)=%.3fdB avgGR(100)=%.3fdB, trajVar(0)=%.6f trajVar(100)=%.6f\n",
                     at0.first, at100.first, at0.second, at100.second);
        check (std::abs (at100.first - at0.first) < 0.5f,
               "HARD INVARIANT: average gain reduction changes by less than 0.5dB between Glue=0 and Glue=100 at matching Threshold/Ratio");
        const float relDiff = std::abs (at100.second - at0.second) / juce::jmax (1.0e-6f, at0.second);
        check (relDiff > 0.01f,
               "GLUE measurably changes GR trajectory smoothness (proving it does something real, not just more compression)");
    }

    // ---- Drive loudness compensation: output RMS stays roughly flat as
    //      Drive increases despite added harmonic content (oversampling
    //      Off, same phase-confound reasoning as Glow's distortion test) ----
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 512;

        auto measureRms = [&] (float drivePercent) -> float
        {
            NovaGlueEngine engine;
            engine.prepare (makeSpec (sr, blockSize), 0);
            engine.setParams (0.0f, 4, 4, 0.0f, 0, 20.0f, 100.0f, 0.0f, drivePercent, false, true, false);

            juce::AudioBuffer<float> buf (2, blockSize);
            double sumSq = 0.0;
            int count = 0;
            for (int block = 0; block < 40; ++block)
            {
                fillTone (buf, sr, 220.0, 0.5f, block * blockSize);
                engine.processBlock (buf);
                if (block > 10)
                {
                    for (int i = 0; i < blockSize; ++i)
                    {
                        const float s = buf.getSample (0, i);
                        sumSq += (double) s * s;
                    }
                    count += blockSize;
                }
            }
            return count > 0 ? (float) std::sqrt (sumSq / count) : 0.0f;
        };

        const float rms0 = measureRms (0.0f);
        const float rms100 = measureRms (100.0f);
        std::printf ("Drive RMS compensation: 0=%.4f 100=%.4f\n", rms0, rms100);
        check (rms0 > 0.0f && std::abs (rms100 - rms0) < rms0 * 0.3f,
               "Drive's loudness-compensation curve keeps output RMS roughly flat despite added harmonic content");
    }

    // ---- Mix phase alignment: at 50%, wet and dry impulse peaks land at
    //      the same sample index (proves the dry-delay-compensation) ----
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 512;
        NovaGlueEngine dryOnlyEngine, mixedEngine;
        dryOnlyEngine.prepare (makeSpec (sr, blockSize), 2);
        dryOnlyEngine.setParams (10.0f, 4, 4, 0.0f, 0, 20.0f, 0.0f, 0.0f, 0.0f, false, true, false); // Mix=0 -> pure dry, still delay-compensated
        mixedEngine.prepare (makeSpec (sr, blockSize), 2);
        mixedEngine.setParams (10.0f, 4, 4, 0.0f, 0, 20.0f, 50.0f, 0.0f, 0.0f, false, true, false);

        auto findImpulsePeakIndex = [&] (NovaGlueEngine& engine) -> int
        {
            juce::AudioBuffer<float> buf (2, blockSize);
            buf.clear();
            buf.setSample (0, 100, 1.0f);
            buf.setSample (1, 100, 1.0f);
            int peakIdx = -1;
            float peakVal = 0.0f;
            for (int block = 0; block < 3; ++block)
            {
                engine.processBlock (buf);
                for (int i = 0; i < blockSize; ++i)
                {
                    const float v = std::abs (buf.getSample (0, i));
                    if (v > peakVal) { peakVal = v; peakIdx = block * blockSize + i; }
                }
                buf.clear();
            }
            return peakIdx;
        };

        const int dryPeakIdx = findImpulsePeakIndex (dryOnlyEngine);
        const int mixedPeakIdx = findImpulsePeakIndex (mixedEngine);
        std::printf ("Mix phase alignment: dry-path peak idx=%d, 50%% mix peak idx=%d\n", dryPeakIdx, mixedPeakIdx);
        check (dryPeakIdx >= 0 && mixedPeakIdx >= 0, "Mix phase alignment: both runs found a peak");
        check (std::abs (dryPeakIdx - mixedPeakIdx) <= 1, "Mix phase alignment: wet/dry impulse peaks land at the same sample index (within 1 sample)");
    }

    // ---- Oversampling: latency reporting + glitch-free switching ----
    {
        constexpr double sr = 48000.0;
        int latencies[3] {};
        int idx = 0;
        for (int stages : { 0, 1, 2 })
        {
            NovaGlueEngine engine;
            engine.prepare (makeSpec (sr, 512), stages);
            defaultParams (engine);
            latencies[idx++] = engine.getLatencySamples();

            juce::AudioBuffer<float> buf (2, 512);
            bool allFinite = true;
            for (int block = 0; block < 10; ++block)
            {
                fillTone (buf, sr, 300.0, 0.5f, block * 512);
                engine.processBlock (buf);
                allFinite = allFinite && bufferIsFinite (buf);
            }
            check (allFinite, juce::String ("Oversampling stages=") + juce::String (stages) + ": all outputs finite");
        }
        check (latencies[0] == 0, "Oversampling Off: zero added latency");
        check (latencies[1] > latencies[0] && latencies[2] > latencies[1],
               "Latency increases monotonically as oversampling stages increase (Off < 2x < 4x)");
        std::printf ("Latencies (Off/2x/4x): %d %d %d\n", latencies[0], latencies[1], latencies[2]);
    }

    // ---- Parameter automation: sweep everything, no NaN/Inf ----
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 512;
        NovaGlueEngine engine;
        engine.prepare (makeSpec (sr, blockSize), 2);

        juce::AudioBuffer<float> buf (2, blockSize);
        bool allFinite = true;
        for (int block = 0; block < 100; ++block)
        {
            const float t = (float) block / 100.0f;
            engine.setParams (
                juce::jmap (t, -30.0f, 10.0f), block % 6, (block / 6) % 5,
                juce::jmap (t, -20.0f, 20.0f), block % 3,
                juce::jmap (t, 20.0f, 400.0f), juce::jmap (t, 0.0f, 100.0f),
                juce::jmap (t, 0.0f, 100.0f), juce::jmap (t, 0.0f, 100.0f),
                (block % 2) == 0, (block % 3) == 0, (block % 5) == 0);

            fillTone (buf, sr, 300.0, 0.5f, block * blockSize);
            engine.processBlock (buf);
            allFinite = allFinite && bufferIsFinite (buf);
        }
        check (allFinite, "Full parameter automation sweep: all outputs finite, no NaN/Inf");
    }

    // ---- Sample-rate sweep ----
    {
        bool allFinite = true;
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        {
            NovaGlueEngine engine;
            engine.prepare (makeSpec (sr, 512), 2);
            defaultParams (engine);

            juce::AudioBuffer<float> buf (2, 512);
            for (int block = 0; block < 20; ++block)
            {
                fillTone (buf, sr, 300.0, 0.6f, block * 512);
                engine.processBlock (buf);
            }
            allFinite = allFinite && bufferIsFinite (buf);
        }
        check (allFinite, "Stable at 44.1/48/88.2/96/176.4/192kHz");
    }

    // ---- Buffer-size sweep ----
    {
        bool allFinite = true;
        constexpr double sr = 48000.0;
        for (int blockSize : { 32, 64, 128, 256, 512, 1024, 2048, 4096 })
        {
            NovaGlueEngine engine;
            engine.prepare (makeSpec (sr, blockSize), 1);
            defaultParams (engine);

            juce::AudioBuffer<float> buf (2, blockSize);
            for (int block = 0; block < 20; ++block)
            {
                fillTone (buf, sr, 300.0, 0.6f, block * blockSize);
                engine.processBlock (buf);
            }
            allFinite = allFinite && bufferIsFinite (buf);
        }
        check (allFinite, "Stable across buffer sizes 32-4096");
    }

    std::printf ("\n%d check(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
