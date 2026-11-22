#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

#include <cmath>

NovaPunchAudioProcessor::NovaPunchAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    for (int i = 0; i < presetManager.getNumPresets(); ++i)
    {
        if (presetManager.getPreset (i).id == "default")
        {
            presetManager.applyToApvts (i, apvts);
            break;
        }
    }
}

NovaPunchAudioProcessor::~NovaPunchAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout
NovaPunchAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::punch, "Punch",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::body, "Body",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::focus, "Focus",
        juce::StringArray { "Full", "Low", "Mid", "High" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::sensitivity, "Sensitivity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 65.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::mix, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::clipSafe, "Clip Safe", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::bypass, "Bypass", false));

    return { params.begin(), params.end() };
}

void NovaPunchAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
}

void NovaPunchAudioProcessor::releaseResources()
{
}

void NovaPunchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    const float* chL = buffer.getReadPointer (0);
    const float* chR = (numChannels > 1) ? buffer.getReadPointer (1) : chL;

    nova::punch::NovaPunchEngine::Params p;
    p.punchPercent = apvts.getRawParameterValue (ParameterIDs::punch)->load();
    p.bodyPercent = apvts.getRawParameterValue (ParameterIDs::body)->load();
    p.focus = (nova::punch::NovaPunchEngine::Focus) (int) apvts.getRawParameterValue (ParameterIDs::focus)->load();
    p.sensitivityPercent = apvts.getRawParameterValue (ParameterIDs::sensitivity)->load();
    p.mixPercent = apvts.getRawParameterValue (ParameterIDs::mix)->load();
    p.clipSafe = apvts.getRawParameterValue (ParameterIDs::clipSafe)->load() >= 0.5f;
    p.bypassed = apvts.getRawParameterValue (ParameterIDs::bypass)->load() >= 0.5f;

    engine.setParams (p);

    float* outL = buffer.getWritePointer (0);
    if (numChannels > 1)
    {
        float* outR = buffer.getWritePointer (1);
        engine.process (chL, chR, outL, outR, numSamples);
    }
    else
    {
        // Mono bus: engine always runs stereo internally with identical
        // L/R input (chR==chL above), so both outputs are deterministically
        // identical - write both into the same mono buffer, R after L.
        engine.process (chL, chR, outL, outL, numSamples);
    }

    transientProbability.store (engine.getTransientProbability());
    transientStrength.store (engine.getTransientStrength());
    attackGainDb.store (engine.getAttackGainDb());
    decayGainDb.store (engine.getDecayGainDb());
    sustainGainDb.store (engine.getSustainGainDb());
    lowActivity.store (engine.getLowActivity());
    midActivity.store (engine.getMidActivity());
    highActivity.store (engine.getHighActivity());
    inputPeakL.store (engine.getInputPeakL());
    inputPeakR.store (engine.getInputPeakR());
    outputPeakL.store (engine.getOutputPeakL());
    outputPeakR.store (engine.getOutputPeakR());
    currentRegion.store (engine.getCurrentRegion());

    const int writeIdx = activityHistoryWrite.load();
    punchActivityHistory[(size_t) writeIdx].store (engine.getPunchActivity());
    bodyActivityHistory[(size_t) writeIdx].store (engine.getBodyActivity());
    activityHistoryWrite.store ((writeIdx + 1) % kActivityHistorySize);
}

float NovaPunchAudioProcessor::getPunchActivityHistoryAt (int indexFromOldest) const noexcept
{
    const int oldest = activityHistoryWrite.load();
    const int idx = (oldest + juce::jlimit (0, kActivityHistorySize - 1, indexFromOldest)) % kActivityHistorySize;
    return punchActivityHistory[(size_t) idx].load();
}

float NovaPunchAudioProcessor::getBodyActivityHistoryAt (int indexFromOldest) const noexcept
{
    const int oldest = activityHistoryWrite.load();
    const int idx = (oldest + juce::jlimit (0, kActivityHistorySize - 1, indexFromOldest)) % kActivityHistorySize;
    return bodyActivityHistory[(size_t) idx].load();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaPunchAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}
#endif

juce::AudioProcessorEditor* NovaPunchAudioProcessor::createEditor()
{
    return new NovaPunchAudioProcessorEditor (*this);
}

bool NovaPunchAudioProcessor::hasEditor() const { return true; }
const juce::String NovaPunchAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaPunchAudioProcessor::acceptsMidi()  const { return false; }
bool NovaPunchAudioProcessor::producesMidi() const { return false; }
bool NovaPunchAudioProcessor::isMidiEffect() const { return false; }
double NovaPunchAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int NovaPunchAudioProcessor::getNumPrograms()    { return 1; }
int NovaPunchAudioProcessor::getCurrentProgram() { return 0; }
void NovaPunchAudioProcessor::setCurrentProgram (int) {}
const juce::String NovaPunchAudioProcessor::getProgramName (int) { return {}; }
void NovaPunchAudioProcessor::changeProgramName (int, const juce::String&) {}

void NovaPunchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("novaPunchSelectedPresetIndex", presetManager.getCurrentIndex());
    copyXmlToBinary (*xml, destData);
}

void NovaPunchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xmlState));

    if (xmlState->hasAttribute ("novaPunchSelectedPresetIndex"))
        presetManager.setSelectedIndexOnly (xmlState->getIntAttribute ("novaPunchSelectedPresetIndex", 0));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaPunchAudioProcessor();
}
