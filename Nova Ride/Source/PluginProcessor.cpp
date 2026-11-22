#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

namespace
{
    // juce::String's plain `const char*` constructor assumes ASCII (each
    // byte becomes its own codepoint) - a raw non-ASCII literal like "±3 dB"
    // would have its UTF-8 bytes misinterpreted that way and re-encode as
    // mojibake ("Â±3 dB") once serialized to the WebView. CharPointer_UTF8
    // tells it these bytes are UTF-8, as they actually are in this source file.
    const juce::StringArray kRangeChoices {
        juce::String (juce::CharPointer_UTF8 ("±3 dB")),
        juce::String (juce::CharPointer_UTF8 ("±6 dB")),
        juce::String (juce::CharPointer_UTF8 ("±9 dB")),
        juce::String (juce::CharPointer_UTF8 ("±12 dB"))
    };
    constexpr float kRangeDbValues[] = { 3.0f, 6.0f, 9.0f, 12.0f };
    constexpr const char* kModeChoices[] = { "Natural", "Tight", "Live" };
}

NovaRideAudioProcessor::NovaRideAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",           juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output",           juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Music Sidechain",  juce::AudioChannelSet::stereo(), false)),
#else
    :
#endif
      apvts (*this, nullptr, juce::Identifier ("NovaRide"), createParameterLayout())
{
}

NovaRideAudioProcessor::~NovaRideAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaRideAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::target, 1 },
        "Target",
        juce::NormalisableRange<float> (-24.0f, 6.0f, 0.01f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " dB"; }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::range, 1 },
        "Range",
        kRangeChoices,
        1));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::speed, 1 },
        "Speed",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::sensitivity, 1 },
        "Sensitivity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::output, 1 },
        "Output",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " dB"; }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::mode, 1 },
        "Mode",
        juce::StringArray (kModeChoices, static_cast<int> (std::size (kModeChoices))),
        0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::music, 1 }, "Music", false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::write, 1 }, "Write", false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::bypass, 1 }, "Bypass", false));

    return layout;
}

const juce::String NovaRideAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaRideAudioProcessor::acceptsMidi() const  { return false; }
bool NovaRideAudioProcessor::producesMidi() const { return false; }
bool NovaRideAudioProcessor::isMidiEffect() const { return false; }
double NovaRideAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int NovaRideAudioProcessor::getNumPrograms()              { return 1; }
int NovaRideAudioProcessor::getCurrentProgram()           { return 0; }
void NovaRideAudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String NovaRideAudioProcessor::getProgramName (int index) { juce::ignoreUnused (index); return {}; }
void NovaRideAudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName); }

void NovaRideAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

void NovaRideAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaRideAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != mainOut)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sidechain = layouts.inputBuses.getReference (1);
        if (! sidechain.isDisabled()
            && sidechain != juce::AudioChannelSet::mono()
            && sidechain != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}
#endif

nova::ride::NovaRideEngine::Params NovaRideAudioProcessor::paramsFromApvts() const
{
    nova::ride::NovaRideEngine::Params params;

    params.targetDb = apvts.getRawParameterValue (ParameterIDs::target)->load();

    const int rangeIndex = juce::jlimit (0, 3, static_cast<int> (apvts.getRawParameterValue (ParameterIDs::range)->load()));
    params.rangeDb = kRangeDbValues[rangeIndex];

    params.speedNorm = apvts.getRawParameterValue (ParameterIDs::speed)->load() / 100.0f;
    params.sensitivityNorm = apvts.getRawParameterValue (ParameterIDs::sensitivity)->load() / 100.0f;
    params.outputDb = apvts.getRawParameterValue (ParameterIDs::output)->load();
    params.modeIndex = juce::jlimit (0, 2, static_cast<int> (apvts.getRawParameterValue (ParameterIDs::mode)->load()));
    params.musicEnabled = apvts.getRawParameterValue (ParameterIDs::music)->load() > 0.5f;
    params.writeEnabled = apvts.getRawParameterValue (ParameterIDs::write)->load() > 0.5f;
    params.bypassed = apvts.getRawParameterValue (ParameterIDs::bypass)->load() > 0.5f;

    return params;
}

void NovaRideAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto mainBuffer = getBusBuffer (buffer, true, 0);
    const bool hasSidechain = getBusCount (true) > 1 && getBus (true, 1)->isEnabled();
    auto sidechainBuffer = hasSidechain ? getBusBuffer (buffer, true, 1) : juce::AudioBuffer<float> (0, buffer.getNumSamples());

    engine.setParams (paramsFromApvts());
    engine.processBlock (mainBuffer, sidechainBuffer, hasSidechain);

    // Main output isn't guaranteed to alias main input once a sidechain bus
    // exists - but NovaRideEngine::processBlock already wrote in place into
    // mainBuffer (the getBusBuffer view IS the actual host buffer memory for
    // that bus), so nothing further is needed here.

    const auto& telemetry = engine.getTelemetry();
    inputLevelDb.store (telemetry.inputLevelDb);
    outputLevelDb.store (telemetry.outputLevelDb);
    rideCorrectionDb.store (telemetry.rideCorrectionDb);
    gateOpen.store (telemetry.gateOpen);
    sidechainConnected.store (telemetry.sidechainConnected);
    sidechainPresence.store (telemetry.sidechainPresence);
}

bool NovaRideAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* NovaRideAudioProcessor::createEditor() { return new NovaRideAudioProcessorEditor (*this); }

bool NovaRideAudioProcessor::exportRideAutomation (const juce::File& destination) const
{
    return engine.getAutomationBuffer().exportToCsv (destination);
}

void NovaRideAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaRideAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaRideAudioProcessor();
}
