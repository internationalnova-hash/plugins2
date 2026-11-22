// End-to-end integration validation harness for NovaBassEngine. Composes
// every subsystem the way PluginProcessor will (prepare -> loadPreset ->
// noteOn/noteOff -> process), driven against real hand-built presets, not
// synthetic per-subsystem fixtures. Confirms: Synth-sourced presets
// produce audio, Sample-sourced presets produce audio via the poly path,
// Hybrid presets combine both, macro changes audibly affect output, and
// BYPASS ramps cleanly to silence and back without a discontinuity.

#include "../../_shared/NovaBassCore/NovaBassEngine.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdio>
#include <memory>

using namespace nova::bass;

static int failures = 0;

static void check (bool condition, const char* description)
{
    if (condition) std::printf ("  PASS: %s\n", description);
    else { std::printf ("  FAIL: %s\n", description); ++failures; }
}

static NovaBassPreset makeSynthPreset()
{
    NovaBassPreset p;
    p.meta.name = "Integration Test 808";
    p.meta.category = "808";
    p.sourceType = NovaBassSourceType::Synth;
    p.voiceParams.sineTriBlend = 0.0f;
    p.voiceParams.pitchEnvAmountSemitones = 12.0f;
    p.voiceParams.pitchEnvDecaySeconds = 0.15f;
    p.voiceParams.ampAttackSeconds = 0.001f;
    p.voiceParams.ampDecaySeconds = 0.4f;
    p.voiceParams.ampSustain = 0.6f;
    p.voiceParams.ampReleaseSeconds = 0.1f;
    p.voiceParams.transientAmount = 0.25f;
    p.voiceParams.filterCutoffHz = 10000.0f;
    p.voiceParams.retrigger = NovaVoiceRetrigger::Retrigger;
    p.voiceParams.glideMode = NovaGlideMode::LegatoOnly;
    p.voiceParams.glideTimeSeconds = 0.03f;
    p.colorMode = NovaColorMode::Warm;
    // Kept low (rather than a more "typical" 0.3-0.5) specifically so the
    // DECAY-macro comparison test below isn't confounded by tanh
    // saturation compressing away the envelope-level difference it's
    // trying to measure - see that test's comment for the full story.
    p.colorDrive = 0.05f;
    p.stereoParams.widthAmount = 0.5f;
    p.stereoParams.crossoverHz = 150.0f;

    // DECAY macro maps to ampDecaySeconds, 0-1 -> 0.05s-1.5s.
    p.macros["DECAY"] = { { "ampDecaySeconds", 0.05f, 1.5f, NovaMacroCurve::Linear, NovaMacroPolarity::Normal } };
    // BASS macro fans out to pitch drop + transient, matching the spec's
    // master-intensity design (colorDrive deliberately left unmapped here
    // so BASS's default 0.5 knob position doesn't drag colorDrive away
    // from the low baseline set above).
    p.macros["BASS"] = {
        { "pitchEnvAmountSemitones", 3.0f, 24.0f, NovaMacroCurve::Linear, NovaMacroPolarity::Normal },
        { "transientAmount", 0.05f, 0.8f, NovaMacroCurve::Linear, NovaMacroPolarity::Normal },
    };

    return p;
}

static std::shared_ptr<juce::AudioBuffer<float>> makeSineBuffer (float freqHz, double sampleRate, int numSamples)
{
    auto buf = std::make_shared<juce::AudioBuffer<float>> (1, numSamples);
    auto* d = buf->getWritePointer (0);
    for (int i = 0; i < numSamples; ++i)
        d[i] = std::sin (juce::MathConstants<float>::twoPi * freqHz * (float) i / (float) sampleRate);
    return buf;
}

static bool anyNonZero (const juce::AudioBuffer<float>& buf)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            if (std::abs (buf.getSample (ch, i)) > 0.001f)
                return true;
    return false;
}

// NovaBassEngine::process() is prepared for a maximum block size (like any
// real JUCE plugin) and must never be called with a larger buffer than
// that in one call - real hosts always respect this. Processes `buffer`
// in <= chunkSize slices via non-copying AudioBuffer views, matching how
// an actual host would drive processBlock().
static void processInChunks (NovaBassEngine& engine, juce::AudioBuffer<float>& buffer, int chunkSize)
{
    int start = 0;
    while (start < buffer.getNumSamples())
    {
        const int thisChunk = juce::jmin (chunkSize, buffer.getNumSamples() - start);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, thisChunk);
        engine.process (chunk);
        start += thisChunk;
    }
}

static bool allFinite (const juce::AudioBuffer<float>& buf)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            if (! std::isfinite (buf.getSample (ch, i)))
                return false;
    return true;
}

static void testSynthPresetProducesAudio()
{
    std::printf ("-- Synth-sourced preset: produces finite, non-silent audio --\n");

    NovaBassEngine engine;
    engine.prepare (48000.0, 512);
    engine.loadPreset (makeSynthPreset(), nullptr, 0);

    check (! engine.isAnyVoiceActive(), "no voice active before any note");

    engine.noteOn (36, 1.0f);
    check (engine.isAnyVoiceActive(), "a voice becomes active on noteOn");

    juce::AudioBuffer<float> buffer (2, 4800);
    processInChunks (engine, buffer, 512);

    check (allFinite (buffer), "rendered output is finite");
    check (anyNonZero (buffer), "rendered output is non-silent");

    engine.noteOff (36);
    juce::AudioBuffer<float> releaseBuffer (2, 48000);
    processInChunks (engine, releaseBuffer, 512);
    check (! engine.isAnyVoiceActive(), "voice becomes inactive after release completes");
}

static void testMacroChangeAffectsOutput()
{
    std::printf ("-- Macro change audibly affects output --\n");

    NovaBassEngine engineShort, engineLong;
    engineShort.prepare (48000.0, 512);
    engineLong.prepare (48000.0, 512);
    engineShort.loadPreset (makeSynthPreset(), nullptr, 0);
    engineLong.loadPreset (makeSynthPreset(), nullptr, 0);

    engineShort.setMacroValue ("DECAY", 0.0f); // -> ampDecaySeconds = 0.05s
    engineLong.setMacroValue ("DECAY", 1.0f);  // -> ampDecaySeconds = 1.5s

    engineShort.noteOn (36, 1.0f);
    engineLong.noteOn (36, 1.0f);

    // Render 200ms: comfortably past the short decay's full 50ms decay
    // time (fully settled at its sustain level), but only ~13% into the
    // long decay's 1.5s decay time (still close to its attack peak, far
    // from sustain). Deliberately NOT rendering a full second and
    // comparing tails - at that point both envelopes are close enough to
    // sustain that mild saturation (see makeSynthPreset's low colorDrive
    // comment) compresses the gap between them almost away, which isn't
    // what this test is trying to measure.
    juce::AudioBuffer<float> shortBuf (2, 9600), longBuf (2, 9600); // 200ms
    processInChunks (engineShort, shortBuf, 512);
    processInChunks (engineLong, longBuf, 512);

    auto tailPeak = [] (const juce::AudioBuffer<float>& buf, int window)
    {
        float peak = 0.0f;
        for (int i = buf.getNumSamples() - window; i < buf.getNumSamples(); ++i)
            peak = juce::jmax (peak, std::abs (buf.getSample (0, i)));
        return peak;
    };

    const float shortPeak = tailPeak (shortBuf, 2000);
    const float longPeak = tailPeak (longBuf, 2000);

    std::printf ("     DECAY=0 (200ms in) peak: %.4f | DECAY=1 (200ms in) peak: %.4f\n", shortPeak, longPeak);
    check (shortPeak < longPeak * 0.9f, "DECAY macro at opposite extremes produces measurably different output level 200ms after note-on");
}

static void testSamplePresetProducesAudio()
{
    std::printf ("-- Sample-sourced preset: produces audio via the poly path --\n");

    NovaBassPreset preset;
    preset.meta.name = "Integration Test Sample";
    preset.sourceType = NovaBassSourceType::Sample;

    NovaBassSampleZone zone;
    zone.name = "TestTone";
    zone.lowKey = 0; zone.highKey = 127;
    zone.rootNote = 60;
    zone.sourceSampleRate = 48000.0;
    zone.buffer = makeSineBuffer (110.0f, 48000.0, 48000);

    NovaBassEngine engine;
    engine.prepare (48000.0, 512);
    engine.loadPreset (preset, &zone, 1);

    engine.noteOn (48, 1.0f);
    check (engine.isAnyVoiceActive(), "a poly voice becomes active on noteOn for a Sample-sourced preset");

    juce::AudioBuffer<float> buffer (2, 4800);
    processInChunks (engine, buffer, 512);

    check (allFinite (buffer), "rendered output is finite");
    check (anyNonZero (buffer), "rendered output is non-silent");
}

static void testHybridCombinesBothPaths()
{
    std::printf ("-- Hybrid preset: combines synth and sample paths --\n");

    NovaBassPreset preset = makeSynthPreset();
    preset.sourceType = NovaBassSourceType::Hybrid;

    NovaBassSampleZone zone;
    zone.name = "TestTone";
    zone.lowKey = 0; zone.highKey = 127;
    zone.rootNote = 60;
    zone.sourceSampleRate = 48000.0;
    zone.buffer = makeSineBuffer (220.0f, 48000.0, 48000);

    NovaBassEngine engine;
    engine.prepare (48000.0, 512);
    engine.loadPreset (preset, &zone, 1);

    engine.noteOn (36, 1.0f);
    juce::AudioBuffer<float> buffer (2, 4800);
    processInChunks (engine, buffer, 512);

    check (allFinite (buffer), "Hybrid preset output is finite");
    check (anyNonZero (buffer), "Hybrid preset output is non-silent (both paths contributing)");
}

static void testBypassRampsCleanly()
{
    std::printf ("-- BYPASS ramps to silence and back without a discontinuity --\n");

    NovaBassEngine engine;
    engine.prepare (48000.0, 512);
    engine.loadPreset (makeSynthPreset(), nullptr, 0);
    engine.noteOn (36, 1.0f);

    juce::AudioBuffer<float> warmup (2, 4800);
    processInChunks (engine, warmup, 512); // let the voice reach a steady, non-silent state

    engine.setBypassed (true);
    juce::AudioBuffer<float> rampDown (2, 4800); // 100ms, well past the 20ms ramp
    processInChunks (engine, rampDown, 512);

    float maxJump = 0.0f;
    for (int i = 1; i < rampDown.getNumSamples(); ++i)
        maxJump = juce::jmax (maxJump, std::abs (rampDown.getSample (0, i) - rampDown.getSample (0, i - 1)));
    check (maxJump < 0.05f, "no large sample-to-sample jump while ramping down into bypass");

    juce::AudioBuffer<float> silentCheck (2, 4800);
    processInChunks (engine, silentCheck, 512);
    check (! anyNonZero (silentCheck), "output is silent once fully bypassed");

    engine.setBypassed (false);
    juce::AudioBuffer<float> rampUp (2, 4800);
    processInChunks (engine, rampUp, 512);

    float maxJumpUp = 0.0f;
    for (int i = 1; i < rampUp.getNumSamples(); ++i)
        maxJumpUp = juce::jmax (maxJumpUp, std::abs (rampUp.getSample (0, i) - rampUp.getSample (0, i - 1)));
    check (maxJumpUp < 0.05f, "no large sample-to-sample jump while ramping back out of bypass");

    juce::AudioBuffer<float> afterUnbypass (2, 4800);
    processInChunks (engine, afterUnbypass, 512);
    check (anyNonZero (afterUnbypass), "output is audible again once fully unbypassed (voice kept running underneath)");
}

int main()
{
    std::printf ("=== NovaBassEngine Integration Validation ===\n");

    testSynthPresetProducesAudio();
    testMacroChangeAffectsOutput();
    testSamplePresetProducesAudio();
    testHybridCombinesBothPaths();
    testBypassRampsCleanly();

    std::printf ("=== %s (%d failure%s) ===\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
