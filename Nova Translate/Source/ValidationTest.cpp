/*
  Nova Translate — Core DSP Validation
  Headless correctness checks for the real-time-safe engine core
  (PluginProcessorCore). No APVTS/editor/audio-hardware dependency, so this
  links and runs without a plugin host or GUI event loop.
*/

#include <JuceHeader.h>
#include <chrono>
#include <cmath>

#include "PluginProcessorCore.h"
#include "EnvironmentProfiles.hpp"
#include "ParameterIDs.hpp"

// ─── helpers ───────────────────────────────────────────────────────────────

static int passed = 0;
static int failed = 0;

static void check (const char* label, bool result)
{
    if (result) { std::cout << "  [PASS] " << label << "\n"; ++passed; }
    else        { std::cout << "  [FAIL] " << label << "\n"; ++failed; }
}

static void section (const char* title)
{
    std::cout << "\n=== " << title << " ===\n";
}

static const int kImplementedEnvironments[12] = {
    EnvironmentLibrary::NovaMainRoom, EnvironmentLibrary::NearfieldStudio, EnvironmentLibrary::MasteringSuite,
    EnvironmentLibrary::BedroomStudio, EnvironmentLibrary::Sedan, EnvironmentLibrary::LuxurySUV,
    EnvironmentLibrary::Smartphone, EnvironmentLibrary::Laptop, EnvironmentLibrary::BluetoothSpeaker,
    EnvironmentLibrary::Boombox, EnvironmentLibrary::ClubPA, EnvironmentLibrary::MonoCube
};

static bool bufferIsFinite (const juce::AudioBuffer<float>& buf)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const auto* d = buf.getReadPointer (ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            if (! std::isfinite (d[i]))
                return false;
    }
    return true;
}

static float bufferPeak (const juce::AudioBuffer<float>& buf)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        peak = juce::jmax (peak, buf.getMagnitude (ch, 0, buf.getNumSamples()));
    return peak;
}

static void fillSilence (juce::AudioBuffer<float>& buf) { buf.clear(); }

static void fillImpulse (juce::AudioBuffer<float>& buf)
{
    buf.clear();
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        buf.setSample (ch, 0, 1.0f);
}

// Fills a block of a swept sine that continues phase across calls.
static void fillSineSweep (juce::AudioBuffer<float>& buf, double sampleRate, double& phase, double& freqHz)
{
    const int n = buf.getNumSamples();
    for (int i = 0; i < n; ++i)
    {
        const float sample = 0.5f * std::sin (static_cast<float> (phase));
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            buf.setSample (ch, i, sample);
        phase += 2.0 * juce::MathConstants<double>::pi * freqHz / sampleRate;
        if (phase > 2.0 * juce::MathConstants<double>::pi)
            phase -= 2.0 * juce::MathConstants<double>::pi;
        freqHz = juce::jlimit (20.0, 20000.0, freqHz * 1.0005); // slow upward sweep
    }
}

static NovaTranslateParams makeParams (int environmentIndex)
{
    NovaTranslateParams p;
    p.environmentIndex = environmentIndex;
    p.environmentAmount = 100.0f;
    p.roomAmount = 50.0f;
    p.characterAmount = 60.0f;
    p.noiseAmount = 0.0f;
    p.noiseEnabled = false;
    p.levelMatchEnabled = true;
    return p;
}

// ─── 1. Silence ──────────────────────────────────────────────────────────────

static void testSilence()
{
    section ("SILENCE — All 12 environments, sustained silent input");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    for (int envIndex : kImplementedEnvironments)
    {
        NovaTranslateEngine engine;
        engine.prepare (sampleRate, blockSize, 2);
        engine.loadEnvironmentSynchronously (envIndex);

        juce::AudioBuffer<float> buffer (2, blockSize);
        bool allFinite = true;
        for (int i = 0; i < 40; ++i)
        {
            fillSilence (buffer);
            engine.process (buffer, makeParams (envIndex));
            if (! bufferIsFinite (buffer)) allFinite = false;
        }

        const auto& profile = EnvironmentLibrary::getProfile (envIndex);
        std::cout << "  [INFO] " << profile.name << " silence peak after settle: "
                   << bufferPeak (buffer) << "\n";
        check ((profile.name + " stays finite on silence").c_str(), allFinite);
        check ((profile.name + " settles near-silent (peak < 0.01)").c_str(), bufferPeak (buffer) < 0.01f);
    }
}

// ─── 2. Impulse ──────────────────────────────────────────────────────────────

static void testImpulse()
{
    section ("IMPULSE — All 12 environments, single-sample impulse");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    for (int envIndex : kImplementedEnvironments)
    {
        NovaTranslateEngine engine;
        engine.prepare (sampleRate, blockSize, 2);
        engine.loadEnvironmentSynchronously (envIndex);

        juce::AudioBuffer<float> buffer (2, blockSize);
        fillImpulse (buffer);

        bool allFinite = true;
        float maxPeak = 0.0f;
        for (int i = 0; i < 20; ++i)
        {
            engine.process (buffer, makeParams (envIndex));
            if (! bufferIsFinite (buffer)) allFinite = false;
            maxPeak = juce::jmax (maxPeak, bufferPeak (buffer));
            if (i == 0) fillSilence (buffer); // only the very first block carries the impulse
        }

        const auto& profile = EnvironmentLibrary::getProfile (envIndex);
        std::cout << "  [INFO] " << profile.name << " impulse response max peak: " << maxPeak << "\n";
        check ((profile.name + " impulse stays finite").c_str(), allFinite);
        check ((profile.name + " impulse response bounded (< 10.0)").c_str(), maxPeak < 10.0f);
    }
}

// ─── 3. Sine sweep ───────────────────────────────────────────────────────────

static void testSineSweep()
{
    section ("SINE SWEEP — All 12 environments, 20Hz-20kHz sweep");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    for (int envIndex : kImplementedEnvironments)
    {
        NovaTranslateEngine engine;
        engine.prepare (sampleRate, blockSize, 2);
        engine.loadEnvironmentSynchronously (envIndex);

        juce::AudioBuffer<float> buffer (2, blockSize);
        double phase = 0.0, freq = 20.0;

        bool allFinite = true;
        float maxPeak = 0.0f;
        for (int i = 0; i < 400; ++i) // ~4.3s of audio, enough to sweep 20Hz-20kHz
        {
            fillSineSweep (buffer, sampleRate, phase, freq);
            engine.process (buffer, makeParams (envIndex));
            if (! bufferIsFinite (buffer)) allFinite = false;
            maxPeak = juce::jmax (maxPeak, bufferPeak (buffer));
        }

        const auto& profile = EnvironmentLibrary::getProfile (envIndex);
        std::cout << "  [INFO] " << profile.name << " sweep max peak: " << maxPeak << "\n";
        check ((profile.name + " sweep stays finite").c_str(), allFinite);
        check ((profile.name + " sweep output bounded (< 4.0)").c_str(), maxPeak < 4.0f);
    }
}

// ─── 4. Mono / stereo ────────────────────────────────────────────────────────

static void testMonoStereo()
{
    section ("MONO / STEREO — Both channel configurations");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    {
        NovaTranslateEngine engine;
        engine.prepare (sampleRate, blockSize, 2);
        engine.loadEnvironmentSynchronously (EnvironmentLibrary::Sedan);
        juce::AudioBuffer<float> buffer (2, blockSize);
        double phase = 0.0, freq = 440.0;
        bool ok = true;
        for (int i = 0; i < 20; ++i)
        {
            fillSineSweep (buffer, sampleRate, phase, freq);
            engine.process (buffer, makeParams (EnvironmentLibrary::Sedan));
            if (! bufferIsFinite (buffer)) ok = false;
        }
        check ("Stereo (2ch) processes without crashing/NaN", ok);
    }

    {
        NovaTranslateEngine engine;
        bool prepareCrashed = false, processCrashed = false;
        try { engine.prepare (sampleRate, blockSize, 1); }
        catch (...) { prepareCrashed = true; }
        check ("Mono prepare() does not throw", ! prepareCrashed);

        if (! prepareCrashed)
        {
            engine.loadEnvironmentSynchronously (EnvironmentLibrary::Sedan);
            juce::AudioBuffer<float> buffer (1, blockSize);
            double phase = 0.0, freq = 440.0;
            bool ok = true;
            try
            {
                for (int i = 0; i < 20; ++i)
                {
                    fillSineSweep (buffer, sampleRate, phase, freq);
                    engine.process (buffer, makeParams (EnvironmentLibrary::Sedan));
                    if (! bufferIsFinite (buffer)) ok = false;
                }
            }
            catch (...) { processCrashed = true; }
            check ("Mono (1ch) process() does not throw", ! processCrashed);
            if (! processCrashed)
                check ("Mono (1ch) processes without NaN/Inf", ok);
        }
    }
}

// ─── 5. Rapid environment switching ──────────────────────────────────────────

static void testRapidEnvironmentSwitching()
{
    section ("RAPID SWITCHING — Cycling all 12 environments with crossfade");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    NovaTranslateEngine engine;
    engine.prepare (sampleRate, blockSize, 2);

    juce::AudioBuffer<float> buffer (2, blockSize);
    double phase = 0.0, freq = 300.0;

    bool allFinite = true;
    float maxPeak = 0.0f;
    int switchCount = 0;

    for (int block = 0; block < 300; ++block)
    {
        // Force a switch every 5 blocks - far faster than the 150ms crossfade
        // can complete at 512 samples/block @ 48kHz (~14 blocks), so this
        // deliberately stacks overlapping crossfades.
        if (block % 5 == 0)
        {
            const int env = kImplementedEnvironments[(block / 5) % 12];
            engine.loadEnvironmentSynchronously (env);
            ++switchCount;
        }

        fillSineSweep (buffer, sampleRate, phase, freq);
        engine.process (buffer, makeParams (kImplementedEnvironments[(block / 5) % 12]));
        if (! bufferIsFinite (buffer)) allFinite = false;
        maxPeak = juce::jmax (maxPeak, bufferPeak (buffer));
    }

    std::cout << "  [INFO] " << switchCount << " forced environment switches, max peak: " << maxPeak << "\n";
    check ("Rapid switching stays finite throughout", allFinite);
    check ("Rapid switching stays bounded (< 6.0, no runaway crossfade gain)", maxPeak < 6.0f);
}

// ─── 6. Bypass ────────────────────────────────────────────────────────────────

static void testBypass()
{
    section ("BYPASS — Fully transparent passthrough");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    NovaTranslateEngine engine;
    engine.prepare (sampleRate, blockSize, 2);
    engine.loadEnvironmentSynchronously (EnvironmentLibrary::Sedan);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::Random random (12345);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < blockSize; ++i)
            buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (buffer);

    auto p = makeParams (EnvironmentLibrary::Sedan);
    p.bypass = true;

    bool identical = true;
    for (int pass = 0; pass < 5; ++pass)
    {
        engine.process (buffer, p);
        for (int ch = 0; ch < 2 && identical; ++ch)
            for (int i = 0; i < blockSize && identical; ++i)
                if (buffer.getSample (ch, i) != reference.getSample (ch, i))
                    identical = false;
    }
    check ("Bypassed buffer is bit-identical to input across repeated blocks", identical);
}

// ─── 7. NaN / Inf input resilience ───────────────────────────────────────────

static void testNaNInfResilience()
{
    section ("NaN/Inf — Corrupt input sample resilience");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    NovaTranslateEngine engine;
    engine.prepare (sampleRate, blockSize, 2);
    engine.loadEnvironmentSynchronously (EnvironmentLibrary::Sedan);

    juce::AudioBuffer<float> buffer (2, blockSize);
    fillSilence (buffer);
    buffer.setSample (0, 0, std::numeric_limits<float>::quiet_NaN());
    buffer.setSample (1, 0, std::numeric_limits<float>::infinity());

    engine.process (buffer, makeParams (EnvironmentLibrary::Sedan));
    const bool corruptedImmediately = ! bufferIsFinite (buffer);
    std::cout << "  [INFO] Output finite immediately after NaN/Inf input block: " << (! corruptedImmediately ? "yes" : "no") << "\n";

    // Feed clean silence afterward and see whether the engine's internal
    // filter/compressor state recovers, since JUCE's IIR filters have no
    // built-in NaN scrubbing and a NaN in feedback state persists forever.
    bool recovered = false;
    for (int i = 0; i < 50; ++i)
    {
        fillSilence (buffer);
        engine.process (buffer, makeParams (EnvironmentLibrary::Sedan));
        if (bufferIsFinite (buffer)) { recovered = true; break; }
    }

    check ("Engine does not throw/crash on NaN/Inf input", true);
    if (! recovered)
        std::cout << "  [NOTE] Engine state does NOT self-recover from a NaN/Inf input sample - "
                     "the IIR filter feedback state has no NaN guard, so corruption is permanent "
                     "until the next environment reload. A malformed upstream sample could "
                     "silently break metering/output indefinitely.\n";
    check ("Engine recovers to finite output after NaN/Inf input clears", recovered);
}

// ─── 8. Level-match bounds ────────────────────────────────────────────────────

static void testLevelMatchBounds()
{
    section ("LEVEL MATCH — Compensation stays within +/-9dB clamp");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    NovaTranslateEngine engine;
    engine.prepare (sampleRate, blockSize, 2);
    engine.loadEnvironmentSynchronously (EnvironmentLibrary::Smartphone); // aggressive cut/attenuation profile

    juce::AudioBuffer<float> buffer (2, blockSize);
    double phase = 0.0, freq = 1000.0;

    auto p = makeParams (EnvironmentLibrary::Smartphone);
    p.levelMatchEnabled = true;

    bool withinBounds = true;
    float maxAbsCompensation = 0.0f;
    for (int i = 0; i < 500; ++i) // several seconds - long enough for the slow envelopes to converge
    {
        fillSineSweep (buffer, sampleRate, phase, freq);
        freq = 1000.0; // hold a steady tone rather than sweeping, to let level-match settle
        engine.process (buffer, p);
        const auto comp = engine.levelMatchCompensationDb.load();
        maxAbsCompensation = juce::jmax (maxAbsCompensation, std::abs (comp));
        if (std::abs (comp) > 9.0001f) withinBounds = false;
    }

    std::cout << "  [INFO] Max |level-match compensation| observed: " << maxAbsCompensation << " dB\n";
    check ("Level-match compensation never exceeds +/-9dB clamp", withinBounds);
}

// ─── 9. Latency + coarse CPU timing ──────────────────────────────────────────

static void testLatencyAndTiming()
{
    section ("LATENCY / CPU — getLatencySamples() + coarse throughput check");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    for (int envIndex : kImplementedEnvironments)
    {
        NovaTranslateEngine engine;
        engine.prepare (sampleRate, blockSize, 2);
        engine.loadEnvironmentSynchronously (envIndex);

        const auto& profile = EnvironmentLibrary::getProfile (envIndex);
        check ((profile.name + " reports 0 samples latency (unity direct-sound IR tap)").c_str(),
               engine.getLatencySamples() == 0);

        juce::AudioBuffer<float> buffer (2, blockSize);
        double phase = 0.0, freq = 300.0;
        auto p = makeParams (envIndex);

        constexpr int numBlocks = 2000; // ~21.3s of audio at 48kHz/512-sample blocks
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < numBlocks; ++i)
        {
            fillSineSweep (buffer, sampleRate, phase, freq);
            engine.process (buffer, p);
        }
        const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();

        const double audioSeconds = (static_cast<double> (numBlocks) * blockSize) / sampleRate;
        const double realtimeMultiple = audioSeconds / elapsed;

        std::cout << "  [INFO] " << profile.name << ": processed " << audioSeconds << "s of audio in "
                   << elapsed << "s wall time (" << realtimeMultiple << "x real-time on this machine, single-threaded, "
                   << "release-config-dependent - not a substitute for real DAW CPU-meter measurement).\n";
        check ((profile.name + " processes faster than real-time").c_str(), realtimeMultiple > 1.0);
    }
}

// ─── 10. Environment profile dump ────────────────────────────────────────────

static void testEnvironmentProfileDump()
{
    section ("PROFILES — Exact numeric values for all 12 implemented environments");

    for (int envIndex : kImplementedEnvironments)
    {
        const auto& p = EnvironmentLibrary::getProfile (envIndex);
        std::cout << "\n  " << p.name << " (" << p.category << ") - " << p.descriptor << "\n"
                   << "    lowCutHz=" << p.lowCutHz << " highCutHz=" << p.highCutHz << "\n"
                   << "    lowShelfGain=" << p.lowShelfGain << "dB@" << p.lowShelfFreq << "Hz\n"
                   << "    midPeakGain=" << p.midPeakGain << "dB@" << p.midPeakFreq << "Hz Q=" << p.midPeakQ << "\n"
                   << "    highShelfGain=" << p.highShelfGain << "dB@" << p.highShelfFreq << "Hz\n"
                   << "    stereoWidth=" << p.stereoWidth << " stereoOffset=" << p.stereoOffset << "\n"
                   << "    compressionAmount=" << p.compressionAmount << " saturationAmount=" << p.saturationAmount << "\n"
                   << "    roomAmount=" << p.roomAmount << " earlyReflectionAmount=" << p.earlyReflectionAmount
                   << " noiseAmount=" << p.noiseAmount << "\n"
                   << "    supportsPosition=" << (p.supportsPosition ? "true" : "false")
                   << " impulseResponseId=" << p.impulseResponseId << "\n";
        if (p.supportsPosition)
        {
            std::cout << "    positions:";
            for (int i = 0; i < p.numPositions; ++i)
                std::cout << " [" << i << "]" << p.positions[static_cast<size_t> (i)].label
                           << "(bias=" << p.positions[static_cast<size_t> (i)].lateralBias
                           << ",diffuse=" << p.positions[static_cast<size_t> (i)].diffuseAmount << ")";
            std::cout << "\n";
        }
        check ((p.name + " is marked implemented").c_str(), EnvironmentLibrary::isImplemented (envIndex));
    }

    check ("Out-of-roster environment index (e.g. 99) falls back to neutral profile",
           EnvironmentLibrary::getProfile (99).name == "Unbuilt");
    check ("Neutral fallback profile applies zero coloration",
           EnvironmentLibrary::neutralProfile().roomAmount == 0.0f
        && EnvironmentLibrary::neutralProfile().saturationAmount == 0.0f
        && EnvironmentLibrary::neutralProfile().noiseAmount == 0.0f);

    for (int envIndex : kImplementedEnvironments)
    {
        const auto& p = EnvironmentLibrary::getProfile (envIndex);
        check ((p.name + " numPositions within kMaxPositions").c_str(), p.numPositions <= kMaxPositions && p.numPositions >= 0);
        check ((p.name + " supportsPosition/numPositions are consistent").c_str(),
               p.supportsPosition ? (p.numPositions > 0) : (p.numPositions == 0));
    }
}

// ─── 11. Out-of-range position index clamps to defaultPositionIndex ──────────

static void testPositionOutOfRangeClampsToDefault()
{
    section ("POSITION CLAMP — Out-of-range index behaves identically to defaultPositionIndex");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    const auto& sedanProfile = EnvironmentLibrary::getProfile (EnvironmentLibrary::Sedan);

    NovaTranslateEngine engineA, engineB;
    engineA.prepare (sampleRate, blockSize, 2);
    engineB.prepare (sampleRate, blockSize, 2);
    engineA.loadEnvironmentSynchronously (EnvironmentLibrary::Sedan);
    engineB.loadEnvironmentSynchronously (EnvironmentLibrary::Sedan);

    juce::AudioBuffer<float> bufferA (2, blockSize), bufferB (2, blockSize);
    double phaseA = 0.0, freqA = 300.0, phaseB = 0.0, freqB = 300.0;

    auto paramsA = makeParams (EnvironmentLibrary::Sedan);
    paramsA.positionIndex = 99; // out of range for numPositions=4
    auto paramsB = makeParams (EnvironmentLibrary::Sedan);
    paramsB.positionIndex = sedanProfile.defaultPositionIndex; // explicit, in range

    bool allFinite = true, identical = true;
    for (int block = 0; block < 10; ++block)
    {
        fillSineSweep (bufferA, sampleRate, phaseA, freqA);
        fillSineSweep (bufferB, sampleRate, phaseB, freqB);
        engineA.process (bufferA, paramsA);
        engineB.process (bufferB, paramsB);
        if (! bufferIsFinite (bufferA) || ! bufferIsFinite (bufferB)) allFinite = false;
        for (int ch = 0; ch < 2 && identical; ++ch)
            for (int i = 0; i < blockSize && identical; ++i)
                if (bufferA.getSample (ch, i) != bufferB.getSample (ch, i))
                    identical = false;
    }
    check ("Out-of-range positionIndex stays finite", allFinite);
    check ("Out-of-range index (99) is bit-identical to explicit defaultPositionIndex", identical);
}

// ─── 12. Smartphone/Mono Cube truly ignore any stored position value ────────

static void testFixedPositionEnvironmentsIgnoreIndex()
{
    section ("FIXED POSITION — No-position environments' output is identical regardless of positionIndex");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    for (int envIndex : { (int) EnvironmentLibrary::Smartphone, (int) EnvironmentLibrary::MonoCube,
                           (int) EnvironmentLibrary::Laptop, (int) EnvironmentLibrary::BluetoothSpeaker,
                           (int) EnvironmentLibrary::Boombox })
    {
        NovaTranslateEngine engineA, engineB;
        engineA.prepare (sampleRate, blockSize, 2);
        engineB.prepare (sampleRate, blockSize, 2);
        engineA.loadEnvironmentSynchronously (envIndex);
        engineB.loadEnvironmentSynchronously (envIndex);

        juce::AudioBuffer<float> bufferA (2, blockSize), bufferB (2, blockSize);
        double phaseA = 0.0, freqA = 300.0, phaseB = 0.0, freqB = 300.0;

        auto paramsA = makeParams (envIndex);
        paramsA.positionIndex = 0;
        auto paramsB = makeParams (envIndex);
        paramsB.positionIndex = 3; // different, "stale" stored value

        bool identical = true;
        for (int block = 0; block < 10; ++block)
        {
            fillSineSweep (bufferA, sampleRate, phaseA, freqA);
            fillSineSweep (bufferB, sampleRate, phaseB, freqB);
            engineA.process (bufferA, paramsA);
            engineB.process (bufferB, paramsB);
            for (int ch = 0; ch < 2 && identical; ++ch)
                for (int i = 0; i < blockSize && identical; ++i)
                    if (bufferA.getSample (ch, i) != bufferB.getSample (ch, i))
                        identical = false;
        }
        const std::string label = std::string (EnvironmentLibrary::getProfile (envIndex).name) + " ignores stored positionIndex at the DSP level";
        check (label.c_str(), identical);
    }
}

// ─── 13. Position changes are smoothed, not applied as an instant per-block jump ─

static void testPositionChangeIsSmoothed()
{
    section ("POSITION SMOOTHING — Position changes ramp, no click/instant jump");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    NovaTranslateEngine engine;
    engine.prepare (sampleRate, blockSize, 2);
    engine.loadEnvironmentSynchronously (EnvironmentLibrary::Sedan);

    juce::AudioBuffer<float> buffer (2, blockSize);
    double phase = 0.0, freq = 1000.0;

    auto params = makeParams (EnvironmentLibrary::Sedan);
    params.positionIndex = 1; // Front Passenger: target bias = (0.15+0.15) = +0.3

    // Let the position bias fully settle (well beyond the ~40ms/~1920-sample
    // smoothing time constant at 48kHz).
    for (int block = 0; block < 10; ++block)
    {
        fillSineSweep (buffer, sampleRate, phase, freq);
        engine.process (buffer, params);
    }

    // Baseline: how much the L-R difference naturally moves sample-to-sample
    // in this settled, position-unchanging block (driven by the sweeping
    // tone itself, not by any position change).
    float baselineMaxStep = 0.0f;
    float prevDiff = buffer.getSample (0, 0) - buffer.getSample (1, 0);
    for (int i = 1; i < blockSize; ++i)
    {
        const float diff = buffer.getSample (0, i) - buffer.getSample (1, i);
        baselineMaxStep = juce::jmax (baselineMaxStep, std::abs (diff - prevDiff));
        prevDiff = diff;
    }
    const float lastDiffBeforeSwitch = prevDiff;

    // Abruptly change position and process one more block - if unsmoothed,
    // the L-R difference would jump to the new target within a single
    // sample right at the block boundary.
    params.positionIndex = 2; // Rear Left: target bias = (0.15-0.12)*(1-0.4*0.5) = +0.024 - a large change from +0.3
    fillSineSweep (buffer, sampleRate, phase, freq);
    engine.process (buffer, params);
    const float firstDiffAfterSwitch = buffer.getSample (0, 0) - buffer.getSample (1, 0);
    const float boundaryStep = std::abs (firstDiffAfterSwitch - lastDiffBeforeSwitch);

    std::cout << "  [INFO] Baseline (steady-state) max per-sample L-R step: " << baselineMaxStep << "\n";
    std::cout << "  [INFO] Step at the position-change boundary: " << boundaryStep << "\n";
    check ("Position-change boundary step is not a large discontinuity relative to steady-state signal movement",
           boundaryStep < baselineMaxStep * 5.0f + 0.001f);
}

// ─── main ────────────────────────────────────────────────────────────────────

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::ScopedNoDenormals noDenormals;

    std::cout << "\n╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║       NOVA TRANSLATE — CORE DSP VALIDATION            ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n";

    testSilence();
    testImpulse();
    testSineSweep();
    testMonoStereo();
    testRapidEnvironmentSwitching();
    testBypass();
    testNaNInfResilience();
    testLevelMatchBounds();
    testLatencyAndTiming();
    testEnvironmentProfileDump();
    testPositionOutOfRangeClampsToDefault();
    testFixedPositionEnvironmentsIgnoreIndex();
    testPositionChangeIsSmoothed();

    std::cout << "\n══════════════════════════════════════════════════════\n";
    std::cout << "  Results: " << passed << " passed, " << failed << " failed\n";
    std::cout << "══════════════════════════════════════════════════════\n\n";

    return failed > 0 ? 1 : 0;
}
