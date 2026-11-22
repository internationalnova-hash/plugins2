#pragma once

#include "DSP/NovaVelvetCalibration.h"
#include "DSP/NovaVelvetDetector.h"
#include "DSP/NovaVelvetOpticalCell.h"
#include "DSP/NovaVelvetOversampledInputStage.h"
#include "DSP/NovaVelvetOversampledOutputStage.h"
#include "DSP/NovaVelvetAnalogModeProfile.h"
#include "DSP/NovaVelvetLevelEnvelope.h"
#include "Meter/NovaVUBallistics.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace nova::velvet
{

// Top-level facade - the only class PluginProcessor talks to for audio.
// Signal flow (matching the spec's conceptual architecture exactly):
//   Input -> Input Transformer (oversampled) -> per-sample: Stereo-Link-
//   blended detector -> optical cell (Time Select: Fixed/FixMan/Manual) ->
//   gain multiply -> Gain (makeup, partially coupled into Tube Drive) ->
//   Tube Push-Pull Amplifier + Tube-Drive makeup + Output Transformer
//   (oversampled, one wrapper) -> equal-power Mix (phase-aligned against a
//   delay-compensated dry tap) -> Output Trim -> VU telemetry.
//
// Stereo-only by design (Stereo Link needs a channel pair); PluginProcessor
// adapts a mono host bus the same way this catalog's other stereo-only
// engines do (duplicate to a stereo scratch buffer, average back down on
// output).
class NovaVelvetEngine
{
public:
    NovaVelvetEngine() = default;

    void prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages) noexcept;
    void reset() noexcept;

    // All raw values as they arrive from APVTS; call once per block from
    // processBlock. timeSelectChoice: 0=Fixed, 1=FixMan, 2=Manual.
    // analogModeChoice: 0=Clean, 1=Tube, 2=Vintage.
    void setParams (float thresholdDbu, float ratio, float attackMs, float releaseMs,
                     float gainDb, int timeSelectChoice, float scHpfHz,
                     float mixPercent, float outputDb, bool stereoLinkOn,
                     int analogModeChoice, float tubeDrivePercent,
                     bool bypassed) noexcept;

    // In-place stereo processing (buffer must have exactly 2 channels).
    void processBlock (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept;
    int getPreparedOversamplingStages() const noexcept { return preparedOversamplingStages; }

    // Metering, read by PluginProcessor once per block for UI telemetry.
    // GR is fed already-in-dB values, so its ballistics-smoothed reading
    // comes from getDisplayLevel() directly; Output/Input are fed linear
    // audio amplitude, so getDisplayLevelVu() (dB relative to the
    // -18dBFS=0VU reference) is the correct reading - same split as this
    // catalog's other VU-metered engines (see NovaGlowEngine).
    float getGrDisplayDb() const noexcept { return vuGr.getDisplayLevel(); }
    float getOutputDisplayVu() const noexcept { return vuOutput.getDisplayLevelVu(); }
    float getInputDisplayVu() const noexcept { return vuInput.getDisplayLevelVu(); }
    float getGainReductionDb() const noexcept { return lastGainReductionDb; }

private:
    double sampleRate = 44100.0;
    int preparedOversamplingStages = 1;

    NovaVelvetOversampledInputStage inputStage;
    NovaVelvetOversampledOutputStage outputStage;

    NovaVelvetDetector detectorL, detectorR;
    NovaVelvetOpticalCell cellL, cellR;

    NovaVelvetLevelEnvelope inputLevelEnvelope, outputLevelEnvelope;

    NovaVUBallistics vuGr, vuOutput, vuInput;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        mixSmoothed, stereoLinkSmoothed, bypassSmoothed, outputTrimGainSmoothed;

    // Block-rate one-pole followers toward target values (same idiom as
    // this catalog's other engines) for values that drive whole-block
    // stages or discrete choices needing continuous blending for
    // click-free switching.
    float thresholdDbuTarget = -20.0f, thresholdDbuSmoothed = -20.0f;
    float ratioTarget = 4.0f, ratioSmoothed = 4.0f;
    float attackSecondsTarget = 0.010f, attackSecondsSmoothed = 0.010f;
    float releaseSecondsTarget = 0.300f, releaseSecondsSmoothed = 0.300f;
    float gainDbTarget = 6.0f, gainDbSmoothed = 6.0f;
    float tubeDrivePercentTarget = 25.0f, tubeDrivePercentSmoothed = 25.0f;
    NovaVelvetAnalogModeProfile profileTarget, profileSmoothed;

    NovaVelvetOpticalCell::TimeSelectMode timeSelectMode = NovaVelvetOpticalCell::TimeSelectMode::FixMan;
    float scHpfHz = 20.0f;
    float outputDbTarget = 0.0f;

    float lastGainReductionDb = 0.0f;

    // Pre-allocated dry-tap delay line, sized to total oversampling latency
    // (input stage + output stage), so MIX stays phase-aligned - resized
    // only in prepare(), never inside the per-sample loop.
    juce::dsp::DelayLine<float> dryDelayLine { 8192 };
    juce::AudioBuffer<float> delayedDryBuffer;

    // Mono scratch (max of L/R per sample) feeding the three VU ballistics
    // instances - pre-allocated, never reallocated in processBlock().
    juce::AudioBuffer<float> grMonoScratch, outputMonoScratch, inputMonoScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVelvetEngine)
};

} // namespace nova::velvet
