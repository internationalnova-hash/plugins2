#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    constexpr auto modeId = "console_mode";
    constexpr auto qualityId = "quality";
    constexpr auto oversamplingId = "oversampling";

    constexpr auto inputId = "input";
    constexpr auto outputId = "output";

    constexpr auto preampOnId = "preamp_on";
    constexpr auto driveId = "drive";
    constexpr auto colorId = "color";
    constexpr auto trimId = "trim";

    constexpr auto filterOnId = "filter_on";
    constexpr auto hpfId = "hpf";
    constexpr auto lpfId = "lpf";
    constexpr auto hpfSlopeId = "hpf_slope";
    constexpr auto lpfSlopeId = "lpf_slope";

    constexpr auto eqOnId = "eq_on";
    constexpr auto lowId = "eq_low";
    constexpr auto lowFreqId = "eq_low_freq";
    constexpr auto lowQId = "eq_low_q";
    constexpr auto lowMidId = "eq_low_mid";
    constexpr auto lowMidFreqId = "eq_low_mid_freq";
    constexpr auto lowMidQId = "eq_low_mid_q";
    constexpr auto highMidId = "eq_high_mid";
    constexpr auto highMidFreqId = "eq_high_mid_freq";
    constexpr auto highMidQId = "eq_high_mid_q";
    constexpr auto highId = "eq_high";
    constexpr auto highFreqId = "eq_high_freq";
    constexpr auto highQId = "eq_high_q";
    constexpr auto airId = "eq_air";
    constexpr auto airFreqId = "eq_air_freq";
    constexpr auto airQId = "eq_air_q";
    constexpr auto lowModeId = "eq_low_mode";
    constexpr auto highModeId = "eq_high_mode";
    constexpr auto airModeId = "eq_air_mode";

    constexpr auto compOnId = "comp_on";
    constexpr auto thresholdId = "comp_threshold";
    constexpr auto ratioId = "comp_ratio";
    constexpr auto attackId = "comp_attack";
    constexpr auto releaseId = "comp_release";
    constexpr auto mixId = "comp_mix";
    constexpr auto makeupId = "comp_makeup";
    constexpr auto punchId = "comp_punch";

    constexpr auto gateOnId = "gate_on";
    constexpr auto gateThresholdId = "gate_threshold";
    constexpr auto gateReleaseId = "gate_release";
    constexpr auto gateRangeId = "gate_range";
    constexpr auto gateAttackId = "gate_attack";
    constexpr auto gateHoldId = "gate_hold";
    constexpr auto gateSmoothId = "gate_smooth";

    constexpr auto analogOnId = "analog_on";
    constexpr auto heatId = "analog_heat";
    constexpr auto depthId = "analog_depth";
    constexpr auto widthId = "analog_width";
    constexpr auto driftId = "analog_drift";
    constexpr auto crosstalkId = "analog_crosstalk";
    constexpr auto noiseId = "analog_noise";

    constexpr auto smartGainId = "smart_gain";
    constexpr auto focusModeId = "focus_mode";
    constexpr auto mixAssistId = "mix_assist";
    constexpr auto sidechainModeId = "sidechain_mode";

    constexpr float minHz = 20.0f;
    constexpr float maxHz = 20000.0f;

    float dbToGain (float db) noexcept
    {
        return juce::Decibels::decibelsToGain (db);
    }

    float gainToDb (float gain) noexcept
    {
        return juce::Decibels::gainToDecibels (juce::jmax (gain, 1.0e-6f));
    }

    float saturateSmooth (float x) noexcept
    {
        return std::tanh (x);
    }

    float applyColorTilt (float sample, float color) noexcept
    {
        const float dark = juce::jmap (color, 0.0f, 1.0f, 1.08f, 0.96f);
        const float bright = juce::jmap (color, 0.0f, 1.0f, 0.94f, 1.08f);
        const float blended = sample * (0.65f * dark + 0.35f * bright);
        return blended;
    }
}

NovaConsoleAudioProcessor::NovaConsoleAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts (*this, nullptr, juce::Identifier ("NovaConsole"), createParameterLayout())
{
    rng.seed (0x5A17u);
}

NovaConsoleAudioProcessor::~NovaConsoleAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaConsoleAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { modeId, 1 }, "Console Mode",
        juce::StringArray { "Clean", "British", "Tube", "Tape", "Gold", "Modern" }, 1));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { qualityId, 1 }, "Quality",
        juce::StringArray { "Eco", "Mix", "Master" }, 1));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { oversamplingId, 1 }, "Oversampling",
        juce::StringArray { "Off", "2x", "4x" }, 1));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { inputId, 1 }, "Input",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputId, 1 }, "Output",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { preampOnId, 1 }, "Preamp Enabled", true));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveId, 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 38.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { colorId, 1 }, "Color",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 50.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { trimId, 1 }, "Trim",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { filterOnId, 1 }, "Filters Enabled", true));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { hpfId, 1 }, "HPF",
        juce::NormalisableRange<float> (minHz, 1200.0f, 0.01f, 0.35f), 20.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { lpfId, 1 }, "LPF",
        juce::NormalisableRange<float> (1800.0f, maxHz, 0.01f, 0.35f), maxHz));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { hpfSlopeId, 1 }, "HPF Slope",
        juce::StringArray { "12 dB", "24 dB", "48 dB" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { lpfSlopeId, 1 }, "LPF Slope",
        juce::StringArray { "12 dB", "24 dB", "48 dB" }, 1));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { eqOnId, 1 }, "EQ Enabled", true));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { lowId, 1 }, "Low",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { lowFreqId, 1 }, "Low Freq",
        juce::NormalisableRange<float> (35.0f, 180.0f, 0.01f, 0.33f), 90.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { lowQId, 1 }, "Low Q",
        juce::NormalisableRange<float> (0.40f, 1.40f, 0.001f, 0.55f), 0.62f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { lowModeId, 1 }, "Low Mode",
        juce::StringArray { "Shelf", "Bell" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { lowMidId, 1 }, "Low Mid",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { lowMidFreqId, 1 }, "Low Mid Freq",
        juce::NormalisableRange<float> (180.0f, 1200.0f, 0.01f, 0.38f), 420.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { lowMidQId, 1 }, "Low Mid Q",
        juce::NormalisableRange<float> (0.35f, 2.50f, 0.001f, 0.45f), 0.80f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { highMidId, 1 }, "High Mid",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { highMidFreqId, 1 }, "High Mid Freq",
        juce::NormalisableRange<float> (1000.0f, 7000.0f, 0.01f, 0.40f), 2800.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { highMidQId, 1 }, "High Mid Q",
        juce::NormalisableRange<float> (0.35f, 2.50f, 0.001f, 0.45f), 0.85f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { highId, 1 }, "High",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { highFreqId, 1 }, "High Freq",
        juce::NormalisableRange<float> (3500.0f, 12000.0f, 0.01f, 0.40f), 7600.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { highQId, 1 }, "High Q",
        juce::NormalisableRange<float> (0.40f, 1.40f, 0.001f, 0.55f), 0.70f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { highModeId, 1 }, "High Mode",
        juce::StringArray { "Shelf", "Bell" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { airId, 1 }, "Air",
        juce::NormalisableRange<float> (-8.0f, 8.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { airFreqId, 1 }, "Air Freq",
        juce::NormalisableRange<float> (8000.0f, 18000.0f, 0.01f, 0.42f), 14500.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { airQId, 1 }, "Air Q",
        juce::NormalisableRange<float> (0.40f, 1.40f, 0.001f, 0.55f), 0.75f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { airModeId, 1 }, "Air Mode",
        juce::StringArray { "Shelf", "Bell" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { compOnId, 1 }, "Compressor Enabled", true));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { thresholdId, 1 }, "Threshold",
        juce::NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -16.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ratioId, 1 }, "Ratio",
        juce::NormalisableRange<float> (1.0f, 10.0f, 0.01f), 4.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { attackId, 1 }, "Attack",
        juce::NormalisableRange<float> (0.5f, 60.0f, 0.01f, 0.45f), 15.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { releaseId, 1 }, "Release",
        juce::NormalisableRange<float> (20.0f, 500.0f, 0.01f, 0.45f), 180.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, 1 }, "Comp Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 100.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { makeupId, 1 }, "Makeup",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { punchId, 1 }, "Punch",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 35.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { gateOnId, 1 }, "Gate Enabled", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gateThresholdId, 1 }, "Gate Threshold",
        juce::NormalisableRange<float> (-70.0f, -10.0f, 0.1f), -42.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gateAttackId, 1 }, "Gate Attack",
        juce::NormalisableRange<float> (0.1f, 100.0f, 0.01f, 0.4f), 8.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gateHoldId, 1 }, "Gate Hold",
        juce::NormalisableRange<float> (0.0f, 500.0f, 0.1f), 40.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gateReleaseId, 1 }, "Gate Release",
        juce::NormalisableRange<float> (20.0f, 500.0f, 0.01f, 0.45f), 120.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gateRangeId, 1 }, "Gate Range",
        juce::NormalisableRange<float> (-36.0f, -3.0f, 0.1f), -18.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { gateSmoothId, 1 }, "Gate Smooth", true));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { analogOnId, 1 }, "Analog Enabled", true));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { heatId, 1 }, "Heat",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 28.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { depthId, 1 }, "Depth",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 24.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { widthId, 1 }, "Width",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 52.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driftId, 1 }, "Drift",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 12.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { crosstalkId, 1 }, "Crosstalk",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { noiseId, 1 }, "Noise",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { smartGainId, 1 }, "Smart Gain", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { focusModeId, 1 }, "Focus Mode", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixAssistId, 1 }, "Mix Assist",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 50.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { sidechainModeId, 1 }, "Sidechain Mode",
        juce::StringArray { "Internal", "External 1", "External 2" }, 0));

    return layout;
}

const juce::String NovaConsoleAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaConsoleAudioProcessor::acceptsMidi() const { return false; }
bool NovaConsoleAudioProcessor::producesMidi() const { return false; }
bool NovaConsoleAudioProcessor::isMidiEffect() const { return false; }
double NovaConsoleAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int NovaConsoleAudioProcessor::getNumPrograms() { return 1; }
int NovaConsoleAudioProcessor::getCurrentProgram() { return 0; }
void NovaConsoleAudioProcessor::setCurrentProgram (int) {}
const juce::String NovaConsoleAudioProcessor::getProgramName (int) { return {}; }
void NovaConsoleAudioProcessor::changeProgramName (int, const juce::String&) {}

void NovaConsoleAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 1 };
    for (int ch = 0; ch < 2; ++ch)
    {
        hpf[ch].reset();
        lpf[ch].reset();

        hpf[ch].setType (juce::dsp::StateVariableTPTFilterType::highpass);
        lpf[ch].setType (juce::dsp::StateVariableTPTFilterType::lowpass);

        hpf[ch].prepare (spec);
        lpf[ch].prepare (spec);

        lowShelf[ch].prepare (spec);
        lowMidPeak[ch].prepare (spec);
        highMidPeak[ch].prepare (spec);
        highShelf[ch].prepare (spec);
        airShelf[ch].prepare (spec);
    }

    // Preamp smoothing (30ms)
    driveSmoothed.reset (sampleRate, 0.03);
    colorSmoothed.reset (sampleRate, 0.03);
    trimSmoothed.reset (sampleRate, 0.03);
    
    // Input/Output smoothing (30ms)
    inputSmoothed.reset (sampleRate, 0.03);
    outputSmoothed.reset (sampleRate, 0.03);
    
    // EQ smoothing (gains: 35ms, frequencies: 50ms, Q: 15ms - light on Q)
    lowSmoothed.reset (sampleRate, 0.035);
    lowFreqSmoothed.reset (sampleRate, 0.050);
    lowQSmoothed.reset (sampleRate, 0.015);
    lowMidSmoothed.reset (sampleRate, 0.035);
    lowMidFreqSmoothed.reset (sampleRate, 0.050);
    lowMidQSmoothed.reset (sampleRate, 0.015);
    highMidSmoothed.reset (sampleRate, 0.035);
    highMidFreqSmoothed.reset (sampleRate, 0.050);
    highMidQSmoothed.reset (sampleRate, 0.015);
    highSmoothed.reset (sampleRate, 0.035);
    highFreqSmoothed.reset (sampleRate, 0.050);
    highQSmoothed.reset (sampleRate, 0.015);
    airSmoothed.reset (sampleRate, 0.035);
    airFreqSmoothed.reset (sampleRate, 0.050);
    airQSmoothed.reset (sampleRate, 0.015);
    
    // Filter frequency smoothing (50ms - avoid ringing)
    hpfSmoothed.reset (sampleRate, 0.050);
    lpfSmoothed.reset (sampleRate, 0.050);
    
    // Compressor smoothing (threshold/release: 45ms, attack/ratio: 15ms light, mix/makeup/punch: 30ms)
    compThresholdSmoothed.reset (sampleRate, 0.045);
    compRatioSmoothed.reset (sampleRate, 0.050);
    compAttackSmoothed.reset (sampleRate, 0.015);
    compReleaseSmoothed.reset (sampleRate, 0.045);
    compMixSmoothed.reset (sampleRate, 0.03);
    compMakeupSmoothed.reset (sampleRate, 0.03);
    compPunchSmoothed.reset (sampleRate, 0.03);
    
    // Gate smoothing (40-50ms, attack/hold: 30ms)
    gateThresholdSmoothed.reset (sampleRate, 0.045);
    gateReleaseSmoothed.reset (sampleRate, 0.045);
    gateRangeSmoothed.reset (sampleRate, 0.045);
    gateAttackSmoothed.reset (sampleRate, 0.030);
    gateHoldSmoothed.reset (sampleRate, 0.030);
    
    // Analog engine smoothing (50ms, except drift which can be slower)
    heatSmoothed.reset (sampleRate, 0.050);
    depthSmoothed.reset (sampleRate, 0.050);
    widthSmoothed.reset (sampleRate, 0.050);
    driftSmoothed.reset (sampleRate, 0.080);
    noiseSmoothed.reset (sampleRate, 0.050);
    crosstalkSmoothed.reset (sampleRate, 0.050);

    compressorDetector = 0.0f;
    compressorGainState = 1.0f;
    compressorPunchMemory = { 0.0f, 0.0f };
    gateEnv = { 0.0f, 0.0f };
    driftState = { 0.0f, 0.0f };
    preampPrevInput = { 0.0f, 0.0f };
    analogPrevInput = { 0.0f, 0.0f };
    analogToneMemory = { 0.0f, 0.0f };

    lastHpfHz = -1.0f;
    lastLpfHz = -1.0f;
    lastLowDb = 999.0f;
    lastLowMidDb = 999.0f;
    lastHighMidDb = 999.0f;
    lastHighDb = 999.0f;
    lastAirDb = 999.0f;
    lastLowFreq = -1.0f;
    lastLowMidFreq = -1.0f;
    lastHighMidFreq = -1.0f;
    lastHighFreq = -1.0f;
    lastAirFreq = -1.0f;
    lastLowQ = -1.0f;
    lastLowMidQ = -1.0f;
    lastHighMidQ = -1.0f;
    lastHighQ = -1.0f;
    lastAirQ = -1.0f;

    updateLinearStageCoefficients();
}

void NovaConsoleAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaConsoleAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}
#endif

float NovaConsoleAudioProcessor::modeWarmth (ConsoleMode mode) noexcept
{
    switch (mode)
    {
        case ConsoleMode::clean:   return 0.10f;
        case ConsoleMode::british: return 0.28f;
        case ConsoleMode::tube:    return 0.40f;
        case ConsoleMode::tape:    return 0.36f;
        case ConsoleMode::gold:    return 0.24f;
        case ConsoleMode::modern:  return 0.14f;
    }

    return 0.2f;
}

float NovaConsoleAudioProcessor::modePresence (ConsoleMode mode) noexcept
{
    switch (mode)
    {
        case ConsoleMode::clean:   return 1.03f;
        case ConsoleMode::british: return 1.08f;
        case ConsoleMode::tube:    return 0.95f;
        case ConsoleMode::tape:    return 0.92f;
        case ConsoleMode::gold:    return 1.06f;
        case ConsoleMode::modern:  return 1.11f;
    }

    return 1.0f;
}

void NovaConsoleAudioProcessor::updateLinearStageCoefficients()
{
    hpfSmoothed.setTargetValue (apvts.getRawParameterValue (hpfId)->load());
    lpfSmoothed.setTargetValue (apvts.getRawParameterValue (lpfId)->load());
    
    const auto hpfHz = hpfSmoothed.getNextValue();
    const auto lpfHz = lpfSmoothed.getNextValue();

    lowSmoothed.setTargetValue (apvts.getRawParameterValue (lowId)->load());
    lowFreqSmoothed.setTargetValue (apvts.getRawParameterValue (lowFreqId)->load());
    lowQSmoothed.setTargetValue (apvts.getRawParameterValue (lowQId)->load());
    lowMidSmoothed.setTargetValue (apvts.getRawParameterValue (lowMidId)->load());
    lowMidFreqSmoothed.setTargetValue (apvts.getRawParameterValue (lowMidFreqId)->load());
    lowMidQSmoothed.setTargetValue (apvts.getRawParameterValue (lowMidQId)->load());
    highMidSmoothed.setTargetValue (apvts.getRawParameterValue (highMidId)->load());
    highMidFreqSmoothed.setTargetValue (apvts.getRawParameterValue (highMidFreqId)->load());
    highMidQSmoothed.setTargetValue (apvts.getRawParameterValue (highMidQId)->load());
    highSmoothed.setTargetValue (apvts.getRawParameterValue (highId)->load());
    highFreqSmoothed.setTargetValue (apvts.getRawParameterValue (highFreqId)->load());
    highQSmoothed.setTargetValue (apvts.getRawParameterValue (highQId)->load());
    airSmoothed.setTargetValue (apvts.getRawParameterValue (airId)->load());
    airFreqSmoothed.setTargetValue (apvts.getRawParameterValue (airFreqId)->load());
    airQSmoothed.setTargetValue (apvts.getRawParameterValue (airQId)->load());
    
    const auto lowDb = lowSmoothed.getNextValue();
    const auto lowFreq = lowFreqSmoothed.getNextValue();
    const auto lowQ = lowQSmoothed.getNextValue();
    const auto lowMidDb = lowMidSmoothed.getNextValue();
    const auto lowMidFreq = lowMidFreqSmoothed.getNextValue();
    const auto lowMidQ = lowMidQSmoothed.getNextValue();
    const auto highMidDb = highMidSmoothed.getNextValue();
    const auto highMidFreq = highMidFreqSmoothed.getNextValue();
    const auto highMidQ = highMidQSmoothed.getNextValue();
    const auto highDb = highSmoothed.getNextValue();
    const auto highFreq = highFreqSmoothed.getNextValue();
    const auto highQ = highQSmoothed.getNextValue();
    const auto airDb = airSmoothed.getNextValue();
    const auto airFreq = airFreqSmoothed.getNextValue();
    const auto airQ = airQSmoothed.getNextValue();
    const int hpfSlopeChoice = juce::jlimit (0, 2, static_cast<int> (apvts.getRawParameterValue (hpfSlopeId)->load()));
    const int lpfSlopeChoice = juce::jlimit (0, 2, static_cast<int> (apvts.getRawParameterValue (lpfSlopeId)->load()));
    const int lowModeChoice = juce::jlimit (0, 1, static_cast<int> (apvts.getRawParameterValue (lowModeId)->load()));
    const int highModeChoice = juce::jlimit (0, 1, static_cast<int> (apvts.getRawParameterValue (highModeId)->load()));
    const int airModeChoice = juce::jlimit (0, 1, static_cast<int> (apvts.getRawParameterValue (airModeId)->load()));

    const bool hpfChanged = std::abs (hpfHz - lastHpfHz) > 0.0001f;
    const bool lpfChanged = std::abs (lpfHz - lastLpfHz) > 0.0001f;
    const bool eqChanged = std::abs (lowDb - lastLowDb) > 0.0001f
                        || std::abs (lowFreq - lastLowFreq) > 0.0001f
                        || std::abs (lowQ - lastLowQ) > 0.0001f
                        || std::abs (lowMidDb - lastLowMidDb) > 0.0001f
                        || std::abs (lowMidFreq - lastLowMidFreq) > 0.0001f
                        || std::abs (lowMidQ - lastLowMidQ) > 0.0001f
                        || std::abs (highMidDb - lastHighMidDb) > 0.0001f
                        || std::abs (highMidFreq - lastHighMidFreq) > 0.0001f
                        || std::abs (highMidQ - lastHighMidQ) > 0.0001f
                        || std::abs (highDb - lastHighDb) > 0.0001f
                        || std::abs (highFreq - lastHighFreq) > 0.0001f
                        || std::abs (highQ - lastHighQ) > 0.0001f
                        || std::abs (airDb - lastAirDb) > 0.0001f
                        || std::abs (airFreq - lastAirFreq) > 0.0001f
                        || std::abs (airQ - lastAirQ) > 0.0001f;
    const bool modeChanged = lowModeChoice != lastLowMode
                          || highModeChoice != lastHighMode
                          || airModeChoice != lastAirMode
                          || hpfSlopeChoice != lastHpfSlope
                          || lpfSlopeChoice != lastLpfSlope;

    if (! hpfChanged && ! lpfChanged && ! eqChanged && ! modeChanged)
        return;

    for (int ch = 0; ch < 2; ++ch)
    {
        if (hpfChanged)
            hpf[ch].setCutoffFrequency (hpfHz);

        if (lpfChanged)
            lpf[ch].setCutoffFrequency (lpfHz);
    }

    if (eqChanged || modeChanged)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            if (lowModeChoice == 0)
                lowShelf[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, lowFreq, lowQ, dbToGain (lowDb));
            else
                lowShelf[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, lowFreq, lowQ, dbToGain (lowDb));

            lowMidPeak[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, lowMidFreq, lowMidQ, dbToGain (lowMidDb));
            highMidPeak[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, highMidFreq, highMidQ, dbToGain (highMidDb));

            if (highModeChoice == 0)
                highShelf[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, highFreq, highQ, dbToGain (highDb));
            else
                highShelf[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, highFreq, highQ, dbToGain (highDb));

            if (airModeChoice == 0)
                airShelf[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, airFreq, airQ, dbToGain (airDb));
            else
                airShelf[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, airFreq, airQ, dbToGain (airDb));
        }
    }

    lastHpfHz = hpfHz;
    lastLpfHz = lpfHz;
    lastLowDb = lowDb;
    lastLowMidDb = lowMidDb;
    lastHighMidDb = highMidDb;
    lastHighDb = highDb;
    lastAirDb = airDb;
    lastLowFreq = lowFreq;
    lastLowMidFreq = lowMidFreq;
    lastHighMidFreq = highMidFreq;
    lastHighFreq = highFreq;
    lastAirFreq = airFreq;
    lastLowQ = lowQ;
    lastLowMidQ = lowMidQ;
    lastHighMidQ = highMidQ;
    lastHighQ = highQ;
    lastAirQ = airQ;
    lastHpfSlope = hpfSlopeChoice;
    lastLpfSlope = lpfSlopeChoice;
    lastLowMode = lowModeChoice;
    lastHighMode = highModeChoice;
    lastAirMode = airModeChoice;
}

void NovaConsoleAudioProcessor::processPreamp (juce::AudioBuffer<float>& buffer, ConsoleMode mode, int osFactor)
{
    const float driveNorm = apvts.getRawParameterValue (driveId)->load() / 100.0f;
    const float colorNorm = apvts.getRawParameterValue (colorId)->load() / 100.0f;
    const float trimDb = apvts.getRawParameterValue (trimId)->load();

    const float warmth = modeWarmth (mode);
    const float osRelief = juce::jmap (static_cast<float> (osFactor), 1.0f, 4.0f, 0.0f, 0.16f);

    driveSmoothed.setTargetValue (driveNorm);
    colorSmoothed.setTargetValue (colorNorm);
    trimSmoothed.setTargetValue (dbToGain (trimDb));

    const int channels = juce::jmin (2, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* channelData = buffer.getWritePointer (ch);
        float previousIn = preampPrevInput[static_cast<size_t> (ch)];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float driveNow = driveSmoothed.getNextValue();
            const float colorNow = colorSmoothed.getNextValue();
            const float trimNow = trimSmoothed.getNextValue();

            const float stageGain = 1.0f + driveNow * (3.4f + 1.3f * warmth - osRelief);
            const float asym = 0.03f + 0.14f * driveNow + 0.06f * warmth;

            const float input = channelData[i] * stageGain;
            float x = 0.0f;

            // Selective oversampling: only nonlinear drive stage is oversampled.
            for (int os = 0; os < osFactor; ++os)
            {
                const float t = static_cast<float> (os + 1) / static_cast<float> (osFactor);
                const float interp = previousIn + (input - previousIn) * t;
                x += (saturateSmooth (interp + asym) - saturateSmooth (asym));
            }

            x /= static_cast<float> (osFactor);
            x = applyColorTilt (x, colorNow);
            x = x * trimNow;
            channelData[i] = juce::jlimit (-1.2f, 1.2f, x);
            previousIn = input;
        }

        preampPrevInput[static_cast<size_t> (ch)] = previousIn;
    }
}

void NovaConsoleAudioProcessor::processFilters (juce::AudioBuffer<float>& buffer)
{
    const int hpfSlopeChoice = juce::jlimit (0, 2, static_cast<int> (apvts.getRawParameterValue (hpfSlopeId)->load()));
    const int lpfSlopeChoice = juce::jlimit (0, 2, static_cast<int> (apvts.getRawParameterValue (lpfSlopeId)->load()));
    const int hpfStages = hpfSlopeChoice == 0 ? 1 : (hpfSlopeChoice == 1 ? 2 : 4);
    const int lpfStages = lpfSlopeChoice == 0 ? 1 : (lpfSlopeChoice == 1 ? 2 : 4);
    const int channels = juce::jmin (2, buffer.getNumChannels());

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float sample = data[i];
            for (int s = 0; s < hpfStages; ++s)
                sample = hpf[ch].processSample (0, sample);
            for (int s = 0; s < lpfStages; ++s)
                sample = lpf[ch].processSample (0, sample);
            data[i] = sample;
        }
    }
}

void NovaConsoleAudioProcessor::processEq (juce::AudioBuffer<float>& buffer, ConsoleMode mode)
{
    const float presence = modePresence (mode);
    const float width = (mode == ConsoleMode::british || mode == ConsoleMode::gold) ? 0.95f : 1.0f;

    const int channels = juce::jmin (2, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float x = data[i];
            x = lowShelf[ch].processSample (x);
            x = lowMidPeak[ch].processSample (x);
            x = highMidPeak[ch].processSample (x * presence);
            x = highShelf[ch].processSample (x * width);
            x = airShelf[ch].processSample (x);
            data[i] = x;
        }
    }
}

void NovaConsoleAudioProcessor::processCompressor (juce::AudioBuffer<float>& buffer, ConsoleMode mode, QualityMode quality)
{
    compThresholdSmoothed.setTargetValue (apvts.getRawParameterValue (thresholdId)->load());
    compRatioSmoothed.setTargetValue (apvts.getRawParameterValue (ratioId)->load());
    compAttackSmoothed.setTargetValue (apvts.getRawParameterValue (attackId)->load());
    compReleaseSmoothed.setTargetValue (apvts.getRawParameterValue (releaseId)->load());
    compMixSmoothed.setTargetValue (apvts.getRawParameterValue (mixId)->load() / 100.0f);
    compMakeupSmoothed.setTargetValue (apvts.getRawParameterValue (makeupId)->load());
    compPunchSmoothed.setTargetValue (apvts.getRawParameterValue (punchId)->load() / 100.0f);

    const float modePunch = (mode == ConsoleMode::british ? 1.2f : 1.0f);
    const float qualityTightness = (quality == QualityMode::master ? 1.03f : (quality == QualityMode::eco ? 0.92f : 1.0f));
    const float sr = static_cast<float> (currentSampleRate);

    float maxReduction = 0.0f;

    const int channels = juce::jmin (2, buffer.getNumChannels());
    if (channels == 0)
        return;

    auto* left = buffer.getWritePointer (0);
    auto* right = channels > 1 ? buffer.getWritePointer (1) : nullptr;

    const float kneeDb = 4.0f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float thresholdDb = compThresholdSmoothed.getNextValue();
        const float ratio = compRatioSmoothed.getNextValue();
        const float attackMs = compAttackSmoothed.getNextValue();
        const float releaseMs = compReleaseSmoothed.getNextValue();
        const float compMix = compMixSmoothed.getNextValue();
        const float makeup = compMakeupSmoothed.getNextValue();
        const float punch = compPunchSmoothed.getNextValue();

        const float attackCoeff = std::exp (-1.0f / (0.001f * juce::jmax (0.6f, attackMs * 0.78f) * sr));

        const float dryL = left[i];
        const float dryR = right != nullptr ? right[i] : dryL;

        compressorPunchMemory[0] = 0.92f * compressorPunchMemory[0] + 0.08f * dryL;
        compressorPunchMemory[1] = 0.92f * compressorPunchMemory[1] + 0.08f * dryR;

        const float transL = dryL - compressorPunchMemory[0];
        const float transR = dryR - compressorPunchMemory[1];

        const float drivenL = dryL + transL * punch * 0.45f * modePunch;
        const float drivenR = dryR + transR * punch * 0.45f * modePunch;

        const float absL = std::abs (drivenL);
        const float absR = std::abs (drivenR);
        const float peak = juce::jmax (absL, absR);
        const float rms = std::sqrt ((drivenL * drivenL + drivenR * drivenR) * 0.5f);
        const float detector = 0.66f * rms + 0.34f * peak;

        const float crest = peak / juce::jmax (rms, 1.0e-5f);
        const float autoReleaseMs = juce::jlimit (35.0f, 450.0f,
            releaseMs * juce::jmap (juce::jlimit (1.0f, 6.0f, crest), 1.0f, 6.0f, 1.18f, 0.44f));
        const float releaseBlendMs = 0.45f * releaseMs + 0.55f * autoReleaseMs;
        const float releaseCoeff = std::exp (-1.0f / (0.001f * releaseBlendMs * sr));

        if (detector > compressorDetector)
            compressorDetector = attackCoeff * compressorDetector + (1.0f - attackCoeff) * detector;
        else
            compressorDetector = releaseCoeff * compressorDetector + (1.0f - releaseCoeff) * detector;

        const float envDb = gainToDb (compressorDetector);
        const float x = envDb - thresholdDb;

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
        const float targetGain = dbToGain (-compressedDb * qualityTightness);

        const float gainReleaseCoeff = std::exp (-1.0f / (0.001f * releaseBlendMs * 1.2f * sr));
        const float gainAttackCoeff = std::exp (-1.0f / (0.001f * juce::jmax (0.25f, attackMs * 0.45f) * sr));

        if (targetGain < compressorGainState)
            compressorGainState = gainAttackCoeff * compressorGainState + (1.0f - gainAttackCoeff) * targetGain;
        else
            compressorGainState = gainReleaseCoeff * compressorGainState + (1.0f - gainReleaseCoeff) * targetGain;

        maxReduction = juce::jmax (maxReduction, -gainToDb (juce::jmax (compressorGainState, 1.0e-5f)));

        const float thickenedL = 0.985f * drivenL + 0.015f * saturateSmooth (drivenL * 1.35f);
        const float thickenedR = 0.985f * drivenR + 0.015f * saturateSmooth (drivenR * 1.35f);

        const float wetL = thickenedL * compressorGainState * dbToGain (makeup);
        const float wetR = thickenedR * compressorGainState * dbToGain (makeup);

        left[i] = dryL * (1.0f - compMix) + wetL * compMix;
        if (right != nullptr)
            right[i] = dryR * (1.0f - compMix) + wetR * compMix;
    }

    gainReductionMeter.store (0.84f * gainReductionMeter.load() + 0.16f * juce::jlimit (0.0f, 1.0f, maxReduction / 18.0f));
}

void NovaConsoleAudioProcessor::processGate (juce::AudioBuffer<float>& buffer)
{
    gateThresholdSmoothed.setTargetValue (apvts.getRawParameterValue (gateThresholdId)->load());
    gateReleaseSmoothed.setTargetValue (apvts.getRawParameterValue (gateReleaseId)->load());
    gateRangeSmoothed.setTargetValue (apvts.getRawParameterValue (gateRangeId)->load());
    gateAttackSmoothed.setTargetValue (apvts.getRawParameterValue (gateAttackId)->load());
    gateHoldSmoothed.setTargetValue (apvts.getRawParameterValue (gateHoldId)->load());

    const bool smooth = apvts.getRawParameterValue (gateSmoothId)->load() > 0.5f;
    const float sr = static_cast<float> (currentSampleRate);
    const float hysteresisDb = 3.0f;

    const int channels = juce::jmin (2, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float thresholdDb = gateThresholdSmoothed.getNextValue();
            const float releaseMs = gateReleaseSmoothed.getNextValue();
            const float rangeDb = gateRangeSmoothed.getNextValue();
            const float attackMs = gateAttackSmoothed.getNextValue();
            const float holdMs = gateHoldSmoothed.getNextValue();

            const float releaseCoeff = std::exp (-1.0f / (0.001f * releaseMs * sr));
            const float attackCoeff = std::exp (-1.0f / (0.001f * juce::jmax (0.1f, attackMs * 0.8f) * sr));
            const int32_t holdSamples = static_cast<int32_t> ((holdMs * sr) / 1000.0f);
            const float floorGain = dbToGain (rangeDb);

            const float x = data[i];
            const float level = std::abs (x);
            const float levelDb = gainToDb (level);

            float targetDb = thresholdDb;
            if (levelDb > (thresholdDb + hysteresisDb))
                targetDb = thresholdDb + hysteresisDb;
            else if (levelDb < (thresholdDb - hysteresisDb))
                targetDb = thresholdDb - hysteresisDb;

            float target = levelDb > targetDb ? 1.0f : floorGain;

            if (levelDb > thresholdDb)
            {
                gateHoldCounter[static_cast<size_t> (ch)] = holdSamples;
            }
            else if (gateHoldCounter[static_cast<size_t> (ch)] > 0)
            {
                gateHoldCounter[static_cast<size_t> (ch)]--;
                target = 1.0f;
            }

            if (smooth)
                target = juce::jmap (juce::jlimit (0.0f, 1.0f, (levelDb - (thresholdDb - 12.0f)) / 12.0f), floorGain, 1.0f);

            float& gateEnvRef = gateEnv[static_cast<size_t> (ch)];
            if (target > gateEnvRef)
                gateEnvRef = attackCoeff * gateEnvRef + (1.0f - attackCoeff) * target;
            else
                gateEnvRef = releaseCoeff * gateEnvRef + (1.0f - releaseCoeff) * target;

            data[i] = x * gateEnvRef;
        }
    }
}

void NovaConsoleAudioProcessor::processAnalogEngine (juce::AudioBuffer<float>& buffer, ConsoleMode mode, int osFactor, QualityMode quality)
{
    heatSmoothed.setTargetValue (apvts.getRawParameterValue (heatId)->load() / 100.0f);
    depthSmoothed.setTargetValue (apvts.getRawParameterValue (depthId)->load() / 100.0f);
    widthSmoothed.setTargetValue (apvts.getRawParameterValue (widthId)->load() / 100.0f);
    driftSmoothed.setTargetValue (apvts.getRawParameterValue (driftId)->load() / 100.0f);
    noiseSmoothed.setTargetValue (apvts.getRawParameterValue (noiseId)->load() / 100.0f);
    crosstalkSmoothed.setTargetValue (apvts.getRawParameterValue (crosstalkId)->load() / 100.0f);

    const float warmth = modeWarmth (mode);
    const float qualityScale = quality == QualityMode::master ? 1.15f : (quality == QualityMode::eco ? 0.85f : 1.0f);
    const float osScale = juce::jmap (static_cast<float> (osFactor), 1.0f, 4.0f, 1.0f, 0.88f);

    if (buffer.getNumChannels() < 2)
    {
        auto* data = buffer.getWritePointer (0);
        float previousIn = analogPrevInput[0];
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float heat = heatSmoothed.getNextValue();
            const float drift = driftSmoothed.getNextValue();
            const float noise = noiseSmoothed.getNextValue();

            driftState[0] = 0.9985f * driftState[0] + 0.0015f * randDist (rng);
            const float driftMod = 1.0f + driftState[0] * drift * 0.003f;

            const float driven = data[i] * driftMod;

            float x = 0.0f;
            for (int os = 0; os < osFactor; ++os)
            {
                const float t = static_cast<float> (os + 1) / static_cast<float> (osFactor);
                const float interp = previousIn + (driven - previousIn) * t;
                x += saturateSmooth (interp * (1.0f + heat * (2.2f + warmth) * qualityScale * osScale));
            }

            x /= static_cast<float> (osFactor);
            analogToneMemory[0] = 0.996f * analogToneMemory[0] + 0.004f * x;
            x = 0.985f * x + 0.015f * analogToneMemory[0];
            x += (noise * 0.0007f) * randDist (rng);
            data[i] = x;
            previousIn = driven;
        }

        analogPrevInput[0] = previousIn;
        return;
    }

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    float previousL = analogPrevInput[0];
    float previousR = analogPrevInput[1];

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float heat = heatSmoothed.getNextValue();
        const float depth = depthSmoothed.getNextValue();
        const float width = widthSmoothed.getNextValue();
        const float drift = driftSmoothed.getNextValue();
        const float noise = noiseSmoothed.getNextValue();
        const float crosstalkAmt = crosstalkSmoothed.getNextValue();

        driftState[0] = 0.9985f * driftState[0] + 0.0015f * randDist (rng);
        driftState[1] = 0.9985f * driftState[1] + 0.0015f * randDist (rng);

        const float lDrift = 1.0f + driftState[0] * drift * 0.0025f;
        const float rDrift = 1.0f + driftState[1] * drift * 0.0025f;

        const float lDriven = left[i] * lDrift;
        const float rDriven = right[i] * rDrift;

        float l = 0.0f;
        float r = 0.0f;
        for (int os = 0; os < osFactor; ++os)
        {
            const float t = static_cast<float> (os + 1) / static_cast<float> (osFactor);
            const float lInterp = previousL + (lDriven - previousL) * t;
            const float rInterp = previousR + (rDriven - previousR) * t;

            l += saturateSmooth (lInterp * (1.0f + heat * (2.0f + warmth) * qualityScale * osScale));
            r += saturateSmooth (rInterp * (1.0f + heat * (2.0f + warmth) * qualityScale * osScale));
        }

        l /= static_cast<float> (osFactor);
        r /= static_cast<float> (osFactor);
        previousL = lDriven;
        previousR = rDriven;

        analogToneMemory[0] = 0.996f * analogToneMemory[0] + 0.004f * l;
        analogToneMemory[1] = 0.996f * analogToneMemory[1] + 0.004f * r;
        l = 0.988f * l + 0.012f * analogToneMemory[0];
        r = 0.988f * r + 0.012f * analogToneMemory[1];

        float mid = 0.5f * (l + r);
        float side = 0.5f * (l - r);

        mid = mid * (1.0f + depth * 0.08f);
        mid += saturateSmooth (mid * (1.0f + 0.4f * heat)) * (0.015f + 0.02f * heat);
        side = side * juce::jlimit (0.85f, 1.25f, 0.92f + width * 0.32f);
        const float crosstalk = (0.004f + 0.008f * depth) * crosstalkAmt * (r - l);

        const float noiseAmt = noise * 0.0007f;
        const float n = randDist (rng) * noiseAmt;

        left[i] = mid + side + crosstalk + n;
        right[i] = mid - side - crosstalk - n;
    }

    analogPrevInput[0] = previousL;
    analogPrevInput[1] = previousR;
}

void NovaConsoleAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    updateLinearStageCoefficients();

    const auto mode = static_cast<ConsoleMode> (static_cast<int> (apvts.getRawParameterValue (modeId)->load()));
    const auto quality = static_cast<QualityMode> (static_cast<int> (apvts.getRawParameterValue (qualityId)->load()));
    const int oversamplingChoice = static_cast<int> (apvts.getRawParameterValue (oversamplingId)->load());

    int osFactor = (oversamplingChoice == 2 ? 4 : (oversamplingChoice == 1 ? 2 : 1));
    if (quality == QualityMode::eco)
        osFactor = 1;

    inputSmoothed.setTargetValue (dbToGain (apvts.getRawParameterValue (inputId)->load()));
    outputSmoothed.setTargetValue (dbToGain (apvts.getRawParameterValue (outputId)->load()));

    float inPeak = 0.0f;
    float outPeak = 0.0f;

    const int channels = juce::jmin (2, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            data[i] *= inputSmoothed.getNextValue();
            inPeak = juce::jmax (inPeak, std::abs (data[i]));
        }
    }

    // Dynamic unload: if a module is bypassed, its DSP stage is skipped entirely.
    if (apvts.getRawParameterValue (preampOnId)->load() > 0.5f)
        processPreamp (buffer, mode, osFactor);

    if (apvts.getRawParameterValue (filterOnId)->load() > 0.5f)
        processFilters (buffer);

    if (apvts.getRawParameterValue (eqOnId)->load() > 0.5f)
        processEq (buffer, mode);

    if (apvts.getRawParameterValue (compOnId)->load() > 0.5f)
        processCompressor (buffer, mode, quality);
    else
        gainReductionMeter.store (0.92f * gainReductionMeter.load());

    if (apvts.getRawParameterValue (gateOnId)->load() > 0.5f)
        processGate (buffer);

    if (apvts.getRawParameterValue (analogOnId)->load() > 0.5f)
        processAnalogEngine (buffer, mode, osFactor, quality);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            data[i] *= outputSmoothed.getNextValue();
            outPeak = juce::jmax (outPeak, std::abs (data[i]));
        }
    }

    inputMeter.store (0.84f * inputMeter.load() + 0.16f * juce::jlimit (0.0f, 1.0f, inPeak));
    outputMeter.store (0.84f * outputMeter.load() + 0.16f * juce::jlimit (0.0f, 1.0f, outPeak));
}

bool NovaConsoleAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* NovaConsoleAudioProcessor::createEditor() { return new NovaConsoleAudioProcessorEditor (*this); }

void NovaConsoleAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaConsoleAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaConsoleAudioProcessor();
}
