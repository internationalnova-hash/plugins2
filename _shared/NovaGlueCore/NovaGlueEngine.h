#pragma once

#include "DSP/NovaGlueDetector.h"
#include "DSP/NovaOversampledConsoleStage.h"
#include "DSP/NovaGlueMacro.h"
#include "DSP/NovaGlueAutoGain.h"
#include "Meter/NovaVUBallistics.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace nova::glue
{

// Top-level facade - the only class PluginProcessor talks to for audio.
// Signal flow (matching the spec's own block order):
//   Input -> Console input stage (oversampled)
//   -> per-sample: NovaGlueDetector (SC HPF + energy magnitude,
//      Stereo-Link-blended, Attack/Release envelope incl. AUTO, Ratio+Knee)
//   -> VCA gain multiply
//   -> Console output stage (oversampled; DRIVE lives here)
//   -> Auto Gain (slow) + Makeup trim
//   -> equal-power Mix (phase-aligned against a delay-compensated dry tap)
//   -> Bypass crossfade (Compressor IN/BYPASS)
//   -> GR VU ballistics (raw dB, fed continuously)
//
// Stereo-only by design (Stereo Link needs a channel pair); PluginProcessor
// adapts a mono host bus the same way Glow/Royal 670 do (duplicate to a
// stereo scratch buffer, average back down on output).
class NovaGlueEngine
{
public:
    NovaGlueEngine() = default;

    void prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages) noexcept;
    void reset() noexcept;

    void setParams (float thresholdDb, int attackChoice, int releaseChoice, float makeupDb,
                     int ratioChoice, float scHpfHz, float mixPercent, float gluePercent,
                     float drivePercent, bool autoGainEnabled, bool stereoLinkEnabled,
                     bool bypassed) noexcept;

    // In-place stereo processing (buffer must have exactly 2 channels).
    void processBlock (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept;
    int getPreparedOversamplingStages() const noexcept { return preparedOversamplingStages; }

    // GR is already in dB, so its ballistics-smoothed reading comes from
    // getDisplayLevel() (the plain smoothed input) rather than
    // getDisplayLevelVu(), which assumes a linear-amplitude input and would
    // double-convert an already-dB value - the exact bug found and fixed on
    // Nova Glow, avoided here from the start.
    float getGrDisplayDb() const noexcept { return vuGr.getDisplayLevel(); }
    float getGainReductionDb() const noexcept { return lastGainReductionDb; }

private:
    static constexpr float kSmoothSeconds = 0.02f;
    static constexpr float kBypassSmoothSeconds = 0.03f;
    static constexpr float kBlockSmoothSeconds = 0.03f;

    // Drive 0-100 -> a capped drive01 range fed to both console stages,
    // plus a centralized, monotonic loudness-compensation curve so "more
    // Drive" isn't perceived as "better" purely from being louder.
    static constexpr float kDriveCompensationDbPerUnit = -2.0f; // at drive01=1

    double sampleRate = 44100.0;
    int preparedOversamplingStages = 1;

    NovaOversampledConsoleStage inputStage, outputStage;
    NovaGlueDetector detectorL, detectorR;
    NovaGlueAutoGain autoGain;
    NovaVUBallistics vuGr;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        mixSmoothed, stereoLinkSmoothed, bypassSmoothed, makeupGainSmoothed;

    // Block-rate one-pole followers toward target values for parameters
    // that drive whole-block stages (console drive) or need continuous
    // blending for click-free switching across discrete choices - same
    // idiom as NovaGlowEngine's thresholdDbSmoothed etc.
    float thresholdDbTarget = -10.0f, thresholdDbSmoothed = -10.0f;
    float driveTarget01 = 0.0f, driveSmoothed01 = 0.0f;
    float glueTarget01 = 0.45f, glueSmoothed01 = 0.45f;

    int attackChoice = 4;
    int releaseChoice = 4;
    int ratioChoice = 0;
    float scHpfHz = 20.0f;
    bool autoGainEnabled = true;

    float lastGainReductionDb = 0.0f;

    // Pre-allocated dry-tap delay line, sized to total oversampling
    // latency, so Mix stays phase-aligned - resized only in
    // prepare()/reconfigure, never inside the per-sample loop.
    juce::dsp::DelayLine<float> dryDelayLine { 8192 };
    juce::AudioBuffer<float> delayedDryBuffer;

    // Mono scratch feeding the GR VU ballistics with real sample-accurate
    // telemetry - pre-allocated, never reallocated in processBlock().
    juce::AudioBuffer<float> grMonoScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaGlueEngine)
};

} // namespace nova::glue
