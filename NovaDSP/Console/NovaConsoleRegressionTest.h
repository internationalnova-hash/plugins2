#pragma once

// Regression test — Phase 4D: + Compressor stage.
//
// Verifies NovaConsoleDSP produces sample-identical output to the original
// Nova Console PluginProcessor algorithm for all currently extracted stages.
//
// Usage: NovaConsoleRegressionTest::runAll();   // asserts on failure
//
// OriginalState replicates the processBlock code from
// Nova Console/Source/PluginProcessor.cpp verbatim.  The only substitution
// is apvts.getRawParameterValue(id)->load() → params.field.
//
// Signal chain (Phase 4D, verbatim from processBlock):
//   Input gain → [preamp_on] Preamp → [filter_on] Filters → [eq_on] EQ →
//   [comp_on] Compressor → [gate_on] Gate →
//   modeTrim + clip(±1.35) → Output gain
//
// Both OriginalState and NovaConsoleDSP are seeded identically via
// prepare(spec, initialParams).  peakAbsDiff == 0.0f expected for all
// deterministic scenarios.

#include <cassert>
#include <cmath>
#include <cstring>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "NovaConsoleDSP.h"
#include "NovaConsoleParameters.h"

class NovaConsoleRegressionTest
{
public:
    struct Result
    {
        float       peakAbsDiff = 0.0f;
        float       rmsAbsDiff  = 0.0f;
        bool        passed      = false;
        const char* scenario    = "";
    };

    // ── Original algorithm reproduced verbatim ────────────────────────────────
    // Extracted from Nova Console/Source/PluginProcessor.cpp at Phase 4C time.
    struct OriginalState
    {
        // ── Filters ───────────────────────────────────────────────────────────
        juce::dsp::StateVariableTPTFilter<float> hpf[2];
        juce::dsp::StateVariableTPTFilter<float> lpf[2];
        juce::dsp::StateVariableTPTFilter<float> hpfStage2[2];
        juce::dsp::StateVariableTPTFilter<float> hpfStage3[2];
        juce::dsp::StateVariableTPTFilter<float> hpfStage4[2];
        juce::dsp::StateVariableTPTFilter<float> lpfStage2[2];
        juce::dsp::StateVariableTPTFilter<float> lpfStage3[2];
        juce::dsp::StateVariableTPTFilter<float> lpfStage4[2];

        // ── EQ ────────────────────────────────────────────────────────────────
        juce::dsp::IIR::Filter<float> lowShelf[2];
        juce::dsp::IIR::Filter<float> lowMidPeak[2];
        juce::dsp::IIR::Filter<float> highMidPeak[2];
        juce::dsp::IIR::Filter<float> highShelf[2];
        juce::dsp::IIR::Filter<float> airShelf[2];

        // ── Smoothers ─────────────────────────────────────────────────────────
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

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> colorSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> trimSmoothed;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputSmoothed;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compThresholdSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compRatioSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compAttackSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compReleaseSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compMixSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compMakeupSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compPunchSmoothed;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateThresholdSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateReleaseSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateRangeSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateAttackSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateHoldSmoothed;

        // ── Mode morph ────────────────────────────────────────────────────────
        enum class ConsoleMode : int { clean = 0, british, tubeTape, gold, modern };

        struct ModeProfile
        {
            float warmth=0.2f, presence=1.0f, eqWidth=1.0f, upperMidAggression=1.0f;
            float airSmoothness=1.0f, lowMidWeight=1.0f;
            float oddDrive=0.5f, evenDrive=0.5f, clipSoftness=1.0f;
            float transientPunch=1.0f, transientRetention=1.0f;
            float stereoWidthBias=1.0f, centerWeight=1.0f, sideSoftness=1.0f;
            float crosstalkBias=1.0f, outputTrim=1.0f;
        };

        ConsoleMode modeFrom = ConsoleMode::british;
        ConsoleMode modeTo   = ConsoleMode::british;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> modeMorph;

        // ── Dirty-check cache ─────────────────────────────────────────────────
        float lastHpfHz=-1.f, lastLpfHz=-1.f;
        float lastLowDb=999.f, lastLowMidDb=999.f, lastHighMidDb=999.f;
        float lastHighDb=999.f, lastAirDb=999.f;
        float lastLowFreq=-1.f, lastLowMidFreq=-1.f, lastHighMidFreq=-1.f;
        float lastHighFreq=-1.f, lastAirFreq=-1.f;
        float lastLowQ=-1.f, lastLowMidQ=-1.f, lastHighMidQ=-1.f;
        float lastHighQ=-1.f, lastAirQ=-1.f;
        int   lastHpfSlope=-1, lastLpfSlope=-1;
        int   lastLowMode=-1, lastHighMode=-1, lastAirMode=-1;

        // ── Compressor state ──────────────────────────────────────────────────
        float compressorDetector  = 0.0f;
        float compressorGainState = 1.0f;
        std::array<float, 2> compressorPunchMemory  { 0.0f, 0.0f };
        std::array<std::array<float, 2>, 4> compDetectorHpfState {};
        std::array<std::array<float, 2>, 4> compDetectorLpfState {};
        float gainReductionMeter = 0.0f; // non-atomic: single-threaded in tests

        // ── Preamp state ──────────────────────────────────────────────────────
        std::array<float, 2> preampPrevInput { 0.0f, 0.0f };

        // ── Gate state ────────────────────────────────────────────────────────
        std::array<float,   2> gateEnv          { 1.0f, 1.0f };
        std::array<int32_t, 2> gateHoldCounter  { 0, 0 };
        std::array<float,   2> gatePreviousEnv  { 0.0f, 0.0f }; // dead — preserved from original
        std::array<std::array<float, 2>, 4> gateDetectorHpfState {};
        std::array<std::array<float, 2>, 4> gateDetectorLpfState {};

        double currentSampleRate = 44100.0;

        // ── Static helpers ────────────────────────────────────────────────────
        static float dbToGain (float db) noexcept
        {
            return juce::Decibels::decibelsToGain (db);
        }

        static float gainToDb (float gain) noexcept
        {
            return juce::Decibels::gainToDecibels (juce::jmax (gain, 1.0e-6f));
        }

        static float saturateSmooth (float x) noexcept
        {
            return std::tanh (x);
        }

        static float applyColorTilt (float sample, float color) noexcept
        {
            const float dark   = juce::jmap (color, 0.0f, 1.0f, 1.08f, 0.96f);
            const float bright = juce::jmap (color, 0.0f, 1.0f, 0.94f, 1.08f);
            return sample * (0.65f * dark + 0.35f * bright);
        }

        static ModeProfile profileForMode (ConsoleMode mode) noexcept
        {
            ModeProfile p {};
            switch (mode)
            {
                case ConsoleMode::clean:
                    p.warmth=0.09f; p.presence=1.02f; p.eqWidth=1.03f;
                    p.upperMidAggression=0.96f; p.airSmoothness=1.04f; p.lowMidWeight=0.95f;
                    p.oddDrive=0.45f; p.evenDrive=0.34f; p.clipSoftness=1.06f;
                    p.transientPunch=0.98f; p.transientRetention=1.06f;
                    p.stereoWidthBias=1.02f; p.centerWeight=0.98f; p.sideSoftness=1.00f;
                    p.crosstalkBias=0.90f; p.outputTrim=1.00f; break;
                case ConsoleMode::british:
                    p.warmth=0.30f; p.presence=1.09f; p.eqWidth=0.95f;
                    p.upperMidAggression=1.08f; p.airSmoothness=0.96f; p.lowMidWeight=0.98f;
                    p.oddDrive=0.70f; p.evenDrive=0.40f; p.clipSoftness=0.90f;
                    p.transientPunch=1.18f; p.transientRetention=0.95f;
                    p.stereoWidthBias=0.98f; p.centerWeight=1.04f; p.sideSoftness=0.96f;
                    p.crosstalkBias=1.08f; p.outputTrim=0.975f; break;
                case ConsoleMode::tubeTape:
                    p.warmth=0.39f; p.presence=0.93f; p.eqWidth=0.98f;
                    p.upperMidAggression=0.92f; p.airSmoothness=1.10f; p.lowMidWeight=1.08f;
                    p.oddDrive=0.46f; p.evenDrive=0.76f; p.clipSoftness=1.14f;
                    p.transientPunch=0.90f; p.transientRetention=0.92f;
                    p.stereoWidthBias=0.99f; p.centerWeight=1.01f; p.sideSoftness=1.05f;
                    p.crosstalkBias=1.02f; p.outputTrim=0.955f; break;
                case ConsoleMode::gold:
                    p.warmth=0.24f; p.presence=1.06f; p.eqWidth=0.97f;
                    p.upperMidAggression=1.00f; p.airSmoothness=1.08f; p.lowMidWeight=1.02f;
                    p.oddDrive=0.52f; p.evenDrive=0.58f; p.clipSoftness=1.10f;
                    p.transientPunch=1.00f; p.transientRetention=0.98f;
                    p.stereoWidthBias=1.01f; p.centerWeight=1.00f; p.sideSoftness=1.03f;
                    p.crosstalkBias=0.98f; p.outputTrim=0.97f; break;
                case ConsoleMode::modern:
                    p.warmth=0.14f; p.presence=1.11f; p.eqWidth=1.04f;
                    p.upperMidAggression=1.01f; p.airSmoothness=1.03f; p.lowMidWeight=0.97f;
                    p.oddDrive=0.48f; p.evenDrive=0.44f; p.clipSoftness=1.04f;
                    p.transientPunch=1.03f; p.transientRetention=1.08f;
                    p.stereoWidthBias=1.06f; p.centerWeight=0.97f; p.sideSoftness=1.01f;
                    p.crosstalkBias=0.92f; p.outputTrim=0.985f; break;
            }
            return p;
        }

        static ModeProfile blendProfiles (const ModeProfile& a, const ModeProfile& b, float t) noexcept
        {
            const float m = juce::jlimit (0.0f, 1.0f, t);
            ModeProfile p {};
            p.warmth=juce::jmap(m,a.warmth,b.warmth);
            p.presence=juce::jmap(m,a.presence,b.presence);
            p.eqWidth=juce::jmap(m,a.eqWidth,b.eqWidth);
            p.upperMidAggression=juce::jmap(m,a.upperMidAggression,b.upperMidAggression);
            p.airSmoothness=juce::jmap(m,a.airSmoothness,b.airSmoothness);
            p.lowMidWeight=juce::jmap(m,a.lowMidWeight,b.lowMidWeight);
            p.oddDrive=juce::jmap(m,a.oddDrive,b.oddDrive);
            p.evenDrive=juce::jmap(m,a.evenDrive,b.evenDrive);
            p.clipSoftness=juce::jmap(m,a.clipSoftness,b.clipSoftness);
            p.transientPunch=juce::jmap(m,a.transientPunch,b.transientPunch);
            p.transientRetention=juce::jmap(m,a.transientRetention,b.transientRetention);
            p.stereoWidthBias=juce::jmap(m,a.stereoWidthBias,b.stereoWidthBias);
            p.centerWeight=juce::jmap(m,a.centerWeight,b.centerWeight);
            p.sideSoftness=juce::jmap(m,a.sideSoftness,b.sideSoftness);
            p.crosstalkBias=juce::jmap(m,a.crosstalkBias,b.crosstalkBias);
            p.outputTrim=juce::jmap(m,a.outputTrim,b.outputTrim);
            return p;
        }

        // ── Lifecycle ─────────────────────────────────────────────────────────
        void prepare (double sampleRate, int samplesPerBlock, const NovaConsoleParameters& init)
        {
            currentSampleRate = juce::jmax (1.0, sampleRate);
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)samplesPerBlock, 1 };

            for (int ch = 0; ch < 2; ++ch)
            {
                hpf[ch].reset(); lpf[ch].reset();
                hpf[ch].setType       (juce::dsp::StateVariableTPTFilterType::highpass);
                lpf[ch].setType       (juce::dsp::StateVariableTPTFilterType::lowpass);
                hpfStage2[ch].setType (juce::dsp::StateVariableTPTFilterType::highpass);
                hpfStage3[ch].setType (juce::dsp::StateVariableTPTFilterType::highpass);
                hpfStage4[ch].setType (juce::dsp::StateVariableTPTFilterType::highpass);
                lpfStage2[ch].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
                lpfStage3[ch].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
                lpfStage4[ch].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
                hpf[ch].prepare (spec); lpf[ch].prepare (spec);
                hpfStage2[ch].prepare (spec); hpfStage3[ch].prepare (spec);
                hpfStage4[ch].prepare (spec);
                lpfStage2[ch].prepare (spec); lpfStage3[ch].prepare (spec);
                lpfStage4[ch].prepare (spec);
                lowShelf[ch].prepare (spec); lowMidPeak[ch].prepare (spec);
                highMidPeak[ch].prepare (spec); highShelf[ch].prepare (spec);
                airShelf[ch].prepare (spec);
            }

            hpfSmoothed.reset (sampleRate, 0.050);
            lpfSmoothed.reset (sampleRate, 0.050);
            lowSmoothed.reset         (sampleRate, 0.035);
            lowFreqSmoothed.reset     (sampleRate, 0.050);
            lowQSmoothed.reset        (sampleRate, 0.015);
            lowMidSmoothed.reset      (sampleRate, 0.035);
            lowMidFreqSmoothed.reset  (sampleRate, 0.050);
            lowMidQSmoothed.reset     (sampleRate, 0.015);
            highMidSmoothed.reset     (sampleRate, 0.035);
            highMidFreqSmoothed.reset (sampleRate, 0.050);
            highMidQSmoothed.reset    (sampleRate, 0.015);
            highSmoothed.reset        (sampleRate, 0.035);
            highFreqSmoothed.reset    (sampleRate, 0.050);
            highQSmoothed.reset       (sampleRate, 0.015);
            airSmoothed.reset         (sampleRate, 0.035);
            airFreqSmoothed.reset     (sampleRate, 0.050);
            airQSmoothed.reset        (sampleRate, 0.015);

            driveSmoothed.reset (sampleRate, 0.030);
            colorSmoothed.reset (sampleRate, 0.030);
            trimSmoothed.reset  (sampleRate, 0.030);

            inputSmoothed.reset  (sampleRate, 0.030);
            outputSmoothed.reset (sampleRate, 0.030);

            compThresholdSmoothed.reset (sampleRate, 0.045);
            compRatioSmoothed.reset     (sampleRate, 0.050);
            compAttackSmoothed.reset    (sampleRate, 0.015);
            compReleaseSmoothed.reset   (sampleRate, 0.045);
            compMixSmoothed.reset       (sampleRate, 0.030);
            compMakeupSmoothed.reset    (sampleRate, 0.030);
            compPunchSmoothed.reset     (sampleRate, 0.030);

            gateThresholdSmoothed.reset (sampleRate, 0.045);
            gateReleaseSmoothed.reset   (sampleRate, 0.045);
            gateRangeSmoothed.reset     (sampleRate, 0.045);
            gateAttackSmoothed.reset    (sampleRate, 0.030);
            gateHoldSmoothed.reset      (sampleRate, 0.030);

            hpfSmoothed.setCurrentAndTargetValue (init.hpfHz);
            lpfSmoothed.setCurrentAndTargetValue (init.lpfHz);
            lowSmoothed.setCurrentAndTargetValue         (init.lowDb);
            lowFreqSmoothed.setCurrentAndTargetValue     (init.lowFreqHz);
            lowQSmoothed.setCurrentAndTargetValue        (init.lowQ);
            lowMidSmoothed.setCurrentAndTargetValue      (init.lowMidDb);
            lowMidFreqSmoothed.setCurrentAndTargetValue  (init.lowMidFreqHz);
            lowMidQSmoothed.setCurrentAndTargetValue     (init.lowMidQ);
            highMidSmoothed.setCurrentAndTargetValue     (init.highMidDb);
            highMidFreqSmoothed.setCurrentAndTargetValue (init.highMidFreqHz);
            highMidQSmoothed.setCurrentAndTargetValue    (init.highMidQ);
            highSmoothed.setCurrentAndTargetValue        (init.highDb);
            highFreqSmoothed.setCurrentAndTargetValue    (init.highFreqHz);
            highQSmoothed.setCurrentAndTargetValue       (init.highQ);
            airSmoothed.setCurrentAndTargetValue         (init.airDb);
            airFreqSmoothed.setCurrentAndTargetValue     (init.airFreqHz);
            airQSmoothed.setCurrentAndTargetValue        (init.airQ);

            driveSmoothed.setCurrentAndTargetValue (init.drive / 100.0f);
            colorSmoothed.setCurrentAndTargetValue (init.color / 100.0f);
            trimSmoothed.setCurrentAndTargetValue  (dbToGain (init.trimDb));

            inputSmoothed.setCurrentAndTargetValue  (dbToGain (init.inputDb));
            outputSmoothed.setCurrentAndTargetValue (dbToGain (init.outputDb));

            compThresholdSmoothed.setCurrentAndTargetValue (init.compThreshDb);
            compRatioSmoothed.setCurrentAndTargetValue     (init.compRatio);
            compAttackSmoothed.setCurrentAndTargetValue    (init.compAttackMs);
            compReleaseSmoothed.setCurrentAndTargetValue   (init.compReleaseMs);
            compMixSmoothed.setCurrentAndTargetValue       (init.compMix / 100.0f);
            compMakeupSmoothed.setCurrentAndTargetValue    (init.compMakeupDb);
            compPunchSmoothed.setCurrentAndTargetValue     (init.compPunch / 100.0f);

            gateThresholdSmoothed.setCurrentAndTargetValue (init.gateThreshDb);
            gateReleaseSmoothed.setCurrentAndTargetValue   (init.gateReleaseMs);
            gateRangeSmoothed.setCurrentAndTargetValue     (init.gateRangeDb);
            gateAttackSmoothed.setCurrentAndTargetValue    (init.gateAttackMs);
            gateHoldSmoothed.setCurrentAndTargetValue      (init.gateHoldMs);

            const int rawMode = juce::jlimit (0, 4, init.mode);
            modeFrom = static_cast<ConsoleMode> (rawMode);
            modeTo   = modeFrom;
            modeMorph.reset (sampleRate, 0.035);
            modeMorph.setCurrentAndTargetValue (1.0f);

            lastHpfHz=-1.f; lastLpfHz=-1.f;
            lastLowDb=999.f; lastLowMidDb=999.f; lastHighMidDb=999.f;
            lastHighDb=999.f; lastAirDb=999.f;
            lastLowFreq=-1.f; lastLowMidFreq=-1.f; lastHighMidFreq=-1.f;
            lastHighFreq=-1.f; lastAirFreq=-1.f;
            lastLowQ=-1.f; lastLowMidQ=-1.f; lastHighMidQ=-1.f;
            lastHighQ=-1.f; lastAirQ=-1.f;
            lastHpfSlope=-1; lastLpfSlope=-1;
            lastLowMode=-1; lastHighMode=-1; lastAirMode=-1;

            compressorDetector   = 0.0f;
            compressorGainState  = 1.0f;
            compressorPunchMemory  = { 0.0f, 0.0f };
            compDetectorHpfState   = {};
            compDetectorLpfState   = {};
            gainReductionMeter     = 0.0f;

            preampPrevInput      = { 0.0f, 0.0f };
            gateEnv              = { 1.0f, 1.0f };
            gateHoldCounter      = { 0, 0 };
            gatePreviousEnv      = { 0.0f, 0.0f };
            gateDetectorHpfState = {};
            gateDetectorLpfState = {};

            updateCoefficients (init);
        }

        void updateCoefficients (const NovaConsoleParameters& p)
        {
            hpfSmoothed.setTargetValue (p.hpfHz);
            lpfSmoothed.setTargetValue (p.lpfHz);
            const auto hpfHz = hpfSmoothed.getNextValue();
            const auto lpfHz = lpfSmoothed.getNextValue();

            lowSmoothed.setTargetValue         (p.lowDb);
            lowFreqSmoothed.setTargetValue     (p.lowFreqHz);
            lowQSmoothed.setTargetValue        (p.lowQ);
            lowMidSmoothed.setTargetValue      (p.lowMidDb);
            lowMidFreqSmoothed.setTargetValue  (p.lowMidFreqHz);
            lowMidQSmoothed.setTargetValue     (p.lowMidQ);
            highMidSmoothed.setTargetValue     (p.highMidDb);
            highMidFreqSmoothed.setTargetValue (p.highMidFreqHz);
            highMidQSmoothed.setTargetValue    (p.highMidQ);
            highSmoothed.setTargetValue        (p.highDb);
            highFreqSmoothed.setTargetValue    (p.highFreqHz);
            highQSmoothed.setTargetValue       (p.highQ);
            airSmoothed.setTargetValue         (p.airDb);
            airFreqSmoothed.setTargetValue     (p.airFreqHz);
            airQSmoothed.setTargetValue        (p.airQ);

            const auto lowDb      = lowSmoothed.getNextValue();
            const auto lowFreq    = lowFreqSmoothed.getNextValue();
            const auto lowQ_      = lowQSmoothed.getNextValue();
            const auto lowMidDb   = lowMidSmoothed.getNextValue();
            const auto lowMidFreq = lowMidFreqSmoothed.getNextValue();
            const auto lowMidQ_   = lowMidQSmoothed.getNextValue();
            const auto highMidDb  = highMidSmoothed.getNextValue();
            const auto highMidFreq= highMidFreqSmoothed.getNextValue();
            const auto highMidQ_  = highMidQSmoothed.getNextValue();
            const auto highDb     = highSmoothed.getNextValue();
            const auto highFreq   = highFreqSmoothed.getNextValue();
            const auto highQ_     = highQSmoothed.getNextValue();
            const auto airDb      = airSmoothed.getNextValue();
            const auto airFreq    = airFreqSmoothed.getNextValue();
            const auto airQ_      = airQSmoothed.getNextValue();

            const int hpfSl = juce::jlimit (0, 2, p.hpfSlope);
            const int lpfSl = juce::jlimit (0, 2, p.lpfSlope);
            const int loMd  = juce::jlimit (0, 1, p.lowMode);
            const int hiMd  = juce::jlimit (0, 1, p.highMode);
            const int airMd = juce::jlimit (0, 1, p.airMode);

            const bool hpfCh = std::abs (hpfHz - lastHpfHz) > 0.0001f;
            const bool lpfCh = std::abs (lpfHz - lastLpfHz) > 0.0001f;
            const bool eqCh  = std::abs (lowDb      - lastLowDb)      > 0.0001f
                            || std::abs (lowFreq     - lastLowFreq)    > 0.0001f
                            || std::abs (lowQ_       - lastLowQ)       > 0.0001f
                            || std::abs (lowMidDb    - lastLowMidDb)   > 0.0001f
                            || std::abs (lowMidFreq  - lastLowMidFreq) > 0.0001f
                            || std::abs (lowMidQ_    - lastLowMidQ)    > 0.0001f
                            || std::abs (highMidDb   - lastHighMidDb)  > 0.0001f
                            || std::abs (highMidFreq - lastHighMidFreq)> 0.0001f
                            || std::abs (highMidQ_   - lastHighMidQ)   > 0.0001f
                            || std::abs (highDb      - lastHighDb)     > 0.0001f
                            || std::abs (highFreq    - lastHighFreq)   > 0.0001f
                            || std::abs (highQ_      - lastHighQ)      > 0.0001f
                            || std::abs (airDb       - lastAirDb)      > 0.0001f
                            || std::abs (airFreq     - lastAirFreq)    > 0.0001f
                            || std::abs (airQ_       - lastAirQ)       > 0.0001f;
            const bool mdCh  = loMd != lastLowMode || hiMd != lastHighMode
                            || airMd != lastAirMode
                            || hpfSl != lastHpfSlope || lpfSl != lastLpfSlope;

            if (!hpfCh && !lpfCh && !eqCh && !mdCh) return;

            for (int ch = 0; ch < 2; ++ch)
            {
                if (hpfCh)
                {
                    hpf[ch].setCutoffFrequency (hpfHz); hpfStage2[ch].setCutoffFrequency (hpfHz);
                    hpfStage3[ch].setCutoffFrequency (hpfHz); hpfStage4[ch].setCutoffFrequency (hpfHz);
                }
                if (lpfCh)
                {
                    lpf[ch].setCutoffFrequency (lpfHz); lpfStage2[ch].setCutoffFrequency (lpfHz);
                    lpfStage3[ch].setCutoffFrequency (lpfHz); lpfStage4[ch].setCutoffFrequency (lpfHz);
                }
            }

            if (eqCh || mdCh)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    lowShelf[ch].coefficients = loMd == 0
                        ? juce::dsp::IIR::Coefficients<float>::makeLowShelf  (currentSampleRate, lowFreq, lowQ_, dbToGain (lowDb))
                        : juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, lowFreq, lowQ_, dbToGain (lowDb));

                    lowMidPeak[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                        currentSampleRate, lowMidFreq, lowMidQ_, dbToGain (lowMidDb));
                    highMidPeak[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                        currentSampleRate, highMidFreq, highMidQ_, dbToGain (highMidDb));

                    highShelf[ch].coefficients = hiMd == 0
                        ? juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, highFreq, highQ_, dbToGain (highDb))
                        : juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, highFreq, highQ_, dbToGain (highDb));

                    airShelf[ch].coefficients = airMd == 0
                        ? juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, airFreq, airQ_, dbToGain (airDb))
                        : juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, airFreq, airQ_, dbToGain (airDb));
                }
            }

            lastHpfHz=hpfHz; lastLpfHz=lpfHz;
            lastLowDb=lowDb; lastLowMidDb=lowMidDb; lastHighMidDb=highMidDb;
            lastHighDb=highDb; lastAirDb=airDb;
            lastLowFreq=lowFreq; lastLowMidFreq=lowMidFreq; lastHighMidFreq=highMidFreq;
            lastHighFreq=highFreq; lastAirFreq=airFreq;
            lastLowQ=lowQ_; lastLowMidQ=lowMidQ_; lastHighMidQ=highMidQ_;
            lastHighQ=highQ_; lastAirQ=airQ_;
            lastHpfSlope=hpfSl; lastLpfSlope=lpfSl;
            lastLowMode=loMd; lastHighMode=hiMd; lastAirMode=airMd;
        }

        void processFilters (juce::AudioBuffer<float>& buf, const NovaConsoleParameters& p)
        {
            const int hpfStages = p.hpfSlope == 0 ? 1 : (p.hpfSlope == 1 ? 2 : 4);
            const int lpfStages = p.lpfSlope == 0 ? 1 : (p.lpfSlope == 1 ? 2 : 4);
            const int channels  = juce::jmin (2, buf.getNumChannels());
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* data = buf.getWritePointer (ch);
                for (int i = 0; i < buf.getNumSamples(); ++i)
                {
                    float s = data[i];
                    s = hpf[ch].processSample (0, s);
                    if (hpfStages >= 2) s = hpfStage2[ch].processSample (0, s);
                    if (hpfStages >= 3) s = hpfStage3[ch].processSample (0, s);
                    if (hpfStages >= 4) s = hpfStage4[ch].processSample (0, s);
                    s = lpf[ch].processSample (0, s);
                    if (lpfStages >= 2) s = lpfStage2[ch].processSample (0, s);
                    if (lpfStages >= 3) s = lpfStage3[ch].processSample (0, s);
                    if (lpfStages >= 4) s = lpfStage4[ch].processSample (0, s);
                    data[i] = s;
                }
            }
        }

        void processEq (juce::AudioBuffer<float>& buf, const ModeProfile& profile)
        {
            const float presence    = profile.presence;
            const float width       = profile.eqWidth;
            const float upperMidAgg = profile.upperMidAggression;
            const float airSmooth   = profile.airSmoothness;
            const float lowMidWt    = profile.lowMidWeight;

            const int channels = juce::jmin (2, buf.getNumChannels());
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* data = buf.getWritePointer (ch);
                for (int i = 0; i < buf.getNumSamples(); ++i)
                {
                    float x = data[i];
                    x = lowShelf[ch].processSample   (x);
                    x = lowMidPeak[ch].processSample  (x * lowMidWt);
                    x = highMidPeak[ch].processSample (x * presence * upperMidAgg);
                    x = highShelf[ch].processSample   (x * width);
                    x = airShelf[ch].processSample    (x * airSmooth);
                    data[i] = x;
                }
            }
        }

        void processPreamp (juce::AudioBuffer<float>& buf,
                            const NovaConsoleParameters& p,
                            const ModeProfile& profile,
                            int osFactor)
        {
            const float driveNorm = p.drive / 100.0f;
            const float colorNorm = p.color / 100.0f;
            const float trimDb    = p.trimDb;

            const float warmth   = profile.warmth;
            const float osRelief = juce::jmap (static_cast<float> (osFactor), 1.0f, 4.0f, 0.0f, 0.16f);

            driveSmoothed.setTargetValue (driveNorm);
            colorSmoothed.setTargetValue (colorNorm);
            trimSmoothed.setTargetValue  (dbToGain (trimDb));

            const int channels = juce::jmin (2, buf.getNumChannels());
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* channelData = buf.getWritePointer (ch);
                float previousIn = preampPrevInput[static_cast<size_t> (ch)];

                for (int i = 0; i < buf.getNumSamples(); ++i)
                {
                    const float driveNow = driveSmoothed.getNextValue();
                    const float colorNow = colorSmoothed.getNextValue();
                    const float trimNow  = trimSmoothed.getNextValue();

                    const float stageGain = 1.0f + driveNow * (3.2f + 1.2f * warmth - osRelief);
                    const float asym      = 0.025f + 0.08f * driveNow + 0.05f * profile.oddDrive;
                    const float clipDrive = juce::jlimit (0.85f, 1.2f, 1.0f / profile.clipSoftness);
                    const float oddW      = 0.55f + 0.45f * profile.oddDrive;
                    const float evenW     = 0.35f + 0.55f * profile.evenDrive;
                    const float satNorm   = 1.0f / juce::jmax (0.35f, oddW + evenW);

                    const float input = channelData[i] * stageGain;
                    float x = 0.0f;

                    for (int os = 0; os < osFactor; ++os)
                    {
                        const float t      = static_cast<float> (os + 1) / static_cast<float> (osFactor);
                        const float interp = previousIn + (input - previousIn) * t;
                        const float oddSat = saturateSmooth ((interp + asym) * clipDrive)
                                           - saturateSmooth (asym * clipDrive);
                        const float evenSat = 0.5f * (saturateSmooth ((interp + asym) * 0.92f * clipDrive)
                                                    + saturateSmooth ((interp - asym) * 0.92f * clipDrive));
                        const float mixedSat = (oddSat * oddW + evenSat * evenW) * satNorm;
                        x += mixedSat;
                    }

                    x /= static_cast<float> (osFactor);
                    x = applyColorTilt (x, colorNow);
                    x = x * (0.985f + 0.03f * profile.transientRetention);
                    x = x * trimNow;
                    channelData[i] = juce::jlimit (-1.2f, 1.2f, x);
                    previousIn = input;
                }

                preampPrevInput[static_cast<size_t> (ch)] = previousIn;
            }
        }

        void processCompressor (juce::AudioBuffer<float>& buf,
                               const NovaConsoleParameters& p,
                               const ModeProfile& profile)
        {
            compThresholdSmoothed.setTargetValue (p.compThreshDb);
            compRatioSmoothed.setTargetValue     (p.compRatio);
            compAttackSmoothed.setTargetValue    (p.compAttackMs);
            compReleaseSmoothed.setTargetValue   (p.compReleaseMs);
            compMixSmoothed.setTargetValue       (p.compMix / 100.0f);
            compMakeupSmoothed.setTargetValue    (p.compMakeupDb);
            compPunchSmoothed.setTargetValue     (p.compPunch / 100.0f);

            const float modePunch     = profile.transientPunch;
            const float modeRetention = profile.transientRetention;
            const float qualityTightness = (p.quality == 2 ? 1.03f : (p.quality == 0 ? 0.92f : 1.0f));
            const float sr = static_cast<float> (currentSampleRate);

            float maxReduction = 0.0f;

            const int channels = juce::jmin (2, buf.getNumChannels());
            if (channels == 0) return;

            auto* left  = buf.getWritePointer (0);
            auto* right = channels > 1 ? buf.getWritePointer (1) : nullptr;

            // No external sidechain in unit tests — detector always reads driven signal
            const auto* detLeft  = left;
            const auto* detRight = right != nullptr ? right : left;

            const int hpfSlopeChoice = juce::jlimit (0, 2, p.hpfSlope);
            const int lpfSlopeChoice = juce::jlimit (0, 2, p.lpfSlope);
            const int hpfStages = hpfSlopeChoice == 0 ? 1 : (hpfSlopeChoice == 1 ? 2 : 4);
            const int lpfStages = lpfSlopeChoice == 0 ? 1 : (lpfSlopeChoice == 1 ? 2 : 4);
            const float detectorHpfCoeff = juce::jlimit (0.0001f, 0.999f,
                1.0f - std::exp (-juce::MathConstants<float>::twoPi * p.hpfHz / sr));
            const float detectorLpfCoeff = juce::jlimit (0.0001f, 0.999f,
                1.0f - std::exp (-juce::MathConstants<float>::twoPi * p.lpfHz / sr));

            const float kneeDb = 4.0f;

            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const float thresholdDb = compThresholdSmoothed.getNextValue();
                const float ratio       = compRatioSmoothed.getNextValue();
                const float attackMs    = compAttackSmoothed.getNextValue();
                const float releaseMs   = compReleaseSmoothed.getNextValue();
                const float compMix     = compMixSmoothed.getNextValue();
                const float makeup      = compMakeupSmoothed.getNextValue();
                const float punch       = compPunchSmoothed.getNextValue();

                const float shapedAttackMs = juce::jmax (0.6f, attackMs * 0.78f * (2.0f - modeRetention));
                const float attackCoeff    = std::exp (-1.0f / (0.001f * shapedAttackMs * sr));

                const float dryL = left[i];
                const float dryR = right != nullptr ? right[i] : dryL;

                compressorPunchMemory[0] = 0.92f * compressorPunchMemory[0] + 0.08f * dryL;
                compressorPunchMemory[1] = 0.92f * compressorPunchMemory[1] + 0.08f * dryR;

                const float transL = dryL - compressorPunchMemory[0];
                const float transR = dryR - compressorPunchMemory[1];

                const float drivenL = dryL + transL * punch * 0.45f * modePunch;
                const float drivenR = dryR + transR * punch * 0.45f * modePunch;

                float detectorL = drivenL;
                float detectorR = drivenR;

                if (p.sidechainMode > 0)
                {
                    for (int stage = 0; stage < hpfStages; ++stage)
                    {
                        auto& hpStateL = compDetectorHpfState[static_cast<size_t> (stage)][0];
                        auto& hpStateR = compDetectorHpfState[static_cast<size_t> (stage)][1];
                        hpStateL += detectorHpfCoeff * (detectorL - hpStateL);
                        hpStateR += detectorHpfCoeff * (detectorR - hpStateR);
                        detectorL -= hpStateL;
                        detectorR -= hpStateR;
                    }
                    for (int stage = 0; stage < lpfStages; ++stage)
                    {
                        auto& lpStateL = compDetectorLpfState[static_cast<size_t> (stage)][0];
                        auto& lpStateR = compDetectorLpfState[static_cast<size_t> (stage)][1];
                        lpStateL += detectorLpfCoeff * (detectorL - lpStateL);
                        lpStateR += detectorLpfCoeff * (detectorR - lpStateR);
                        detectorL = lpStateL;
                        detectorR = lpStateR;
                    }
                }

                const float absL = std::abs (detectorL);
                const float absR = std::abs (detectorR);
                const float peak = juce::jmax (absL, absR);
                const float rms  = std::sqrt ((detectorL * detectorL + detectorR * detectorR) * 0.5f);
                const float detector = 0.66f * rms + 0.34f * peak;

                const float crest = peak / juce::jmax (rms, 1.0e-5f);
                const float autoReleaseMs = juce::jlimit (35.0f, 450.0f,
                    releaseMs * juce::jmap (juce::jlimit (1.0f, 6.0f, crest), 1.0f, 6.0f, 1.18f, 0.44f));
                const float releaseBlendMs = 0.45f * releaseMs + 0.55f * autoReleaseMs;
                const float releaseCoeff   = std::exp (-1.0f / (0.001f * releaseBlendMs * sr));

                if (detector > compressorDetector)
                    compressorDetector = attackCoeff  * compressorDetector + (1.0f - attackCoeff)  * detector;
                else
                    compressorDetector = releaseCoeff * compressorDetector + (1.0f - releaseCoeff) * detector;

                const float envDb = gainToDb (compressorDetector);
                const float x     = envDb - thresholdDb;

                float overDb = 0.0f;
                if (x > -kneeDb * 0.5f)
                {
                    if (x < kneeDb * 0.5f)
                    {
                        const float k = x + kneeDb * 0.5f;
                        overDb = (k * k) / (2.0f * kneeDb);
                    }
                    else
                    {
                        overDb = x;
                    }
                }

                const float compressedDb = overDb - (overDb / juce::jmax (ratio, 1.0f));
                const float targetGain   = dbToGain (-compressedDb * qualityTightness);

                const float gainReleaseCoeff = std::exp (-1.0f / (0.001f * releaseBlendMs * 1.2f * sr));
                const float gainAttackCoeff  = std::exp (-1.0f / (0.001f * juce::jmax (0.25f, attackMs * 0.45f) * sr));

                if (targetGain < compressorGainState)
                    compressorGainState = gainAttackCoeff  * compressorGainState + (1.0f - gainAttackCoeff)  * targetGain;
                else
                    compressorGainState = gainReleaseCoeff * compressorGainState + (1.0f - gainReleaseCoeff) * targetGain;

                maxReduction = juce::jmax (maxReduction, -gainToDb (juce::jmax (compressorGainState, 1.0e-5f)));

                const float thickBlend = 0.012f + 0.01f * profile.evenDrive;
                const float thickenedL = (1.0f - thickBlend) * drivenL
                                       + thickBlend * saturateSmooth (drivenL * (1.25f + 0.25f * profile.oddDrive));
                const float thickenedR = (1.0f - thickBlend) * drivenR
                                       + thickBlend * saturateSmooth (drivenR * (1.25f + 0.25f * profile.oddDrive));

                const float wetL = thickenedL * compressorGainState * dbToGain (makeup);
                const float wetR = thickenedR * compressorGainState * dbToGain (makeup);

                left[i] = dryL * (1.0f - compMix) + wetL * compMix;
                if (right != nullptr)
                    right[i] = dryR * (1.0f - compMix) + wetR * compMix;
            }

            gainReductionMeter = 0.84f * gainReductionMeter
                               + 0.16f * juce::jlimit (0.0f, 1.0f, maxReduction / 18.0f);
        }

        void processGate (juce::AudioBuffer<float>& buf, const NovaConsoleParameters& p)
        {
            gateThresholdSmoothed.setTargetValue (p.gateThreshDb);
            gateReleaseSmoothed.setTargetValue   (p.gateReleaseMs);
            gateRangeSmoothed.setTargetValue     (p.gateRangeDb);
            gateAttackSmoothed.setTargetValue    (p.gateAttackMs);
            gateHoldSmoothed.setTargetValue      (p.gateHoldMs);

            const bool  expandMode = p.gateSmooth;
            const float sr = static_cast<float> (currentSampleRate);
            const auto  mode = static_cast<ConsoleMode> (juce::jlimit (0, 4, p.mode));

            float modeExpandSoftness = 1.0f, modeGateTightness = 1.0f;
            switch (mode)
            {
                case ConsoleMode::clean:    modeExpandSoftness=1.12f; modeGateTightness=0.92f; break;
                case ConsoleMode::british:  modeExpandSoftness=0.96f; modeGateTightness=1.10f; break;
                case ConsoleMode::tubeTape: modeExpandSoftness=1.20f; modeGateTightness=0.88f; break;
                case ConsoleMode::gold:     modeExpandSoftness=1.15f; modeGateTightness=0.95f; break;
                case ConsoleMode::modern:   modeExpandSoftness=1.04f; modeGateTightness=1.02f; break;
            }

            const int hpfStages = p.hpfSlope == 0 ? 1 : (p.hpfSlope == 1 ? 2 : 4);
            const int lpfStages = p.lpfSlope == 0 ? 1 : (p.lpfSlope == 1 ? 2 : 4);
            const float detectorHpfCoeff = juce::jlimit (0.0001f, 0.999f,
                1.0f - std::exp (-juce::MathConstants<float>::twoPi * p.hpfHz / sr));
            const float detectorLpfCoeff = juce::jlimit (0.0001f, 0.999f,
                1.0f - std::exp (-juce::MathConstants<float>::twoPi * p.lpfHz / sr));

            const int channels = juce::jmin (2, buf.getNumChannels());
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* data = buf.getWritePointer (ch);
                for (int i = 0; i < buf.getNumSamples(); ++i)
                {
                    const float thresholdDb = gateThresholdSmoothed.getNextValue();
                    const float releaseMs   = gateReleaseSmoothed.getNextValue();
                    const float rangeDb     = gateRangeSmoothed.getNextValue();
                    const float attackMs    = gateAttackSmoothed.getNextValue();
                    const float holdMs      = gateHoldSmoothed.getNextValue();

                    const float maxAttenDb   = juce::jmax (3.0f, -rangeDb);
                    const float userAttackMs = juce::jmax (0.1f, attackMs);
                    const float userReleaseMs= juce::jmax (8.0f, releaseMs);

                    const float effAttackMs = expandMode
                        ? userAttackMs  * (1.35f * modeExpandSoftness)
                        : userAttackMs  * (0.68f / juce::jmax (0.75f, modeGateTightness));
                    const float effReleaseMs = expandMode
                        ? userReleaseMs * (1.45f * modeExpandSoftness)
                        : userReleaseMs * (0.72f / juce::jmax (0.75f, modeGateTightness));

                    const float releaseCoeff  = std::exp (-1.0f / (0.001f * effReleaseMs * sr));
                    const float attackCoeff   = std::exp (-1.0f / (0.001f * effAttackMs * sr));
                    const int32_t holdSamples = static_cast<int32_t> ((holdMs * sr) / 1000.0f);
                    const float hysteresisDb  = expandMode
                        ? (4.2f * modeExpandSoftness)
                        : (2.0f / juce::jmax (0.75f, modeGateTightness));

                    const float x = data[i];
                    float detectorSample = x;

                    // Detector sidechain filter (internal signal, no external sidechain in tests)
                    if (p.sidechainMode > 0)
                    {
                        for (int stage = 0; stage < hpfStages; ++stage)
                        {
                            auto& hpState = gateDetectorHpfState[static_cast<size_t> (stage)]
                                                                 [static_cast<size_t> (ch)];
                            hpState += detectorHpfCoeff * (detectorSample - hpState);
                            detectorSample -= hpState;
                        }
                        for (int stage = 0; stage < lpfStages; ++stage)
                        {
                            auto& lpState = gateDetectorLpfState[static_cast<size_t> (stage)]
                                                                 [static_cast<size_t> (ch)];
                            lpState += detectorLpfCoeff * (detectorSample - lpState);
                            detectorSample = lpState;
                        }
                    }

                    const float level   = std::abs (detectorSample);
                    const float levelDb = gainToDb (level);

                    const bool aboveOpenThreshold  = levelDb > (thresholdDb + hysteresisDb);
                    const bool belowCloseThreshold = levelDb < (thresholdDb - hysteresisDb);

                    float target = 1.0f;
                    if (expandMode)
                    {
                        const float ratio       = juce::jlimit (2.0f, 6.0f, 2.0f + (maxAttenDb - 3.0f) * (4.0f / 33.0f));
                        const float kneeDb      = 8.0f * modeExpandSoftness;
                        const float belowDb     = juce::jmax (0.0f, thresholdDb - levelDb);
                        const float expansionDb = -juce::jmin (maxAttenDb, belowDb * (1.0f - 1.0f / ratio));
                        const float blend       = juce::jlimit (0.0f, 1.0f, belowDb / juce::jmax (1.0f, kneeDb));
                        const float blendShaped = blend * blend * (3.0f - 2.0f * blend);
                        target = dbToGain (expansionDb * blendShaped);
                    }
                    else
                    {
                        const float kneeDb      = 2.0f / juce::jmax (0.75f, modeGateTightness);
                        const float closeSpanDb = juce::jmax (4.0f, 10.0f / juce::jmax (0.75f, modeGateTightness));
                        const float close       = juce::jlimit (0.0f, 1.0f, (thresholdDb - levelDb + kneeDb) / closeSpanDb);
                        const float curve       = std::pow (close, 1.6f * modeGateTightness);
                        target = dbToGain (-juce::jmin (maxAttenDb, maxAttenDb * curve));
                    }

                    if (aboveOpenThreshold)
                    {
                        gateHoldCounter[static_cast<size_t> (ch)] = holdSamples;
                        target = 1.0f;
                    }
                    else if (!belowCloseThreshold || gateHoldCounter[static_cast<size_t> (ch)] > 0)
                    {
                        if (gateHoldCounter[static_cast<size_t> (ch)] > 0)
                            gateHoldCounter[static_cast<size_t> (ch)]--;
                        target = 1.0f;
                    }

                    float& gateEnvRef = gateEnv[static_cast<size_t> (ch)];
                    gateEnvRef = (target > gateEnvRef)
                        ? attackCoeff  * gateEnvRef + (1.0f - attackCoeff)  * target
                        : releaseCoeff * gateEnvRef + (1.0f - releaseCoeff) * target;
                    data[i] = x * gateEnvRef;
                }
            }
        }

        // ── Main process — verbatim signal chain from processBlock() ──────────
        void process (juce::AudioBuffer<float>& buf, const NovaConsoleParameters& p)
        {
            if (buf.getNumSamples() == 0) return;

            const auto reqMode = static_cast<ConsoleMode> (juce::jlimit (0, 4, p.mode));
            if (reqMode != modeTo)
            {
                modeFrom = modeTo; modeTo = reqMode;
                modeMorph.setCurrentAndTargetValue (0.0f);
                modeMorph.setTargetValue (1.0f);
            }

            const ModeProfile fromP  = profileForMode (modeFrom);
            const ModeProfile toP    = profileForMode (modeTo);
            const float       morph  = modeMorph.skip (buf.getNumSamples());
            const ModeProfile active = blendProfiles (fromP, toP, morph);

            if (!modeMorph.isSmoothing()) modeFrom = modeTo;

            updateCoefficients (p);

            // osFactor
            int osFactor = (p.oversampling == 2 ? 4 : (p.oversampling == 1 ? 2 : 1));
            if (p.quality == 0) osFactor = 1;

            // Input gain
            inputSmoothed.setTargetValue (dbToGain (p.inputDb));
            const int channels = juce::jmin (2, buf.getNumChannels());
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* data = buf.getWritePointer (ch);
                for (int i = 0; i < buf.getNumSamples(); ++i)
                    data[i] *= inputSmoothed.getNextValue();
            }

            if (p.preampOn)  processPreamp (buf, p, active, osFactor);
            if (p.filterOn)  processFilters (buf, p);
            if (p.eqOn)      processEq (buf, active);
            if (p.compOn)    processCompressor (buf, p, active);
            else             gainReductionMeter = 0.92f * gainReductionMeter;
            if (p.gateOn)    processGate (buf, p);

            // modeTrim + clip + output gain
            const float modeTrim = active.outputTrim;
            outputSmoothed.setTargetValue (dbToGain (p.outputDb));
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* data = buf.getWritePointer (ch);
                for (int i = 0; i < buf.getNumSamples(); ++i)
                {
                    data[i] = juce::jlimit (-1.35f, 1.35f, data[i] * modeTrim);
                    data[i] *= outputSmoothed.getNextValue();
                }
            }
        }
    };

    // ── Scenario runner ───────────────────────────────────────────────────────

    static Result runScenario (const char* name,
                               double sampleRate,
                               int blockSize,
                               int numBlocks,
                               const NovaConsoleParameters& params,
                               juce::AudioBuffer<float>& signal)
    {
        juce::AudioBuffer<float> bufOrig (signal.getNumChannels(), signal.getNumSamples());
        juce::AudioBuffer<float> bufEng  (signal.getNumChannels(), signal.getNumSamples());

        for (int ch = 0; ch < signal.getNumChannels(); ++ch)
        {
            std::memcpy (bufOrig.getWritePointer (ch), signal.getReadPointer (ch),
                         (size_t) signal.getNumSamples() * sizeof (float));
            std::memcpy (bufEng.getWritePointer (ch), signal.getReadPointer (ch),
                         (size_t) signal.getNumSamples() * sizeof (float));
        }

        OriginalState orig;
        orig.prepare (sampleRate, blockSize, params);

        for (int block = 0; block < numBlocks; ++block)
        {
            const int offset = block * blockSize;
            const int nSamps = juce::jmin (blockSize, signal.getNumSamples() - offset);
            if (nSamps <= 0) break;
            juce::AudioBuffer<float> slice (bufOrig.getArrayOfWritePointers(),
                                            bufOrig.getNumChannels(), offset, nSamps);
            orig.process (slice, params);
        }

        NovaConsoleDSP engine;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize,
                                      (juce::uint32) signal.getNumChannels() };
        engine.prepare (spec, params);
        engine.setParameters (params);

        for (int block = 0; block < numBlocks; ++block)
        {
            const int offset = block * blockSize;
            const int nSamps = juce::jmin (blockSize, signal.getNumSamples() - offset);
            if (nSamps <= 0) break;
            juce::AudioBuffer<float> slice (bufEng.getArrayOfWritePointers(),
                                            bufEng.getNumChannels(), offset, nSamps);
            engine.setParameters (params);
            engine.process (slice);
        }

        Result r;
        r.scenario = name;
        float peakDiff = 0.0f, rmsSq = 0.0f;
        int totalSamples = 0;

        for (int ch = 0; ch < signal.getNumChannels(); ++ch)
        {
            const float* a = bufOrig.getReadPointer (ch);
            const float* b = bufEng.getReadPointer (ch);
            for (int i = 0; i < signal.getNumSamples(); ++i)
            {
                const float d = std::abs (a[i] - b[i]);
                peakDiff = juce::jmax (peakDiff, d);
                rmsSq   += d * d;
            }
            totalSamples += signal.getNumSamples();
        }

        r.peakAbsDiff = peakDiff;
        r.rmsAbsDiff  = totalSamples > 0 ? std::sqrt (rmsSq / (float) totalSamples) : 0.0f;
        r.passed      = (r.peakAbsDiff == 0.0f);
        return r;
    }

    // ── Signal generators ─────────────────────────────────────────────────────

    static juce::AudioBuffer<float> makeSilence (int channels, int numSamples)
    {
        juce::AudioBuffer<float> buf (channels, numSamples);
        buf.clear();
        return buf;
    }

    static juce::AudioBuffer<float> makeImpulse (int channels, int numSamples)
    {
        juce::AudioBuffer<float> buf (channels, numSamples);
        buf.clear();
        for (int ch = 0; ch < channels; ++ch)
            buf.getWritePointer (ch)[0] = 1.0f;
        return buf;
    }

    static juce::AudioBuffer<float> makeSine (int channels, int numSamples,
                                              double sampleRate, double freqHz,
                                              float amplitude = 0.5f)
    {
        juce::AudioBuffer<float> buf (channels, numSamples);
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* data = buf.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = amplitude * std::sin (juce::MathConstants<double>::twoPi * freqHz * i / sampleRate);
        }
        return buf;
    }

    static juce::AudioBuffer<float> makeNoise (int channels, int numSamples, float amplitude = 0.5f)
    {
        juce::AudioBuffer<float> buf (channels, numSamples);
        uint32_t state = 0xDEADBEEFu;
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* data = buf.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                state ^= state << 13; state ^= state >> 17; state ^= state << 5;
                data[i] = amplitude * (static_cast<float> (state & 0xFFFFu) / 32767.5f - 1.0f);
            }
        }
        return buf;
    }

    // ── Full scenario matrix ──────────────────────────────────────────────────

    static bool runAll()
    {
        bool allPassed = true;
        const int totalSamples = 4096;

        auto check = [&] (Result r)
        {
            if (!r.passed)
                allPassed = false;
            assert (r.passed);
            return r;
        };

        // ════════════════════════════════════════════════════════════════════
        // Phase 4B scenarios (30): verified to still pass with full chain.
        // All use default preampOn=true; eqOn/filterOn per scenario.
        // ════════════════════════════════════════════════════════════════════

        // ── 1. Passthrough — both stages off, preamp on default ───────────────
        {
            NovaConsoleParameters p;
            p.filterOn = false; p.eqOn = false;
            auto sig = makeSine (2, totalSamples, 44100.0, 1000.0);
            check (runScenario ("passthrough-stereo-both-off", 44100.0, 512, 8, p, sig));
        }

        // ── 2. Silence → filters on at default ───────────────────────────────
        {
            NovaConsoleParameters p;
            p.filterOn = true; p.eqOn = false;
            auto sig = makeSilence (2, totalSamples);
            check (runScenario ("filter-silence-default", 44100.0, 512, 8, p, sig));
        }

        // ── 3. Sine 1 kHz → filter at factory defaults ────────────────────────
        {
            NovaConsoleParameters p;
            p.filterOn = true; p.eqOn = false;
            auto sig = makeSine (2, totalSamples, 44100.0, 1000.0);
            check (runScenario ("filter-default-1khz", 44100.0, 512, 8, p, sig));
        }

        // ── 4–6. HPF 500 Hz, sine 100 Hz — all three slopes ──────────────────
        for (int slope = 0; slope < 3; ++slope)
        {
            const char* names[] = { "hpf-500hz-slope12-100hz",
                                    "hpf-500hz-slope24-100hz",
                                    "hpf-500hz-slope48-100hz" };
            NovaConsoleParameters p;
            p.filterOn = true; p.eqOn = false;
            p.hpfHz = 500.0f; p.hpfSlope = slope;
            auto sig = makeSine (2, totalSamples, 44100.0, 100.0);
            check (runScenario (names[slope], 44100.0, 512, 8, p, sig));
        }

        // ── 7–8. LPF 2 kHz, sine 5 kHz — slopes 12 and 48 ───────────────────
        {
            NovaConsoleParameters p; p.filterOn = true; p.eqOn = false;
            p.lpfHz = 2000.0f; p.lpfSlope = 0;
            auto sig = makeSine (2, totalSamples, 44100.0, 5000.0);
            check (runScenario ("lpf-2khz-slope12-5khz", 44100.0, 512, 8, p, sig));
        }
        {
            NovaConsoleParameters p; p.filterOn = true; p.eqOn = false;
            p.lpfHz = 2000.0f; p.lpfSlope = 2;
            auto sig = makeSine (2, totalSamples, 44100.0, 5000.0);
            check (runScenario ("lpf-2khz-slope48-5khz", 44100.0, 512, 8, p, sig));
        }

        // ── 9. EQ only — silence in ───────────────────────────────────────────
        {
            NovaConsoleParameters p; p.filterOn = false; p.eqOn = true;
            auto sig = makeSilence (2, totalSamples);
            check (runScenario ("eq-silence", 44100.0, 512, 8, p, sig));
        }

        // ── 10–11. EQ low shelf boost / cut ──────────────────────────────────
        {
            NovaConsoleParameters p; p.filterOn = false; p.eqOn = true;
            p.lowDb = 6.0f;
            auto sig = makeSine (2, totalSamples, 44100.0, 90.0);
            check (runScenario ("eq-low-shelf-boost6", 44100.0, 512, 8, p, sig));
        }
        {
            NovaConsoleParameters p; p.filterOn = false; p.eqOn = true;
            p.lowDb = -6.0f;
            auto sig = makeSine (2, totalSamples, 44100.0, 90.0);
            check (runScenario ("eq-low-shelf-cut6", 44100.0, 512, 8, p, sig));
        }

        // ── 12. EQ high shelf boost ───────────────────────────────────────────
        {
            NovaConsoleParameters p; p.filterOn = false; p.eqOn = true;
            p.highDb = 6.0f;
            auto sig = makeSine (2, totalSamples, 44100.0, 10000.0);
            check (runScenario ("eq-high-shelf-boost6", 44100.0, 512, 8, p, sig));
        }

        // ── 13. EQ air shelf boost ────────────────────────────────────────────
        {
            NovaConsoleParameters p; p.filterOn = false; p.eqOn = true;
            p.airDb = 6.0f;
            auto sig = makeSine (2, totalSamples, 44100.0, 14500.0);
            check (runScenario ("eq-air-shelf-boost6", 44100.0, 512, 8, p, sig));
        }

        // ── 14. EQ all bands at maximum ───────────────────────────────────────
        {
            NovaConsoleParameters p; p.filterOn = false; p.eqOn = true;
            p.lowDb=12.0f; p.lowMidDb=12.0f; p.highMidDb=12.0f; p.highDb=12.0f; p.airDb=8.0f;
            auto sig = makeNoise (2, totalSamples);
            check (runScenario ("eq-all-bands-max", 44100.0, 512, 8, p, sig));
        }

        // ── 15. EQ all bands at minimum ───────────────────────────────────────
        {
            NovaConsoleParameters p; p.filterOn = false; p.eqOn = true;
            p.lowDb=-12.0f; p.lowMidDb=-12.0f; p.highMidDb=-12.0f; p.highDb=-12.0f; p.airDb=-8.0f;
            auto sig = makeNoise (2, totalSamples);
            check (runScenario ("eq-all-bands-min", 44100.0, 512, 8, p, sig));
        }

        // ── 16. Low band bell mode ────────────────────────────────────────────
        {
            NovaConsoleParameters p; p.filterOn = false; p.eqOn = true;
            p.lowDb = 6.0f; p.lowMode = 1;
            auto sig = makeSine (2, totalSamples, 44100.0, 90.0);
            check (runScenario ("eq-low-bell-boost6", 44100.0, 512, 8, p, sig));
        }

        // ── 17. High band bell mode ───────────────────────────────────────────
        {
            NovaConsoleParameters p; p.filterOn = false; p.eqOn = true;
            p.highDb = 6.0f; p.highMode = 1;
            auto sig = makeSine (2, totalSamples, 44100.0, 7600.0);
            check (runScenario ("eq-high-bell-boost6", 44100.0, 512, 8, p, sig));
        }

        // ── 18. Air band bell mode ────────────────────────────────────────────
        {
            NovaConsoleParameters p; p.filterOn = false; p.eqOn = true;
            p.airDb = 6.0f; p.airMode = 1;
            auto sig = makeSine (2, totalSamples, 44100.0, 14500.0);
            check (runScenario ("eq-air-bell-boost6", 44100.0, 512, 8, p, sig));
        }

        // ── 19–23. Mode-specific EQ — each of the five modes ─────────────────
        {
            const char* modeNames[] = { "mode-clean", "mode-british", "mode-tubetape",
                                        "mode-gold",  "mode-modern" };
            for (int m = 0; m < 5; ++m)
            {
                NovaConsoleParameters p; p.filterOn = false; p.eqOn = true;
                p.mode = m;
                p.lowDb = 3.0f; p.highDb = 3.0f;
                auto sig = makeNoise (2, totalSamples);
                check (runScenario (modeNames[m], 44100.0, 512, 8, p, sig));
            }
        }

        // ── 24. Mode switch mid-stream (morph blend) ──────────────────────────
        {
            NovaConsoleParameters pA; pA.filterOn = false; pA.eqOn = true; pA.mode = 1;
            NovaConsoleParameters pB = pA; pB.mode = 3;

            juce::AudioBuffer<float> sigA = makeNoise (2, totalSamples);
            juce::AudioBuffer<float> sigB (2, totalSamples);
            for (int ch = 0; ch < 2; ++ch)
                std::memcpy (sigB.getWritePointer (ch), sigA.getReadPointer (ch),
                             (size_t) totalSamples * sizeof (float));

            OriginalState orig; orig.prepare (44100.0, 512, pA);
            for (int block = 0; block < 4; ++block)
            {
                juce::AudioBuffer<float> sl (sigA.getArrayOfWritePointers(), 2, block*512, 512);
                orig.process (sl, pA);
            }
            for (int block = 4; block < 8; ++block)
            {
                juce::AudioBuffer<float> sl (sigA.getArrayOfWritePointers(), 2, block*512, 512);
                orig.process (sl, pB);
            }

            NovaConsoleDSP eng;
            juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            eng.prepare (spec, pA);
            for (int block = 0; block < 4; ++block)
            {
                eng.setParameters (pA);
                juce::AudioBuffer<float> sl (sigB.getArrayOfWritePointers(), 2, block*512, 512);
                eng.process (sl);
            }
            for (int block = 4; block < 8; ++block)
            {
                eng.setParameters (pB);
                juce::AudioBuffer<float> sl (sigB.getArrayOfWritePointers(), 2, block*512, 512);
                eng.process (sl);
            }

            Result r; r.scenario = "mode-switch-british-to-gold";
            float peak = 0.0f, rmsSq = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
            {
                const float* a = sigA.getReadPointer (ch);
                const float* b = sigB.getReadPointer (ch);
                for (int i = 0; i < totalSamples; ++i)
                {
                    const float d = std::abs (a[i] - b[i]);
                    peak = juce::jmax (peak, d); rmsSq += d * d;
                }
            }
            r.peakAbsDiff = peak;
            r.rmsAbsDiff  = std::sqrt (rmsSq / (float)(totalSamples * 2));
            r.passed      = (r.peakAbsDiff == 0.0f);
            check (r);
        }

        // ── 25. Filter + EQ together — full chain ─────────────────────────────
        {
            NovaConsoleParameters p; p.filterOn = true; p.eqOn = true;
            p.hpfHz = 80.0f; p.hpfSlope = 1;
            p.lowDb = 3.0f; p.highMidDb = -2.0f; p.airDb = 4.0f;
            auto sig = makeNoise (2, totalSamples);
            check (runScenario ("filter-eq-full-chain", 44100.0, 512, 8, p, sig));
        }

        // ── 26. Different block sizes ─────────────────────────────────────────
        {
            NovaConsoleParameters p; p.filterOn = true; p.eqOn = true;
            p.hpfHz = 100.0f; p.lowDb = 2.0f;
            const int blockSizes[] = { 64, 128, 1024, 2048 };
            const char* names[] = { "blocksize-64", "blocksize-128",
                                    "blocksize-1024", "blocksize-2048" };
            for (int bi = 0; bi < 4; ++bi)
            {
                const int bs = blockSizes[bi];
                const int blocks = totalSamples / bs;
                auto sig = makeNoise (2, blocks * bs);
                check (runScenario (names[bi], 44100.0, bs, blocks, p, sig));
            }
        }

        // ── 27–28. Alternative sample rates ───────────────────────────────────
        {
            NovaConsoleParameters p; p.filterOn = true; p.eqOn = true;
            p.hpfHz = 80.0f; p.highDb = 3.0f;
            for (double sr : { 48000.0, 96000.0 })
            {
                auto sig = makeNoise (2, totalSamples);
                check (runScenario (sr == 48000.0 ? "sr-48000" : "sr-96000",
                                    sr, 512, 8, p, sig));
            }
        }

        // ── 29. Mono buffer ───────────────────────────────────────────────────
        {
            NovaConsoleParameters p; p.filterOn = true; p.eqOn = true;
            p.hpfHz = 80.0f; p.lowDb = 2.0f;
            auto sig = makeNoise (1, totalSamples);
            check (runScenario ("mono-filter-eq", 44100.0, 512, 8, p, sig));
        }

        // ── 30. Impulse — EQ only ─────────────────────────────────────────────
        {
            NovaConsoleParameters p; p.filterOn = false; p.eqOn = true;
            p.lowDb = 6.0f; p.highDb = -3.0f; p.airDb = 4.0f;
            auto sig = makeImpulse (2, totalSamples);
            check (runScenario ("impulse-eq", 44100.0, 512, 8, p, sig));
        }

        // ════════════════════════════════════════════════════════════════════
        // Phase 4C scenarios (50): Preamp + Gate
        // ════════════════════════════════════════════════════════════════════

        // ── Preamp: basic ─────────────────────────────────────────────────────

        // 31. Preamp only — silence
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            auto sig = makeSilence (2, totalSamples);
            check (runScenario ("preamp-silence", 44100.0, 512, 8, p, sig));
        }

        // 32. Preamp only — impulse, default drive
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            auto sig = makeImpulse (2, totalSamples);
            check (runScenario ("preamp-impulse-default-drive", 44100.0, 512, 8, p, sig));
        }

        // 33. Preamp off — verify passthrough (no saturation)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            auto sig = makeSine (2, totalSamples, 44100.0, 440.0);
            check (runScenario ("preamp-off-passthrough", 44100.0, 512, 8, p, sig));
        }

        // 34. Preamp drive minimum (drive=0)
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.drive = 0.0f;
            auto sig = makeSine (2, totalSamples, 44100.0, 440.0, 0.3f);
            check (runScenario ("preamp-drive-min", 44100.0, 512, 8, p, sig));
        }

        // 35. Preamp drive maximum (drive=100)
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.drive = 100.0f;
            auto sig = makeSine (2, totalSamples, 44100.0, 440.0, 0.3f);
            check (runScenario ("preamp-drive-max", 44100.0, 512, 8, p, sig));
        }

        // 36. Preamp drive midpoint (drive=50)
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.drive = 50.0f;
            auto sig = makeNoise (2, totalSamples, 0.4f);
            check (runScenario ("preamp-drive-50", 44100.0, 512, 8, p, sig));
        }

        // 37. Preamp color=0 (darkest)
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.drive = 40.0f; p.color = 0.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("preamp-color-min", 44100.0, 512, 8, p, sig));
        }

        // 38. Preamp color=100 (brightest)
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.drive = 40.0f; p.color = 100.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("preamp-color-max", 44100.0, 512, 8, p, sig));
        }

        // 39. Preamp trim +6 dB
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.trimDb = 6.0f;
            auto sig = makeSine (2, totalSamples, 44100.0, 440.0, 0.2f);
            check (runScenario ("preamp-trim-plus6", 44100.0, 512, 8, p, sig));
        }

        // 40. Preamp trim -6 dB
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.trimDb = -6.0f;
            auto sig = makeSine (2, totalSamples, 44100.0, 440.0, 0.2f);
            check (runScenario ("preamp-trim-minus6", 44100.0, 512, 8, p, sig));
        }

        // 41. Preamp oversampling x2
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.oversampling = 1; p.quality = 1;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("preamp-os-x2", 44100.0, 512, 8, p, sig));
        }

        // 42. Preamp oversampling x4
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.oversampling = 2; p.quality = 1;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("preamp-os-x4", 44100.0, 512, 8, p, sig));
        }

        // 43. Preamp quality=0 forces osFactor=1
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.oversampling = 2; p.quality = 0; // even with os=2, quality=0 resets to 1x
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("preamp-quality0-forces-os1", 44100.0, 512, 8, p, sig));
        }

        // 44–48. Preamp mode interaction — each of the five modes
        {
            const char* modeNames[] = {
                "preamp-mode-clean", "preamp-mode-british", "preamp-mode-tubetape",
                "preamp-mode-gold",  "preamp-mode-modern"
            };
            for (int m = 0; m < 5; ++m)
            {
                NovaConsoleParameters p;
                p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
                p.mode = m; p.drive = 55.0f;
                auto sig = makeNoise (2, totalSamples, 0.3f);
                check (runScenario (modeNames[m], 44100.0, 512, 8, p, sig));
            }
        }

        // 49. Preamp + Filter chain
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = true; p.eqOn = false; p.gateOn = false;
            p.drive = 45.0f; p.hpfHz = 80.0f; p.hpfSlope = 1;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("preamp-filter-chain", 44100.0, 512, 8, p, sig));
        }

        // 50. Preamp + EQ chain
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = true; p.gateOn = false;
            p.drive = 45.0f; p.lowDb = 3.0f; p.highDb = 2.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("preamp-eq-chain", 44100.0, 512, 8, p, sig));
        }

        // 51. Input gain +6 dB
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.inputDb = 6.0f;
            auto sig = makeSine (2, totalSamples, 44100.0, 440.0, 0.2f);
            check (runScenario ("input-gain-plus6", 44100.0, 512, 8, p, sig));
        }

        // 52. Output gain -6 dB
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.outputDb = -6.0f;
            auto sig = makeSine (2, totalSamples, 44100.0, 440.0, 0.3f);
            check (runScenario ("output-gain-minus6", 44100.0, 512, 8, p, sig));
        }

        // 53. Preamp mono
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.drive = 45.0f;
            auto sig = makeNoise (1, totalSamples, 0.3f);
            check (runScenario ("preamp-mono", 44100.0, 512, 8, p, sig));
        }

        // 54. Preamp SR 48000
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.drive = 45.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("preamp-sr-48000", 48000.0, 512, 8, p, sig));
        }

        // 55. Preamp SR 96000
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            p.drive = 45.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("preamp-sr-96000", 96000.0, 512, 8, p, sig));
        }

        // ── Gate: basic ───────────────────────────────────────────────────────

        // 56. Gate off — verify no effect
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = false;
            auto sig = makeSine (2, totalSamples, 44100.0, 440.0, 0.3f);
            check (runScenario ("gate-off-passthrough", 44100.0, 512, 8, p, sig));
        }

        // 57. Gate on — silence input (below threshold, gate closes fully)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateThreshDb = -30.0f; p.gateRangeDb = -60.0f; p.gateSmooth = false;
            auto sig = makeSilence (2, totalSamples);
            check (runScenario ("gate-silence-below-threshold", 44100.0, 512, 8, p, sig));
        }

        // 58. Gate on — loud sine, well above threshold (gate stays open)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateThreshDb = -42.0f; p.gateSmooth = false;
            auto sig = makeSine (2, totalSamples, 44100.0, 440.0, 0.5f);
            check (runScenario ("gate-loud-signal-above-threshold", 44100.0, 512, 8, p, sig));
        }

        // 59. Gate expand mode (gateSmooth=true) — noise input
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateSmooth = true; p.gateThreshDb = -30.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("gate-expand-mode-noise", 44100.0, 512, 8, p, sig));
        }

        // 60. Gate hard mode (gateSmooth=false) — noise input
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateSmooth = false; p.gateThreshDb = -30.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("gate-hard-mode-noise", 44100.0, 512, 8, p, sig));
        }

        // 61. Gate attack fast (1 ms)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateThreshDb = -30.0f; p.gateAttackMs = 1.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("gate-attack-fast-1ms", 44100.0, 512, 8, p, sig));
        }

        // 62. Gate attack slow (100 ms)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateThreshDb = -30.0f; p.gateAttackMs = 100.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("gate-attack-slow-100ms", 44100.0, 512, 8, p, sig));
        }

        // 63. Gate release fast (10 ms)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateThreshDb = -30.0f; p.gateReleaseMs = 10.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("gate-release-fast-10ms", 44100.0, 512, 8, p, sig));
        }

        // 64. Gate release slow (500 ms)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateThreshDb = -30.0f; p.gateReleaseMs = 500.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("gate-release-slow-500ms", 44100.0, 512, 8, p, sig));
        }

        // 65. Gate hold long (200 ms)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateThreshDb = -30.0f; p.gateHoldMs = 200.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("gate-hold-200ms", 44100.0, 512, 8, p, sig));
        }

        // 66. Gate range -6 dB (shallow)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateThreshDb = -30.0f; p.gateRangeDb = -6.0f; p.gateSmooth = false;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("gate-range-minus6db", 44100.0, 512, 8, p, sig));
        }

        // 67. Gate range -60 dB (deep)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateThreshDb = -30.0f; p.gateRangeDb = -60.0f; p.gateSmooth = false;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("gate-range-minus60db", 44100.0, 512, 8, p, sig));
        }

        // 68–72. Gate per-mode behavior
        {
            const char* modeNames[] = {
                "gate-mode-clean", "gate-mode-british", "gate-mode-tubetape",
                "gate-mode-gold",  "gate-mode-modern"
            };
            for (int m = 0; m < 5; ++m)
            {
                NovaConsoleParameters p;
                p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
                p.mode = m; p.gateThreshDb = -30.0f; p.gateSmooth = true;
                auto sig = makeNoise (2, totalSamples, 0.3f);
                check (runScenario (modeNames[m], 44100.0, 512, 8, p, sig));
            }
        }

        // 73. Gate mono
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateThreshDb = -30.0f; p.gateSmooth = true;
            auto sig = makeNoise (1, totalSamples, 0.3f);
            check (runScenario ("gate-mono", 44100.0, 512, 8, p, sig));
        }

        // 74. Gate SR 48000
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateThreshDb = -30.0f; p.gateSmooth = true;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("gate-sr-48000", 48000.0, 512, 8, p, sig));
        }

        // 75. Gate SR 96000
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateThreshDb = -30.0f; p.gateSmooth = true;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("gate-sr-96000", 96000.0, 512, 8, p, sig));
        }

        // 76. Gate block size 64
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateThreshDb = -30.0f; p.gateSmooth = true;
            auto sig = makeNoise (2, 4096, 0.3f);
            check (runScenario ("gate-blocksize-64", 44100.0, 64, 64, p, sig));
        }

        // 77. Gate block size 2048
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false; p.gateOn = true;
            p.gateThreshDb = -30.0f; p.gateSmooth = true;
            auto sig = makeNoise (2, 4096, 0.3f);
            check (runScenario ("gate-blocksize-2048", 44100.0, 2048, 2, p, sig));
        }

        // 78. Gate + Filter chain (detector uses filter params)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = true; p.eqOn = false; p.gateOn = true;
            p.hpfHz = 100.0f; p.hpfSlope = 1; p.gateThreshDb = -30.0f; p.gateSmooth = false;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("gate-filter-chain", 44100.0, 512, 8, p, sig));
        }

        // ── Full chain: all stages together ───────────────────────────────────

        // 79. Full chain: Preamp + Filter + EQ + Gate, stereo, British mode
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = true; p.eqOn = true; p.gateOn = true;
            p.mode = 1; // british
            p.drive = 45.0f; p.color = 60.0f;
            p.hpfHz = 80.0f; p.hpfSlope = 1;
            p.lowDb = 2.0f; p.highDb = 2.0f;
            p.gateThreshDb = -40.0f; p.gateSmooth = true;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("full-chain-british-stereo", 44100.0, 512, 8, p, sig));
        }

        // 80. Full chain: all stages, tube tape mode, SR 48000
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = true; p.eqOn = true; p.gateOn = true;
            p.mode = 2; // tubeTape
            p.drive = 55.0f; p.oversampling = 1; p.quality = 1;
            p.hpfHz = 60.0f; p.lowDb = 3.0f; p.airDb = 2.0f;
            p.gateThreshDb = -35.0f; p.gateRangeDb = -24.0f; p.gateSmooth = false;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("full-chain-tubetape-48k", 48000.0, 512, 8, p, sig));
        }

        // ════════════════════════════════════════════════════════════════════
        // Phase 4D scenarios: Compressor
        // ════════════════════════════════════════════════════════════════════

        // ── Compressor: basic ─────────────────────────────────────────────────

        // 81. Comp off — no effect on output
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = false; p.gateOn = false;
            auto sig = makeSine (2, totalSamples, 44100.0, 440.0, 0.3f);
            check (runScenario ("comp-off-passthrough", 44100.0, 512, 8, p, sig));
        }

        // 82. Comp on — silence (no gain reduction)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            auto sig = makeSilence (2, totalSamples);
            check (runScenario ("comp-silence", 44100.0, 512, 8, p, sig));
        }

        // 83. Comp on — impulse
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            auto sig = makeImpulse (2, totalSamples);
            check (runScenario ("comp-impulse-default", 44100.0, 512, 8, p, sig));
        }

        // 84. Comp on — sine, loud (strong compression)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.compRatio = 8.0f;
            auto sig = makeSine (2, totalSamples, 44100.0, 440.0, 0.5f);
            check (runScenario ("comp-sine-loud-high-ratio", 44100.0, 512, 8, p, sig));
        }

        // 85. Comp threshold minimum (-60 dB)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -60.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-threshold-min", 44100.0, 512, 8, p, sig));
        }

        // 86. Comp threshold default (-16 dB)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            // compThreshDb default = -16.0f (from NovaConsoleParameters)
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-threshold-default", 44100.0, 512, 8, p, sig));
        }

        // 87. Comp threshold max (0 dB — above peak, no GR)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = 0.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-threshold-max", 44100.0, 512, 8, p, sig));
        }

        // 88. Comp ratio minimum (1:1)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.compRatio = 1.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-ratio-min-1to1", 44100.0, 512, 8, p, sig));
        }

        // 89. Comp ratio default (4:1)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f;
            // compRatio default = 4.0f
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-ratio-default-4to1", 44100.0, 512, 8, p, sig));
        }

        // 90. Comp ratio max (20:1)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.compRatio = 20.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-ratio-max-20to1", 44100.0, 512, 8, p, sig));
        }

        // 91. Comp attack minimum (0.1 ms)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.compAttackMs = 0.1f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-attack-min-0.1ms", 44100.0, 512, 8, p, sig));
        }

        // 92. Comp attack default (15 ms)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f;
            // compAttackMs default = 15.0f
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-attack-default-15ms", 44100.0, 512, 8, p, sig));
        }

        // 93. Comp attack maximum (150 ms)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.compAttackMs = 150.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-attack-max-150ms", 44100.0, 512, 8, p, sig));
        }

        // 94. Comp release minimum (20 ms)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.compReleaseMs = 20.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-release-min-20ms", 44100.0, 512, 8, p, sig));
        }

        // 95. Comp release default (180 ms)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f;
            // compReleaseMs default = 180.0f
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-release-default-180ms", 44100.0, 512, 8, p, sig));
        }

        // 96. Comp release maximum (1000 ms)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.compReleaseMs = 1000.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-release-max-1000ms", 44100.0, 512, 8, p, sig));
        }

        // 97. Comp punch minimum (0)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.compPunch = 0.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-punch-min", 44100.0, 512, 8, p, sig));
        }

        // 98. Comp punch default (35)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f;
            // compPunch default = 35.0f
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-punch-default-35", 44100.0, 512, 8, p, sig));
        }

        // 99. Comp punch maximum (100)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.compPunch = 100.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-punch-max-100", 44100.0, 512, 8, p, sig));
        }

        // 100. Comp makeup gain +6 dB
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.compMakeupDb = 6.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-makeup-plus6", 44100.0, 512, 8, p, sig));
        }

        // 101. Comp makeup gain -6 dB
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.compMakeupDb = -6.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-makeup-minus6", 44100.0, 512, 8, p, sig));
        }

        // 102. Parallel compression — mix 50%
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.compRatio = 8.0f; p.compMix = 50.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-parallel-mix-50pct", 44100.0, 512, 8, p, sig));
        }

        // 103. Parallel compression — mix 0% (dry only)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.compRatio = 8.0f; p.compMix = 0.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-parallel-mix-0pct", 44100.0, 512, 8, p, sig));
        }

        // 104. Sidechain off (sidechainMode=0) — detector reads driven signal directly
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.sidechainMode = 0;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-sidechain-off", 44100.0, 512, 8, p, sig));
        }

        // 105. Sidechain internal (sidechainMode=1, no external buffer)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.sidechainMode = 1;
            p.hpfHz = 100.0f; p.hpfSlope = 1;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-sidechain-internal", 44100.0, 512, 8, p, sig));
        }

        // 106. Quality eco (tightness 0.92)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.quality = 0;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-quality-eco", 44100.0, 512, 8, p, sig));
        }

        // 107. Quality master (tightness 1.03)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f; p.quality = 2;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-quality-master", 44100.0, 512, 8, p, sig));
        }

        // 108–112. Per-mode compressor (mode profile affects transientPunch/Retention)
        {
            const char* modeNames[] = {
                "comp-mode-clean", "comp-mode-british", "comp-mode-tubetape",
                "comp-mode-gold",  "comp-mode-modern"
            };
            for (int m = 0; m < 5; ++m)
            {
                NovaConsoleParameters p;
                p.preampOn = false; p.filterOn = false; p.eqOn = false;
                p.compOn = true; p.gateOn = false;
                p.mode = m; p.compThreshDb = -20.0f; p.compRatio = 4.0f;
                auto sig = makeNoise (2, totalSamples, 0.3f);
                check (runScenario (modeNames[m], 44100.0, 512, 8, p, sig));
            }
        }

        // 113. Comp mono
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f;
            auto sig = makeNoise (1, totalSamples, 0.3f);
            check (runScenario ("comp-mono", 44100.0, 512, 8, p, sig));
        }

        // 114. Comp SR 48000
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-sr-48000", 48000.0, 512, 8, p, sig));
        }

        // 115. Comp SR 96000
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-sr-96000", 96000.0, 512, 8, p, sig));
        }

        // 116. Comp block size 64
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f;
            auto sig = makeNoise (2, 4096, 0.3f);
            check (runScenario ("comp-blocksize-64", 44100.0, 64, 64, p, sig));
        }

        // 117. Comp block size 2048
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.compThreshDb = -20.0f;
            auto sig = makeNoise (2, 4096, 0.3f);
            check (runScenario ("comp-blocksize-2048", 44100.0, 2048, 2, p, sig));
        }

        // ── Compressor + stage interactions ──────────────────────────────────

        // 118. Filter + Comp: detector uses HPF-filtered signal (sidechainMode=1)
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = true; p.eqOn = false;
            p.compOn = true; p.gateOn = false;
            p.hpfHz = 100.0f; p.hpfSlope = 1;
            p.compThreshDb = -20.0f; p.sidechainMode = 1;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-filter-sidechain-chain", 44100.0, 512, 8, p, sig));
        }

        // 119. EQ + Comp: EQ before compression
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = true;
            p.compOn = true; p.gateOn = false;
            p.lowDb = 4.0f; p.compThreshDb = -20.0f; p.compRatio = 4.0f;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-eq-chain", 44100.0, 512, 8, p, sig));
        }

        // 120. Gate + Comp: both active
        {
            NovaConsoleParameters p;
            p.preampOn = false; p.filterOn = false; p.eqOn = false;
            p.compOn = true; p.gateOn = true;
            p.compThreshDb = -20.0f; p.gateThreshDb = -40.0f; p.gateSmooth = true;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("comp-gate-both-active", 44100.0, 512, 8, p, sig));
        }

        // 121. Full chain Phase 4D: all stages active
        {
            NovaConsoleParameters p;
            p.preampOn = true; p.filterOn = true; p.eqOn = true;
            p.compOn = true; p.gateOn = true;
            p.mode = 1; // british
            p.drive = 40.0f;
            p.hpfHz = 80.0f; p.hpfSlope = 1;
            p.lowDb = 2.0f; p.highDb = 1.5f;
            p.compThreshDb = -18.0f; p.compRatio = 4.0f; p.compMakeupDb = 3.0f;
            p.gateThreshDb = -45.0f; p.gateSmooth = true;
            auto sig = makeNoise (2, totalSamples, 0.3f);
            check (runScenario ("full-chain-4D-british-all-stages", 44100.0, 512, 8, p, sig));
        }

        return allPassed;
    }
};
