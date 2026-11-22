#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

NovaSevenAudioProcessor::NovaSevenAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("NovaSeven"), createParameterLayout())
{
}

NovaSevenAudioProcessor::~NovaSevenAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaSevenAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // input_trim: UI shows a 0-80 vintage-console-style label, but the
    // param itself stores real applied gain in dB (-20..+24), skewed so
    // 0dB sits at the knob's center - plan section 5.8.
    {
        juce::NormalisableRange<float> range (-20.0f, 24.0f);
        range.setSkewForCentre (0.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParameterIDs::inputTrim, 1 },
            "Input Trim",
            range,
            0.0f,
            juce::String(),
            juce::AudioProcessorParameter::genericParameter,
            [] (float value, int)
            {
                const auto consoleLabel = juce::jmap (value, -20.0f, 24.0f, 0.0f, 80.0f);
                return juce::String ((int) std::round (consoleLabel));
            }));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::preampGain, 1 },
        "Preamp Gain",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        3.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::characterMode, 1 },
        "Character",
        juce::StringArray { "Clean", "Classic", "Hot" },
        1));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::hpfFreq, 1 },
        "HPF",
        juce::StringArray { "Off", "20", "50", "80", "160", "300" },
        0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::lowFreq, 1 },
        "Low Frequency",
        juce::StringArray { "35", "60", "110", "220", "360", "700" },
        2));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::lowGain, 1 },
        "Low Gain",
        juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f),
        0.0f,
        "dB",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " dB"; }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::midFreq, 1 },
        "Mid Frequency",
        juce::StringArray { "110", "220", "360", "700", "1600", "3200" },
        3));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::midGain, 1 },
        "Mid Gain",
        juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f),
        0.0f,
        "dB",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " dB"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::highShelfGain, 1 },
        "High Shelf (12k)",
        juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f),
        0.0f,
        "dB",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " dB"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::mix, 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        100.0f,
        "%",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 0) + "%"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::outputTrim, 1 },
        "Output Trim",
        juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f),
        0.0f,
        "dB",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " dB"; }));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::phaseInvert, 1 },
        "Phase Invert",
        false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::bypass, 1 },
        "Bypass",
        false));

    // Choice index maps 1:1 onto NovaOversampledColorStage's own
    // "numStages" argument (0=Off, 1=2x, 2=4x) - no translation table
    // needed anywhere this value is read.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::oversamplingMode, 1 },
        "Oversampling",
        juce::StringArray { "Off", "2x", "4x" },
        1));

    return layout;
}

const juce::String NovaSevenAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NovaSevenAudioProcessor::acceptsMidi() const { return false; }
bool NovaSevenAudioProcessor::producesMidi() const { return false; }
bool NovaSevenAudioProcessor::isMidiEffect() const { return false; }
double NovaSevenAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int NovaSevenAudioProcessor::getNumPrograms() { return 1; }
int NovaSevenAudioProcessor::getCurrentProgram() { return 0; }
void NovaSevenAudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String NovaSevenAudioProcessor::getProgramName (int index) { juce::ignoreUnused (index); return {}; }
void NovaSevenAudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName); }

void NovaSevenAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

    lastAppliedOversamplingStages = (int) apvts.getRawParameterValue (ParameterIDs::oversamplingMode)->load();
    colorStage.prepare (spec, lastAppliedOversamplingStages);
    eq.prepare (spec);
    vuBallistics.prepare (currentSampleRate);

    setLatencySamples (colorStage.getLatencySamples());

    dryBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock);
    dryBuffer.clear();

    vuDisplayLevel.store (0.0f);

    startTimer (250);
}

void NovaSevenAudioProcessor::releaseResources()
{
    stopTimer();
}

void NovaSevenAudioProcessor::reconfigureOversamplingIfNeeded()
{
    const int requestedStages = (int) apvts.getRawParameterValue (ParameterIDs::oversamplingMode)->load();
    if (requestedStages == lastAppliedOversamplingStages)
        return;

    // Rebuild happens on the MESSAGE thread only (via timerCallback(), or
    // called directly by tests) - never from processBlock(). suspendProcessing()
    // blocks the audio thread from calling processBlock() concurrently
    // while NovaOversampledColorStage::prepare() reallocates its
    // oversampler, so there is no real-time-thread allocation and no race.
    suspendProcessing (true);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = (juce::uint32) getBlockSize();
    spec.numChannels = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

    colorStage.prepare (spec, requestedStages);
    setLatencySamples (colorStage.getLatencySamples());
    lastAppliedOversamplingStages = requestedStages;

    suspendProcessing (false);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaSevenAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainOutput == layouts.getMainInputChannelSet();
}
#endif

void NovaSevenAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    using namespace nova::seven;

    // Bypass: a true hard A/B of the entire chain (plan section 5.1) -
    // the raw input passes through completely untouched, not even Input
    // Trim. VU meter taps the input directly when bypassed (plan's own
    // "pre-bypass-passthrough when bypassed shows input instead" note).
    const auto bypassed = apvts.getRawParameterValue (ParameterIDs::bypass)->load() > 0.5f;
    if (bypassed)
    {
        vuBallistics.process (buffer);
        vuDisplayLevel.store (vuBallistics.getDisplayLevelVu());
        return;
    }

    const auto inputTrimDb = apvts.getRawParameterValue (ParameterIDs::inputTrim)->load();
    buffer.applyGain (juce::Decibels::decibelsToGain (inputTrimDb));

    // Dry tap for Mix is post-Input-Trim (Input Trim sits before the
    // branch point in the locked signal flow) but pre-color/EQ.
    dryBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    dryBuffer.makeCopyOf (buffer, true);

    const auto characterModeIndex = (int) apvts.getRawParameterValue (ParameterIDs::characterMode)->load();
    const auto mode = characterModeIndex == 0 ? NovaCharacterMode::Clean
                     : characterModeIndex == 2 ? NovaCharacterMode::Hot
                                                : NovaCharacterMode::Classic;
    colorStage.setMode (mode);
    colorStage.setPreampGain (apvts.getRawParameterValue (ParameterIDs::preampGain)->load());
    // How "hot" the incoming signal is, for the transformer's always-on
    // drive-dependent character (plan section 5.2) - derived from Input
    // Trim's position across its full range, not a separate control.
    colorStage.setTransformerDriveLevel (juce::jmap (inputTrimDb, -20.0f, 24.0f, 0.0f, 1.0f));
    colorStage.process (buffer);

    const auto hpfChoiceIndex = (int) apvts.getRawParameterValue (ParameterIDs::hpfFreq)->load();
    eq.setHpfFreqIndex (hpfChoiceIndex);
    eq.setLowFreqIndex ((int) apvts.getRawParameterValue (ParameterIDs::lowFreq)->load());
    eq.setLowGainDb (apvts.getRawParameterValue (ParameterIDs::lowGain)->load());
    eq.setMidFreqIndex ((int) apvts.getRawParameterValue (ParameterIDs::midFreq)->load());
    eq.setMidGainDb (apvts.getRawParameterValue (ParameterIDs::midGain)->load());
    eq.setHighShelfGainDb (apvts.getRawParameterValue (ParameterIDs::highShelfGain)->load());
    eq.process (buffer);

    // Mix: dry/wet crossfade of everything above (plan section 5.1).
    const auto mixNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (ParameterIDs::mix)->load() / 100.0f);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* wet = buffer.getWritePointer (channel);
        const auto* dry = dryBuffer.getReadPointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            wet[sample] = (dry[sample] * (1.0f - mixNorm)) + (wet[sample] * mixNorm);
    }

    // Out Trim: follows the locked section 5.1 diagram (after Mix) - a
    // master level control over the full blended signal. Section 5.8's
    // prose says "pre-mix-blend-with-dry" instead, but that contradicts
    // the diagram and isn't how an "Output Trim" knob is normally
    // expected to behave (affecting everything you hear, not just the
    // wet portion) - trusting the diagram as the actual locked spec.
    const auto outputTrimDb = apvts.getRawParameterValue (ParameterIDs::outputTrim)->load();
    buffer.applyGain (juce::Decibels::decibelsToGain (outputTrimDb));

    const auto phaseInvert = apvts.getRawParameterValue (ParameterIDs::phaseInvert)->load() > 0.5f;
    if (phaseInvert)
        buffer.applyGain (-1.0f);

    // VU meter taps the final output, post Out Trim (plan section 5.1).
    vuBallistics.process (buffer);
    vuDisplayLevel.store (vuBallistics.getDisplayLevelVu());
}

bool NovaSevenAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* NovaSevenAudioProcessor::createEditor()
{
    return new NovaSevenEditor (*this);
}

void NovaSevenAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        abSnapshot.addToXml (*xml);
        copyXmlToBinary (*xml, destData);
    }
}

void NovaSevenAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
    {
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
        abSnapshot.restoreFromXml (*xmlState);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaSevenAudioProcessor();
}
