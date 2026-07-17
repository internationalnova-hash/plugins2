#pragma once

// Regression test — Phase 4B: Filter + EQ stages.
//
// Verifies NovaConsoleDSP produces sample-identical output to the original
// Nova Console PluginProcessor algorithm for the Filter and EQ stages.
//
// Usage: NovaConsoleRegressionTest::runAll();   // asserts on failure
//
// The OriginalState struct replicates the filter+EQ code from
// Nova Console/Source/PluginProcessor.cpp verbatim.  The only substitution
// is apvts.getRawParameterValue(id)->load() → params.field.
//
// Both OriginalState and NovaConsoleDSP are seeded identically via
// prepare(spec, initialParams).  Because both engines call the exact same
// JUCE IIR coefficient functions with the same inputs, peakAbsDiff == 0.0f
// is expected for all deterministic (non-RNG) filter/EQ scenarios.

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
    // Extracted from Nova Console/Source/PluginProcessor.cpp at Phase 4B time.
    struct OriginalState
    {
        juce::dsp::StateVariableTPTFilter<float> hpf[2];
        juce::dsp::StateVariableTPTFilter<float> lpf[2];
        juce::dsp::StateVariableTPTFilter<float> hpfStage2[2];
        juce::dsp::StateVariableTPTFilter<float> hpfStage3[2];
        juce::dsp::StateVariableTPTFilter<float> hpfStage4[2];
        juce::dsp::StateVariableTPTFilter<float> lpfStage2[2];
        juce::dsp::StateVariableTPTFilter<float> lpfStage3[2];
        juce::dsp::StateVariableTPTFilter<float> lpfStage4[2];

        juce::dsp::IIR::Filter<float> lowShelf[2];
        juce::dsp::IIR::Filter<float> lowMidPeak[2];
        juce::dsp::IIR::Filter<float> highMidPeak[2];
        juce::dsp::IIR::Filter<float> highShelf[2];
        juce::dsp::IIR::Filter<float> airShelf[2];

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

        // Mode morph
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

        // Dirty-check cache
        float lastHpfHz=-1.f, lastLpfHz=-1.f;
        float lastLowDb=999.f, lastLowMidDb=999.f, lastHighMidDb=999.f;
        float lastHighDb=999.f, lastAirDb=999.f;
        float lastLowFreq=-1.f, lastLowMidFreq=-1.f, lastHighMidFreq=-1.f;
        float lastHighFreq=-1.f, lastAirFreq=-1.f;
        float lastLowQ=-1.f, lastLowMidQ=-1.f, lastHighMidQ=-1.f;
        float lastHighQ=-1.f, lastAirQ=-1.f;
        int   lastHpfSlope=-1, lastLpfSlope=-1;
        int   lastLowMode=-1, lastHighMode=-1, lastAirMode=-1;

        double currentSampleRate = 44100.0;

        static float dbToGain (float db) noexcept
        {
            return juce::Decibels::decibelsToGain (db);
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

        void prepare (double sampleRate, int samplesPerBlock, const NovaConsoleParameters& init)
        {
            currentSampleRate = juce::jmax (1.0, sampleRate);
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)samplesPerBlock, 1 };

            for (int ch = 0; ch < 2; ++ch)
            {
                hpf[ch].reset(); lpf[ch].reset();
                hpf[ch].setType      (juce::dsp::StateVariableTPTFilterType::highpass);
                lpf[ch].setType      (juce::dsp::StateVariableTPTFilterType::lowpass);
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
                        ? juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, lowFreq, lowQ_, dbToGain (lowDb))
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

            if (p.filterOn) processFilters (buf, p);
            if (p.eqOn)     processEq (buf, active);
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
        // Clone input for each engine
        juce::AudioBuffer<float> bufOrig (signal.getNumChannels(), signal.getNumSamples());
        juce::AudioBuffer<float> bufEng  (signal.getNumChannels(), signal.getNumSamples());

        for (int ch = 0; ch < signal.getNumChannels(); ++ch)
        {
            std::memcpy (bufOrig.getWritePointer (ch),
                         signal.getReadPointer (ch),
                         (size_t) signal.getNumSamples() * sizeof (float));
            std::memcpy (bufEng.getWritePointer (ch),
                         signal.getReadPointer (ch),
                         (size_t) signal.getNumSamples() * sizeof (float));
        }

        // Run original
        OriginalState orig;
        orig.prepare (sampleRate, blockSize, params);

        for (int block = 0; block < numBlocks; ++block)
        {
            const int offset = block * blockSize;
            const int nSamps = juce::jmin (blockSize, signal.getNumSamples() - offset);
            if (nSamps <= 0) break;

            juce::AudioBuffer<float> slice (bufOrig.getArrayOfWritePointers(),
                                            bufOrig.getNumChannels(),
                                            offset, nSamps);
            orig.process (slice, params);
        }

        // Run extracted engine
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
                                            bufEng.getNumChannels(),
                                            offset, nSamps);
            engine.setParameters (params);
            engine.process (slice);
        }

        // Compare
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
        // Fixed-seed noise — independent of engine rng
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

        // ── 1. Passthrough — both stages off ─────────────────────────────────
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

        // ── 3. Sine 1 kHz → filter at factory defaults (HPF 20 Hz, LPF 20 kHz) ─
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
            // Two identical engines started in British, then switched to Gold.
            // The morph blend must be bit-identical.
            NovaConsoleParameters pA; pA.filterOn = false; pA.eqOn = true; pA.mode = 1; // British
            NovaConsoleParameters pB = pA; pB.mode = 3; // Gold

            juce::AudioBuffer<float> sigA = makeNoise (2, totalSamples);
            juce::AudioBuffer<float> sigB (2, totalSamples);
            for (int ch = 0; ch < 2; ++ch)
                std::memcpy (sigB.getWritePointer (ch), sigA.getReadPointer (ch),
                             (size_t) totalSamples * sizeof (float));

            // Run OriginalState
            OriginalState orig; orig.prepare (44100.0, 512, pA);
            for (int block = 0; block < 4; ++block)   // first 4 blocks: British
            {
                juce::AudioBuffer<float> sl (sigA.getArrayOfWritePointers(), 2, block*512, 512);
                orig.process (sl, pA);
            }
            for (int block = 4; block < 8; ++block)   // switch to Gold
            {
                juce::AudioBuffer<float> sl (sigA.getArrayOfWritePointers(), 2, block*512, 512);
                orig.process (sl, pB);
            }

            // Run extracted engine
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
            for (int blockSize : { 64, 128, 1024, 2048 })
            {
                const char* names[] = { "blocksize-64", "blocksize-128",
                                        "blocksize-1024", "blocksize-2048" };
                const int blocks = totalSamples / blockSize;
                auto sig = makeNoise (2, blocks * blockSize);
                check (runScenario (names[blockSize == 64 ? 0 : blockSize == 128 ? 1 :
                                          blockSize == 1024 ? 2 : 3],
                                    44100.0, blockSize, blocks, p, sig));
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

        // ── 30. Impulse — EQ only (impulse response comparison) ───────────────
        {
            NovaConsoleParameters p; p.filterOn = false; p.eqOn = true;
            p.lowDb = 6.0f; p.highDb = -3.0f; p.airDb = 4.0f;
            auto sig = makeImpulse (2, totalSamples);
            check (runScenario ("impulse-eq", 44100.0, 512, 8, p, sig));
        }

        return allPassed;
    }
};
