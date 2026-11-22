// Headless validation harness for NovaBassRenderer. Renders a known
// synthetic preset and asserts: correct sample rate/duration, leading
// silence trimmed without chopping the transient, peak level lands at
// the declared headroom target, and the written WAV file round-trips
// through a real AudioFormatManager read.

#include "Render/NovaBassRenderer.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <cstdio>

using namespace nova::bass;
using namespace nova::basslab;

static int failures = 0;

static void check (bool condition, const char* description)
{
    if (condition) std::printf ("  PASS: %s\n", description);
    else { std::printf ("  FAIL: %s\n", description); ++failures; }
}

static NovaBassPreset makeFastAttackPreset()
{
    NovaBassPreset p;
    p.meta.name = "Render Test 808";
    p.meta.category = "808";
    p.sourceType = NovaBassSourceType::Synth;
    p.voiceParams.pitchEnvAmountSemitones = 10.0f;
    p.voiceParams.pitchEnvDecaySeconds = 0.1f;
    p.voiceParams.ampAttackSeconds = 0.0005f; // fast attack - trim should barely touch it
    p.voiceParams.ampDecaySeconds = 0.3f;
    p.voiceParams.ampSustain = 0.5f;
    p.voiceParams.ampReleaseSeconds = 0.1f;
    p.voiceParams.transientAmount = 0.3f;
    p.colorMode = NovaColorMode::Warm;
    p.colorDrive = 0.3f;
    p.stereoParams.widthAmount = 0.5f;
    return p;
}

static float peakDb (const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));
    return juce::Decibels::gainToDecibels (peak);
}

int main()
{
    std::printf ("=== NovaBassRenderer Validation ===\n");

    NovaBassEngine headlessEngine;
    auto preset = makeFastAttackPreset();

    NovaBassRenderOptions options;
    options.rootNote = 36;
    options.velocity = 1.0f;
    options.durationSeconds = 0.3f;
    options.tailSeconds = 0.5f;
    options.sampleRate = 48000.0;
    options.bitDepth = 24;
    options.headroomTargetDb = -1.0f;

    std::printf ("-- Render output --\n");
    auto result = NovaBassRenderer::renderToBuffer (headlessEngine, preset, options);

    check (result.success, "render succeeds");
    check (result.errorMessage.isEmpty(), "no error message on success");

    const int expectedTotalSamples = (int) std::round ((options.durationSeconds + options.tailSeconds) * options.sampleRate);
    const int actualLength = result.buffer.getNumSamples() + result.trimmedLeadingSamples;
    check (std::abs (actualLength - expectedTotalSamples) <= 2, "pre-trim render length matches duration+tail at the target sample rate");

    const double trimmedMs = (double) result.trimmedLeadingSamples / options.sampleRate * 1000.0;
    std::printf ("     trimmed leading samples: %d (%.2fms)\n", result.trimmedLeadingSamples, trimmedMs);
    check (trimmedMs < 10.0, "trim removes only a small amount of leading silence for a fast-attack preset (transient not chopped)");

    const float measuredPeakDb = peakDb (result.buffer);
    std::printf ("     measured peak: %.3f dBFS (target %.3f)\n", measuredPeakDb, options.headroomTargetDb);
    check (std::abs (measuredPeakDb - options.headroomTargetDb) < 0.05f, "output peak level lands at the declared headroom target");

    std::printf ("-- WAV file round-trip --\n");
    auto tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("NovaBassLabRendererValidation_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)) + ".wav");

    const bool wroteOk = NovaBassRenderer::writeToWavFile (result, tempFile, options.bitDepth);
    check (wroteOk, "writeToWavFile succeeds");
    check (tempFile.existsAsFile(), "the WAV file actually exists on disk after writing");

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (tempFile));

    check (reader != nullptr, "the written file is readable by a real AudioFormatReader");
    if (reader != nullptr)
    {
        check (std::abs (reader->sampleRate - options.sampleRate) < 0.5, "read-back sample rate matches the render options");
        check (reader->numChannels == 2, "read-back channel count is stereo");
        check (reader->bitsPerSample == (unsigned int) options.bitDepth, "read-back bit depth matches the render options");
        check ((int) reader->lengthInSamples == result.buffer.getNumSamples(), "read-back sample length matches the trimmed render buffer");
    }

    tempFile.deleteFile();

    std::printf ("-- Deterministic asset naming --\n");
    const auto fileName = NovaBassRenderer::makeAssetFileName ("Hard 808", "Dark Knock", 24 /* C1 */, 1);
    std::printf ("     %s\n", fileName.toRawUTF8());
    check (fileName == "NVB_hard_808_dark_knock_C1_v001.wav", "makeAssetFileName produces the expected deterministic slug/version format");

    std::printf ("=== %s (%d failure%s) ===\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
