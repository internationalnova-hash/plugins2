#pragma once

#include "DSP/NovaVariableMuDetector.h"
#include "DSP/NovaOversampledInputStage.h"
#include "DSP/NovaOversampledOutputStage.h"
#include "DSP/NovaWeightMacro.h"
#include "Stereo/NovaStereoModeCrossfader.h"
#include "Meter/NovaVUBallistics.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace nova::royal670
{

// Top-level facade - the only class PluginProcessor talks to for audio.
// Signal flow (matching the spec exactly):
//   Input -> input tube stage (oversampled) -> [Stereo/M-S encode] ->
//   variable-mu gain-control stage (detector + gain multiply, click-free
//   across mode switches) -> [decode] -> output tube stage + output
//   transformer (oversampled, one wrapper) -> Analog Noise -> Auto Gain ->
//   Output/Mix -> Bypass crossfade.
//
// The GR-coupling requirement ("gain-control behavior, harmonic
// generation, program dependency, and nonlinear stages should interact")
// is satisfied at exactly one point: the output tube stage's drive is
// re-set every block from the just-measured gainReductionDb, not held at
// a static amount independent of how hard the limiter is working.
class NovaRoyal670Engine
{
public:
    NovaRoyal670Engine() = default;

    void prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages) noexcept;
    void reset() noexcept;

    // All raw values as they arrive from APVTS (percent/dB/choice as
    // documented per-field); call once per block from processBlock.
    void setParams (float inputDb, float hrmDrivePercent,
                     float acThresholdDb, float dcThresholdDb, int timeConstantMode,
                     float outputDb, float mixPercent,
                     bool midSideMode, float linkPercent,
                     float sidechainHpfHz, bool sidechainListen,
                     float weightPercent, float tubeSaturationPercent, float transformerPercent,
                     float outputTrimDb, bool autoGain,
                     float analogNoisePercent, bool bypassed) noexcept;

    // In-place stereo processing (buffer must have exactly 2 channels).
    void processBlock (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept;
    int getPreparedOversamplingStages() const noexcept { return preparedOversamplingStages; }

    // Metering, read by PluginProcessor once per block for UI telemetry.
    float getVuLeftOrMidDisplayVu() const noexcept { return vuA.getDisplayLevelVu(); }
    float getVuRightOrSideDisplayVu() const noexcept { return vuB.getDisplayLevelVu(); }
    float getGainReductionDb() const noexcept { return lastGainReductionDb; }

private:
    static constexpr float kSmoothSeconds = 0.02f;
    static constexpr float kBypassSmoothSeconds = 0.03f;
    static constexpr float kAutoGainSmoothSeconds = 0.05f;

    double sampleRate = 44100.0;
    int preparedOversamplingStages = 1;

    NovaOversampledInputStage inputStage;
    NovaOversampledOutputStage outputStage;

    // Both domains' detector state persists for the plugin's lifetime
    // (never reallocated) so a mode switch can crossfade between them -
    // see NovaStereoModeCrossfader's header for why this is cheap (no
    // oversampling duplication needed, just per-sample detector state).
    NovaVariableMuDetector detectorStereoA, detectorStereoB;
    NovaVariableMuDetector detectorMidSideA, detectorMidSideB;
    NovaStereoModeCrossfader stereoCrossfader;

    NovaVariableMuDetector& detectorFor (NovaStereoModeCrossfader::Mode mode, bool channelA) noexcept
    {
        if (mode == NovaStereoModeCrossfader::Mode::MidSide)
            return channelA ? detectorMidSideA : detectorMidSideB;
        return channelA ? detectorStereoA : detectorStereoB;
    }

    // Runs one domain's detector pair on one sample (l,r already encoded
    // by the caller into that domain's basis) and returns the decoded
    // L/R output plus the larger of the two channels' gain reduction (for
    // metering/coupling). Shared by both the active-domain and, during a
    // transition, previous-domain passes in processBlock().
    void runDomainDetectors (float l, float r, NovaStereoModeCrossfader::Mode mode, float linkAmount,
                              float& outL, float& outR, float& gainReductionDbOut) noexcept;

    NovaVUBallistics vuA, vuB;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        inputGainSmoothed, outputGainSmoothed, mixSmoothed, outputTrimGainSmoothed,
        linkSmoothed, analogNoiseSmoothed, bypassSmoothed, autoGainDbSmoothed;

    // Non-smoothed (block-rate) parameters - either discrete/choice values
    // or values whose own internal logic already handles smooth change
    // (e.g. detector thresholds ramp via the detector's own pole
    // followers, so double-smoothing them here would just add latency to
    // an automation move without benefit).
    float acThresholdDb = 0.0f, dcThresholdDb = 0.0f, sidechainHpfHz = 80.0f;
    int timeConstantMode = 4;
    bool sidechainListen = false;
    bool autoGainEnabled = true;

    // Targets set from setParams(); the *Smoothed values actually fed to
    // the tube/transformer stages creep toward these once per block
    // (NovaTubeStage/NovaTransformerStage process a whole block at a
    // fixed drive, so true per-sample SmoothedValue isn't applicable
    // here - this block-rate one-pole follower is what keeps a sudden
    // knob move from producing a single-block step, per the "automation
    // must not click or zipper" requirement).
    float hrmDriveTarget01 = 0.0f, tubeSaturationTarget01 = 0.0f, transformerTarget01 = 0.0f;
    float hrmDriveSmoothed01 = 0.0f, tubeSaturationSmoothed01 = 0.0f, transformerSmoothed01 = 0.0f;
    static constexpr float kBlockSmoothSeconds = 0.03f;

    NovaWeightMacro weightMacro;

    float lastGainReductionDb = 0.0f;
    juce::Random noiseRandom;

    // Pre-allocated scratch for the bypass dry/wet crossfade - resized
    // once in prepare(), never allocated on the audio thread.
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaRoyal670Engine)
};

} // namespace nova::royal670
