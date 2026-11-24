#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"
#include "DSP/NovaVelvetCalibration.h"

NovaVelvetAudioProcessor::NovaVelvetAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    for (int i = 0; i < presetManager.getNumPresets(); ++i)
    {
        if (presetManager.getPreset (i).id == "vocal_smooth_control")
        {
            presetManager.applyToApvts (i, apvts);
            break;
        }
    }
}

NovaVelvetAudioProcessor::~NovaVelvetAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout
NovaVelvetAudioProcessor::createParameterLayout()
{
    using namespace nova::velvet;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::threshold, "Threshold",
        juce::NormalisableRange<float> (NovaVelvetCalibration::kThresholdMinDbu, NovaVelvetCalibration::kThresholdMaxDbu, 0.1f), -20.0f, "dBu"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::ratio, "Ratio",
        juce::NormalisableRange<float> (NovaVelvetCalibration::kRatioMin, NovaVelvetCalibration::kRatioMax, 0.01f), 4.0f, ":1"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::attack, "Attack",
        juce::NormalisableRange<float> (NovaVelvetCalibration::kManualAttackMinSeconds * 1000.0f,
                                         NovaVelvetCalibration::kManualAttackMaxSeconds * 1000.0f, 0.01f, 0.4f), 10.0f, "ms"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::release, "Release",
        juce::NormalisableRange<float> (NovaVelvetCalibration::kManualReleaseMinSeconds * 1000.0f,
                                         NovaVelvetCalibration::kManualReleaseMaxSeconds * 1000.0f, 1.0f, 0.4f), 300.0f, "ms"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::gain, "Gain",
        juce::NormalisableRange<float> (0.0f, 30.0f, 0.1f), 6.0f, "dB"));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::timeSelect, "Time Select",
        juce::StringArray { "Fixed", "Fix-Man", "Manual" }, 1));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::scHpf, "Sidechain HPF",
        juce::NormalisableRange<float> (20.0f, 400.0f, 0.1f, 0.4f), 20.0f, "Hz"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::mix, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::output, "Output", juce::NormalisableRange<float> (-10.0f, 10.0f, 0.1f), 0.0f, "dB"));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::stereoLink, "Stereo Link", true));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::analogMode, "Analog Mode",
        juce::StringArray { "Clean", "Tube", "Vintage" }, 1));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::tubeDrive, "Tube Drive", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::bypass, "Bypass", false));

    return { params.begin(), params.end() };
}

void NovaVelvetAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    engine.prepare (spec, nova::velvet::NovaVelvetCalibration::kOversamplingStagesRealtime);
    setLatencySamples (engine.getLatencySamples());

    monoScratchBuffer.setSize (2, samplesPerBlock, false, false, true);
}

bool NovaVelvetAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainIn != mainOut)
        return false;
    return mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo();
}

void NovaVelvetAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ScopedNoDenormals noDenormals;

    engine.setParams (
        apvts.getRawParameterValue (ParameterIDs::threshold)->load(),
        apvts.getRawParameterValue (ParameterIDs::ratio)->load(),
        apvts.getRawParameterValue (ParameterIDs::attack)->load(),
        apvts.getRawParameterValue (ParameterIDs::release)->load(),
        apvts.getRawParameterValue (ParameterIDs::gain)->load(),
        (int) apvts.getRawParameterValue (ParameterIDs::timeSelect)->load(),
        apvts.getRawParameterValue (ParameterIDs::scHpf)->load(),
        apvts.getRawParameterValue (ParameterIDs::mix)->load(),
        apvts.getRawParameterValue (ParameterIDs::output)->load(),
        apvts.getRawParameterValue (ParameterIDs::stereoLink)->load() >= 0.5f,
        (int) apvts.getRawParameterValue (ParameterIDs::analogMode)->load(),
        apvts.getRawParameterValue (ParameterIDs::tubeDrive)->load(),
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
    telemetryGainReductionDb.store (engine.getGainReductionDb());
}

juce::AudioProcessorEditor* NovaVelvetAudioProcessor::createEditor()
{
    return new NovaVelvetAudioProcessorEditor (*this);
}

void NovaVelvetAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("novaVelvetSelectedPresetIndex", presetManager.getCurrentIndex());
    copyXmlToBinary (*xml, destData);
}

void NovaVelvetAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xmlState));

    if (xmlState->hasAttribute ("novaVelvetSelectedPresetIndex"))
        presetManager.setSelectedIndexOnly (xmlState->getIntAttribute ("novaVelvetSelectedPresetIndex", 0));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaVelvetAudioProcessor();
}
