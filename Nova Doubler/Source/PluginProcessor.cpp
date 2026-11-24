#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

NovaDoublerProcessor::NovaDoublerProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaDoublerProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::width, "Width", 0.f, 100.f, 56.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::tightness, "Tightness", 0.f, 100.f, 45.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::mix, "Mix", 0.f, 100.f, 100.f));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::voicesFourMode, "Voices", false)); // false = 2 voices, true = 4
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::bypass, "Bypass", false));

    return layout;
}

void NovaDoublerProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaDoublerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Output must always be stereo; input may be mono OR stereo (spec
    // section 21: "mono vocal -> stereo output" is the primary intended
    // use, but stereo-in/stereo-out must also work). No existing plugin
    // in this codebase does mono-in/stereo-out - every other effect here
    // requires mainInput == mainOutput.
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::stereo())
        return false;

    const auto mainIn = layouts.getMainInputChannelSet();
    return mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo();
}
#endif

void NovaDoublerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = getMainBusNumInputChannels();

    engine.setParams (apvts.getRawParameterValue (ParameterIDs::width)->load(),
                       apvts.getRawParameterValue (ParameterIDs::tightness)->load(),
                       apvts.getRawParameterValue (ParameterIDs::mix)->load(),
                       apvts.getRawParameterValue (ParameterIDs::voicesFourMode)->load() >= 0.5f,
                       apvts.getRawParameterValue (ParameterIDs::bypass)->load() >= 0.5f);

    // Output must always be 2 channels (isBusesLayoutSupported enforces
    // this); input is 1 or 2. engine.processBlock reads dry samples before
    // writing outputs each sample, so in-place aliasing (input/output
    // sharing the same buffer, as JUCE gives us here) is safe.
    engine.processBlock (buffer.getArrayOfReadPointers(), numIn,
                          buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);

    for (int ch = 2; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto& analyzer = engine.getAnalyzer();
    telemetryLeftRms.store (analyzer.getLeftRms());
    telemetryRightRms.store (analyzer.getRightRms());
    telemetryMidRms.store (analyzer.getMidRms());
    telemetrySideRms.store (analyzer.getSideRms());
    telemetryCorrelation.store (analyzer.getCorrelation());
    telemetrySpread.store (analyzer.getSpread());
}

juce::AudioProcessorEditor* NovaDoublerProcessor::createEditor()
{
    return new NovaDoublerEditor (*this);
}

void NovaDoublerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NovaDoublerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NovaDoublerProcessor(); }
