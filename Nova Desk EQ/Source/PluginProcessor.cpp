#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

NovaDeskEQAudioProcessor::NovaDeskEQAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("NovaDeskEQ"), createParameterLayout())
{
    // Startup state is "Default Channel EQ" (build spec section 34: "only
    // subtle console character... a neutral starting point"), not the
    // individually-scaffolded per-parameter defaults declared above -
    // single-sourced from the preset data rather than hand-duplicating
    // its values into createParameterLayout(). Found by id rather than
    // assuming index 0, so reordering the factory library can't silently
    // change the plugin's startup sound.
    for (int i = 0; i < presetManager.getNumPresets(); ++i)
    {
        if (presetManager.getPreset (i).id == "default_channel_eq")
        {
            presetManager.applyToApvts (i, apvts);
            break;
        }
    }
}

NovaDeskEQAudioProcessor::~NovaDeskEQAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaDeskEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto dbAttributes = juce::AudioProcessorParameter::genericParameter;
    auto dbFormat = [] (float value, int) { return juce::String (value, 1) + " dB"; };

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::input, 1 }, "Input",
        juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f, "dB", dbAttributes, dbFormat));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::inputIn, 1 }, "Input In", true));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::output, 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f, "dB", dbAttributes, dbFormat));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::outputIn, 1 }, "Output In", true));

    // HPF/LPF: stepped rotary switches (build spec sections 9-10). Off is
    // index 0 for both, matching a physically "up"/off detent position.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::hpfFreqIndex, 1 }, "HPF Frequency",
        juce::StringArray { "Off", "20", "50", "100", "200", "400" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::hpfIn, 1 }, "HPF In", true));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::lpfFreqIndex, 1 }, "LPF Frequency",
        juce::StringArray { "Off", "5k", "8k", "12k", "16k", "20k" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::lpfIn, 1 }, "LPF In", true));

    // Low band: continuous freq/gain, Bell/Shelf type (spec section 11).
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::lowFreq, 1 }, "Low Frequency",
        juce::NormalisableRange<float> (30.0f, 300.0f, 0.1f, 0.5f), 100.0f, "Hz",
        dbAttributes, [] (float value, int) { return juce::String ((int) std::round (value)) + " Hz"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::lowGain, 1 }, "Low Gain",
        juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f, "dB", dbAttributes, dbFormat));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::lowType, 1 }, "Low Type",
        juce::StringArray { "Bell", "Shelf" }, 1));

    // Low-Mid band: continuous freq/gain/Q (spec section 12).
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::lowMidFreq, 1 }, "Low Mid Frequency",
        juce::NormalisableRange<float> (100.0f, 2000.0f, 0.1f, 0.4f), 400.0f, "Hz",
        dbAttributes, [] (float value, int) { return juce::String ((int) std::round (value)) + " Hz"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::lowMidGain, 1 }, "Low Mid Gain",
        juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f, "dB", dbAttributes, dbFormat));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::lowMidQ, 1 }, "Low Mid Q",
        juce::NormalisableRange<float> (0.3f, 4.0f, 0.01f), 0.8f, juce::String(),
        dbAttributes, [] (float value, int) { return juce::String (value, 2); }));

    // High-Mid band: continuous freq/gain/Q (spec section 13).
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::highMidFreq, 1 }, "High Mid Frequency",
        juce::NormalisableRange<float> (600.0f, 8000.0f, 0.1f, 0.35f), 2000.0f, "Hz",
        dbAttributes, [] (float value, int)
        {
            return value >= 1000.0f ? juce::String (value / 1000.0f, 2) + " kHz"
                                     : juce::String ((int) std::round (value)) + " Hz";
        }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::highMidGain, 1 }, "High Mid Gain",
        juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f, "dB", dbAttributes, dbFormat));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::highMidQ, 1 }, "High Mid Q",
        juce::NormalisableRange<float> (0.3f, 4.0f, 0.01f), 0.8f, juce::String(),
        dbAttributes, [] (float value, int) { return juce::String (value, 2); }));

    // High band: continuous freq/gain, Bell/Shelf type (spec section 14).
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::highFreq, 1 }, "High Frequency",
        juce::NormalisableRange<float> (3000.0f, 20000.0f, 0.1f, 0.3f), 8000.0f, "Hz",
        dbAttributes, [] (float value, int) { return juce::String (value / 1000.0f, 1) + " kHz"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::highGain, 1 }, "High Gain",
        juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f, "dB", dbAttributes, dbFormat));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::highType, 1 }, "High Type",
        juce::StringArray { "Bell", "Shelf" }, 1));

    // EQ personality (spec section 5-8): E = bolder/faster, G = smoother/broader.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::egMode, 1 }, "EQ Type",
        juce::StringArray { "E", "G" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::drive, 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 0.0f, juce::String(),
        dbAttributes, [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::mix, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%",
        dbAttributes, [] (float value, int) { return juce::String (value, 0) + "%"; }));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::bypass, 1 }, "Bypass", false));

    // Settings-menu only (build spec gap-closure item 3) - not a visible
    // front-panel control alongside the mockup's locked knob layout.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::oversamplingMode, 1 }, "Oversampling",
        juce::StringArray { "Off", "2x", "4x" }, 1));

    return layout;
}

const juce::String NovaDeskEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NovaDeskEQAudioProcessor::acceptsMidi() const { return false; }
bool NovaDeskEQAudioProcessor::producesMidi() const { return false; }
bool NovaDeskEQAudioProcessor::isMidiEffect() const { return false; }
double NovaDeskEQAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int NovaDeskEQAudioProcessor::getNumPrograms() { return 1; }
int NovaDeskEQAudioProcessor::getCurrentProgram() { return 0; }
void NovaDeskEQAudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String NovaDeskEQAudioProcessor::getProgramName (int index) { juce::ignoreUnused (index); return {}; }
void NovaDeskEQAudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName); }

void NovaDeskEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

    // Oversampling mode (Off/2x/4x) choice index maps directly to JUCE's
    // Oversampling "numStages" (0/1/2 - each stage doubles the factor),
    // so no translation table is needed.
    lastAppliedOversamplingStages = (int) apvts.getRawParameterValue (ParameterIDs::oversamplingMode)->load();

    const auto driveAmountAtPrepare = apvts.getRawParameterValue (ParameterIDs::drive)->load();
    const auto inputInOnAtPrepare = apvts.getRawParameterValue (ParameterIDs::inputIn)->load() > 0.5f;
    const auto inputDbAtPrepare = apvts.getRawParameterValue (ParameterIDs::input)->load();
    const auto outputInOnAtPrepare = apvts.getRawParameterValue (ParameterIDs::outputIn)->load() > 0.5f;
    const auto outputDbAtPrepare = apvts.getRawParameterValue (ParameterIDs::output)->load();

    // Float-precision path.
    eqBands.prepare (spec);
    drive.prepare (spec, lastAppliedOversamplingStages); // the oversampler itself always runs (see PluginProcessor.h)
    drive.setDriveAmount (driveAmountAtPrepare);
    inputGain.prepare (spec);
    inputGain.setRampDurationSeconds (kSmoothingSeconds);
    inputGain.setGainDecibels (inputInOnAtPrepare ? inputDbAtPrepare : 0.0f);
    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (kSmoothingSeconds);
    outputGain.setGainDecibels (outputInOnAtPrepare ? outputDbAtPrepare : 0.0f);
    dcBlockR = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 5.0 / currentSampleRate);
    inputDcX1.fill (0.0f); inputDcY1.fill (0.0f);
    outputDcX1.fill (0.0f); outputDcY1.fill (0.0f);
    dryBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock);
    dryBuffer.clear();
    rawInputBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock);
    rawInputBuffer.clear();

    // Double-precision path (build spec's "64-bit double precision
    // processing" - genuinely mirrors the float path, not a float-
    // converted approximation, since every DSP class here is templated).
    eqBandsD.prepare (spec);
    driveD.prepare (spec, lastAppliedOversamplingStages);
    driveD.setDriveAmount (driveAmountAtPrepare);
    inputGainD.prepare (spec);
    inputGainD.setRampDurationSeconds (kSmoothingSeconds);
    inputGainD.setGainDecibels (inputInOnAtPrepare ? inputDbAtPrepare : 0.0f);
    outputGainD.prepare (spec);
    outputGainD.setRampDurationSeconds (kSmoothingSeconds);
    outputGainD.setGainDecibels (outputInOnAtPrepare ? outputDbAtPrepare : 0.0f);
    dcBlockRD = std::exp (-2.0 * juce::MathConstants<double>::pi * 5.0 / currentSampleRate);
    inputDcX1D.fill (0.0); inputDcY1D.fill (0.0);
    outputDcX1D.fill (0.0); outputDcY1D.fill (0.0);
    dryBufferD.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock);
    dryBufferD.clear();
    rawInputBufferD.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock);
    rawInputBufferD.clear();

    setLatencySamples (drive.getLatencySamples());

    // Mix/bypass smoothers are shared control-rate state across both
    // precision paths (see the template declaration's comment).
    mixSm.reset (currentSampleRate, kSmoothingSeconds);
    const auto mixAtPrepare = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (ParameterIDs::mix)->load() / 100.0f);
    mixSm.setCurrentAndTargetValue (mixAtPrepare);

    bypassBlendSm.reset (currentSampleRate, kBypassSeconds);
    const auto bypassedAtPrepare = apvts.getRawParameterValue (ParameterIDs::bypass)->load() > 0.5f;
    bypassBlendSm.setCurrentAndTargetValue (bypassedAtPrepare ? 1.0f : 0.0f);

    inputMeterLevel.store (0.0f);
    outputMeterLevel.store (0.0f);

    startTimer (250); // message-thread poll for oversampling-mode changes
}

void NovaDeskEQAudioProcessor::releaseResources()
{
    stopTimer();
}

void NovaDeskEQAudioProcessor::reconfigureOversamplingIfNeeded()
{
    const int requestedStages = (int) apvts.getRawParameterValue (ParameterIDs::oversamplingMode)->load();
    if (requestedStages == lastAppliedOversamplingStages)
        return;

    // Rebuild happens on the MESSAGE thread only (this is called from
    // timerCallback(), or directly by tests - never from processBlock()).
    // suspendProcessing() blocks the audio thread from calling
    // processBlock() concurrently while NovaDeskDrive::prepare()
    // reallocates its oversampler, so there is no real-time-thread
    // allocation and no race.
    suspendProcessing (true);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = (juce::uint32) getBlockSize();
    spec.numChannels = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

    const auto currentDriveAmount = apvts.getRawParameterValue (ParameterIDs::drive)->load();
    drive.prepare (spec, requestedStages);
    drive.setDriveAmount (currentDriveAmount);
    driveD.prepare (spec, requestedStages);
    driveD.setDriveAmount (currentDriveAmount);
    setLatencySamples (drive.getLatencySamples());
    lastAppliedOversamplingStages = requestedStages;

    suspendProcessing (false);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaDeskEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainOutput == layouts.getMainInputChannelSet();
}
#endif

template <typename FloatType>
float NovaDeskEQAudioProcessor::peakFollow (float currentLevel, const juce::AudioBuffer<FloatType>& buffer)
{
    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        peak = juce::jmax (peak, (float) buffer.getMagnitude (channel, 0, buffer.getNumSamples()));

    constexpr float riseCoeff = 0.6f;
    constexpr float fallCoeff = 0.05f;
    return peak > currentLevel ? currentLevel + riseCoeff * (peak - currentLevel)
                                : currentLevel + fallCoeff * (peak - currentLevel);
}

template <typename FloatType>
void NovaDeskEQAudioProcessor::processChainImpl (
    juce::AudioBuffer<FloatType>& buffer,
    nova::desk::NovaDeskEQBands<FloatType>& eqBandsRef,
    nova::desk::NovaDeskDrive<FloatType>& driveRef,
    juce::dsp::Gain<FloatType>& inputGainRef,
    juce::dsp::Gain<FloatType>& outputGainRef,
    juce::AudioBuffer<FloatType>& dryBufferRef,
    juce::AudioBuffer<FloatType>& rawInputBufferRef,
    std::array<FloatType, 2>& inputDcX1Ref, std::array<FloatType, 2>& inputDcY1Ref,
    std::array<FloatType, 2>& outputDcX1Ref, std::array<FloatType, 2>& outputDcY1Ref,
    FloatType dcBlockRVal)
{
    const int numCh = buffer.getNumChannels();
    const int N = buffer.getNumSamples();

    // True raw input tap, captured before ANY processing - the bypass
    // crossfade's dry reference (catalog convention: bypass passes the
    // raw input through completely untouched, not even Input Trim).
    rawInputBufferRef.setSize (numCh, N, false, false, true);
    rawInputBufferRef.makeCopyOf (buffer, true);

    // Input DC blocker (~5Hz corner, gap-closure item 4).
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* w = buffer.getWritePointer (ch);
        const auto si = (size_t) juce::jmin (ch, 1);
        auto x1 = inputDcX1Ref[si];
        auto y1 = inputDcY1Ref[si];
        for (int i = 0; i < N; ++i)
        {
            const FloatType x = w[i];
            const FloatType y = x - x1 + dcBlockRVal * y1;
            x1 = x; y1 = y;
            w[i] = y;
        }
        inputDcX1Ref[si] = x1; inputDcY1Ref[si] = y1;
    }

    const auto inputInOn = apvts.getRawParameterValue (ParameterIDs::inputIn)->load() > 0.5f;
    const auto inputDb = apvts.getRawParameterValue (ParameterIDs::input)->load();
    inputGainRef.setGainDecibels (inputInOn ? inputDb : 0.0f);
    {
        juce::dsp::AudioBlock<FloatType> block (buffer);
        juce::dsp::ProcessContextReplacing<FloatType> context (block);
        inputGainRef.process (context);
    }

    inputMeterLevel.store (peakFollow (inputMeterLevel.load(), buffer));

    dryBufferRef.setSize (numCh, N, false, false, true);
    dryBufferRef.makeCopyOf (buffer, true);

    using namespace nova::desk;

    const auto egModeIndex = (int) apvts.getRawParameterValue (ParameterIDs::egMode)->load();
    const auto personality = egModeIndex == 0 ? EQPersonality::E : EQPersonality::G;

    const auto driveAmount = apvts.getRawParameterValue (ParameterIDs::drive)->load();
    driveRef.setDriveAmount (driveAmount);
    driveRef.setPersonality (personality);

    // No per-block latency re-check needed: the oversampler now runs
    // unconditionally (see NovaDeskDrive's class comment), so its latency
    // is fixed once prepareToPlay() reports it and never changes with
    // Drive amount.
    driveRef.process (buffer);

    eqBandsRef.setPersonality (personality);

    eqBandsRef.setHpfFreqIndex ((int) apvts.getRawParameterValue (ParameterIDs::hpfFreqIndex)->load());
    eqBandsRef.setHpfIn (apvts.getRawParameterValue (ParameterIDs::hpfIn)->load() > 0.5f);
    eqBandsRef.setLpfFreqIndex ((int) apvts.getRawParameterValue (ParameterIDs::lpfFreqIndex)->load());
    eqBandsRef.setLpfIn (apvts.getRawParameterValue (ParameterIDs::lpfIn)->load() > 0.5f);

    eqBandsRef.setLowFreq (apvts.getRawParameterValue (ParameterIDs::lowFreq)->load());
    eqBandsRef.setLowGainDb (apvts.getRawParameterValue (ParameterIDs::lowGain)->load());
    eqBandsRef.setLowType ((int) apvts.getRawParameterValue (ParameterIDs::lowType)->load() == 0 ? BandType::Bell : BandType::Shelf);

    eqBandsRef.setLowMidFreq (apvts.getRawParameterValue (ParameterIDs::lowMidFreq)->load());
    eqBandsRef.setLowMidGainDb (apvts.getRawParameterValue (ParameterIDs::lowMidGain)->load());
    eqBandsRef.setLowMidQ (apvts.getRawParameterValue (ParameterIDs::lowMidQ)->load());

    eqBandsRef.setHighMidFreq (apvts.getRawParameterValue (ParameterIDs::highMidFreq)->load());
    eqBandsRef.setHighMidGainDb (apvts.getRawParameterValue (ParameterIDs::highMidGain)->load());
    eqBandsRef.setHighMidQ (apvts.getRawParameterValue (ParameterIDs::highMidQ)->load());

    eqBandsRef.setHighFreq (apvts.getRawParameterValue (ParameterIDs::highFreq)->load());
    eqBandsRef.setHighGainDb (apvts.getRawParameterValue (ParameterIDs::highGain)->load());
    eqBandsRef.setHighType ((int) apvts.getRawParameterValue (ParameterIDs::highType)->load() == 0 ? BandType::Bell : BandType::Shelf);

    eqBandsRef.process (buffer);

    // Output trim applies to the WET signal only, before Mix - the
    // locked signal-flow order (see class comment at the top of the
    // header: "...LPF -> OUTPUT -> MIX").
    const auto outputInOn = apvts.getRawParameterValue (ParameterIDs::outputIn)->load() > 0.5f;
    const auto outputDb = apvts.getRawParameterValue (ParameterIDs::output)->load();
    outputGainRef.setGainDecibels (outputInOn ? outputDb : 0.0f);
    {
        juce::dsp::AudioBlock<FloatType> block (buffer);
        juce::dsp::ProcessContextReplacing<FloatType> context (block);
        outputGainRef.process (context);
    }

    // Mix: per-sample smoothed crossfade (dry tap is untouched by Output
    // trim, matching the locked signal-flow order above). mixSm is a
    // shared float-precision control-rate smoother (see the template
    // declaration's comment) - cast its per-sample value to FloatType.
    mixSm.setTargetValue (juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (ParameterIDs::mix)->load() / 100.0f));
    for (int i = 0; i < N; ++i)
    {
        const auto wetAmt = (FloatType) mixSm.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto* dry = dryBufferRef.getReadPointer (ch);
            wet[i] = dry[i] * (FloatType (1) - wetAmt) + wet[i] * wetAmt;
        }
    }

    // Output DC blocker (~5Hz corner, gap-closure item 4), post-Mix.
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* w = buffer.getWritePointer (ch);
        const auto si = (size_t) juce::jmin (ch, 1);
        auto x1 = outputDcX1Ref[si];
        auto y1 = outputDcY1Ref[si];
        for (int i = 0; i < N; ++i)
        {
            const FloatType x = w[i];
            const FloatType y = x - x1 + dcBlockRVal * y1;
            x1 = x; y1 = y;
            w[i] = y;
        }
        outputDcX1Ref[si] = x1; outputDcY1Ref[si] = y1;
    }

    // Bypass: per-sample smoothed crossfade against the true raw input,
    // not a hard branch. The full chain above runs unconditionally even
    // while "bypassed" - see this file's class-comment for why (keeping
    // Drive's oversampler continuously warm to avoid an FIR cold-start
    // click on re-engage).
    const auto bypassed = apvts.getRawParameterValue (ParameterIDs::bypass)->load() > 0.5f;
    bypassBlendSm.setTargetValue (bypassed ? 1.0f : 0.0f);

    const bool settledWet = ! bypassBlendSm.isSmoothing() && bypassBlendSm.getCurrentValue() <= 0.0001f;
    const bool settledDry = ! bypassBlendSm.isSmoothing() && bypassBlendSm.getCurrentValue() >= 0.9999f;

    if (settledDry)
    {
        for (int ch = 0; ch < numCh; ++ch)
            buffer.copyFrom (ch, 0, rawInputBufferRef, ch, 0, N);
    }
    else if (! settledWet)
    {
        for (int i = 0; i < N; ++i)
        {
            const auto dryAmt = (FloatType) bypassBlendSm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* w = buffer.getWritePointer (ch);
                const auto* raw = rawInputBufferRef.getReadPointer (ch);
                w[i] = w[i] * (FloatType (1) - dryAmt) + raw[i] * dryAmt;
            }
        }
    }

    outputMeterLevel.store (peakFollow (outputMeterLevel.load(), buffer));
}

void NovaDeskEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    processChainImpl<float> (buffer, eqBands, drive, inputGain, outputGain, dryBuffer, rawInputBuffer,
                              inputDcX1, inputDcY1, outputDcX1, outputDcY1, dcBlockR);
}

void NovaDeskEQAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    processChainImpl<double> (buffer, eqBandsD, driveD, inputGainD, outputGainD, dryBufferD, rawInputBufferD,
                               inputDcX1D, inputDcY1D, outputDcX1D, outputDcY1D, dcBlockRD);
}

bool NovaDeskEQAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* NovaDeskEQAudioProcessor::createEditor()
{
    return new NovaDeskEQEditor (*this);
}

void NovaDeskEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        abSnapshot.addToXml (*xml);
        // Selected preset index (build spec section 35: "reopen the
        // project... confirm the selected preset/state recalls
        // correctly"). All the underlying param values are already saved
        // by apvts.copyState() above; this alone lets the UI show the
        // correct preset name/highlight after reload.
        xml->setAttribute ("novaDeskSelectedPresetIndex", presetManager.getCurrentIndex());
        copyXmlToBinary (*xml, destData);
    }
}

void NovaDeskEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
    {
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
        abSnapshot.restoreFromXml (*xmlState);

        // Restore the SELECTION only (which preset is highlighted in the
        // browser) - do NOT re-apply its values, since apvts.replaceState
        // above already restored the user's actual (possibly since-
        // tweaked) parameter values; re-applying would discard edits made
        // after loading the preset.
        if (xmlState->hasAttribute ("novaDeskSelectedPresetIndex"))
        {
            const int savedIndex = xmlState->getIntAttribute ("novaDeskSelectedPresetIndex", 0);
            presetManager.setSelectedIndexOnly (savedIndex);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaDeskEQAudioProcessor();
}
