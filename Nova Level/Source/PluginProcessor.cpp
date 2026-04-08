#include "PluginProcessor.h"

NovaLevelAudioProcessor::NovaLevelAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

NovaLevelAudioProcessor::~NovaLevelAudioProcessor() = default;

const juce::String NovaLevelAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NovaLevelAudioProcessor::acceptsMidi() const
{
    return false;
}

bool NovaLevelAudioProcessor::producesMidi() const
{
    return false;
}

bool NovaLevelAudioProcessor::isMidiEffect() const
{
    return false;
}

double NovaLevelAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NovaLevelAudioProcessor::getNumPrograms()
{
    return 1;
}

int NovaLevelAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NovaLevelAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String NovaLevelAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void NovaLevelAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void NovaLevelAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void NovaLevelAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaLevelAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainOutput == layouts.getMainInputChannelSet();
}
#endif

void NovaLevelAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());
}

bool NovaLevelAudioProcessor::hasEditor() const
{
    return false;
}

juce::AudioProcessorEditor* NovaLevelAudioProcessor::createEditor()
{
    return nullptr;
}

void NovaLevelAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
}

void NovaLevelAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}

