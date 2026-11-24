#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

namespace
{
    constexpr int kDefaultOversamplingChoice = 2; // index into {Off,2x,4x,8x} -> 4x, matching the mockup

    int oversamplingChoiceToStages (int choiceIndex) noexcept
    {
        switch (choiceIndex)
        {
            case 0: return 0; // Off
            case 1: return 1; // 2x
            case 2: return 2; // 4x
            case 3: return 3; // 8x
            default: return 2;
        }
    }
}

NovaRoyal670Processor::NovaRoyal670Processor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaRoyal670Processor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::input, 1 }, "Input",
        juce::NormalisableRange<float> (-20.0f, 20.0f), 3.2f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::hrmDrive, 1 }, "HRM Drive",
        juce::NormalisableRange<float> (0.0f, 100.0f), 35.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::acThreshold, 1 }, "AC Threshold",
        juce::NormalisableRange<float> (-20.0f, 10.0f), -6.8f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::dcThreshold, 1 }, "DC Threshold",
        juce::NormalisableRange<float> (-20.0f, 20.0f), 2.1f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::timeConstant, 1 }, "Time Constant",
        juce::StringArray { "0.1 sec", "0.2 sec", "0.6 sec", "1.2 sec", "2.0 sec", "4.0 sec" }, 3));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::output, 1 }, "Output",
        juce::NormalisableRange<float> (-20.0f, 20.0f), 1.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::mix, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::stereoMode, 1 }, "Stereo Mode",
        juce::StringArray { "Stereo", "M/S" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::link, 1 }, "Link",
        juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::sidechainHpf, 1 }, "Sidechain HPF",
        juce::NormalisableRange<float> (20.0f, 2000.0f, 0.0f, 0.35f), 80.0f)); // skewed - musical spacing across the mockup's log-ish tick marks
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::sidechainListen, 1 }, "Sidechain Listen", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::weight, 1 }, "Weight",
        juce::NormalisableRange<float> (0.0f, 100.0f), 50.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::tubeSaturation, 1 }, "Tube Saturation",
        juce::NormalisableRange<float> (0.0f, 100.0f), 35.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::transformer, 1 }, "Transformer",
        juce::NormalisableRange<float> (0.0f, 100.0f), 40.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::outputTrim, 1 }, "Output Trim",
        juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::autoGain, 1 }, "Auto Gain", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::oversampling, 1 }, "Oversampling",
        juce::StringArray { "Off", "2x", "4x", "8x" }, kDefaultOversamplingChoice));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::analogNoise, 1 }, "Analog Noise",
        juce::NormalisableRange<float> (0.0f, 100.0f), 15.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::bypass, 1 }, "Bypass", false));

    return layout;
}

void NovaRoyal670Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    const int choiceIndex = (int) apvts.getRawParameterValue (ParameterIDs::oversampling)->load();
    lastPreparedOversamplingStages = oversamplingChoiceToStages (choiceIndex);

    reprepareEngine();
}

void NovaRoyal670Processor::reprepareEngine()
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = (juce::uint32) currentBlockSize;
    spec.numChannels = 2;

    engine.prepare (spec, lastPreparedOversamplingStages);
    setLatencySamples (engine.getLatencySamples());

    monoScratchBuffer.setSize (2, currentBlockSize, false, false, true);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaRoyal670Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // The Core engine is stereo-only (both Stereo and M/S processing need
    // a channel pair - see NovaRoyal670Engine's header). Mono is adapted
    // in processBlock by duplicating into a scratch stereo pair and
    // averaging back down, so mono-in/mono-out is supported at the bus
    // level even though nothing downstream of this adaptation is mono-
    // aware. Mixed mono-in/stereo-out (or vice versa) is not supported.
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainIn != mainOut)
        return false;
    return mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo();
}
#endif

void NovaRoyal670Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ScopedNoDenormals noDenormals;

    // Oversampling changes reallocate the engine's internal Oversampling
    // objects - never done inside the per-sample DSP path, only here at
    // the top of processBlock when the choice has actually changed (a
    // rare user action, not a per-block cost in the steady state).
    const int choiceIndex = (int) apvts.getRawParameterValue (ParameterIDs::oversampling)->load();
    const int requestedStages = oversamplingChoiceToStages (choiceIndex);
    if (requestedStages != lastPreparedOversamplingStages)
    {
        lastPreparedOversamplingStages = requestedStages;
        reprepareEngine();
    }

    engine.setParams (
        apvts.getRawParameterValue (ParameterIDs::input)->load(),
        apvts.getRawParameterValue (ParameterIDs::hrmDrive)->load(),
        apvts.getRawParameterValue (ParameterIDs::acThreshold)->load(),
        apvts.getRawParameterValue (ParameterIDs::dcThreshold)->load(),
        1 + (int) apvts.getRawParameterValue (ParameterIDs::timeConstant)->load(),
        apvts.getRawParameterValue (ParameterIDs::output)->load(),
        apvts.getRawParameterValue (ParameterIDs::mix)->load(),
        ((int) apvts.getRawParameterValue (ParameterIDs::stereoMode)->load()) == 1,
        apvts.getRawParameterValue (ParameterIDs::link)->load(),
        apvts.getRawParameterValue (ParameterIDs::sidechainHpf)->load(),
        apvts.getRawParameterValue (ParameterIDs::sidechainListen)->load() >= 0.5f,
        apvts.getRawParameterValue (ParameterIDs::weight)->load(),
        apvts.getRawParameterValue (ParameterIDs::tubeSaturation)->load(),
        apvts.getRawParameterValue (ParameterIDs::transformer)->load(),
        apvts.getRawParameterValue (ParameterIDs::outputTrim)->load(),
        apvts.getRawParameterValue (ParameterIDs::autoGain)->load() >= 0.5f,
        apvts.getRawParameterValue (ParameterIDs::analogNoise)->load(),
        apvts.getRawParameterValue (ParameterIDs::bypass)->load() >= 0.5f);

    const int numSamples = buffer.getNumSamples();
    const bool isMonoBus = getMainBusNumInputChannels() == 1 && getMainBusNumOutputChannels() == 1;

    if (isMonoBus)
    {
        monoScratchBuffer.setSize (2, numSamples, false, false, true);
        monoScratchBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
        monoScratchBuffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

        engine.processBlock (monoScratchBuffer);

        // Downmix: average L/R back to the single mono channel.
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (0, i, 0.5f * (monoScratchBuffer.getSample (0, i) + monoScratchBuffer.getSample (1, i)));
    }
    else
    {
        engine.processBlock (buffer);
    }

    for (int ch = getMainBusNumOutputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    telemetryVuLeftOrMid.store (engine.getVuLeftOrMidDisplayVu());
    telemetryVuRightOrSide.store (engine.getVuRightOrSideDisplayVu());
    telemetryGainReductionDb.store (engine.getGainReductionDb());
}

juce::AudioProcessorEditor* NovaRoyal670Processor::createEditor()
{
    return new NovaRoyal670Editor (*this);
}

void NovaRoyal670Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NovaRoyal670Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NovaRoyal670Processor(); }
