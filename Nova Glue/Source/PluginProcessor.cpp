#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

namespace
{
    constexpr int kDefaultOversamplingChoice = 1; // index into {Off,2x,4x} -> 2x

    int oversamplingChoiceToStages (int choiceIndex) noexcept
    {
        switch (choiceIndex)
        {
            case 0: return 0; // Off
            case 1: return 1; // 2x
            case 2: return 2; // 4x
            default: return 1;
        }
    }

    constexpr float kOversamplingReconfigureHz = 20.0f;
}

NovaGlueAudioProcessor::NovaGlueAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    presetManager.scanUserPresets();
    startTimerHz ((int) kOversamplingReconfigureHz);
}

NovaGlueAudioProcessor::~NovaGlueAudioProcessor()
{
    stopTimer();
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaGlueAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::threshold, 1 }, "Threshold",
        juce::NormalisableRange<float> (-30.0f, 10.0f), -10.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::attack, 1 }, "Attack",
        juce::StringArray { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }, 4));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::release, 1 }, "Release",
        juce::StringArray { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "Auto" }, 4));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::makeup, 1 }, "Makeup",
        juce::NormalisableRange<float> (-20.0f, 20.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::ratio, 1 }, "Ratio",
        juce::StringArray { "2:1", "4:1", "10:1" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::scHpf, 1 }, "SC HPF",
        juce::NormalisableRange<float> (20.0f, 400.0f, 0.0f, 0.4f), 20.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::mix, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::glue, 1 }, "Glue",
        juce::NormalisableRange<float> (0.0f, 100.0f), 45.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::drive, 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::autoGain, 1 }, "Auto Gain", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::stereoLink, 1 }, "Stereo Link", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::oversampling, 1 }, "Oversampling",
        juce::StringArray { "Off", "2x", "4x" }, kDefaultOversamplingChoice));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::bypass, 1 }, "Bypass", false));

    return layout;
}

void NovaGlueAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    const int choiceIndex = (int) apvts.getRawParameterValue (ParameterIDs::oversampling)->load();
    lastPreparedOversamplingStages = oversamplingChoiceToStages (choiceIndex);

    reprepareEngine();
}

void NovaGlueAudioProcessor::reprepareEngine()
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = (juce::uint32) currentBlockSize;
    spec.numChannels = 2;

    engine.prepare (spec, lastPreparedOversamplingStages);
    setLatencySamples (engine.getLatencySamples());

    monoScratchBuffer.setSize (2, currentBlockSize, false, false, true);
}

void NovaGlueAudioProcessor::reconfigureOversamplingIfNeeded()
{
    const int choiceIndex = (int) apvts.getRawParameterValue (ParameterIDs::oversampling)->load();
    const int requestedStages = oversamplingChoiceToStages (choiceIndex);
    if (requestedStages == lastPreparedOversamplingStages)
        return;

    // Message-thread only (this is a juce::Timer callback) - suspendProcessing()
    // blocks the audio thread from calling processBlock() concurrently
    // while the engine's Oversampling objects are reallocated, so there is
    // no real-time-thread allocation and no race.
    suspendProcessing (true);
    lastPreparedOversamplingStages = requestedStages;
    reprepareEngine();
    suspendProcessing (false);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaGlueAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // The Core engine is stereo-only (Stereo Link needs a channel pair).
    // Mono is adapted in processBlock by duplicating into a scratch stereo
    // pair and averaging back down, so mono-in/mono-out is supported at
    // the bus level even though nothing downstream is mono-aware. Mixed
    // mono-in/stereo-out (or vice versa) is not supported.
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainIn != mainOut)
        return false;
    return mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo();
}
#endif

void NovaGlueAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ScopedNoDenormals noDenormals;

    engine.setParams (
        apvts.getRawParameterValue (ParameterIDs::threshold)->load(),
        (int) apvts.getRawParameterValue (ParameterIDs::attack)->load(),
        (int) apvts.getRawParameterValue (ParameterIDs::release)->load(),
        apvts.getRawParameterValue (ParameterIDs::makeup)->load(),
        (int) apvts.getRawParameterValue (ParameterIDs::ratio)->load(),
        apvts.getRawParameterValue (ParameterIDs::scHpf)->load(),
        apvts.getRawParameterValue (ParameterIDs::mix)->load(),
        apvts.getRawParameterValue (ParameterIDs::glue)->load(),
        apvts.getRawParameterValue (ParameterIDs::drive)->load(),
        apvts.getRawParameterValue (ParameterIDs::autoGain)->load() >= 0.5f,
        apvts.getRawParameterValue (ParameterIDs::stereoLink)->load() >= 0.5f,
        apvts.getRawParameterValue (ParameterIDs::bypass)->load() >= 0.5f);

    const int numSamples = buffer.getNumSamples();
    const bool isMonoBus = getMainBusNumInputChannels() == 1 && getMainBusNumOutputChannels() == 1;

    if (isMonoBus)
    {
        monoScratchBuffer.setSize (2, numSamples, false, false, true);
        monoScratchBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
        monoScratchBuffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

        engine.processBlock (monoScratchBuffer);

        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (0, i, 0.5f * (monoScratchBuffer.getSample (0, i) + monoScratchBuffer.getSample (1, i)));
    }
    else
    {
        engine.processBlock (buffer);
    }

    for (int ch = getMainBusNumOutputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    telemetryGrDb.store (engine.getGrDisplayDb());
}

juce::AudioProcessorEditor* NovaGlueAudioProcessor::createEditor()
{
    return new NovaGlueAudioProcessorEditor (*this);
}

void NovaGlueAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NovaGlueAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NovaGlueAudioProcessor(); }
