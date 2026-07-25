#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

NovaTranslateAudioProcessor::NovaTranslateAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("NovaTranslate"), createParameterLayout())
{
}

NovaTranslateAudioProcessor::~NovaTranslateAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaTranslateAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray environmentChoices;
    for (auto* name : ParameterIDs::environmentNames)
        environmentChoices.add (name);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::environmentId, 1 },
        "Environment",
        environmentChoices,
        EnvironmentLibrary::NovaMainRoom));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::environmentAmount, 1 },
        "Environment Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f),
        100.0f,
        "%",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    juce::StringArray positionChoices;
    for (auto* name : ParameterIDs::positionNames)
        positionChoices.add (name);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::position, 1 },
        "Position",
        positionChoices,
        0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::roomAmount, 1 },
        "Room Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f),
        50.0f,
        "%",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::characterAmount, 1 },
        "Character Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f),
        60.0f,
        "%",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::noiseAmount, 1 },
        "Noise Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f),
        0.0f,
        "%",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::noiseEnabled, 1 }, "Noise Enabled", false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::levelMatchEnabled, 1 }, "Level Match", true));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::outputTrim, 1 },
        "Output Trim",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f),
        0.0f,
        "dB",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int)
        {
            const auto sign = value >= 0.0f ? "+" : "";
            return sign + juce::String (value, 1) + " dB";
        }));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::monoEnabled, 1 }, "Mono", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::leftOnly, 1 }, "Left Only", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::rightOnly, 1 }, "Right Only", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::midOnly, 1 }, "Mid Only", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::sideOnly, 1 }, "Side Only", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::dimEnabled, 1 }, "Dim", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::phaseInvert, 1 }, "Phase Invert", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::bypass, 1 }, "Bypass", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::safeRenderEnabled, 1 }, "Safe Render", true));

    return layout;
}

NovaTranslateParams NovaTranslateAudioProcessor::readCurrentParams() const
{
    NovaTranslateParams p;
    p.environmentIndex = static_cast<int> (apvts.getRawParameterValue (ParameterIDs::environmentId)->load());
    p.environmentAmount = apvts.getRawParameterValue (ParameterIDs::environmentAmount)->load();
    p.positionIndex = static_cast<int> (apvts.getRawParameterValue (ParameterIDs::position)->load());
    p.roomAmount = apvts.getRawParameterValue (ParameterIDs::roomAmount)->load();
    p.characterAmount = apvts.getRawParameterValue (ParameterIDs::characterAmount)->load();
    p.noiseAmount = apvts.getRawParameterValue (ParameterIDs::noiseAmount)->load();
    p.noiseEnabled = apvts.getRawParameterValue (ParameterIDs::noiseEnabled)->load() > 0.5f;
    p.levelMatchEnabled = apvts.getRawParameterValue (ParameterIDs::levelMatchEnabled)->load() > 0.5f;
    p.outputTrimDb = apvts.getRawParameterValue (ParameterIDs::outputTrim)->load();
    p.monoEnabled = apvts.getRawParameterValue (ParameterIDs::monoEnabled)->load() > 0.5f;
    p.leftOnly = apvts.getRawParameterValue (ParameterIDs::leftOnly)->load() > 0.5f;
    p.rightOnly = apvts.getRawParameterValue (ParameterIDs::rightOnly)->load() > 0.5f;
    p.midOnly = apvts.getRawParameterValue (ParameterIDs::midOnly)->load() > 0.5f;
    p.sideOnly = apvts.getRawParameterValue (ParameterIDs::sideOnly)->load() > 0.5f;
    p.dimEnabled = apvts.getRawParameterValue (ParameterIDs::dimEnabled)->load() > 0.5f;
    p.phaseInvert = apvts.getRawParameterValue (ParameterIDs::phaseInvert)->load() > 0.5f;
    p.bypass = apvts.getRawParameterValue (ParameterIDs::bypass)->load() > 0.5f;
    return p;
}

const juce::String NovaTranslateAudioProcessor::getName() const { return "Nova Translate"; }
bool NovaTranslateAudioProcessor::acceptsMidi() const { return false; }
bool NovaTranslateAudioProcessor::producesMidi() const { return false; }
bool NovaTranslateAudioProcessor::isMidiEffect() const { return false; }
double NovaTranslateAudioProcessor::getTailLengthSeconds() const { return 0.4; }

int NovaTranslateAudioProcessor::getNumPrograms() { return 1; }
int NovaTranslateAudioProcessor::getCurrentProgram() { return 0; }
void NovaTranslateAudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String NovaTranslateAudioProcessor::getProgramName (int index) { juce::ignoreUnused (index); return {}; }
void NovaTranslateAudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName); }

void NovaTranslateAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumOutputChannels()));
}

void NovaTranslateAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaTranslateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainOutput == layouts.getMainInputChannelSet();
}
#endif

void NovaTranslateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    engine.process (buffer, readCurrentParams());
}

bool NovaTranslateAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* NovaTranslateAudioProcessor::createEditor()
{
    return new NovaTranslateAudioProcessorEditor (*this);
}

void NovaTranslateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void NovaTranslateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaTranslateAudioProcessor();
}
