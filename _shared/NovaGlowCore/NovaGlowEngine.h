#pragma once

#include "DSP/NovaOpticalDetector.h"
#include "DSP/NovaOpticalGainCell.h"
#include "DSP/NovaOversampledInputStage.h"
#include "DSP/NovaOversampledOutputStage.h"
#include "DSP/NovaGlowMacro.h"
#include "DSP/NovaAnalogModeProfile.h"
#include "DSP/NovaGlowNoise.h"
#include "Meter/NovaVUBallistics.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace nova::glow
{

// Top-level facade - the only class PluginProcessor talks to for audio.
// Signal flow (matching the spec exactly):
//   Input -> Input Transformer + Tube Gain Stage (oversampled, one wrapper)
//   -> per-sample: optical detector (SC HPF + Emphasis, Stereo-Link-blended
//   magnitude, Compress/Limit knee) -> optical gain cell (program-dependent
//   attack/release/memory) -> gain multiply -> Tube Output Stage + Output
//   Transformer (oversampled, one wrapper) -> Analog Noise -> equal-power
//   Mix (phase-aligned against a delay-compensated dry tap) -> Bypass
//   crossfade.
//
// Stereo-only by design (Stereo Link needs a channel pair); PluginProcessor
// adapts a mono host bus the same way Royal 670 does (duplicate to a
// stereo scratch buffer, average back down on output).
class NovaGlowEngine
{
public:
    NovaGlowEngine() = default;

    void prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages) noexcept;
    void reset() noexcept;

    // All raw values as they arrive from APVTS (percent/dB/choice as
    // documented per-field); call once per block from processBlock.
    void setParams (float peakReductionPercent, float gainPercent,
                     int compressLimitChoice, float scHpfHz, float emphasisPercent,
                     float glowPercent, int analogModeChoice,
                     float mixPercent, float stereoLinkPercent, float noisePercent,
                     bool bypassed) noexcept;

    // In-place stereo processing (buffer must have exactly 2 channels).
    void processBlock (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept;
    int getPreparedOversamplingStages() const noexcept { return preparedOversamplingStages; }

    // Metering, read by PluginProcessor once per block for UI telemetry -
    // all three run continuously; meterMode (UI-only, doesn't affect audio)
    // just selects which one feeds the single needle. GR is fed raw dB
    // values (already in dB, not linear amplitude), so its ballistics-
    // smoothed reading comes from getDisplayLevel() directly (the plain
    // smoothed input) rather than getDisplayLevelVu(), which assumes a
    // linear-amplitude input and would double-convert an already-dB value.
    // OUTPUT/INPUT are fed linear audio amplitude, so getDisplayLevelVu()
    // (dB relative to the -18dBFS=0VU reference) is the correct reading.
    float getGrDisplayDb() const noexcept { return vuGr.getDisplayLevel(); }
    float getOutputDisplayVu() const noexcept { return vuOutput.getDisplayLevelVu(); }
    float getInputDisplayVu() const noexcept { return vuInput.getDisplayLevelVu(); }
    float getGainReductionDb() const noexcept { return lastGainReductionDb; }

private:
    static constexpr float kSmoothSeconds = 0.02f;
    static constexpr float kBypassSmoothSeconds = 0.03f;
    static constexpr float kBlockSmoothSeconds = 0.03f;

    // Peak Reduction 0-100 -> threshold dB, shaped (not linear) so the
    // lower third of rotation reads as ~1-3dB gentle leveling, the middle
    // as ~3-7dB classic, the top as ~7-12+dB obvious optical compression.
    // 0.7 (was 1.6): at 1.6 the threshold stayed so high that 50 % gave ~0 dB of
    // gain reduction on a normal vocal and the GR meter looked dead. 0.7 gives
    // ~3 dB average / 5-7 dB peaks at 50 %; 0 % and 100 % are unchanged.
    static constexpr float kPeakReductionShapeExp = 0.7f;
    static constexpr float kThresholdDbAtZero = 6.0f;
    static constexpr float kThresholdDbAtMax = -42.0f;

    // Output tube/transformer drive scale from the GAIN control (not pure
    // makeup gain - it also drives the output stage).
    static constexpr float kGainOutputDriveMax = 0.7f;
    static constexpr float kGainTrimDbMax = 24.0f;
    static constexpr float kGainTrimDbMin = -24.0f;

    double sampleRate = 44100.0;
    int preparedOversamplingStages = 1;

    NovaOversampledInputStage inputStage;
    NovaOversampledOutputStage outputStage;

    NovaOpticalDetector detectorL, detectorR;
    NovaOpticalGainCell gainCellL, gainCellR;

    NovaGlowNoise noise;
    NovaVUBallistics vuGr, vuOutput, vuInput;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        mixSmoothed, stereoLinkSmoothed, bypassSmoothed, outputTrimGainSmoothed;

    // Block-rate one-pole followers toward target values - same idiom as
    // NovaRoyal670Engine's hrmDriveSmoothed01 etc: these feed
    // whole-block-driven stages (tube/transformer drive, analog-mode
    // profile fields, compress/limit blend) that can't be true per-sample
    // SmoothedValue, so a sudden knob/choice change doesn't produce a
    // single-block step.
    float thresholdDbTarget = -18.0f, thresholdDbSmoothed = -18.0f;
    float gainPercentTarget = 50.0f, gainPercentSmoothed = 50.0f;
    float compressLimitTarget01 = 0.0f, compressLimitSmoothed01 = 0.0f;
    float glowTarget01 = 0.45f, glowSmoothed01 = 0.45f;
    NovaAnalogModeProfile profileTarget, profileSmoothed;

    float noisePercentTarget = 0.0f;

    // Not smoothed at the engine level, matching NovaRoyal670Engine's
    // acThresholdDb/sidechainHpfHz pattern - the detector's own pole
    // followers/filter coefficients already ramp smoothly, so double-
    // smoothing here would only add latency without benefit.
    float scHpfHz = 30.0f;
    float emphasisPercent = 0.0f;

    float lastGainReductionDb = 0.0f;

    // Pre-allocated dry-tap delay line, sized to total oversampling latency
    // (input stage + output stage), so MIX stays phase-aligned - resized
    // only in prepare()/reconfigure, never inside the per-sample loop.
    // delayedDryBuffer is scratch (pre-allocated, never reallocated in
    // processBlock) holding this block's delay-compensated dry signal,
    // filled in the same per-sample pass as the push so JUCE's DelayLine
    // push/pop stays in its intended per-sample-pair usage shape.
    juce::dsp::DelayLine<float> dryDelayLine { 8192 };
    juce::AudioBuffer<float> delayedDryBuffer;

    // Mono scratch (max of L/R per sample) feeding the three VU ballistics
    // instances with real sample-accurate telemetry - pre-allocated, never
    // reallocated in processBlock().
    juce::AudioBuffer<float> grMonoScratch, outputMonoScratch, inputMonoScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaGlowEngine)
};

} // namespace nova::glow
