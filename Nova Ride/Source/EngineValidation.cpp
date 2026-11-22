// Headless validation for the full NovaRideEngine (Detector + Sidechain +
// GainEngine + AutomationBuffer + bypass/output-trim composition). No
// PluginProcessor, no host - synthetic signals only. See CMakeLists.txt's
// NovaRideEngineValidation target.
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "NovaRideEngine.h"
#include "NovaRideConstants.h"

using namespace nova::ride;

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

    void section (const char* title)
    {
        std::printf ("\n=== %s ===\n", title);
    }

    void fillSine (juce::AudioBuffer<float>& buffer, double sampleRate, double freqHz, float dBFS, int startSampleIndex = 0)
    {
        const float amp = juce::Decibels::decibelsToGain (dBFS, -100.0f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                data[i] = amp * std::sin (2.0 * juce::MathConstants<double>::pi * freqHz * (startSampleIndex + i) / sampleRate);
        }
    }

    void fillSilence (juce::AudioBuffer<float>& buffer)
    {
        buffer.clear();
    }

    bool bufferAllFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite (data[i])) return false;
        }
        return true;
    }

    float bufferPeakLin (const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                peak = juce::jmax (peak, std::abs (data[i]));
        }
        return peak;
    }

    NovaRideEngine::Params defaultParams()
    {
        NovaRideEngine::Params p;
        p.targetDb = 0.0f;
        p.rangeDb = 6.0f;
        p.speedNorm = 0.5f;
        p.sensitivityNorm = 0.5f;
        p.outputDb = 0.0f;
        p.modeIndex = 0;
        p.musicEnabled = false;
        p.writeEnabled = false;
        p.bypassed = false;
        return p;
    }

    // ---- 1. RANGE is never exceeded, sample-by-sample, across extreme inputs and all 4 range stops ----
    void testRangeNeverExceeded()
    {
        section ("HARD INVARIANT: correction never exceeds Range, checked every sample");
        const double sr = 48000.0;
        const float ranges[] = { 3.0f, 6.0f, 9.0f, 12.0f };
        const float extremeDbs[] = { -60.0f, -40.0f, -20.0f, -6.0f, 0.0f };

        for (float rangeDb : ranges)
        {
            for (float sourceDb : extremeDbs)
            {
                NovaRideEngine engine;
                engine.prepare (sr, 1);
                auto params = defaultParams();
                params.rangeDb = rangeDb;
                params.targetDb = 0.0f;
                engine.setParams (params);

                juce::AudioBuffer<float> scEmpty (0, 1);
                juce::AudioBuffer<float> block (2, 1);

                bool everExceeded = false;
                const int totalSamples = static_cast<int> (sr * 0.5); // 0.5s, one sample at a time
                for (int i = 0; i < totalSamples; ++i)
                {
                    fillSine (block, sr, 220.0, sourceDb, i);
                    engine.processBlock (block, scEmpty, false);
                    if (std::abs (engine.getTelemetry().rideCorrectionDb) > rangeDb + 0.01f)
                        everExceeded = true;
                }

                check (! everExceeded, "range=" + juce::String (rangeDb, 1) + "dB, source=" + juce::String (sourceDb, 0)
                                            + "dBFS: correction stayed within range every sample");
            }
        }
    }

    // ---- 2. Silence never produces runaway positive gain ----
    void testSilenceNeverRunsAway()
    {
        section ("HARD INVARIANT: silence never produces runaway positive gain");
        const double sr = 48000.0;
        NovaRideEngine engine;
        engine.prepare (sr, 512);
        auto params = defaultParams();
        params.targetDb = 6.0f; // ask for a lot of raise
        params.rangeDb = 6.0f;
        engine.setParams (params);

        juce::AudioBuffer<float> scEmpty (0, 512);

        // Build up a boosted state: quiet input vs a loud target, for 2s.
        juce::AudioBuffer<float> quiet (2, 512);
        for (int b = 0; b < static_cast<int> (sr * 2 / 512); ++b)
        {
            fillSine (quiet, sr, 220.0, -30.0f, b * 512);
            engine.processBlock (quiet, scEmpty, false);
        }
        const float correctionBeforeSilence = engine.getTelemetry().rideCorrectionDb;
        check (correctionBeforeSilence > 0.5f, "engine actually built up a positive correction before the silence test ("
                                                    + juce::String (correctionBeforeSilence, 2) + "dB)");

        // Now feed 30s of true silence, checking every block that magnitude
        // never increases.
        juce::AudioBuffer<float> silence (2, 512);
        fillSilence (silence);
        float lastAbsCorrection = std::abs (correctionBeforeSilence);
        bool everIncreased = false;
        const int numBlocks = static_cast<int> (sr * 30 / 512);
        for (int b = 0; b < numBlocks; ++b)
        {
            engine.processBlock (silence, scEmpty, false);
            const float absCorrection = std::abs (engine.getTelemetry().rideCorrectionDb);
            if (absCorrection > lastAbsCorrection + 1.0e-4f)
                everIncreased = true;
            lastAbsCorrection = absCorrection;
        }

        check (! everIncreased, "correction magnitude never increased during 30s of silence");
        check (lastAbsCorrection < 0.1f, "correction settled close to 0dB by the end of 30s of silence (got "
                                             + juce::String (lastAbsCorrection, 4) + "dB)");
    }

    void testQuietPhraseGetsRaised()
    {
        section ("Quiet phrase gets raised");
        const double sr = 48000.0;
        NovaRideEngine engine;
        engine.prepare (sr, 512);
        auto params = defaultParams();
        params.targetDb = 0.0f;
        engine.setParams (params);

        juce::AudioBuffer<float> scEmpty (0, 512);
        juce::AudioBuffer<float> block (2, 512);
        for (int b = 0; b < static_cast<int> (sr * 2 / 512); ++b)
        {
            fillSine (block, sr, 220.0, -24.0f, b * 512);
            engine.processBlock (block, scEmpty, false);
        }

        check (engine.getTelemetry().rideCorrectionDb > 1.0f, "a quiet phrase below Target produces a positive (raising) correction ("
                                                                    + juce::String (engine.getTelemetry().rideCorrectionDb, 2) + "dB)");
    }

    void testLoudPhraseGetsReduced()
    {
        section ("Loud phrase gets reduced");
        const double sr = 48000.0;
        NovaRideEngine engine;
        engine.prepare (sr, 512);
        auto params = defaultParams();
        params.targetDb = -18.0f;
        engine.setParams (params);

        juce::AudioBuffer<float> scEmpty (0, 512);
        juce::AudioBuffer<float> block (2, 512);
        for (int b = 0; b < static_cast<int> (sr * 2 / 512); ++b)
        {
            fillSine (block, sr, 220.0, -3.0f, b * 512);
            engine.processBlock (block, scEmpty, false);
        }

        check (engine.getTelemetry().rideCorrectionDb < -1.0f, "a loud phrase above Target produces a negative (reducing) correction ("
                                                                     + juce::String (engine.getTelemetry().rideCorrectionDb, 2) + "dB)");
    }

    void testDeadBandPreventsMicroRiding()
    {
        section ("Dead-band prevents micro-riding at Target");
        const double sr = 48000.0;
        NovaRideEngine engine;
        engine.prepare (sr, 512);
        auto params = defaultParams();
        params.targetDb = -12.0f;
        engine.setParams (params);

        juce::AudioBuffer<float> scEmpty (0, 512);
        juce::AudioBuffer<float> block (2, 512);
        // The detector reads a rectified-average envelope, which for a sine
        // sits ~3.9dB below its peak dBFS (mean(|sin|) = 2/pi) - offset the
        // source level by that so its *detected* level, not its raw peak
        // dBFS, is what actually lands close to Target.
        for (int b = 0; b < static_cast<int> (sr * 2 / 512); ++b)
        {
            fillSine (block, sr, 220.0, -12.0f + 3.9f - 0.2f, b * 512);
            engine.processBlock (block, scEmpty, false);
        }

        check (std::abs (engine.getTelemetry().rideCorrectionDb) < 0.5f,
               "a tone within the dead-band of Target settles near 0dB correction (got "
                   + juce::String (engine.getTelemetry().rideCorrectionDb, 3) + "dB)");
    }

    void testModesProduceDifferentButValidResults()
    {
        section ("Natural/Tight/Live produce different but valid results");
        const double sr = 48000.0;
        const int checkpointBlocks = static_cast<int> (sr * 0.15 / 512); // early checkpoint, ~150ms in

        float earlyCorrection[3] = { 0.0f, 0.0f, 0.0f };
        for (int mode = 0; mode < 3; ++mode)
        {
            NovaRideEngine engine;
            engine.prepare (sr, 512);
            auto params = defaultParams();
            params.modeIndex = mode;
            params.targetDb = 0.0f;
            engine.setParams (params);

            juce::AudioBuffer<float> scEmpty (0, 512);
            juce::AudioBuffer<float> block (2, 512);
            for (int b = 0; b < checkpointBlocks; ++b)
            {
                fillSine (block, sr, 220.0, -24.0f, b * 512);
                engine.processBlock (block, scEmpty, false);
            }
            earlyCorrection[mode] = engine.getTelemetry().rideCorrectionDb;
            check (std::isfinite (earlyCorrection[mode]), "mode " + juce::String (mode) + " produces finite correction");
        }

        std::printf ("  (early-checkpoint correction: Natural=%.3f Tight=%.3f Live=%.3f)\n",
                     earlyCorrection[0], earlyCorrection[1], earlyCorrection[2]);
        check (earlyCorrection[2] >= earlyCorrection[0] - 0.01f, "Live has moved at least as far as Natural by the early checkpoint (faster response)");
        check (earlyCorrection[1] >= earlyCorrection[0] - 0.01f, "Tight has moved at least as far as Natural by the early checkpoint (faster response)");
    }

    void testSpeedScalesResponseTime()
    {
        section ("Speed parameter scales response time");
        const double sr = 48000.0;
        const int checkpointBlocks = static_cast<int> (sr * 0.1 / 512);

        auto run = [&] (float speedNorm) -> float
        {
            NovaRideEngine engine;
            engine.prepare (sr, 512);
            auto params = defaultParams();
            params.speedNorm = speedNorm;
            params.targetDb = 0.0f;
            engine.setParams (params);

            juce::AudioBuffer<float> scEmpty (0, 512);
            juce::AudioBuffer<float> block (2, 512);
            for (int b = 0; b < checkpointBlocks; ++b)
            {
                fillSine (block, sr, 220.0, -24.0f, b * 512);
                engine.processBlock (block, scEmpty, false);
            }
            return engine.getTelemetry().rideCorrectionDb;
        };

        const float slow = run (0.0f);
        const float fast = run (1.0f);
        check (fast >= slow - 0.01f, "speed=100% reaches at least as far as speed=0% by an early checkpoint (got slow="
                                          + juce::String (slow, 3) + " fast=" + juce::String (fast, 3) + ")");
    }

    void testSidechainConnectedVsDisconnected()
    {
        section ("Sidechain connected vs disconnected changes the effective target");
        const double sr = 48000.0;

        auto run = [&] (bool connected) -> float
        {
            NovaRideEngine engine;
            engine.prepare (sr, 512);
            auto params = defaultParams();
            params.musicEnabled = true;
            params.targetDb = 0.0f;
            // Range wide open so neither scenario saturates at the clamp
            // ceiling - that would mask the real difference in effective
            // target between "riding to an absolute Target" (disconnected)
            // and "riding relative to the music" (connected).
            params.rangeDb = 12.0f;
            engine.setParams (params);

            // Vocal peak -6.1dBFS -> detected level (rectified-average, ~3.9dB
            // below peak for a sine) lands around -10dB. Music peak -12.1dBFS
            // -> detected music level lands around -16dB. Disconnected, the
            // vocal is ridden up toward an absolute Target of 0dB (a big
            // raise); connected, it's ridden toward ~-16dB (music level +
            // Target), which is a reduction relative to the vocal's own -10dB
            // - the two scenarios should land clearly apart, not both pinned
            // to the Range ceiling.
            juce::AudioBuffer<float> vocal (2, 512);
            juce::AudioBuffer<float> music (2, 512);
            for (int b = 0; b < static_cast<int> (sr * 2 / 512); ++b)
            {
                fillSine (vocal, sr, 220.0, -6.1f, b * 512);
                fillSine (music, sr, 110.0, -12.1f, b * 512);
                engine.processBlock (vocal, connected ? music : juce::AudioBuffer<float> (0, 512), connected);
            }
            return engine.getTelemetry().rideCorrectionDb;
        };

        const float connectedResult = run (true);
        const float disconnectedResult = run (false);

        check (std::isfinite (connectedResult) && std::isfinite (disconnectedResult), "both scenarios produce finite results");
        check (std::abs (connectedResult - disconnectedResult) > 0.5f,
               "a present, loud sidechain measurably changes the ride behaviour vs. no sidechain (connected="
                   + juce::String (connectedResult, 2) + " disconnected=" + juce::String (disconnectedResult, 2) + ")");
    }

    void testRapidParameterChangesDoNotCrashOrProduceNaN()
    {
        section ("Rapid parameter changes do not crash or produce NaN");
        const double sr = 48000.0;
        NovaRideEngine engine;
        engine.prepare (sr, 256);

        std::mt19937 rng (12345);
        std::uniform_real_distribution<float> targetDist (-24.0f, 6.0f);
        std::uniform_real_distribution<float> rangeDist (1.0f, 12.0f);
        std::uniform_real_distribution<float> normDist (0.0f, 1.0f);
        std::uniform_int_distribution<int> modeDist (0, 2);
        std::uniform_int_distribution<int> boolDist (0, 1);

        juce::AudioBuffer<float> scEmpty (0, 256);
        juce::AudioBuffer<float> block (2, 256);
        bool allOk = true;
        for (int b = 0; b < 2000; ++b)
        {
            NovaRideEngine::Params params;
            params.targetDb = targetDist (rng);
            params.rangeDb = rangeDist (rng);
            params.speedNorm = normDist (rng);
            params.sensitivityNorm = normDist (rng);
            params.outputDb = targetDist (rng);
            params.modeIndex = modeDist (rng);
            params.musicEnabled = boolDist (rng) != 0;
            params.writeEnabled = boolDist (rng) != 0;
            params.bypassed = boolDist (rng) != 0;
            engine.setParams (params);

            fillSine (block, sr, 220.0 + b, -18.0f, b * 256);
            engine.processBlock (block, scEmpty, false);
            if (! bufferAllFinite (block)) { allOk = false; break; }
        }

        check (allOk, "2000 blocks of fully randomized parameters every block stayed finite throughout");
    }

    void testBypassIsClickFree()
    {
        section ("Bypass is click-free");
        const double sr = 48000.0;
        NovaRideEngine engine;
        engine.prepare (sr, 64);
        auto params = defaultParams();
        params.targetDb = 6.0f; // force a real correction so bypass has something to fade away from
        engine.setParams (params);

        juce::AudioBuffer<float> scEmpty (0, 64);
        juce::AudioBuffer<float> block (2, 64);

        // Run in a boosted state first.
        for (int b = 0; b < static_cast<int> (sr * 1 / 64); ++b)
        {
            fillSine (block, sr, 220.0, -20.0f, b * 64);
            engine.processBlock (block, scEmpty, false);
        }

        params.bypassed = true;
        engine.setParams (params);

        float lastSample = block.getSample (0, block.getNumSamples() - 1);
        float maxDelta = 0.0f;
        for (int b = 0; b < static_cast<int> (sr * 0.2 / 64); ++b)
        {
            fillSine (block, sr, 220.0, -20.0f, (static_cast<int> (sr * 1 / 64) + b) * 64);
            engine.processBlock (block, scEmpty, false);
            for (int i = 0; i < block.getNumSamples(); ++i)
            {
                const float s = block.getSample (0, i);
                maxDelta = juce::jmax (maxDelta, std::abs (s - lastSample));
                lastSample = s;
            }
        }

        // A -20dBFS tone has an inherent sample-to-sample delta on the order
        // of amplitude * 2*pi*f/sr; anything wildly larger indicates a click.
        const float expectedToneDelta = juce::Decibels::decibelsToGain (-20.0f) * 2.0f * juce::MathConstants<float>::pi * 220.0f / static_cast<float> (sr);
        check (maxDelta < expectedToneDelta * 20.0f, "no anomalous sample-to-sample jump while crossfading into bypass (max delta "
                                                          + juce::String (maxDelta, 6) + ", tone's own delta ~" + juce::String (expectedToneDelta, 6) + ")");
    }

    void testNanInfInputResilience()
    {
        section ("NaN/Inf input resilience (full engine)");
        const double sr = 48000.0;
        NovaRideEngine engine;
        engine.prepare (sr, 128);
        engine.setParams (defaultParams());

        juce::AudioBuffer<float> scEmpty (0, 128);
        juce::AudioBuffer<float> block (2, 128);
        for (int i = 0; i < block.getNumSamples(); ++i)
        {
            block.setSample (0, i, (i % 3 == 0) ? std::numeric_limits<float>::quiet_NaN()
                                     : (i % 3 == 1) ? std::numeric_limits<float>::infinity()
                                                     : -std::numeric_limits<float>::infinity());
            block.setSample (1, i, block.getSample (0, i));
        }

        engine.processBlock (block, scEmpty, false);
        check (bufferAllFinite (block), "output buffer is fully finite after a block of NaN/Inf input");
        check (std::isfinite (engine.getTelemetry().rideCorrectionDb), "telemetry correction stays finite");

        fillSine (block, sr, 220.0, -12.0f);
        engine.processBlock (block, scEmpty, false);
        check (bufferAllFinite (block), "engine recovers to finite output on real signal after NaN/Inf");
    }

    void testSampleRateSweep()
    {
        section ("Sample-rate sweep");
        const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0 };
        for (double sr : rates)
        {
            NovaRideEngine engine;
            engine.prepare (sr, 512);
            engine.setParams (defaultParams());

            juce::AudioBuffer<float> scEmpty (0, 512);
            juce::AudioBuffer<float> block (2, 512);
            bool allOk = true;
            for (int b = 0; b < static_cast<int> (sr * 1 / 512); ++b)
            {
                fillSine (block, sr, 220.0, -18.0f, b * 512);
                engine.processBlock (block, scEmpty, false);
                if (! bufferAllFinite (block)) { allOk = false; break; }
            }
            check (allOk, "finite output throughout 1s @ " + juce::String (sr, 0) + "Hz");
            check (engine.getTelemetry().gateOpen, "gate opens on a clear tone @ " + juce::String (sr, 0) + "Hz");
        }
    }

    void testMultipleBufferSizes()
    {
        section ("Multiple buffer sizes");
        const int sizes[] = { 32, 64, 128, 256, 512, 1024, 2048, 4096 };
        const double sr = 48000.0;
        for (int blockSize : sizes)
        {
            NovaRideEngine engine;
            engine.prepare (sr, blockSize);
            engine.setParams (defaultParams());

            juce::AudioBuffer<float> scEmpty (0, blockSize);
            juce::AudioBuffer<float> block (2, blockSize);
            bool allOk = true;
            int sampleCounter = 0;
            const int totalSamples = static_cast<int> (sr * 1);
            while (sampleCounter < totalSamples)
            {
                fillSine (block, sr, 220.0, -18.0f, sampleCounter);
                engine.processBlock (block, scEmpty, false);
                if (! bufferAllFinite (block)) { allOk = false; break; }
                sampleCounter += blockSize;
            }
            check (allOk, "finite output with block size " + juce::String (blockSize));
        }
    }

    void testMonoAndStereoHandling()
    {
        section ("Mono and stereo handling");
        const double sr = 48000.0;
        for (int numChannels : { 1, 2 })
        {
            NovaRideEngine engine;
            engine.prepare (sr, 512);
            engine.setParams (defaultParams());

            juce::AudioBuffer<float> scEmpty (0, 512);
            juce::AudioBuffer<float> block (numChannels, 512);
            bool allOk = true;
            for (int b = 0; b < static_cast<int> (sr * 1 / 512); ++b)
            {
                fillSine (block, sr, 220.0, -18.0f, b * 512);
                engine.processBlock (block, scEmpty, false);
                if (! bufferAllFinite (block)) { allOk = false; break; }
            }
            check (allOk, juce::String (numChannels) + "-channel processing stays finite");
            check (engine.getTelemetry().gateOpen, juce::String (numChannels) + "-channel: gate opens on a clear tone");
        }
    }

    void testWriteModeCapturesTrajectory()
    {
        section ("Write mode captures the ride trajectory");
        const double sr = 48000.0;
        NovaRideEngine engine;
        engine.prepare (sr, 512);
        auto params = defaultParams();
        params.writeEnabled = true;
        engine.setParams (params);

        juce::AudioBuffer<float> scEmpty (0, 512);
        juce::AudioBuffer<float> block (2, 512);
        for (int b = 0; b < static_cast<int> (sr * 2 / 512); ++b)
        {
            fillSine (block, sr, 220.0, -20.0f, b * 512);
            engine.processBlock (block, scEmpty, false);
        }

        const int numPoints = engine.getAutomationBuffer().getNumPoints();
        check (numPoints > 0, "automation buffer captured points while Write was engaged (" + juce::String (numPoints) + " points)");

        bool monotonic = true;
        double lastT = -1.0;
        for (int i = 0; i < numPoints; ++i)
        {
            const auto p = engine.getAutomationBuffer().getPoint (i);
            if (p.timeSeconds <= lastT) { monotonic = false; break; }
            lastT = p.timeSeconds;
        }
        check (monotonic, "captured timestamps are strictly increasing");

        params.writeEnabled = false;
        engine.setParams (params);
        for (int b = 0; b < 10; ++b)
        {
            fillSine (block, sr, 220.0, -20.0f, b * 512);
            engine.processBlock (block, scEmpty, false);
        }
        check (engine.getAutomationBuffer().getNumPoints() == numPoints, "buffer stops growing once Write is disengaged");
    }

    void testAutomationBufferExportRoundTrip()
    {
        section ("Automation buffer CSV export round-trip");
        const double sr = 48000.0;
        NovaRideEngine engine;
        engine.prepare (sr, 512);
        auto params = defaultParams();
        params.writeEnabled = true;
        engine.setParams (params);

        juce::AudioBuffer<float> scEmpty (0, 512);
        juce::AudioBuffer<float> block (2, 512);
        for (int b = 0; b < static_cast<int> (sr * 1 / 512); ++b)
        {
            fillSine (block, sr, 220.0, -20.0f, b * 512);
            engine.processBlock (block, scEmpty, false);
        }

        const int numPoints = engine.getAutomationBuffer().getNumPoints();
        auto tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("NovaRideValidation_export_" + juce::Uuid().toString() + ".csv");
        const bool wroteOk = engine.getAutomationBuffer().exportToCsv (tempFile);
        check (wroteOk, "CSV export reports success");
        check (tempFile.existsAsFile(), "CSV file actually exists on disk");

        auto text = tempFile.loadFileAsString();
        auto lines = juce::StringArray::fromLines (text);
        lines.removeEmptyStrings();
        // header + numPoints data rows
        check (lines.size() == numPoints + 1, "CSV row count matches captured point count + header (expected "
                                                   + juce::String (numPoints + 1) + ", got " + juce::String (lines.size()) + ")");

        tempFile.deleteFile();
    }

    void testOutputNeverProducesUnboundedValues()
    {
        section ("Output never produces unbounded/runaway values");
        const double sr = 48000.0;
        NovaRideEngine engine;
        engine.prepare (sr, 512);
        auto params = defaultParams();
        params.rangeDb = 12.0f;
        params.outputDb = 0.0f;
        engine.setParams (params);

        juce::AudioBuffer<float> scEmpty (0, 512);
        juce::AudioBuffer<float> block (2, 512);
        const float inputPeak = juce::Decibels::decibelsToGain (-18.0f, -100.0f);
        float maxOutputPeak = 0.0f;
        for (int b = 0; b < static_cast<int> (sr * 2 / 512); ++b)
        {
            fillSine (block, sr, 220.0, -18.0f, b * 512);
            engine.processBlock (block, scEmpty, false);
            maxOutputPeak = juce::jmax (maxOutputPeak, bufferPeakLin (block));
        }

        // With Range=12dB and Output trim=0dB, the absolute theoretical
        // ceiling is inputPeak * 10^(12/20), plus a small safety margin for
        // the gain-slew filter's transient response.
        const float theoreticalCeiling = inputPeak * juce::Decibels::decibelsToGain (12.0f) * 1.2f;
        check (maxOutputPeak <= theoreticalCeiling,
               "output peak (" + juce::String (maxOutputPeak, 4) + ") stays within the Range-derived ceiling ("
                   + juce::String (theoreticalCeiling, 4) + ")");
    }
}

int main()
{
    testRangeNeverExceeded();
    testSilenceNeverRunsAway();
    testQuietPhraseGetsRaised();
    testLoudPhraseGetsReduced();
    testDeadBandPreventsMicroRiding();
    testModesProduceDifferentButValidResults();
    testSpeedScalesResponseTime();
    testSidechainConnectedVsDisconnected();
    testRapidParameterChangesDoNotCrashOrProduceNaN();
    testBypassIsClickFree();
    testNanInfInputResilience();
    testSampleRateSweep();
    testMultipleBufferSizes();
    testMonoAndStereoHandling();
    testWriteModeCapturesTrajectory();
    testAutomationBufferExportRoundTrip();
    testOutputNeverProducesUnboundedValues();

    std::printf ("\n  NovaRideEngineValidation: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
