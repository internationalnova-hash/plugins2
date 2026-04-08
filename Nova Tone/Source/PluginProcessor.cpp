#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

#include <array>
#include <cmath>

namespace
{
    template <size_t N>
    float getChoiceValue (const std::array<float, N>& values, int index) noexcept
    {
        return values[static_cast<size_t> (juce::jlimit (0, static_cast<int> (N) - 1, index))];
    }

    float mapBandwidthToQ (float bandwidth) noexcept
    {
        return juce::jmap (bandwidth, 1.0f, 10.0f, 0.45f, 1.8f);
    }
}

NovaToneAudioProcessor::NovaToneAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("NovaTone"), createParameterLayout())
{
}

NovaToneAudioProcessor::~NovaToneAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaToneAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::lowFreq, 1 },
        "Low Frequency",
        juce::StringArray { "20", "30", "60", "100" },
        2));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::lowBoost, 1 },
        "Low Boost",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::lowAttenuation, 1 },
        "Low Attenuation",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::highBoostFreq, 1 },
        "High Boost Frequency",
        juce::StringArray { "3k", "4k", "5k", "8k", "10k", "12k", "16k" },
        3));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::highBoost, 1 },
        "High Boost",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::bandwidth, 1 },
        "Bandwidth",
        juce::NormalisableRange<float> (1.0f, 10.0f, 0.1f),
        5.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return "BW " + juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::highAttenuationFreq, 1 },
        "High Attenuation Frequency",
        juce::StringArray { "5k", "10k", "20k" },
        1));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::highAttenuation, 1 },
        "High Attenuation",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::outputGain, 1 },
        "Output Gain",
        juce::NormalisableRange<float> (-10.0f, 10.0f, 0.1f),
        0.0f,
        "dB",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " dB"; }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::modePreset, 1 },
        "Mode Preset",
        juce::StringArray { "Neutral", "Vocal", "Bass", "Air", "Master" },
        0));

    return layout;
}

const juce::String NovaToneAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NovaToneAudioProcessor::acceptsMidi() const
{
    return false;
}

bool NovaToneAudioProcessor::producesMidi() const
{
    return false;
}

bool NovaToneAudioProcessor::isMidiEffect() const
{
    return false;
}

double NovaToneAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NovaToneAudioProcessor::getNumPrograms()
{
    return 1;
}

int NovaToneAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NovaToneAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String NovaToneAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void NovaToneAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void NovaToneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (juce::jmax (1, getTotalNumOutputChannels()));

    lowBoostFilter.prepare (spec);
    lowAttenuationFilter.prepare (spec);
    highBoostFilter.prepare (spec);
    highAttenuationFilter.prepare (spec);
    outputTrim.prepare (spec);
    outputTrim.setRampDurationSeconds (0.03);

    lowBoostFilter.reset();
    lowAttenuationFilter.reset();
    highBoostFilter.reset();
    highAttenuationFilter.reset();
    outputTrim.reset();

    peakHold = 0.0f;
    outputPeakLevel.store (0.0f);
    outputIsHot.store (false);
}

void NovaToneAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaToneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainOutput == layouts.getMainInputChannelSet();
}
#endif

void NovaToneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    static constexpr std::array<float, 4> lowFreqChoices { 20.0f, 30.0f, 60.0f, 100.0f };
    static constexpr std::array<float, 7> highBoostFreqChoices { 3000.0f, 4000.0f, 5000.0f, 8000.0f, 10000.0f, 12000.0f, 16000.0f };
    static constexpr std::array<float, 3> highAttenFreqChoices { 5000.0f, 10000.0f, 20000.0f };

    const auto lowFreq = getChoiceValue (lowFreqChoices, static_cast<int> (apvts.getRawParameterValue (ParameterIDs::lowFreq)->load()));
    const auto lowBoostAmount = apvts.getRawParameterValue (ParameterIDs::lowBoost)->load();
    const auto lowAttenuationAmount = apvts.getRawParameterValue (ParameterIDs::lowAttenuation)->load();
    const auto highBoostFreq = getChoiceValue (highBoostFreqChoices, static_cast<int> (apvts.getRawParameterValue (ParameterIDs::highBoostFreq)->load()));
    const auto highBoostAmount = apvts.getRawParameterValue (ParameterIDs::highBoost)->load();
    const auto bandwidth = apvts.getRawParameterValue (ParameterIDs::bandwidth)->load();
    const auto highAttenFreq = getChoiceValue (highAttenFreqChoices, static_cast<int> (apvts.getRawParameterValue (ParameterIDs::highAttenuationFreq)->load()));
    const auto highAttenAmount = apvts.getRawParameterValue (ParameterIDs::highAttenuation)->load();
    const auto outputGainDb = apvts.getRawParameterValue (ParameterIDs::outputGain)->load();
    juce::ignoreUnused (apvts.getRawParameterValue (ParameterIDs::modePreset)->load());

    const auto lowBoostGain = juce::Decibels::decibelsToGain (lowBoostAmount * 1.2f);
    const auto lowAttenGain = juce::Decibels::decibelsToGain (-lowAttenuationAmount * 1.15f);
    const auto highBoostGain = juce::Decibels::decibelsToGain (highBoostAmount * 1.35f);
    const auto highAttenGain = juce::Decibels::decibelsToGain (-highAttenAmount * 1.1f);
    const auto q = mapBandwidthToQ (bandwidth);

    *lowBoostFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, lowFreq, 0.58f, lowBoostGain);
    *lowAttenuationFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, lowFreq * 1.18f, 0.72f, lowAttenGain);
    *highBoostFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, highBoostFreq, q, highBoostGain);
    *highAttenuationFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, highAttenFreq, 0.62f, highAttenGain);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    lowBoostFilter.process (context);
    lowAttenuationFilter.process (context);
    highBoostFilter.process (context);
    highAttenuationFilter.process (context);

    const auto saturationDrive = 1.0f + 0.0225f * (lowBoostAmount + highBoostAmount) + 0.01f * std::abs (outputGainDb);
    const auto normalization = std::tanh (saturationDrive);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* samples = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto clean = samples[sample];
            const auto saturated = std::tanh (clean * saturationDrive) / normalization;
            samples[sample] = juce::jmap (0.16f, clean, saturated);
        }
    }

    outputTrim.setGainDecibels (outputGainDb);
    outputTrim.process (context);

    float peak = 0.0f;
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const auto* samples = buffer.getReadPointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            peak = juce::jmax (peak, std::abs (samples[sample]));
    }

    peakHold = peak > peakHold ? peak : peakHold * 0.92f;
    outputPeakLevel.store (juce::jlimit (0.0f, 1.0f, peakHold));
    outputIsHot.store (peak >= 0.985f);
}

bool NovaToneAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NovaToneAudioProcessor::createEditor()
{
    return new NovaToneAudioProcessorEditor (*this);
}

void NovaToneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NovaToneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaToneAudioProcessor();
}
