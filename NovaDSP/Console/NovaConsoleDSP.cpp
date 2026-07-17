#include "NovaConsoleDSP.h"

#include <cmath>

namespace
{
    float dbToGain (float db) noexcept
    {
        return juce::Decibels::decibelsToGain (db);
    }

    float gainToDb (float gain) noexcept
    {
        return juce::Decibels::gainToDecibels (juce::jmax (gain, 1.0e-6f));
    }
}

// ── Static profile helpers ────────────────────────────────────────────────────
// Verbatim from Nova Console/Source/PluginProcessor.cpp :: profileForMode()

NovaConsoleDSP::ModeProfile NovaConsoleDSP::profileForMode (ConsoleMode mode) noexcept
{
    ModeProfile p {};

    switch (mode)
    {
        case ConsoleMode::clean:
            p.warmth = 0.09f; p.presence = 1.02f; p.eqWidth = 1.03f;
            p.upperMidAggression = 0.96f; p.airSmoothness = 1.04f; p.lowMidWeight = 0.95f;
            p.oddDrive = 0.45f; p.evenDrive = 0.34f; p.clipSoftness = 1.06f;
            p.transientPunch = 0.98f; p.transientRetention = 1.06f;
            p.stereoWidthBias = 1.02f; p.centerWeight = 0.98f; p.sideSoftness = 1.00f;
            p.crosstalkBias = 0.90f; p.outputTrim = 1.00f;
            break;

        case ConsoleMode::british:
            p.warmth = 0.30f; p.presence = 1.09f; p.eqWidth = 0.95f;
            p.upperMidAggression = 1.08f; p.airSmoothness = 0.96f; p.lowMidWeight = 0.98f;
            p.oddDrive = 0.70f; p.evenDrive = 0.40f; p.clipSoftness = 0.90f;
            p.transientPunch = 1.18f; p.transientRetention = 0.95f;
            p.stereoWidthBias = 0.98f; p.centerWeight = 1.04f; p.sideSoftness = 0.96f;
            p.crosstalkBias = 1.08f; p.outputTrim = 0.975f;
            break;

        case ConsoleMode::tubeTape:
            p.warmth = 0.39f; p.presence = 0.93f; p.eqWidth = 0.98f;
            p.upperMidAggression = 0.92f; p.airSmoothness = 1.10f; p.lowMidWeight = 1.08f;
            p.oddDrive = 0.46f; p.evenDrive = 0.76f; p.clipSoftness = 1.14f;
            p.transientPunch = 0.90f; p.transientRetention = 0.92f;
            p.stereoWidthBias = 0.99f; p.centerWeight = 1.01f; p.sideSoftness = 1.05f;
            p.crosstalkBias = 1.02f; p.outputTrim = 0.955f;
            break;

        case ConsoleMode::gold:
            p.warmth = 0.24f; p.presence = 1.06f; p.eqWidth = 0.97f;
            p.upperMidAggression = 1.00f; p.airSmoothness = 1.08f; p.lowMidWeight = 1.02f;
            p.oddDrive = 0.52f; p.evenDrive = 0.58f; p.clipSoftness = 1.10f;
            p.transientPunch = 1.00f; p.transientRetention = 0.98f;
            p.stereoWidthBias = 1.01f; p.centerWeight = 1.00f; p.sideSoftness = 1.03f;
            p.crosstalkBias = 0.98f; p.outputTrim = 0.97f;
            break;

        case ConsoleMode::modern:
            p.warmth = 0.14f; p.presence = 1.11f; p.eqWidth = 1.04f;
            p.upperMidAggression = 1.01f; p.airSmoothness = 1.03f; p.lowMidWeight = 0.97f;
            p.oddDrive = 0.48f; p.evenDrive = 0.44f; p.clipSoftness = 1.04f;
            p.transientPunch = 1.03f; p.transientRetention = 1.08f;
            p.stereoWidthBias = 1.06f; p.centerWeight = 0.97f; p.sideSoftness = 1.01f;
            p.crosstalkBias = 0.92f; p.outputTrim = 0.985f;
            break;
    }

    return p;
}

NovaConsoleDSP::ModeProfile NovaConsoleDSP::blendProfiles (const ModeProfile& a,
                                                            const ModeProfile& b,
                                                            float t) noexcept
{
    const float m = juce::jlimit (0.0f, 1.0f, t);
    ModeProfile p {};
    p.warmth             = juce::jmap (m, a.warmth,             b.warmth);
    p.presence           = juce::jmap (m, a.presence,           b.presence);
    p.eqWidth            = juce::jmap (m, a.eqWidth,            b.eqWidth);
    p.upperMidAggression = juce::jmap (m, a.upperMidAggression, b.upperMidAggression);
    p.airSmoothness      = juce::jmap (m, a.airSmoothness,      b.airSmoothness);
    p.lowMidWeight       = juce::jmap (m, a.lowMidWeight,       b.lowMidWeight);
    p.oddDrive           = juce::jmap (m, a.oddDrive,           b.oddDrive);
    p.evenDrive          = juce::jmap (m, a.evenDrive,          b.evenDrive);
    p.clipSoftness       = juce::jmap (m, a.clipSoftness,       b.clipSoftness);
    p.transientPunch     = juce::jmap (m, a.transientPunch,     b.transientPunch);
    p.transientRetention = juce::jmap (m, a.transientRetention, b.transientRetention);
    p.stereoWidthBias    = juce::jmap (m, a.stereoWidthBias,    b.stereoWidthBias);
    p.centerWeight       = juce::jmap (m, a.centerWeight,       b.centerWeight);
    p.sideSoftness       = juce::jmap (m, a.sideSoftness,       b.sideSoftness);
    p.crosstalkBias      = juce::jmap (m, a.crosstalkBias,      b.crosstalkBias);
    p.outputTrim         = juce::jmap (m, a.outputTrim,         b.outputTrim);
    return p;
}

// ── Preamp static helpers ─────────────────────────────────────────────────────

float NovaConsoleDSP::saturateSmooth (float x) noexcept
{
    return std::tanh (x);
}

float NovaConsoleDSP::applyColorTilt (float sample, float color) noexcept
{
    const float dark   = juce::jmap (color, 0.0f, 1.0f, 1.08f, 0.96f);
    const float bright = juce::jmap (color, 0.0f, 1.0f, 0.94f, 1.08f);
    return sample * (0.65f * dark + 0.35f * bright);
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void NovaConsoleDSP::prepare (const juce::dsp::ProcessSpec& spec,
                               const NovaConsoleParameters& initial)
{
    currentSampleRate = juce::jmax (1.0, spec.sampleRate);
    params = initial;

    // Filters are per-channel mono processors inside a stereo buffer.
    juce::dsp::ProcessSpec monoSpec { spec.sampleRate, spec.maximumBlockSize, 1 };

    for (int ch = 0; ch < 2; ++ch)
    {
        hpf[ch].reset();
        lpf[ch].reset();

        hpf[ch].setType      (juce::dsp::StateVariableTPTFilterType::highpass);
        lpf[ch].setType      (juce::dsp::StateVariableTPTFilterType::lowpass);
        hpfStage2[ch].setType (juce::dsp::StateVariableTPTFilterType::highpass);
        hpfStage3[ch].setType (juce::dsp::StateVariableTPTFilterType::highpass);
        hpfStage4[ch].setType (juce::dsp::StateVariableTPTFilterType::highpass);
        lpfStage2[ch].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        lpfStage3[ch].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        lpfStage4[ch].setType (juce::dsp::StateVariableTPTFilterType::lowpass);

        hpf[ch].prepare      (monoSpec);
        lpf[ch].prepare      (monoSpec);
        hpfStage2[ch].prepare (monoSpec);
        hpfStage3[ch].prepare (monoSpec);
        hpfStage4[ch].prepare (monoSpec);
        lpfStage2[ch].prepare (monoSpec);
        lpfStage3[ch].prepare (monoSpec);
        lpfStage4[ch].prepare (monoSpec);

        lowShelf[ch].prepare   (monoSpec);
        lowMidPeak[ch].prepare (monoSpec);
        highMidPeak[ch].prepare (monoSpec);
        highShelf[ch].prepare  (monoSpec);
        airShelf[ch].prepare   (monoSpec);
    }

    const double sr = spec.sampleRate;

    // ── Smoother time constants (verbatim from prepareToPlay) ─────────────────
    hpfSmoothed.reset (sr, 0.050);
    lpfSmoothed.reset (sr, 0.050);
    lowSmoothed.reset         (sr, 0.035);
    lowFreqSmoothed.reset     (sr, 0.050);
    lowQSmoothed.reset        (sr, 0.015);
    lowMidSmoothed.reset      (sr, 0.035);
    lowMidFreqSmoothed.reset  (sr, 0.050);
    lowMidQSmoothed.reset     (sr, 0.015);
    highMidSmoothed.reset     (sr, 0.035);
    highMidFreqSmoothed.reset (sr, 0.050);
    highMidQSmoothed.reset    (sr, 0.015);
    highSmoothed.reset        (sr, 0.035);
    highFreqSmoothed.reset    (sr, 0.050);
    highQSmoothed.reset       (sr, 0.015);
    airSmoothed.reset         (sr, 0.035);
    airFreqSmoothed.reset     (sr, 0.050);
    airQSmoothed.reset        (sr, 0.015);

    driveSmoothed.reset (sr, 0.030);
    colorSmoothed.reset (sr, 0.030);
    trimSmoothed.reset  (sr, 0.030);

    inputSmoothed.reset  (sr, 0.030);
    outputSmoothed.reset (sr, 0.030);

    gateThresholdSmoothed.reset (sr, 0.045);
    gateReleaseSmoothed.reset   (sr, 0.045);
    gateRangeSmoothed.reset     (sr, 0.045);
    gateAttackSmoothed.reset    (sr, 0.030);
    gateHoldSmoothed.reset      (sr, 0.030);

    // ── Seed smoothers with initial parameter values ───────────────────────────
    // (NovaDSP v1.0.0 contract: prepare(spec, initial) eliminates first-block
    // ramp-from-zero artefact that existed in the original prepareToPlay.)
    hpfSmoothed.setCurrentAndTargetValue (initial.hpfHz);
    lpfSmoothed.setCurrentAndTargetValue (initial.lpfHz);
    lowSmoothed.setCurrentAndTargetValue         (initial.lowDb);
    lowFreqSmoothed.setCurrentAndTargetValue     (initial.lowFreqHz);
    lowQSmoothed.setCurrentAndTargetValue        (initial.lowQ);
    lowMidSmoothed.setCurrentAndTargetValue      (initial.lowMidDb);
    lowMidFreqSmoothed.setCurrentAndTargetValue  (initial.lowMidFreqHz);
    lowMidQSmoothed.setCurrentAndTargetValue     (initial.lowMidQ);
    highMidSmoothed.setCurrentAndTargetValue     (initial.highMidDb);
    highMidFreqSmoothed.setCurrentAndTargetValue (initial.highMidFreqHz);
    highMidQSmoothed.setCurrentAndTargetValue    (initial.highMidQ);
    highSmoothed.setCurrentAndTargetValue        (initial.highDb);
    highFreqSmoothed.setCurrentAndTargetValue    (initial.highFreqHz);
    highQSmoothed.setCurrentAndTargetValue       (initial.highQ);
    airSmoothed.setCurrentAndTargetValue         (initial.airDb);
    airFreqSmoothed.setCurrentAndTargetValue     (initial.airFreqHz);
    airQSmoothed.setCurrentAndTargetValue        (initial.airQ);

    driveSmoothed.setCurrentAndTargetValue (initial.drive / 100.0f);
    colorSmoothed.setCurrentAndTargetValue (initial.color / 100.0f);
    trimSmoothed.setCurrentAndTargetValue  (dbToGain (initial.trimDb));

    inputSmoothed.setCurrentAndTargetValue  (dbToGain (initial.inputDb));
    outputSmoothed.setCurrentAndTargetValue (dbToGain (initial.outputDb));

    gateThresholdSmoothed.setCurrentAndTargetValue (initial.gateThreshDb);
    gateReleaseSmoothed.setCurrentAndTargetValue   (initial.gateReleaseMs);
    gateRangeSmoothed.setCurrentAndTargetValue     (initial.gateRangeDb);
    gateAttackSmoothed.setCurrentAndTargetValue    (initial.gateAttackMs);
    gateHoldSmoothed.setCurrentAndTargetValue      (initial.gateHoldMs);

    // ── Mode morph ────────────────────────────────────────────────────────────
    const int rawMode = juce::jlimit (0, 4, initial.mode);
    modeFrom = static_cast<ConsoleMode> (rawMode);
    modeTo   = modeFrom;
    modeMorph.reset (sr, 0.035);
    modeMorph.setCurrentAndTargetValue (1.0f);

    // ── Reset dirty-check cache (forces coefficient computation on first block) ─
    lastHpfHz = -1.0f;   lastLpfHz = -1.0f;
    lastLowDb = 999.0f;  lastLowMidDb = 999.0f;
    lastHighMidDb = 999.0f; lastHighDb = 999.0f; lastAirDb = 999.0f;
    lastLowFreq = -1.0f; lastLowMidFreq = -1.0f; lastHighMidFreq = -1.0f;
    lastHighFreq = -1.0f; lastAirFreq = -1.0f;
    lastLowQ = -1.0f; lastLowMidQ = -1.0f; lastHighMidQ = -1.0f;
    lastHighQ = -1.0f; lastAirQ = -1.0f;
    lastHpfSlope = -1; lastLpfSlope = -1;
    lastLowMode = -1; lastHighMode = -1; lastAirMode = -1;

    // ── Reset runtime state ───────────────────────────────────────────────────
    preampPrevInput     = { 0.0f, 0.0f };
    gateEnv             = { 1.0f, 1.0f };
    gateHoldCounter     = { 0, 0 };
    gatePreviousEnv     = { 0.0f, 0.0f };
    gateDetectorHpfState = {};
    gateDetectorLpfState = {};

    updateLinearStageCoefficients();
}

void NovaConsoleDSP::reset() noexcept
{
    for (int ch = 0; ch < 2; ++ch)
    {
        hpf[ch].reset();      lpf[ch].reset();
        hpfStage2[ch].reset(); hpfStage3[ch].reset(); hpfStage4[ch].reset();
        lpfStage2[ch].reset(); lpfStage3[ch].reset(); lpfStage4[ch].reset();
        lowShelf[ch].reset();  lowMidPeak[ch].reset();
        highMidPeak[ch].reset(); highShelf[ch].reset(); airShelf[ch].reset();
    }

    preampPrevInput      = { 0.0f, 0.0f };
    gateEnv              = { 1.0f, 1.0f };
    gateHoldCounter      = { 0, 0 };
    gatePreviousEnv      = { 0.0f, 0.0f };
    gateDetectorHpfState = {};
    gateDetectorLpfState = {};
}

void NovaConsoleDSP::setParameters (const NovaConsoleParameters& p) noexcept
{
    params = p;
}

// ── Process ───────────────────────────────────────────────────────────────────

void NovaConsoleDSP::process (juce::AudioBuffer<float>& buffer,
                               const juce::AudioBuffer<float>* sidechain) noexcept
{
    if (buffer.getNumSamples() == 0)
        return;

    // Mode morph — verbatim from processBlock()
    const auto requestedMode = static_cast<ConsoleMode> (
        juce::jlimit (0, 4, params.mode));

    if (requestedMode != modeTo)
    {
        modeFrom = modeTo;
        modeTo   = requestedMode;
        modeMorph.setCurrentAndTargetValue (0.0f);
        modeMorph.setTargetValue (1.0f);
    }

    const ModeProfile fromProfile   = profileForMode (modeFrom);
    const ModeProfile toProfile     = profileForMode (modeTo);
    const float       morphNow      = modeMorph.skip (buffer.getNumSamples());
    const ModeProfile activeProfile = blendProfiles (fromProfile, toProfile, morphNow);

    if (!modeMorph.isSmoothing())
        modeFrom = modeTo;

    updateLinearStageCoefficients();

    // osFactor — verbatim from processBlock()
    int osFactor = (params.oversampling == 2 ? 4 : (params.oversampling == 1 ? 2 : 1));
    if (params.quality == 0)
        osFactor = 1;

    // Sidechain routing
    const bool detectorSidechainEnabled = params.sidechainMode > 0;
    const bool sidechainExtRequested    = params.sidechainMode == 2;
    const bool sidechainExtActive = sidechainExtRequested
                                 && sidechain != nullptr
                                 && sidechain->getNumChannels() > 0;
    const juce::AudioBuffer<float>* detectorBuffer = sidechainExtActive ? sidechain : nullptr;

    // Input gain — per-sample smoother, verbatim from processBlock()
    inputSmoothed.setTargetValue (dbToGain (params.inputDb));
    const int channels = juce::jmin (2, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] *= inputSmoothed.getNextValue();
    }

    if (params.preampOn)
        processPreamp (buffer, activeProfile, osFactor);

    if (params.filterOn)
        processFilters (buffer);

    if (params.eqOn)
        processEq (buffer, activeProfile);

    if (params.gateOn)
        processGate (buffer, detectorBuffer, detectorSidechainEnabled, sidechainExtActive);

    // modeTrim + clip + output gain — verbatim from processBlock()
    const float modeTrim = activeProfile.outputTrim;
    outputSmoothed.setTargetValue (dbToGain (params.outputDb));
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            data[i] = juce::jlimit (-1.35f, 1.35f, data[i] * modeTrim);
            data[i] *= outputSmoothed.getNextValue();
        }
    }
}

// ── Private DSP helpers ───────────────────────────────────────────────────────
// All logic is verbatim from Nova Console/Source/PluginProcessor.cpp with the
// only change being: apvts.getRawParameterValue(id)->load() → params.field

void NovaConsoleDSP::updateLinearStageCoefficients() noexcept
{
    hpfSmoothed.setTargetValue (params.hpfHz);
    lpfSmoothed.setTargetValue (params.lpfHz);

    const auto hpfHz = hpfSmoothed.getNextValue();
    const auto lpfHz = lpfSmoothed.getNextValue();

    lowSmoothed.setTargetValue         (params.lowDb);
    lowFreqSmoothed.setTargetValue     (params.lowFreqHz);
    lowQSmoothed.setTargetValue        (params.lowQ);
    lowMidSmoothed.setTargetValue      (params.lowMidDb);
    lowMidFreqSmoothed.setTargetValue  (params.lowMidFreqHz);
    lowMidQSmoothed.setTargetValue     (params.lowMidQ);
    highMidSmoothed.setTargetValue     (params.highMidDb);
    highMidFreqSmoothed.setTargetValue (params.highMidFreqHz);
    highMidQSmoothed.setTargetValue    (params.highMidQ);
    highSmoothed.setTargetValue        (params.highDb);
    highFreqSmoothed.setTargetValue    (params.highFreqHz);
    highQSmoothed.setTargetValue       (params.highQ);
    airSmoothed.setTargetValue         (params.airDb);
    airFreqSmoothed.setTargetValue     (params.airFreqHz);
    airQSmoothed.setTargetValue        (params.airQ);

    const auto lowDb       = lowSmoothed.getNextValue();
    const auto lowFreq     = lowFreqSmoothed.getNextValue();
    const auto lowQ        = lowQSmoothed.getNextValue();
    const auto lowMidDb    = lowMidSmoothed.getNextValue();
    const auto lowMidFreq  = lowMidFreqSmoothed.getNextValue();
    const auto lowMidQ     = lowMidQSmoothed.getNextValue();
    const auto highMidDb   = highMidSmoothed.getNextValue();
    const auto highMidFreq = highMidFreqSmoothed.getNextValue();
    const auto highMidQ    = highMidQSmoothed.getNextValue();
    const auto highDb      = highSmoothed.getNextValue();
    const auto highFreq    = highFreqSmoothed.getNextValue();
    const auto highQ       = highQSmoothed.getNextValue();
    const auto airDb       = airSmoothed.getNextValue();
    const auto airFreq     = airFreqSmoothed.getNextValue();
    const auto airQ        = airQSmoothed.getNextValue();

    const int hpfSlopeChoice = juce::jlimit (0, 2, params.hpfSlope);
    const int lpfSlopeChoice = juce::jlimit (0, 2, params.lpfSlope);
    const int lowModeChoice  = juce::jlimit (0, 1, params.lowMode);
    const int highModeChoice = juce::jlimit (0, 1, params.highMode);
    const int airModeChoice  = juce::jlimit (0, 1, params.airMode);

    const bool hpfChanged = std::abs (hpfHz - lastHpfHz) > 0.0001f;
    const bool lpfChanged = std::abs (lpfHz - lastLpfHz) > 0.0001f;
    const bool eqChanged  = std::abs (lowDb      - lastLowDb)      > 0.0001f
                         || std::abs (lowFreq     - lastLowFreq)    > 0.0001f
                         || std::abs (lowQ        - lastLowQ)       > 0.0001f
                         || std::abs (lowMidDb    - lastLowMidDb)   > 0.0001f
                         || std::abs (lowMidFreq  - lastLowMidFreq) > 0.0001f
                         || std::abs (lowMidQ     - lastLowMidQ)    > 0.0001f
                         || std::abs (highMidDb   - lastHighMidDb)  > 0.0001f
                         || std::abs (highMidFreq - lastHighMidFreq)> 0.0001f
                         || std::abs (highMidQ    - lastHighMidQ)   > 0.0001f
                         || std::abs (highDb      - lastHighDb)     > 0.0001f
                         || std::abs (highFreq    - lastHighFreq)   > 0.0001f
                         || std::abs (highQ       - lastHighQ)      > 0.0001f
                         || std::abs (airDb       - lastAirDb)      > 0.0001f
                         || std::abs (airFreq     - lastAirFreq)    > 0.0001f
                         || std::abs (airQ        - lastAirQ)       > 0.0001f;
    const bool modeChanged = lowModeChoice  != lastLowMode
                          || highModeChoice != lastHighMode
                          || airModeChoice  != lastAirMode
                          || hpfSlopeChoice != lastHpfSlope
                          || lpfSlopeChoice != lastLpfSlope;

    if (!hpfChanged && !lpfChanged && !eqChanged && !modeChanged)
        return;

    for (int ch = 0; ch < 2; ++ch)
    {
        if (hpfChanged)
        {
            hpf[ch].setCutoffFrequency      (hpfHz);
            hpfStage2[ch].setCutoffFrequency (hpfHz);
            hpfStage3[ch].setCutoffFrequency (hpfHz);
            hpfStage4[ch].setCutoffFrequency (hpfHz);
        }

        if (lpfChanged)
        {
            lpf[ch].setCutoffFrequency      (lpfHz);
            lpfStage2[ch].setCutoffFrequency (lpfHz);
            lpfStage3[ch].setCutoffFrequency (lpfHz);
            lpfStage4[ch].setCutoffFrequency (lpfHz);
        }
    }

    if (eqChanged || modeChanged)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            if (lowModeChoice == 0)
                lowShelf[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                    currentSampleRate, lowFreq, lowQ, dbToGain (lowDb));
            else
                lowShelf[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                    currentSampleRate, lowFreq, lowQ, dbToGain (lowDb));

            lowMidPeak[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                currentSampleRate, lowMidFreq, lowMidQ, dbToGain (lowMidDb));

            highMidPeak[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                currentSampleRate, highMidFreq, highMidQ, dbToGain (highMidDb));

            if (highModeChoice == 0)
                highShelf[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                    currentSampleRate, highFreq, highQ, dbToGain (highDb));
            else
                highShelf[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                    currentSampleRate, highFreq, highQ, dbToGain (highDb));

            if (airModeChoice == 0)
                airShelf[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                    currentSampleRate, airFreq, airQ, dbToGain (airDb));
            else
                airShelf[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                    currentSampleRate, airFreq, airQ, dbToGain (airDb));
        }
    }

    lastHpfHz       = hpfHz;      lastLpfHz       = lpfHz;
    lastLowDb       = lowDb;      lastLowMidDb    = lowMidDb;
    lastHighMidDb   = highMidDb;  lastHighDb      = highDb;    lastAirDb       = airDb;
    lastLowFreq     = lowFreq;    lastLowMidFreq  = lowMidFreq;
    lastHighMidFreq = highMidFreq; lastHighFreq   = highFreq;  lastAirFreq     = airFreq;
    lastLowQ        = lowQ;       lastLowMidQ     = lowMidQ;
    lastHighMidQ    = highMidQ;   lastHighQ       = highQ;     lastAirQ        = airQ;
    lastHpfSlope    = hpfSlopeChoice; lastLpfSlope = lpfSlopeChoice;
    lastLowMode     = lowModeChoice;  lastHighMode = highModeChoice;
    lastAirMode     = airModeChoice;
}

void NovaConsoleDSP::processFilters (juce::AudioBuffer<float>& buffer) noexcept
{
    const int hpfStages = params.hpfSlope == 0 ? 1 : (params.hpfSlope == 1 ? 2 : 4);
    const int lpfStages = params.lpfSlope == 0 ? 1 : (params.lpfSlope == 1 ? 2 : 4);
    const int channels  = juce::jmin (2, buffer.getNumChannels());

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
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

void NovaConsoleDSP::processEq (juce::AudioBuffer<float>& buffer,
                                 const ModeProfile& profile) noexcept
{
    const float presence    = profile.presence;
    const float width       = profile.eqWidth;
    const float upperMidAgg = profile.upperMidAggression;
    const float airSmooth   = profile.airSmoothness;
    const float lowMidWt    = profile.lowMidWeight;

    const int channels = juce::jmin (2, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
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

void NovaConsoleDSP::processPreamp (juce::AudioBuffer<float>& buffer,
                                     const ModeProfile& profile,
                                     int osFactor) noexcept
{
    const float driveNorm = params.drive / 100.0f;
    const float colorNorm = params.color / 100.0f;
    const float trimDb    = params.trimDb;

    const float warmth   = profile.warmth;
    const float osRelief = juce::jmap (static_cast<float> (osFactor), 1.0f, 4.0f, 0.0f, 0.16f);

    driveSmoothed.setTargetValue (driveNorm);
    colorSmoothed.setTargetValue (colorNorm);
    trimSmoothed.setTargetValue  (dbToGain (trimDb));

    const int channels = juce::jmin (2, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* channelData = buffer.getWritePointer (ch);
        float previousIn = preampPrevInput[static_cast<size_t> (ch)];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float driveNow = driveSmoothed.getNextValue();
            const float colorNow = colorSmoothed.getNextValue();
            const float trimNow  = trimSmoothed.getNextValue();

            const float stageGain  = 1.0f + driveNow * (3.2f + 1.2f * warmth - osRelief);
            const float asym       = 0.025f + 0.08f * driveNow + 0.05f * profile.oddDrive;
            const float clipDrive  = juce::jlimit (0.85f, 1.2f, 1.0f / profile.clipSoftness);
            const float oddW       = 0.55f + 0.45f * profile.oddDrive;
            const float evenW      = 0.35f + 0.55f * profile.evenDrive;
            const float satNorm    = 1.0f / juce::jmax (0.35f, oddW + evenW);

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

void NovaConsoleDSP::processGate (juce::AudioBuffer<float>& buffer,
                                   const juce::AudioBuffer<float>* detectorBuffer,
                                   bool detectorSidechainEnabled,
                                   bool useExternalDetector) noexcept
{
    gateThresholdSmoothed.setTargetValue (params.gateThreshDb);
    gateReleaseSmoothed.setTargetValue   (params.gateReleaseMs);
    gateRangeSmoothed.setTargetValue     (params.gateRangeDb);
    gateAttackSmoothed.setTargetValue    (params.gateAttackMs);
    gateHoldSmoothed.setTargetValue      (params.gateHoldMs);

    const bool  expandMode = params.gateSmooth;
    const float sr = static_cast<float> (currentSampleRate);
    const auto  mode = static_cast<ConsoleMode> (juce::jlimit (0, 4, params.mode));

    float modeExpandSoftness = 1.0f, modeGateTightness = 1.0f;
    switch (mode)
    {
        case ConsoleMode::clean:    modeExpandSoftness = 1.12f; modeGateTightness = 0.92f; break;
        case ConsoleMode::british:  modeExpandSoftness = 0.96f; modeGateTightness = 1.10f; break;
        case ConsoleMode::tubeTape: modeExpandSoftness = 1.20f; modeGateTightness = 0.88f; break;
        case ConsoleMode::gold:     modeExpandSoftness = 1.15f; modeGateTightness = 0.95f; break;
        case ConsoleMode::modern:   modeExpandSoftness = 1.04f; modeGateTightness = 1.02f; break;
    }

    const bool externalAvailable = useExternalDetector
                                 && detectorBuffer != nullptr
                                 && detectorBuffer->getNumChannels() > 0;
    const auto* detLeft  = externalAvailable ? detectorBuffer->getReadPointer (0) : nullptr;
    const auto* detRight = externalAvailable
        ? detectorBuffer->getReadPointer (detectorBuffer->getNumChannels() > 1 ? 1 : 0)
        : nullptr;

    const int hpfStages = params.hpfSlope == 0 ? 1 : (params.hpfSlope == 1 ? 2 : 4);
    const int lpfStages = params.lpfSlope == 0 ? 1 : (params.lpfSlope == 1 ? 2 : 4);
    const float detectorHpfCoeff = juce::jlimit (0.0001f, 0.999f,
        1.0f - std::exp (-juce::MathConstants<float>::twoPi * params.hpfHz / sr));
    const float detectorLpfCoeff = juce::jlimit (0.0001f, 0.999f,
        1.0f - std::exp (-juce::MathConstants<float>::twoPi * params.lpfHz / sr));

    const int channels = juce::jmin (2, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
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

            if (detectorSidechainEnabled)
            {
                if (externalAvailable)
                    detectorSample = 0.5f * (detLeft[i] + detRight[i]);
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
