#pragma once

#include <atomic>
#include <random>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "NovaConsoleParameters.h"

// NovaConsoleDSP — shared console strip engine (Phase 4E: + MixAssist/Focus)
//
// Lifecycle (matches every other NovaDSP engine):
//   prepare(spec, initial)   — allocate, reset, seed smoothers
//   reset() noexcept         — clear filter/DSP state only (no realloc)
//   setParameters(p) noexcept
//   process(main, sidechain) noexcept
//
// Phase 4B: ModeProfile blend, Filter stage, EQ stage.
// Phase 4C: Input/Output gain, Preamp stage, Gate stage.
// Phase 4D: Compressor stage + gainReductionMeter.
// Phase 4E: Mix Assist + Focus stage.
// Phase 4F: Smart Gain compensation.
// Phase 4G: Analog Engine.
//
// TD-003  44 SmoothedValue instances are owned individually.  Future v2 may
//         consolidate into a ConsoleSmoothers aggregate for readability.
//         No refactor during extraction — preserve original per-smoother layout.

class NovaConsoleDSP
{
public:
    NovaConsoleDSP() = default;

    void prepare (const juce::dsp::ProcessSpec& spec,
                  const NovaConsoleParameters& initial = {});

    void reset() noexcept;

    void setParameters (const NovaConsoleParameters& p) noexcept;

    void process (juce::AudioBuffer<float>& buffer,
                  const juce::AudioBuffer<float>* sidechain = nullptr) noexcept;

    // Gain reduction in dB (0 = no reduction). Written by audio thread.
    // Read by UI thread via this accessor.
    float getGainReductionDb() const noexcept
    {
        return gainReductionMeter.load (std::memory_order_relaxed);
    }

private:
    // ── Mode profile ─────────────────────────────────────────────────────────
    enum class ConsoleMode : int { clean = 0, british, tubeTape, gold, modern };

    struct ModeProfile
    {
        float warmth              = 0.2f;
        float presence            = 1.0f;
        float eqWidth             = 1.0f;
        float upperMidAggression  = 1.0f;
        float airSmoothness       = 1.0f;
        float lowMidWeight        = 1.0f;
        float oddDrive            = 0.5f;
        float evenDrive           = 0.5f;
        float clipSoftness        = 1.0f;
        float transientPunch      = 1.0f;
        float transientRetention  = 1.0f;
        float stereoWidthBias     = 1.0f;
        float centerWeight        = 1.0f;
        float sideSoftness        = 1.0f;
        float crosstalkBias       = 1.0f;
        float outputTrim          = 1.0f;
    };

    static ModeProfile profileForMode (ConsoleMode mode) noexcept;
    static ModeProfile blendProfiles  (const ModeProfile& a,
                                       const ModeProfile& b,
                                       float t) noexcept;

    // ── Preamp helpers ────────────────────────────────────────────────────────
    static float saturateSmooth  (float x) noexcept;
    static float applyColorTilt  (float sample, float color) noexcept;

    // ── Private DSP helpers ───────────────────────────────────────────────────
    void updateLinearStageCoefficients() noexcept;
    void processFilters (juce::AudioBuffer<float>& buffer) noexcept;
    void processEq      (juce::AudioBuffer<float>& buffer,
                         const ModeProfile& profile) noexcept;
    void processPreamp      (juce::AudioBuffer<float>& buffer,
                             const ModeProfile& profile, int osFactor) noexcept;
    void processCompressor  (juce::AudioBuffer<float>& buffer,
                             const ModeProfile& profile,
                             const juce::AudioBuffer<float>* detectorBuffer,
                             bool detectorSidechainEnabled,
                             bool useExternalDetector) noexcept;
    void processGate             (juce::AudioBuffer<float>& buffer,
                                  const juce::AudioBuffer<float>* detectorBuffer,
                                  bool detectorSidechainEnabled,
                                  bool useExternalDetector) noexcept;
    void processMixAssistAndFocus    (juce::AudioBuffer<float>& buffer,
                                      float mixAssistAmount,
                                      float focusAmount) noexcept;
    void applySmartGainCompensation  (juce::AudioBuffer<float>& buffer,
                                      float preProcessRms,
                                      float postProcessRms,
                                      int numSamples) noexcept;
    void processAnalogEngine         (juce::AudioBuffer<float>& buffer,
                                      const ModeProfile& profile,
                                      int osFactor,
                                      float mixAssistAmount,
                                      float focusAmount) noexcept;

    // ── Filters ───────────────────────────────────────────────────────────────
    juce::dsp::StateVariableTPTFilter<float> hpf[2];
    juce::dsp::StateVariableTPTFilter<float> lpf[2];
    juce::dsp::StateVariableTPTFilter<float> hpfStage2[2];
    juce::dsp::StateVariableTPTFilter<float> hpfStage3[2];
    juce::dsp::StateVariableTPTFilter<float> hpfStage4[2];
    juce::dsp::StateVariableTPTFilter<float> lpfStage2[2];
    juce::dsp::StateVariableTPTFilter<float> lpfStage3[2];
    juce::dsp::StateVariableTPTFilter<float> lpfStage4[2];

    // ── EQ ────────────────────────────────────────────────────────────────────
    juce::dsp::IIR::Filter<float> lowShelf[2];
    juce::dsp::IIR::Filter<float> lowMidPeak[2];
    juce::dsp::IIR::Filter<float> highMidPeak[2];
    juce::dsp::IIR::Filter<float> highShelf[2];
    juce::dsp::IIR::Filter<float> airShelf[2];

    // ── Smoothers (TD-003: individual SmoothedValues, not consolidated) ───────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> hpfSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lpfSmoothed;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowFreqSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowQSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowMidSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowMidFreqSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowMidQSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highMidSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highMidFreqSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highMidQSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highFreqSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highQSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> airSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> airFreqSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> airQSmoothed;

    // ── Preamp smoothers ──────────────────────────────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> colorSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> trimSmoothed;

    // ── Input / Output gain smoothers ─────────────────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputSmoothed;

    // ── Compressor smoothers ──────────────────────────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compThresholdSmoothed; // 45ms
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compRatioSmoothed;     // 50ms
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compAttackSmoothed;    // 15ms
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compReleaseSmoothed;   // 45ms
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compMixSmoothed;       // 30ms
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compMakeupSmoothed;    // 30ms
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compPunchSmoothed;     // 30ms

    // ── Gate smoothers ────────────────────────────────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateThresholdSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateReleaseSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateRangeSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateAttackSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateHoldSmoothed;

    // ── Mix Assist / Focus smoothers ──────────────────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixAssistAmountSmoothed; // 120ms
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> focusAmountSmoothed;     // 100ms

    // ── Analog Engine smoothers ───────────────────────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> heatSmoothed;       // 50ms
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> depthSmoothed;      // 50ms
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoothed;      // 50ms
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driftSmoothed;      // 80ms
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> noiseSmoothed;      // 50ms
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> crosstalkSmoothed;  // 50ms

    // ── Smart Gain smoother ───────────────────────────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smartGainCompDbSmoothed; // 150ms

    // ── Mode morph ────────────────────────────────────────────────────────────
    ConsoleMode modeFrom = ConsoleMode::british;
    ConsoleMode modeTo   = ConsoleMode::british;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> modeMorph;

    // ── EQ coefficient dirty-check cache (avoids per-block IIR allocations) ──
    float lastHpfHz       = -1.0f;
    float lastLpfHz       = -1.0f;
    float lastLowDb       = 999.0f;
    float lastLowMidDb    = 999.0f;
    float lastHighMidDb   = 999.0f;
    float lastHighDb      = 999.0f;
    float lastAirDb       = 999.0f;
    float lastLowFreq     = -1.0f;
    float lastLowMidFreq  = -1.0f;
    float lastHighMidFreq = -1.0f;
    float lastHighFreq    = -1.0f;
    float lastAirFreq     = -1.0f;
    float lastLowQ        = -1.0f;
    float lastLowMidQ     = -1.0f;
    float lastHighMidQ    = -1.0f;
    float lastHighQ       = -1.0f;
    float lastAirQ        = -1.0f;
    int   lastHpfSlope    = -1;
    int   lastLpfSlope    = -1;
    int   lastLowMode     = -1;
    int   lastHighMode    = -1;
    int   lastAirMode     = -1;

    // ── Compressor state ──────────────────────────────────────────────────────
    float compressorDetector  = 0.0f;
    float compressorGainState = 1.0f;
    std::array<float, 2> compressorPunchMemory  { 0.0f, 0.0f };
    std::array<std::array<float, 2>, 4> compDetectorHpfState {};
    std::array<std::array<float, 2>, 4> compDetectorLpfState {};

    // ── Compressor meter (written audio thread, read UI thread) ───────────────
    mutable std::atomic<float> gainReductionMeter { 0.0f };

    // ── Preamp state ──────────────────────────────────────────────────────────
    std::array<float, 2> preampPrevInput { 0.0f, 0.0f };

    // ── Gate state ────────────────────────────────────────────────────────────
    std::array<float,   2> gateEnv          { 1.0f, 1.0f };
    std::array<int32_t, 2> gateHoldCounter  { 0, 0 };
    std::array<float,   2> gatePreviousEnv  { 0.0f, 0.0f }; // preserved from original (never read)
    std::array<std::array<float, 2>, 4> gateDetectorHpfState {};
    std::array<std::array<float, 2>, 4> gateDetectorLpfState {};

    // ── Mix Assist / Focus state ──────────────────────────────────────────────
    std::array<float, 2> mixAssistLowState        { 0.0f, 0.0f };
    std::array<float, 2> mixAssistPresenceLpState { 0.0f, 0.0f };
    std::array<float, 2> mixAssistHarshLpState    { 0.0f, 0.0f };
    std::array<float, 2> mixAssistHarshEnvState   { 0.0f, 0.0f };

    // ── Analog Engine state ───────────────────────────────────────────────────
    std::array<float, 2> driftHarmonicState { 0.0f, 0.0f };
    std::array<float, 2> driftStereoState   { 0.0f, 0.0f };
    std::array<float, 2> driftContourState  { 0.0f, 0.0f };
    float driftFilterState = 0.0f;
    std::array<float, 2> analogPrevInput   { 0.0f, 0.0f };
    std::array<float, 2> analogToneMemory  { 0.0f, 0.0f };
    std::array<float, 2> crosstalkLowState  { 0.0f, 0.0f };
    std::array<float, 2> crosstalkHighState { 0.0f, 0.0f };
    std::array<std::array<float, 64>, 2> crosstalkDelayBuffer {};
    int   crosstalkDelayWriteIndex = 0;
    float assistDensityEnv = 0.0f;

    // RNG — deterministic seed 0x5A17, same call order as original
    std::minstd_rand rng;
    std::uniform_real_distribution<float> randDist { -1.0f, 1.0f };

    // ── Smart Gain state ──────────────────────────────────────────────────────
    float smartGainPreRmsEnv  = 0.0f;
    float smartGainPostRmsEnv = 0.0f;

    double currentSampleRate = 44100.0;
    NovaConsoleParameters params;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaConsoleDSP)
};
