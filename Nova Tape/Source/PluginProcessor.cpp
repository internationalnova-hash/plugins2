#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

NovaTapeAudioProcessor::NovaTapeAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

NovaTapeAudioProcessor::~NovaTapeAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout NovaTapeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::input, "Input", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, juce::AudioParameterFloatAttributes().withLabel ("dB")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::tapeAmount, "Tape", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 65.0f, juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterIDs::tapeType, "Tape Type", juce::StringArray { "Studio", "Vintage", "Hot" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterIDs::ips, "IPS", juce::StringArray { "7.5", "15", "30" }, 1));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::bias, "Bias", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::wowFlutter, "Wow / Flutter", juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 2.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::noise, "Noise", juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 1.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::lowCut, "Low Cut", juce::NormalisableRange<float> (20.0f, 200.0f, 0.1f, 0.4f), 30.0f, juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::highCut, "High Cut", juce::NormalisableRange<float> (5000.0f, 20000.0f, 1.0f, 0.4f), 18000.0f, juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::mix, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::output, "Output", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), -1.3f, juce::AudioParameterFloatAttributes().withLabel ("dB")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::trim, "Trim", juce::NormalisableRange<float> (-6.0f, 6.0f, 0.01f), 0.0f, juce::AudioParameterFloatAttributes().withLabel ("dB")));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterIDs::oversampling, "Oversampling", juce::StringArray { "Off", "2x", "4x" }, 1));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (ParameterIDs::bypass, "Bypass", false));

    return { params.begin(), params.end() };
}

void NovaTapeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
    lastAppliedOversamplingStages = (int) apvts.getRawParameterValue (ParameterIDs::oversampling)->load();
    startTimer (250);
}

void NovaTapeAudioProcessor::releaseResources()
{
    stopTimer();
}

void NovaTapeAudioProcessor::reset()
{
    engine.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaTapeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != mainOut)
        return false;

    return true;
}
#endif

void NovaTapeAudioProcessor::timerCallback()
{
    const int desiredStages = (int) apvts.getRawParameterValue (ParameterIDs::oversampling)->load();
    if (desiredStages != lastAppliedOversamplingStages)
    {
        suspendProcessing (true);
        engine.reconfigureOversamplingIfNeeded (currentSampleRate, getBlockSize(), desiredStages);
        setLatencySamples (engine.getLatencySamples());
        lastAppliedOversamplingStages = desiredStages;
        suspendProcessing (false);
    }
}

void NovaTapeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    nova::tape::NovaTapeEngine::Params p;
    p.inputDb = apvts.getRawParameterValue (ParameterIDs::input)->load();
    p.tapeAmountPercent = apvts.getRawParameterValue (ParameterIDs::tapeAmount)->load();
    p.tapeType = (nova::tape::TapeType) (int) apvts.getRawParameterValue (ParameterIDs::tapeType)->load();
    p.ips = (nova::tape::IpsSpeed) (int) apvts.getRawParameterValue (ParameterIDs::ips)->load();
    p.biasMinus100to100 = apvts.getRawParameterValue (ParameterIDs::bias)->load();
    p.wowFlutter0to10 = apvts.getRawParameterValue (ParameterIDs::wowFlutter)->load();
    p.noise0to10 = apvts.getRawParameterValue (ParameterIDs::noise)->load();
    p.lowCutHz = apvts.getRawParameterValue (ParameterIDs::lowCut)->load();
    p.highCutHz = apvts.getRawParameterValue (ParameterIDs::highCut)->load();
    p.mixPercent = apvts.getRawParameterValue (ParameterIDs::mix)->load();
    p.outputDb = apvts.getRawParameterValue (ParameterIDs::output)->load();
    p.trimDb = apvts.getRawParameterValue (ParameterIDs::trim)->load();
    p.oversamplingStages = lastAppliedOversamplingStages;
    p.bypassed = apvts.getRawParameterValue (ParameterIDs::bypass)->load() >= 0.5f;

    engine.setParams (p);
    engine.process (buffer);

    inputLevelDebug.store (engine.getInputLevelDebug());
    outputLevelDebug.store (engine.getOutputLevelDebug());

    // Reels spin only while genuine signal is actually passing through -
    // not on an arbitrary idle trickle, and not tied to host transport
    // state (unreliable in Standalone, and conceptually wrong anyway: a
    // tape machine's reels turn because tape is moving past the heads,
    // i.e. because there's real audio, not because a DAW's playhead
    // happens to be running with nothing audible on this track).
    constexpr float kSignalThreshold = 0.0008f;
    reelSpeedDebug.store (engine.getInputLevelDebug() > kSignalThreshold ? 1.0f : 0.0f);
}

juce::AudioProcessorEditor* NovaTapeAudioProcessor::createEditor()
{
    return new NovaTapeAudioProcessorEditor (*this);
}

void NovaTapeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    abSnapshot.addToXml (*xml);
    copyXmlToBinary (*xml, destData);
}

void NovaTapeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
    abSnapshot.restoreFromXml (*xmlState);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaTapeAudioProcessor();
}
