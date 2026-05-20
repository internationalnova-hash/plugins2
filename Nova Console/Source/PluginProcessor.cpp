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
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)
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
        juce::StringArray { "Clean", "British", "Tube/Tape", "Gold", "Modern" }, 1));

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
        hpfStage2[ch].setType (juce::dsp::StateVariableTPTFilterType::highpass);
        hpfStage3[ch].setType (juce::dsp::StateVariableTPTFilterType::highpass);
        hpfStage4[ch].setType (juce::dsp::StateVariableTPTFilterType::highpass);
        lpfStage2[ch].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        lpfStage3[ch].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        lpfStage4[ch].setType (juce::dsp::StateVariableTPTFilterType::lowpass);

        hpf[ch].prepare (spec);
        lpf[ch].prepare (spec);
        hpfStage2[ch].prepare (spec);
        hpfStage3[ch].prepare (spec);
        hpfStage4[ch].prepare (spec);
        lpfStage2[ch].prepare (spec);
        lpfStage3[ch].prepare (spec);
        lpfStage4[ch].prepare (spec);

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

    // Smart Gain / Mix Assist / Focus smoothing
    smartGainCompDbSmoothed.reset (sampleRate, 0.150);
    mixAssistAmountSmoothed.reset (sampleRate, 0.120);
    focusAmountSmoothed.reset (sampleRate, 0.100);
    smartGainCompDbSmoothed.setCurrentAndTargetValue (0.0f);
    mixAssistAmountSmoothed.setCurrentAndTargetValue (0.0f);
    focusAmountSmoothed.setCurrentAndTargetValue (0.0f);

    compressorDetector = 0.0f;
    compressorGainState = 1.0f;
    compressorPunchMemory = { 0.0f, 0.0f };
    gateEnv = { 1.0f, 1.0f };
    driftState = { 0.0f, 0.0f };
    driftHarmonicState = { 0.0f, 0.0f };
    driftStereoState = { 0.0f, 0.0f };
    driftContourState = { 0.0f, 0.0f };
    driftFilterState = 0.0f;
    preampPrevInput = { 0.0f, 0.0f };
    analogPrevInput = { 0.0f, 0.0f };
    analogToneMemory = { 0.0f, 0.0f };
    mixAssistLowState = { 0.0f, 0.0f };
    mixAssistPresenceLpState = { 0.0f, 0.0f };
    mixAssistHarshLpState = { 0.0f, 0.0f };
    mixAssistHarshEnvState = { 0.0f, 0.0f };

    smartGainPreRmsEnv = 0.0f;
    smartGainPostRmsEnv = 0.0f;
    assistDensityEnv = 0.0f;
    crosstalkLowState = { 0.0f, 0.0f };
    crosstalkHighState = { 0.0f, 0.0f };
    crosstalkDelayBuffer = {};
    crosstalkDelayWriteIndex = 0;
    compDetectorHpfState = {};
    compDetectorLpfState = {};
    gateDetectorHpfState = {};
    gateDetectorLpfState = {};

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

    const int rawMode = juce::jlimit (0, 4, static_cast<int> (apvts.getRawParameterValue (modeId)->load()));
    modeFrom = static_cast<ConsoleMode> (rawMode);
    modeTo = modeFrom;
    modeMorph.reset (sampleRate, 0.035);
    modeMorph.setCurrentAndTargetValue (1.0f);

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

NovaConsoleAudioProcessor::ModeProfile NovaConsoleAudioProcessor::profileForMode (ConsoleMode mode) noexcept
{
    ModeProfile p {};

    switch (mode)
    {
        case ConsoleMode::clean:
            p.warmth = 0.09f;
            p.presence = 1.02f;
            p.eqWidth = 1.03f;
            p.upperMidAggression = 0.96f;
            p.airSmoothness = 1.04f;
            p.lowMidWeight = 0.95f;
            p.oddDrive = 0.45f;
            p.evenDrive = 0.34f;
            p.clipSoftness = 1.06f;
            p.transientPunch = 0.98f;
            p.transientRetention = 1.06f;
            p.stereoWidthBias = 1.02f;
            p.centerWeight = 0.98f;
            p.sideSoftness = 1.00f;
            p.crosstalkBias = 0.90f;
            p.outputTrim = 1.00f;
            break;

        case ConsoleMode::british:
            p.warmth = 0.30f;
            p.presence = 1.09f;
            p.eqWidth = 0.95f;
            p.upperMidAggression = 1.08f;
            p.airSmoothness = 0.96f;
            p.lowMidWeight = 0.98f;
            p.oddDrive = 0.70f;
            p.evenDrive = 0.40f;
            p.clipSoftness = 0.90f;
            p.transientPunch = 1.18f;
            p.transientRetention = 0.95f;
            p.stereoWidthBias = 0.98f;
            p.centerWeight = 1.04f;
            p.sideSoftness = 0.96f;
            p.crosstalkBias = 1.08f;
            p.outputTrim = 0.975f;
            break;

        case ConsoleMode::tubeTape:
            p.warmth = 0.39f;
            p.presence = 0.93f;
            p.eqWidth = 0.98f;
            p.upperMidAggression = 0.92f;
            p.airSmoothness = 1.10f;
            p.lowMidWeight = 1.08f;
            p.oddDrive = 0.46f;
            p.evenDrive = 0.76f;
            p.clipSoftness = 1.14f;
            p.transientPunch = 0.90f;
            p.transientRetention = 0.92f;
            p.stereoWidthBias = 0.99f;
            p.centerWeight = 1.01f;
            p.sideSoftness = 1.05f;
            p.crosstalkBias = 1.02f;
            p.outputTrim = 0.955f;
            break;

        case ConsoleMode::gold:
            p.warmth = 0.24f;
            p.presence = 1.06f;
            p.eqWidth = 0.97f;
            p.upperMidAggression = 1.00f;
            p.airSmoothness = 1.08f;
            p.lowMidWeight = 1.02f;
            p.oddDrive = 0.52f;
            p.evenDrive = 0.58f;
            p.clipSoftness = 1.10f;
            p.transientPunch = 1.00f;
            p.transientRetention = 0.98f;
            p.stereoWidthBias = 1.01f;
            p.centerWeight = 1.00f;
            p.sideSoftness = 1.03f;
            p.crosstalkBias = 0.98f;
            p.outputTrim = 0.97f;
            break;

        case ConsoleMode::modern:
            p.warmth = 0.14f;
            p.presence = 1.11f;
            p.eqWidth = 1.04f;
            p.upperMidAggression = 1.01f;
            p.airSmoothness = 1.03f;
            p.lowMidWeight = 0.97f;
            p.oddDrive = 0.48f;
            p.evenDrive = 0.44f;
            p.clipSoftness = 1.04f;
            p.transientPunch = 1.03f;
            p.transientRetention = 1.08f;
            p.stereoWidthBias = 1.06f;
            p.centerWeight = 0.97f;
            p.sideSoftness = 1.01f;
            p.crosstalkBias = 0.92f;
                p.outputTrim = 0.985f;
            break;
    }

    return p;
}

NovaConsoleAudioProcessor::ModeProfile NovaConsoleAudioProcessor::blendProfiles (const ModeProfile& a, const ModeProfile& b, float t) noexcept
{
    const float m = juce::jlimit (0.0f, 1.0f, t);
    ModeProfile p {};

    p.warmth = juce::jmap (m, a.warmth, b.warmth);
    p.presence = juce::jmap (m, a.presence, b.presence);
    p.eqWidth = juce::jmap (m, a.eqWidth, b.eqWidth);
    p.upperMidAggression = juce::jmap (m, a.upperMidAggression, b.upperMidAggression);
    p.airSmoothness = juce::jmap (m, a.airSmoothness, b.airSmoothness);
    p.lowMidWeight = juce::jmap (m, a.lowMidWeight, b.lowMidWeight);
    p.oddDrive = juce::jmap (m, a.oddDrive, b.oddDrive);
    p.evenDrive = juce::jmap (m, a.evenDrive, b.evenDrive);
    p.clipSoftness = juce::jmap (m, a.clipSoftness, b.clipSoftness);
    p.transientPunch = juce::jmap (m, a.transientPunch, b.transientPunch);
    p.transientRetention = juce::jmap (m, a.transientRetention, b.transientRetention);
    p.stereoWidthBias = juce::jmap (m, a.stereoWidthBias, b.stereoWidthBias);
    p.centerWeight = juce::jmap (m, a.centerWeight, b.centerWeight);
    p.sideSoftness = juce::jmap (m, a.sideSoftness, b.sideSoftness);
    p.crosstalkBias = juce::jmap (m, a.crosstalkBias, b.crosstalkBias);
    p.outputTrim = juce::jmap (m, a.outputTrim, b.outputTrim);

    return p;
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
        {
            hpf[ch].setCutoffFrequency (hpfHz);
            hpfStage2[ch].setCutoffFrequency (hpfHz);
            hpfStage3[ch].setCutoffFrequency (hpfHz);
            hpfStage4[ch].setCutoffFrequency (hpfHz);
        }

        if (lpfChanged)
        {
            lpf[ch].setCutoffFrequency (lpfHz);
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

void NovaConsoleAudioProcessor::processPreamp (juce::AudioBuffer<float>& buffer, const ModeProfile& profile, int osFactor)
{
    const float driveNorm = apvts.getRawParameterValue (driveId)->load() / 100.0f;
    const float colorNorm = apvts.getRawParameterValue (colorId)->load() / 100.0f;
    const float trimDb = apvts.getRawParameterValue (trimId)->load();

    const float warmth = profile.warmth;
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

            const float stageGain = 1.0f + driveNow * (3.2f + 1.2f * warmth - osRelief);
            const float asym = 0.025f + 0.08f * driveNow + 0.05f * profile.oddDrive;
            const float clipDrive = juce::jlimit (0.85f, 1.2f, 1.0f / profile.clipSoftness);
            const float oddW = 0.55f + 0.45f * profile.oddDrive;
            const float evenW = 0.35f + 0.55f * profile.evenDrive;
            const float satNorm = 1.0f / juce::jmax (0.35f, oddW + evenW);

            const float input = channelData[i] * stageGain;
            float x = 0.0f;

            // Selective oversampling: only nonlinear drive stage is oversampled.
            for (int os = 0; os < osFactor; ++os)
            {
                const float t = static_cast<float> (os + 1) / static_cast<float> (osFactor);
                const float interp = previousIn + (input - previousIn) * t;
                const float oddSat = saturateSmooth ((interp + asym) * clipDrive) - saturateSmooth (asym * clipDrive);
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
            sample = hpf[ch].processSample (0, sample);
            if (hpfStages >= 2) sample = hpfStage2[ch].processSample (0, sample);
            if (hpfStages >= 3) sample = hpfStage3[ch].processSample (0, sample);
            if (hpfStages >= 4) sample = hpfStage4[ch].processSample (0, sample);

            sample = lpf[ch].processSample (0, sample);
            if (lpfStages >= 2) sample = lpfStage2[ch].processSample (0, sample);
            if (lpfStages >= 3) sample = lpfStage3[ch].processSample (0, sample);
            if (lpfStages >= 4) sample = lpfStage4[ch].processSample (0, sample);
            data[i] = sample;
        }
    }
}

void NovaConsoleAudioProcessor::processEq (juce::AudioBuffer<float>& buffer, const ModeProfile& profile)
{
    const float presence = profile.presence;
    const float width = profile.eqWidth;
    const float upperMidAgg = profile.upperMidAggression;
    const float airSmooth = profile.airSmoothness;
    const float lowMidWeight = profile.lowMidWeight;

    const int channels = juce::jmin (2, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float x = data[i];
            x = lowShelf[ch].processSample (x);
            x = lowMidPeak[ch].processSample (x * lowMidWeight);
            x = highMidPeak[ch].processSample (x * presence * upperMidAgg);
            x = highShelf[ch].processSample (x * width);
            x = airShelf[ch].processSample (x * airSmooth);
            data[i] = x;
        }
    }
}

void NovaConsoleAudioProcessor::processCompressor (juce::AudioBuffer<float>& buffer,
                                                   const ModeProfile& profile,
                                                   QualityMode quality,
                                                   const juce::AudioBuffer<float>* detectorBuffer,
                                                   bool useExternalDetector)
{
    compThresholdSmoothed.setTargetValue (apvts.getRawParameterValue (thresholdId)->load());
    compRatioSmoothed.setTargetValue (apvts.getRawParameterValue (ratioId)->load());
    compAttackSmoothed.setTargetValue (apvts.getRawParameterValue (attackId)->load());
    compReleaseSmoothed.setTargetValue (apvts.getRawParameterValue (releaseId)->load());
    compMixSmoothed.setTargetValue (apvts.getRawParameterValue (mixId)->load() / 100.0f);
    compMakeupSmoothed.setTargetValue (apvts.getRawParameterValue (makeupId)->load());
    compPunchSmoothed.setTargetValue (apvts.getRawParameterValue (punchId)->load() / 100.0f);

    const float modePunch = profile.transientPunch;
    const float modeRetention = profile.transientRetention;
    const float qualityTightness = (quality == QualityMode::master ? 1.03f : (quality == QualityMode::eco ? 0.92f : 1.0f));
    const float sr = static_cast<float> (currentSampleRate);

    float maxReduction = 0.0f;

    const int channels = juce::jmin (2, buffer.getNumChannels());
    if (channels == 0)
        return;

    auto* left = buffer.getWritePointer (0);
    auto* right = channels > 1 ? buffer.getWritePointer (1) : nullptr;

    const bool externalAvailable = useExternalDetector && detectorBuffer != nullptr && detectorBuffer->getNumChannels() > 0;
    const auto* detLeft = externalAvailable ? detectorBuffer->getReadPointer (0) : left;
    const auto* detRight = externalAvailable
        ? detectorBuffer->getReadPointer (detectorBuffer->getNumChannels() > 1 ? 1 : 0)
        : (right != nullptr ? right : left);

    const int hpfSlopeChoice = juce::jlimit (0, 2, static_cast<int> (apvts.getRawParameterValue (hpfSlopeId)->load()));
    const int lpfSlopeChoice = juce::jlimit (0, 2, static_cast<int> (apvts.getRawParameterValue (lpfSlopeId)->load()));
    const int hpfStages = hpfSlopeChoice == 0 ? 1 : (hpfSlopeChoice == 1 ? 2 : 4);
    const int lpfStages = lpfSlopeChoice == 0 ? 1 : (lpfSlopeChoice == 1 ? 2 : 4);
    const float detectorHpfHz = apvts.getRawParameterValue (hpfId)->load();
    const float detectorLpfHz = apvts.getRawParameterValue (lpfId)->load();
    const float detectorHpfCoeff = juce::jlimit (0.0001f, 0.999f,
        1.0f - std::exp (-juce::MathConstants<float>::twoPi * detectorHpfHz / sr));
    const float detectorLpfCoeff = juce::jlimit (0.0001f, 0.999f,
        1.0f - std::exp (-juce::MathConstants<float>::twoPi * detectorLpfHz / sr));

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

        const float shapedAttackMs = juce::jmax (0.6f, attackMs * 0.78f * (2.0f - modeRetention));
        const float attackCoeff = std::exp (-1.0f / (0.001f * shapedAttackMs * sr));

        const float dryL = left[i];
        const float dryR = right != nullptr ? right[i] : dryL;

        compressorPunchMemory[0] = 0.92f * compressorPunchMemory[0] + 0.08f * dryL;
        compressorPunchMemory[1] = 0.92f * compressorPunchMemory[1] + 0.08f * dryR;

        const float transL = dryL - compressorPunchMemory[0];
        const float transR = dryR - compressorPunchMemory[1];

        const float drivenL = dryL + transL * punch * 0.45f * modePunch;
        const float drivenR = dryR + transR * punch * 0.45f * modePunch;

        float detectorL = externalAvailable ? detLeft[i] : drivenL;
        float detectorR = externalAvailable ? detRight[i] : drivenR;

        if (externalAvailable)
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
        const float rms = std::sqrt ((detectorL * detectorL + detectorR * detectorR) * 0.5f);
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

        const float thickBlend = 0.012f + 0.01f * profile.evenDrive;
        const float thickenedL = (1.0f - thickBlend) * drivenL + thickBlend * saturateSmooth (drivenL * (1.25f + 0.25f * profile.oddDrive));
        const float thickenedR = (1.0f - thickBlend) * drivenR + thickBlend * saturateSmooth (drivenR * (1.25f + 0.25f * profile.oddDrive));

        const float wetL = thickenedL * compressorGainState * dbToGain (makeup);
        const float wetR = thickenedR * compressorGainState * dbToGain (makeup);

        left[i] = dryL * (1.0f - compMix) + wetL * compMix;
        if (right != nullptr)
            right[i] = dryR * (1.0f - compMix) + wetR * compMix;
    }

    gainReductionMeter.store (0.84f * gainReductionMeter.load() + 0.16f * juce::jlimit (0.0f, 1.0f, maxReduction / 18.0f));
}

void NovaConsoleAudioProcessor::processGate (juce::AudioBuffer<float>& buffer,
                                             const juce::AudioBuffer<float>* detectorBuffer,
                                             bool useExternalDetector)
{
    gateThresholdSmoothed.setTargetValue (apvts.getRawParameterValue (gateThresholdId)->load());
    gateReleaseSmoothed.setTargetValue (apvts.getRawParameterValue (gateReleaseId)->load());
    gateRangeSmoothed.setTargetValue (apvts.getRawParameterValue (gateRangeId)->load());
    gateAttackSmoothed.setTargetValue (apvts.getRawParameterValue (gateAttackId)->load());
    gateHoldSmoothed.setTargetValue (apvts.getRawParameterValue (gateHoldId)->load());

    const bool expandMode = apvts.getRawParameterValue (gateSmoothId)->load() > 0.5f;
    const float sr = static_cast<float> (currentSampleRate);
    const auto mode = static_cast<ConsoleMode> (juce::jlimit (0, 4, static_cast<int> (apvts.getRawParameterValue (modeId)->load())));

    float modeExpandSoftness = 1.0f;
    float modeGateTightness = 1.0f;
    switch (mode)
    {
        case ConsoleMode::clean:    modeExpandSoftness = 1.12f; modeGateTightness = 0.92f; break;
        case ConsoleMode::british:  modeExpandSoftness = 0.96f; modeGateTightness = 1.10f; break;
        case ConsoleMode::tubeTape: modeExpandSoftness = 1.20f; modeGateTightness = 0.88f; break;
        case ConsoleMode::gold:     modeExpandSoftness = 1.15f; modeGateTightness = 0.95f; break;
        case ConsoleMode::modern:   modeExpandSoftness = 1.04f; modeGateTightness = 1.02f; break;
    }

    const bool externalAvailable = useExternalDetector && detectorBuffer != nullptr && detectorBuffer->getNumChannels() > 0;
    const auto* detLeft = externalAvailable ? detectorBuffer->getReadPointer (0) : nullptr;
    const auto* detRight = externalAvailable
        ? detectorBuffer->getReadPointer (detectorBuffer->getNumChannels() > 1 ? 1 : 0)
        : nullptr;

    const int hpfSlopeChoice = juce::jlimit (0, 2, static_cast<int> (apvts.getRawParameterValue (hpfSlopeId)->load()));
    const int lpfSlopeChoice = juce::jlimit (0, 2, static_cast<int> (apvts.getRawParameterValue (lpfSlopeId)->load()));
    const int hpfStages = hpfSlopeChoice == 0 ? 1 : (hpfSlopeChoice == 1 ? 2 : 4);
    const int lpfStages = lpfSlopeChoice == 0 ? 1 : (lpfSlopeChoice == 1 ? 2 : 4);
    const float detectorHpfHz = apvts.getRawParameterValue (hpfId)->load();
    const float detectorLpfHz = apvts.getRawParameterValue (lpfId)->load();
    const float detectorHpfCoeff = juce::jlimit (0.0001f, 0.999f,
        1.0f - std::exp (-juce::MathConstants<float>::twoPi * detectorHpfHz / sr));
    const float detectorLpfCoeff = juce::jlimit (0.0001f, 0.999f,
        1.0f - std::exp (-juce::MathConstants<float>::twoPi * detectorLpfHz / sr));

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

            const float maxAttenDb = juce::jmax (3.0f, -rangeDb);
            const float userAttackMs = juce::jmax (0.1f, attackMs);
            const float userReleaseMs = juce::jmax (8.0f, releaseMs);

            const float effAttackMs = expandMode
                ? userAttackMs * (1.35f * modeExpandSoftness)
                : userAttackMs * (0.68f / juce::jmax (0.75f, modeGateTightness));
            const float effReleaseMs = expandMode
                ? userReleaseMs * (1.45f * modeExpandSoftness)
                : userReleaseMs * (0.72f / juce::jmax (0.75f, modeGateTightness));

            const float releaseCoeff = std::exp (-1.0f / (0.001f * effReleaseMs * sr));
            const float attackCoeff = std::exp (-1.0f / (0.001f * effAttackMs * sr));
            const int32_t holdSamples = static_cast<int32_t> ((holdMs * sr) / 1000.0f);
            const float hysteresisDb = expandMode
                ? (4.2f * modeExpandSoftness)
                : (2.0f / juce::jmax (0.75f, modeGateTightness));

            const float x = data[i];
            float detectorSample = x;

            if (externalAvailable)
            {
                detectorSample = 0.5f * (detLeft[i] + detRight[i]);
                for (int stage = 0; stage < hpfStages; ++stage)
                {
                    auto& hpState = gateDetectorHpfState[static_cast<size_t> (stage)][static_cast<size_t> (ch)];
                    hpState += detectorHpfCoeff * (detectorSample - hpState);
                    detectorSample -= hpState;
                }

                for (int stage = 0; stage < lpfStages; ++stage)
                {
                    auto& lpState = gateDetectorLpfState[static_cast<size_t> (stage)][static_cast<size_t> (ch)];
                    lpState += detectorLpfCoeff * (detectorSample - lpState);
                    detectorSample = lpState;
                }
            }

            const float level = std::abs (detectorSample);
            const float levelDb = gainToDb (level);

            const bool aboveOpenThreshold = levelDb > (thresholdDb + hysteresisDb);
            const bool belowCloseThreshold = levelDb < (thresholdDb - hysteresisDb);

            float target = 1.0f;
            if (expandMode)
            {
                const float ratio = juce::jlimit (2.0f, 6.0f, 2.0f + (maxAttenDb - 3.0f) * (4.0f / 33.0f));
                const float kneeDb = 8.0f * modeExpandSoftness;
                const float belowDb = juce::jmax (0.0f, thresholdDb - levelDb);
                const float expansionDb = -juce::jmin (maxAttenDb, belowDb * (1.0f - 1.0f / ratio));
                const float blend = juce::jlimit (0.0f, 1.0f, belowDb / juce::jmax (1.0f, kneeDb));
                const float blendShaped = blend * blend * (3.0f - 2.0f * blend);
                target = dbToGain (expansionDb * blendShaped);
            }
            else
            {
                const float kneeDb = 2.0f / juce::jmax (0.75f, modeGateTightness);
                const float closeSpanDb = juce::jmax (4.0f, 10.0f / juce::jmax (0.75f, modeGateTightness));
                const float close = juce::jlimit (0.0f, 1.0f, (thresholdDb - levelDb + kneeDb) / closeSpanDb);
                const float curve = std::pow (close, 1.6f * modeGateTightness);
                const float gateDb = -juce::jmin (maxAttenDb, maxAttenDb * curve);
                target = dbToGain (gateDb);
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
            if (target > gateEnvRef)
                gateEnvRef = attackCoeff * gateEnvRef + (1.0f - attackCoeff) * target;
            else
                gateEnvRef = releaseCoeff * gateEnvRef + (1.0f - releaseCoeff) * target;

            data[i] = x * gateEnvRef;
        }
    }
}

void NovaConsoleAudioProcessor::processAnalogEngine (juce::AudioBuffer<float>& buffer,
                                                     const ModeProfile& profile,
                                                     int osFactor,
                                                     QualityMode quality,
                                                     float mixAssistAmount,
                                                     float focusAmount)
{
    heatSmoothed.setTargetValue (apvts.getRawParameterValue (heatId)->load() / 100.0f);
    depthSmoothed.setTargetValue (apvts.getRawParameterValue (depthId)->load() / 100.0f);
    widthSmoothed.setTargetValue (apvts.getRawParameterValue (widthId)->load() / 100.0f);
    driftSmoothed.setTargetValue (apvts.getRawParameterValue (driftId)->load() / 100.0f);
    noiseSmoothed.setTargetValue (apvts.getRawParameterValue (noiseId)->load() / 100.0f);
    crosstalkSmoothed.setTargetValue (apvts.getRawParameterValue (crosstalkId)->load() / 100.0f);

    const float warmth = profile.warmth;
    const float qualityScale = quality == QualityMode::master ? 1.15f : (quality == QualityMode::eco ? 0.85f : 1.0f);
    const float osScale = juce::jmap (static_cast<float> (osFactor), 1.0f, 4.0f, 1.0f, 0.88f);
    const float sr = static_cast<float> (currentSampleRate);
    const auto mode = static_cast<ConsoleMode> (juce::jlimit (0, 4, static_cast<int> (apvts.getRawParameterValue (modeId)->load())));
    float driftModeScale = 1.0f;
    switch (mode)
    {
        case ConsoleMode::clean:    driftModeScale = 0.55f; break;
        case ConsoleMode::british:  driftModeScale = 0.90f; break;
        case ConsoleMode::tubeTape: driftModeScale = 1.18f; break;
        case ConsoleMode::gold:     driftModeScale = 0.82f; break;
        case ConsoleMode::modern:   driftModeScale = 0.65f; break;
    }

    if (buffer.getNumChannels() < 2)
    {
        auto* data = buffer.getWritePointer (0);
        float previousIn = analogPrevInput[0];
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float inputSample = data[i];
            const float heatRaw = heatSmoothed.getNextValue();
            const float driftRaw = driftSmoothed.getNextValue();
            const float noise = noiseSmoothed.getNextValue();

            const float densityProbe = std::abs (data[i]);
            assistDensityEnv = 0.996f * assistDensityEnv + 0.004f * densityProbe;
            const float dense = juce::jlimit (0.0f, 1.0f, (assistDensityEnv - 0.10f) / 0.55f);
            const float assistRestrain = mixAssistAmount * (0.35f + 0.65f * dense);
            const float heat = heatRaw * (1.0f - 0.20f * assistRestrain - 0.12f * focusAmount);
            const float drift = driftRaw * driftModeScale * (1.0f - 0.16f * assistRestrain - 0.30f * focusAmount);

            driftHarmonicState[0] = juce::jlimit (-1.0f, 1.0f, 0.99925f * driftHarmonicState[0] + 0.0020f * randDist (rng));
            driftContourState[0] = juce::jlimit (-1.0f, 1.0f, 0.99945f * driftContourState[0] + 0.0012f * randDist (rng));
            driftFilterState = juce::jlimit (-1.0f, 1.0f, 0.99965f * driftFilterState + 0.0008f * randDist (rng));

            const float harmonicMotion = driftHarmonicState[0] * drift;
            const float contourMotion = driftContourState[0] * drift;
            const float contourTilt = driftFilterState * drift * 0.015f;

            const float clipDrive = juce::jlimit (0.85f, 1.2f, 1.0f / profile.clipSoftness);
            const float clipDriveVar = clipDrive * (1.0f + contourMotion * 0.08f);
            const float oddW = juce::jlimit (0.30f, 1.05f, 0.55f + 0.4f * profile.oddDrive + harmonicMotion * 0.06f);
            const float evenW = juce::jlimit (0.22f, 1.05f, 0.35f + 0.5f * profile.evenDrive - harmonicMotion * 0.05f);
            const float satNorm = 1.0f / juce::jmax (0.35f, oddW + evenW);
            const float harmonicDrive = 1.0f + heat * (2.0f + warmth) * qualityScale * osScale + harmonicMotion * 0.12f;
            const float evenDriveVar = 1.0f + heat * 1.6f - harmonicMotion * 0.08f;

            float x = 0.0f;
            for (int os = 0; os < osFactor; ++os)
            {
                const float t = static_cast<float> (os + 1) / static_cast<float> (osFactor);
                const float interp = previousIn + (inputSample - previousIn) * t;
                const float odd = saturateSmooth (interp * harmonicDrive * clipDriveVar);
                const float even = 0.5f * (saturateSmooth ((interp + 0.03f + contourTilt) * evenDriveVar * clipDriveVar)
                                         + saturateSmooth ((interp - 0.03f - contourTilt) * evenDriveVar * clipDriveVar));
                x += (odd * oddW + even * evenW) * satNorm;
            }

            x /= static_cast<float> (osFactor);
            const float toneMemoryCoeff = juce::jlimit (0.992f, 0.999f, 0.996f + contourTilt * 0.10f);
            analogToneMemory[0] = toneMemoryCoeff * analogToneMemory[0] + (1.0f - toneMemoryCoeff) * x;
            const float toneBlend = juce::jlimit (0.008f, 0.030f, 0.015f + contourMotion * 0.01f);
            x = (1.0f - toneBlend) * x + toneBlend * analogToneMemory[0];
            x += (noise * 0.0007f) * randDist (rng);
            data[i] = x;
            previousIn = inputSample;
        }

        analogPrevInput[0] = previousIn;
        return;
    }

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    float previousL = analogPrevInput[0];
    float previousR = analogPrevInput[1];
    const float crosstalkLowCoeff = juce::jlimit (0.0001f, 0.999f,
        1.0f - std::exp (-juce::MathConstants<float>::twoPi * 80.0f / sr));
    const float crosstalkHighCoeff = juce::jlimit (0.0001f, 0.999f,
        1.0f - std::exp (-juce::MathConstants<float>::twoPi * 10000.0f / sr));
    int delayWrite = crosstalkDelayWriteIndex;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float heatRaw = heatSmoothed.getNextValue();
        const float depth = depthSmoothed.getNextValue();
        const float widthRaw = widthSmoothed.getNextValue();
        const float driftRaw = driftSmoothed.getNextValue();
        const float noise = noiseSmoothed.getNextValue();
        const float crosstalkRaw = crosstalkSmoothed.getNextValue();

        const float densityProbe = 0.5f * (std::abs (left[i]) + std::abs (right[i]));
        assistDensityEnv = 0.996f * assistDensityEnv + 0.004f * densityProbe;
        const float dense = juce::jlimit (0.0f, 1.0f, (assistDensityEnv - 0.10f) / 0.55f);
        const float assistRestrain = mixAssistAmount * (0.35f + 0.65f * dense);

        const float heat = heatRaw * (1.0f - 0.20f * assistRestrain - 0.12f * focusAmount);
        const float width = widthRaw * (1.0f - 0.14f * assistRestrain - 0.08f * focusAmount);
        const float drift = driftRaw * driftModeScale * (1.0f - 0.16f * assistRestrain - 0.30f * focusAmount);
        const float crosstalkAmt = crosstalkRaw * (1.0f - 0.10f * assistRestrain - 0.30f * focusAmount);

        driftHarmonicState[0] = juce::jlimit (-1.0f, 1.0f, 0.99930f * driftHarmonicState[0] + 0.0018f * randDist (rng));
        driftHarmonicState[1] = juce::jlimit (-1.0f, 1.0f, 0.99930f * driftHarmonicState[1] + 0.0018f * randDist (rng));
        driftStereoState[0] = juce::jlimit (-1.0f, 1.0f, 0.99945f * driftStereoState[0] + 0.0012f * randDist (rng));
        driftStereoState[1] = juce::jlimit (-1.0f, 1.0f, 0.99945f * driftStereoState[1] + 0.0012f * randDist (rng));
        driftContourState[0] = juce::jlimit (-1.0f, 1.0f, 0.99955f * driftContourState[0] + 0.0010f * randDist (rng));
        driftContourState[1] = juce::jlimit (-1.0f, 1.0f, 0.99955f * driftContourState[1] + 0.0010f * randDist (rng));
        driftFilterState = juce::jlimit (-1.0f, 1.0f, 0.99970f * driftFilterState + 0.0006f * randDist (rng));

        const float harmonicMotionL = driftHarmonicState[0] * drift;
        const float harmonicMotionR = driftHarmonicState[1] * drift;
        const float stereoMotion = 0.5f * (driftStereoState[0] - driftStereoState[1]) * drift;
        const float contourMotion = 0.5f * (driftContourState[0] + driftContourState[1]) * drift;
        const float contourTilt = driftFilterState * drift * 0.02f;

        const float satDriveL = 1.0f + heat * (2.0f + warmth) * qualityScale * osScale + harmonicMotionL * 0.12f;
        const float satDriveR = 1.0f + heat * (2.0f + warmth) * qualityScale * osScale + harmonicMotionR * 0.12f;

        float l = 0.0f;
        float r = 0.0f;
        for (int os = 0; os < osFactor; ++os)
        {
            const float t = static_cast<float> (os + 1) / static_cast<float> (osFactor);
            const float lInterp = previousL + (left[i] - previousL) * t;
            const float rInterp = previousR + (right[i] - previousR) * t;

            l += saturateSmooth (lInterp * satDriveL);
            r += saturateSmooth (rInterp * satDriveR);
        }

        l /= static_cast<float> (osFactor);
        r /= static_cast<float> (osFactor);
        previousL = left[i];
        previousR = right[i];

        const float toneMemoryCoeff = juce::jlimit (0.992f, 0.999f, 0.996f + contourTilt * 0.08f);
        analogToneMemory[0] = toneMemoryCoeff * analogToneMemory[0] + (1.0f - toneMemoryCoeff) * l;
        analogToneMemory[1] = toneMemoryCoeff * analogToneMemory[1] + (1.0f - toneMemoryCoeff) * r;
        const float toneBlend = juce::jlimit (0.008f, 0.024f, 0.012f + contourMotion * 0.008f);
        l = (1.0f - toneBlend) * l + toneBlend * analogToneMemory[0];
        r = (1.0f - toneBlend) * r + toneBlend * analogToneMemory[1];

        float mid = 0.5f * (l + r);
        float side = 0.5f * (l - r);

        mid = mid * (1.0f + depth * 0.08f);
        mid += saturateSmooth (mid * (1.0f + 0.4f * heat)) * (0.015f + 0.02f * heat);
        const float stereoBias = width * profile.stereoWidthBias;
        side = side * juce::jlimit (0.84f, 1.28f, 0.92f + stereoBias * 0.32f);
        side *= juce::jlimit (0.94f, 1.06f, profile.sideSoftness * (1.0f + stereoMotion * 0.08f));
        mid *= juce::jlimit (0.95f, 1.06f, profile.centerWeight * (1.0f - stereoMotion * 0.05f));
        mid *= (1.0f - contourTilt * 0.35f);
        side *= (1.0f + contourTilt * 0.45f);
        float preOutL = mid + side;
        float preOutR = mid - side;

        crosstalkLowState[0] += crosstalkLowCoeff * (preOutL - crosstalkLowState[0]);
        crosstalkLowState[1] += crosstalkLowCoeff * (preOutR - crosstalkLowState[1]);
        const float hpL = preOutL - crosstalkLowState[0];
        const float hpR = preOutR - crosstalkLowState[1];
        crosstalkHighState[0] += crosstalkHighCoeff * (hpL - crosstalkHighState[0]);
        crosstalkHighState[1] += crosstalkHighCoeff * (hpR - crosstalkHighState[1]);

        const float delayMs = juce::jmap (crosstalkAmt, 0.0f, 1.0f, 0.05f, 0.30f);
        const int delaySamples = juce::jlimit (1, 30, static_cast<int> (std::round (0.001f * delayMs * sr)));
        const int delayRead = (delayWrite - delaySamples + 64) % 64;
        const float delayedL = crosstalkDelayBuffer[0][static_cast<size_t> (delayRead)];
        const float delayedR = crosstalkDelayBuffer[1][static_cast<size_t> (delayRead)];

        crosstalkDelayBuffer[0][static_cast<size_t> (delayWrite)] = crosstalkHighState[0];
        crosstalkDelayBuffer[1][static_cast<size_t> (delayWrite)] = crosstalkHighState[1];
        delayWrite = (delayWrite + 1) % 64;

        const float interaction = juce::jlimit (0.0f, 0.08f,
            crosstalkAmt * 0.08f * profile.crosstalkBias * (1.0f + 0.12f * std::abs (stereoMotion)));
        const float crosstalkL = interaction * (0.76f * delayedR + 0.24f * hpR);
        const float crosstalkR = interaction * (0.76f * delayedL + 0.24f * hpL);

        const float noiseAmt = noise * 0.0007f;
        const float n = randDist (rng) * noiseAmt;

        left[i] = preOutL + crosstalkL + n;
        right[i] = preOutR + crosstalkR - n;
    }

    analogPrevInput[0] = previousL;
    analogPrevInput[1] = previousR;
    crosstalkDelayWriteIndex = delayWrite;
}

void NovaConsoleAudioProcessor::processMixAssistAndFocus (juce::AudioBuffer<float>& buffer,
                                                          float mixAssistAmount,
                                                          float focusAmount)
{
    mixAssistAmount = juce::jlimit (0.0f, 1.0f, mixAssistAmount);
    focusAmount = juce::jlimit (0.0f, 1.0f, focusAmount);

    if (mixAssistAmount <= 0.0001f && focusAmount <= 0.0001f)
        return;

    const int channels = juce::jmin (2, buffer.getNumChannels());
    if (channels == 0 || currentSampleRate <= 1000.0)
        return;

    const float sr = static_cast<float> (currentSampleRate);
    const auto onePoleCoeff = [sr] (float hz)
    {
        const float omega = juce::MathConstants<float>::twoPi * hz / sr;
        return juce::jlimit (0.0001f, 0.999f, 1.0f - std::exp (-omega));
    };

    const float lowCoeff = onePoleCoeff (120.0f);
    const float presenceCoeff = onePoleCoeff (2200.0f);
    const float harshCoeff = onePoleCoeff (5000.0f);
    const float harshEnvCoeff = juce::jlimit (0.0005f, 0.2f, 1.0f - std::exp (-1.0f / (sr * 0.05f)));

    if (channels == 1)
    {
        auto* mono = buffer.getWritePointer (0);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float x = mono[i];
            auto& lowState = mixAssistLowState[0];
            auto& presenceState = mixAssistPresenceLpState[0];
            auto& harshLp = mixAssistHarshLpState[0];
            auto& harshEnv = mixAssistHarshEnvState[0];

            lowState += lowCoeff * (x - lowState);
            presenceState += presenceCoeff * (x - presenceState);
            harshLp += harshCoeff * (x - harshLp);

            const float lowSense = std::abs (lowState);
            const float harshBand = presenceState - harshLp;
            harshEnv += harshEnvCoeff * (std::abs (harshBand) - harshEnv);

            const float lowExcess = juce::jlimit (0.0f, 1.0f, (lowSense - 0.11f) / 0.32f);
            const float harshExcess = juce::jlimit (0.0f, 1.0f, (harshEnv - 0.028f) / 0.20f);

            const float lowTrimDb = juce::jmax (-1.5f, -1.5f * mixAssistAmount * lowExcess * lowExcess);
            const float harshTrimDb = juce::jmax (-1.0f, -1.0f * mixAssistAmount * harshExcess * harshExcess);
            const float tighten = (mixAssistAmount * 0.06f + focusAmount * 0.09f) * lowExcess;

            x = (x - lowState * tighten) * dbToGain (lowTrimDb + harshTrimDb);
            x += (x - presenceState) * (focusAmount * 0.09f);
            mono[i] = x;
        }
        return;
    }

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float processed[2] { left[i], right[i] };
        float lowExcessAccum = 0.0f;

        for (int ch = 0; ch < 2; ++ch)
        {
            float x = processed[ch];
            auto& lowState = mixAssistLowState[static_cast<size_t> (ch)];
            auto& presenceState = mixAssistPresenceLpState[static_cast<size_t> (ch)];
            auto& harshLp = mixAssistHarshLpState[static_cast<size_t> (ch)];
            auto& harshEnv = mixAssistHarshEnvState[static_cast<size_t> (ch)];

            lowState += lowCoeff * (x - lowState);
            presenceState += presenceCoeff * (x - presenceState);
            harshLp += harshCoeff * (x - harshLp);

            const float lowSense = std::abs (lowState);
            const float harshBand = presenceState - harshLp;
            harshEnv += harshEnvCoeff * (std::abs (harshBand) - harshEnv);

            const float lowExcess = juce::jlimit (0.0f, 1.0f, (lowSense - 0.11f) / 0.32f);
            const float harshExcess = juce::jlimit (0.0f, 1.0f, (harshEnv - 0.028f) / 0.20f);
            lowExcessAccum += lowExcess;

            const float lowTrimDb = juce::jmax (-1.5f, -1.5f * mixAssistAmount * lowExcess * lowExcess);
            const float harshTrimDb = juce::jmax (-1.0f, -1.0f * mixAssistAmount * harshExcess * harshExcess);
            const float tighten = (mixAssistAmount * 0.06f + focusAmount * 0.09f) * lowExcess;

            x = (x - lowState * tighten) * dbToGain (lowTrimDb + harshTrimDb);
            x += (x - presenceState) * (focusAmount * 0.09f);
            processed[ch] = x;
        }

        float mid = 0.5f * (processed[0] + processed[1]);
        float side = 0.5f * (processed[0] - processed[1]);
        const float widthFactor = juce::jlimit (0.88f, 1.0f,
                                                1.0f - 0.08f * focusAmount - 0.02f * mixAssistAmount);
        side *= widthFactor;

        const float lowTighten = juce::jlimit (0.0f, 1.0f, 0.5f * lowExcessAccum);
        mid -= lowTighten * focusAmount * 0.05f * mid;

        left[i] = mid + side;
        right[i] = mid - side;
    }
}

void NovaConsoleAudioProcessor::applySmartGainCompensation (juce::AudioBuffer<float>& buffer,
                                                            float preProcessRms,
                                                            float postProcessRms,
                                                            int numSamples)
{
    if (numSamples <= 0 || currentSampleRate <= 1000.0)
        return;

    const float blockSeconds = static_cast<float> (numSamples / currentSampleRate);
    const float rmsWindowSeconds = 0.300f;
    const float envCoeff = juce::jlimit (0.0005f, 1.0f, 1.0f - std::exp (-blockSeconds / rmsWindowSeconds));

    smartGainPreRmsEnv += envCoeff * (preProcessRms - smartGainPreRmsEnv);
    smartGainPostRmsEnv += envCoeff * (postProcessRms - smartGainPostRmsEnv);

    const bool smartGainOn = apvts.getRawParameterValue (smartGainId)->load() > 0.5f;
    float targetCompDb = 0.0f;

    if (smartGainOn)
    {
        const float preDb = gainToDb (smartGainPreRmsEnv + 1.0e-7f);
        const float postDb = gainToDb (smartGainPostRmsEnv + 1.0e-7f);
        const float deltaDb = postDb - preDb;
        const float weightedComp = -deltaDb * 0.68f;
        const float nonlinearWeight = 0.60f + 0.40f * std::tanh (std::abs (deltaDb) / 7.5f);
        targetCompDb = juce::jlimit (-6.0f, 6.0f, weightedComp * nonlinearWeight);
    }

    smartGainCompDbSmoothed.setTargetValue (targetCompDb);

    const int channels = juce::jmin (2, buffer.getNumChannels());
    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = dbToGain (smartGainCompDbSmoothed.getNextValue());
        for (int ch = 0; ch < channels; ++ch)
            buffer.getWritePointer (ch)[i] *= gain;
    }
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

    const auto requestedMode = static_cast<ConsoleMode> (juce::jlimit (0, 4, static_cast<int> (apvts.getRawParameterValue (modeId)->load())));
    const auto quality = static_cast<QualityMode> (static_cast<int> (apvts.getRawParameterValue (qualityId)->load()));
    const int oversamplingChoice = static_cast<int> (apvts.getRawParameterValue (oversamplingId)->load());
    const int sidechainModeChoice = juce::jlimit (0, 2, static_cast<int> (apvts.getRawParameterValue (sidechainModeId)->load()));
    const float mixAssistTarget = apvts.getRawParameterValue (mixAssistId)->load() / 100.0f;
    const float focusTarget = apvts.getRawParameterValue (focusModeId)->load() > 0.5f ? 1.0f : 0.0f;

    mixAssistAmountSmoothed.setTargetValue (mixAssistTarget);
    focusAmountSmoothed.setTargetValue (focusTarget);
    const float mixAssistStart = mixAssistAmountSmoothed.getCurrentValue();
    const float focusStart = focusAmountSmoothed.getCurrentValue();
    const float mixAssistEnd = mixAssistAmountSmoothed.skip (buffer.getNumSamples());
    const float focusEnd = focusAmountSmoothed.skip (buffer.getNumSamples());
    const float mixAssistAmount = 0.5f * (mixAssistStart + mixAssistEnd);
    const float focusAmount = 0.5f * (focusStart + focusEnd);

    int osFactor = (oversamplingChoice == 2 ? 4 : (oversamplingChoice == 1 ? 2 : 1));
    if (quality == QualityMode::eco)
        osFactor = 1;

    const bool sidechainExtRequested = sidechainModeChoice > 0;
    const auto& sidechainInput = getBusBuffer (buffer, true, 1);
    const bool sidechainExtActive = sidechainExtRequested && sidechainInput.getNumChannels() > 0;
    const juce::AudioBuffer<float>* detectorBuffer = sidechainExtActive ? &sidechainInput : nullptr;

    if (requestedMode != modeTo)
    {
        modeFrom = modeTo;
        modeTo = requestedMode;
        modeMorph.setCurrentAndTargetValue (0.0f);
        modeMorph.setTargetValue (1.0f);
    }

    const ModeProfile fromProfile = profileForMode (modeFrom);
    const ModeProfile toProfile = profileForMode (modeTo);
    const float morphNow = modeMorph.skip (buffer.getNumSamples());
    const ModeProfile activeProfile = blendProfiles (fromProfile, toProfile, morphNow);
    const float modeTrim = activeProfile.outputTrim;

    if (modeMorph.isSmoothing() == false)
        modeFrom = modeTo;

    inputSmoothed.setTargetValue (dbToGain (apvts.getRawParameterValue (inputId)->load()));
    outputSmoothed.setTargetValue (dbToGain (apvts.getRawParameterValue (outputId)->load()));

    float inPeak = 0.0f;
    float outPeak = 0.0f;
    float preProcessSqAccum = 0.0f;
    float postProcessSqAccum = 0.0f;

    const int channels = juce::jmin (2, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            data[i] *= inputSmoothed.getNextValue();
            preProcessSqAccum += data[i] * data[i];
            inPeak = juce::jmax (inPeak, std::abs (data[i]));
        }
    }

    const float preProcessRms = std::sqrt (preProcessSqAccum / juce::jmax (1, channels * buffer.getNumSamples()));

    // Dynamic unload: if a module is bypassed, its DSP stage is skipped entirely.
    if (apvts.getRawParameterValue (preampOnId)->load() > 0.5f)
        processPreamp (buffer, activeProfile, osFactor);

    if (apvts.getRawParameterValue (filterOnId)->load() > 0.5f)
        processFilters (buffer);

    if (apvts.getRawParameterValue (eqOnId)->load() > 0.5f)
        processEq (buffer, activeProfile);

    if (apvts.getRawParameterValue (compOnId)->load() > 0.5f)
        processCompressor (buffer, activeProfile, quality, detectorBuffer, sidechainExtActive);
    else
        gainReductionMeter.store (0.92f * gainReductionMeter.load());

    if (apvts.getRawParameterValue (gateOnId)->load() > 0.5f)
        processGate (buffer, detectorBuffer, sidechainExtActive);

    if (apvts.getRawParameterValue (analogOnId)->load() > 0.5f)
        processAnalogEngine (buffer, activeProfile, osFactor, quality, mixAssistAmount, focusAmount);

    processMixAssistAndFocus (buffer, mixAssistAmount, focusAmount);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            data[i] = juce::jlimit (-1.35f, 1.35f, data[i] * modeTrim);
            data[i] *= outputSmoothed.getNextValue();
            postProcessSqAccum += data[i] * data[i];
        }
    }

    const float postProcessRms = std::sqrt (postProcessSqAccum / juce::jmax (1, channels * buffer.getNumSamples()));
    applySmartGainCompensation (buffer, preProcessRms, postProcessRms, buffer.getNumSamples());

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            outPeak = juce::jmax (outPeak, std::abs (data[i]));
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
