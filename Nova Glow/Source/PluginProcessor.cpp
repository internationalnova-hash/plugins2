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

NovaGlowAudioProcessor::NovaGlowAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    presetManager.scanUserPresets();
    startTimerHz ((int) kOversamplingReconfigureHz);
}

NovaGlowAudioProcessor::~NovaGlowAudioProcessor()
{
    stopTimer();
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaGlowAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::peakReduction, 1 }, "Peak Reduction",
        juce::NormalisableRange<float> (0.0f, 100.0f), 30.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::gain, 1 }, "Gain",
        juce::NormalisableRange<float> (0.0f, 100.0f), 50.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::compressLimit, 1 }, "Compress/Limit",
        juce::StringArray { "Compress", "Limit" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::scHpf, 1 }, "SC HPF",
        juce::NormalisableRange<float> (20.0f, 400.0f, 0.0f, 0.4f), 30.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::emphasis, 1 }, "Emphasis",
        juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::glow, 1 }, "Glow",
        juce::NormalisableRange<float> (0.0f, 100.0f), 45.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::analogMode, 1 }, "Analog Mode",
        juce::StringArray { "Clean", "Vintage", "Hot" }, 1));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::mix, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::stereoLink, 1 }, "Stereo Link",
        juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::noise, 1 }, "Noise",
        juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::oversampling, 1 }, "Oversampling",
        juce::StringArray { "Off", "2x", "4x" }, kDefaultOversamplingChoice));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::meterMode, 1 }, "Meter",
        juce::StringArray { "GR", "Output", "Input" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::bypass, 1 }, "Bypass", false));

    return layout;
}

void NovaGlowAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    const int choiceIndex = (int) apvts.getRawParameterValue (ParameterIDs::oversampling)->load();
    lastPreparedOversamplingStages = oversamplingChoiceToStages (choiceIndex);

    reprepareEngine();
}

void NovaGlowAudioProcessor::reprepareEngine()
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = (juce::uint32) currentBlockSize;
    spec.numChannels = 2;

    engine.prepare (spec, lastPreparedOversamplingStages);
    setLatencySamples (engine.getLatencySamples());

    monoScratchBuffer.setSize (2, currentBlockSize, false, false, true);
}

void NovaGlowAudioProcessor::reconfigureOversamplingIfNeeded()
{
    const int choiceIndex = (int) apvts.getRawParameterValue (ParameterIDs::oversampling)->load();
    const int requestedStages = oversamplingChoiceToStages (choiceIndex);
    if (requestedStages == lastPreparedOversamplingStages)
        return;

    // Message-thread only (this is a juce::Timer callback) - suspendProcessing()
    // blocks the audio thread from calling processBlock() concurrently
    // while the engine's Oversampling objects are reallocated, so there is
    // no real-time-thread allocation and no race. See PluginProcessor.h's
    // comment on why this is 7even's pattern, not Royal 670's.
    suspendProcessing (true);
    lastPreparedOversamplingStages = requestedStages;
    reprepareEngine();
    suspendProcessing (false);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaGlowAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void NovaGlowAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    engine.setParams (
        apvts.getRawParameterValue (ParameterIDs::peakReduction)->load(),
        apvts.getRawParameterValue (ParameterIDs::gain)->load(),
        (int) apvts.getRawParameterValue (ParameterIDs::compressLimit)->load(),
        apvts.getRawParameterValue (ParameterIDs::scHpf)->load(),
        apvts.getRawParameterValue (ParameterIDs::emphasis)->load(),
        apvts.getRawParameterValue (ParameterIDs::glow)->load(),
        (int) apvts.getRawParameterValue (ParameterIDs::analogMode)->load(),
        apvts.getRawParameterValue (ParameterIDs::mix)->load(),
        apvts.getRawParameterValue (ParameterIDs::stereoLink)->load(),
        apvts.getRawParameterValue (ParameterIDs::noise)->load(),
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
    telemetryOutputVu.store (engine.getOutputDisplayVu());
    telemetryInputVu.store (engine.getInputDisplayVu());
}

juce::AudioProcessorEditor* NovaGlowAudioProcessor::createEditor()
{
    return new NovaGlowAudioProcessorEditor (*this);
}

void NovaGlowAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NovaGlowAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NovaGlowAudioProcessor(); }
