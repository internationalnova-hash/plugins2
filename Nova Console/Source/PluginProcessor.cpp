#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr auto modeId         = "console_mode";
    constexpr auto qualityId      = "quality";
    constexpr auto oversamplingId = "oversampling";

    constexpr auto inputId  = "input";
    constexpr auto outputId = "output";

    constexpr auto preampOnId = "preamp_on";
    constexpr auto driveId    = "drive";
    constexpr auto colorId    = "color";
    constexpr auto trimId     = "trim";

    constexpr auto filterOnId  = "filter_on";
    constexpr auto hpfId       = "hpf";
    constexpr auto lpfId       = "lpf";
    constexpr auto hpfSlopeId  = "hpf_slope";
    constexpr auto lpfSlopeId  = "lpf_slope";

    constexpr auto eqOnId       = "eq_on";
    constexpr auto lowId        = "eq_low";
    constexpr auto lowFreqId    = "eq_low_freq";
    constexpr auto lowQId       = "eq_low_q";
    constexpr auto lowMidId     = "eq_low_mid";
    constexpr auto lowMidFreqId = "eq_low_mid_freq";
    constexpr auto lowMidQId    = "eq_low_mid_q";
    constexpr auto highMidId    = "eq_high_mid";
    constexpr auto highMidFreqId= "eq_high_mid_freq";
    constexpr auto highMidQId   = "eq_high_mid_q";
    constexpr auto highId       = "eq_high";
    constexpr auto highFreqId   = "eq_high_freq";
    constexpr auto highQId      = "eq_high_q";
    constexpr auto airId        = "eq_air";
    constexpr auto airFreqId    = "eq_air_freq";
    constexpr auto airQId       = "eq_air_q";
    constexpr auto lowModeId    = "eq_low_mode";
    constexpr auto highModeId   = "eq_high_mode";
    constexpr auto airModeId    = "eq_air_mode";

    constexpr auto compOnId    = "comp_on";
    constexpr auto thresholdId = "comp_threshold";
    constexpr auto ratioId     = "comp_ratio";
    constexpr auto attackId    = "comp_attack";
    constexpr auto releaseId   = "comp_release";
    constexpr auto mixId       = "comp_mix";
    constexpr auto makeupId    = "comp_makeup";
    constexpr auto punchId     = "comp_punch";

    constexpr auto gateOnId        = "gate_on";
    constexpr auto gateThresholdId = "gate_threshold";
    constexpr auto gateReleaseId   = "gate_release";
    constexpr auto gateRangeId     = "gate_range";
    constexpr auto gateAttackId    = "gate_attack";
    constexpr auto gateHoldId      = "gate_hold";
    constexpr auto gateSmoothId    = "gate_smooth";

    constexpr auto analogOnId  = "analog_on";
    constexpr auto heatId      = "analog_heat";
    constexpr auto depthId     = "analog_depth";
    constexpr auto widthId     = "analog_width";
    constexpr auto driftId     = "analog_drift";
    constexpr auto crosstalkId = "analog_crosstalk";
    constexpr auto noiseId     = "analog_noise";

    constexpr auto smartGainId    = "smart_gain";
    constexpr auto focusModeId    = "focus_mode";
    constexpr auto mixAssistId    = "mix_assist";
    constexpr auto sidechainModeId = "sidechain_mode";

    constexpr float minHz = 20.0f;
    constexpr float maxHz = 20000.0f;

    static NovaConsoleParameters buildParams (juce::AudioProcessorValueTreeState& apvts)
    {
        auto load = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

        NovaConsoleParameters p;
        p.mode          = juce::jlimit (0, 4, (int) load (modeId));
        p.quality       = (int) load (qualityId);
        p.oversampling  = (int) load (oversamplingId);
        p.inputDb       = load (inputId);
        p.outputDb      = load (outputId);
        p.preampOn      = load (preampOnId) > 0.5f;
        p.drive         = load (driveId);
        p.color         = load (colorId);
        p.trimDb        = load (trimId);
        p.filterOn      = load (filterOnId) > 0.5f;
        p.hpfHz         = load (hpfId);
        p.lpfHz         = load (lpfId);
        p.hpfSlope      = (int) load (hpfSlopeId);
        p.lpfSlope      = (int) load (lpfSlopeId);
        p.eqOn          = load (eqOnId) > 0.5f;
        p.lowDb         = load (lowId);
        p.lowFreqHz     = load (lowFreqId);
        p.lowQ          = load (lowQId);
        p.lowMode       = (int) load (lowModeId);
        p.lowMidDb      = load (lowMidId);
        p.lowMidFreqHz  = load (lowMidFreqId);
        p.lowMidQ       = load (lowMidQId);
        p.highMidDb     = load (highMidId);
        p.highMidFreqHz = load (highMidFreqId);
        p.highMidQ      = load (highMidQId);
        p.highDb        = load (highId);
        p.highFreqHz    = load (highFreqId);
        p.highQ         = load (highQId);
        p.highMode      = (int) load (highModeId);
        p.airDb         = load (airId);
        p.airFreqHz     = load (airFreqId);
        p.airQ          = load (airQId);
        p.airMode       = (int) load (airModeId);
        p.compOn        = load (compOnId) > 0.5f;
        p.compThreshDb  = load (thresholdId);
        p.compRatio     = load (ratioId);
        p.compAttackMs  = load (attackId);
        p.compReleaseMs = load (releaseId);
        p.compMix       = load (mixId);
        p.compMakeupDb  = load (makeupId);
        p.compPunch     = load (punchId);
        p.gateOn        = load (gateOnId) > 0.5f;
        p.gateThreshDb  = load (gateThresholdId);
        p.gateAttackMs  = load (gateAttackId);
        p.gateHoldMs    = load (gateHoldId);
        p.gateReleaseMs = load (gateReleaseId);
        p.gateRangeDb   = load (gateRangeId);
        p.gateSmooth    = load (gateSmoothId) > 0.5f;
        p.analogOn      = load (analogOnId) > 0.5f;
        p.heat          = load (heatId);
        p.depth         = load (depthId);
        p.width         = load (widthId);
        p.drift         = load (driftId);
        p.crosstalk     = load (crosstalkId);
        p.noise         = load (noiseId);
        p.smartGain     = load (smartGainId) > 0.5f;
        p.focusMode     = load (focusModeId) > 0.5f;
        p.mixAssist     = load (mixAssistId);
        p.sidechainMode = juce::jlimit (0, 2, (int) load (sidechainModeId));
        return p;
    }
}

NovaConsoleAudioProcessor::NovaConsoleAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts (*this, nullptr, juce::Identifier ("NovaConsole"), createParameterLayout())
{
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
        juce::StringArray { "Off", "Internal", "External" }, 0));

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
    // Build initial params from current APVTS state.
    // mixAssist and focusMode are seeded to 0/false to match the original
    // prepareToPlay() which called setCurrentAndTargetValue(0.0f) for both.
    NovaConsoleParameters initial = buildParams (apvts);
    initial.mixAssist = 0.0f;
    initial.focusMode = false;

    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32> (samplesPerBlock),
        2
    };
    engine.prepare (spec, initial);
}

void NovaConsoleAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaConsoleAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input  = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}
#endif

void NovaConsoleAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels  = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();

    for (auto ch = totalInputChannels; ch < totalOutputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    // Measure input peak before processing
    float inPeak = 0.0f;
    const int channels = juce::jmin (2, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            inPeak = juce::jmax (inPeak, std::abs (data[i]));
    }

    // Sidechain routing
    const int sidechainModeChoice = juce::jlimit (0, 2, (int) apvts.getRawParameterValue (sidechainModeId)->load());
    const bool sidechainExtRequested = sidechainModeChoice == 2;
    const auto& sidechainInput = getBusBuffer (buffer, true, 1);
    const bool sidechainExtActive = sidechainExtRequested && sidechainInput.getNumChannels() > 0;
    const juce::AudioBuffer<float>* sidechain = sidechainExtActive ? &sidechainInput : nullptr;

    // Translate APVTS → NovaConsoleParameters and process
    const NovaConsoleParameters p = buildParams (apvts);
    engine.setParameters (p);
    engine.process (buffer, sidechain);

    // Measure output peak after processing
    float outPeak = 0.0f;
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            outPeak = juce::jmax (outPeak, std::abs (data[i]));
    }

    inputMeter.store  (0.84f * inputMeter.load()  + 0.16f * juce::jlimit (0.0f, 1.0f, inPeak));
    outputMeter.store (0.84f * outputMeter.load() + 0.16f * juce::jlimit (0.0f, 1.0f, outPeak));
}

bool NovaConsoleAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* NovaConsoleAudioProcessor::createEditor()
{
    return new NovaConsoleAudioProcessorEditor (*this);
}

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
