#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

NovaLevelAudioProcessor::NovaLevelAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("NovaLevel"), createParameterLayout())
{
}

NovaLevelAudioProcessor::~NovaLevelAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaLevelAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "compression", "Compression", juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 4.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "tone", "Tone", juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "output", "Output", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "mode", "Mode", juce::StringArray { "SMOOTH", "PUNCH", "LIMIT" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "magic", "Magic", false));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "meter", "Meter", juce::StringArray { "GR", "OUT" }, 0));

    return layout;
}

void NovaLevelAudioProcessor::applyPreset (const juce::String& presetName)
{
    if (presetName == "VOCAL")
    {
        apvts.getParameter ("mode")->setValueNotifyingHost (0.0f);
        apvts.getParameter ("compression")->setValueNotifyingHost (0.45f);
        apvts.getParameter ("tone")->setValueNotifyingHost (0.55f);
        apvts.getParameter ("output")->setValueNotifyingHost (0.5833f);
    }
    else if (presetName == "BASS")
    {
        apvts.getParameter ("mode")->setValueNotifyingHost (0.0f);
        apvts.getParameter ("compression")->setValueNotifyingHost (0.55f);
        apvts.getParameter ("tone")->setValueNotifyingHost (0.4f);
        apvts.getParameter ("output")->setValueNotifyingHost (0.625f);
    }
    else if (presetName == "DRUMS")
    {
        apvts.getParameter ("mode")->setValueNotifyingHost (0.5f);
        apvts.getParameter ("compression")->setValueNotifyingHost (0.6f);
        apvts.getParameter ("tone")->setValueNotifyingHost (0.5f);
        apvts.getParameter ("output")->setValueNotifyingHost (0.6042f);
    }
    else if (presetName == "MASTER")
    {
        apvts.getParameter ("mode")->setValueNotifyingHost (0.0f);
        apvts.getParameter ("compression")->setValueNotifyingHost (0.3f);
        apvts.getParameter ("tone")->setValueNotifyingHost (0.5f);
        apvts.getParameter ("output")->setValueNotifyingHost (0.5f);
    }
}

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
    dryBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), juce::jmax (1, samplesPerBlock));
    dryBuffer.clear();
    outputPeakLevel.store (0.0f);
    gainReductionLevel.store (0.0f);
    outputIsHot.store (false);
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

    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* read = buffer.getReadPointer (channel);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            peak = juce::jmax (peak, std::abs (read[i]));
    }

    outputPeakLevel.store (peak);
    gainReductionLevel.store (0.0f);
    outputIsHot.store (peak > 0.98f);
}

bool NovaLevelAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NovaLevelAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void NovaLevelAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void NovaLevelAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

