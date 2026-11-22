#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr auto spaceId    = "space";
    constexpr auto airId      = "air";
    constexpr auto depthId    = "depth";
    constexpr auto mixId      = "mix";
    constexpr auto widthId    = "width";
    constexpr auto modeId     = "nova_mode";
    constexpr auto preDelayId = "pre_delay_ms";
    constexpr auto decayId    = "decay";
    constexpr auto dampingId  = "damping";
    constexpr auto earlyId    = "early_reflections";

    float percentText (float value, float maxValue)
    {
        return juce::jlimit (0.0f, 100.0f, (value / maxValue) * 100.0f);
    }
}

SpaceByNovaAudioProcessor::SpaceByNovaAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts (*this, nullptr, juce::Identifier ("SpaceByNova"), createParameterLayout())
{
}

SpaceByNovaAudioProcessor::~SpaceByNovaAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout SpaceByNovaAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { spaceId, 1 }, "Space",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 1.8f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (percentText (v, 10.0f))) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { airId, 1 }, "Air",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 3.2f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (percentText (v, 10.0f))) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { depthId, 1 }, "Depth",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 2.6f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (percentText (v, 10.0f))) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 16.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { widthId, 1 }, "Width",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 3.8f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (percentText (v, 10.0f))) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { modeId, 1 }, "Nova Mode",
        juce::StringArray { "Studio", "Arena", "Dream", "Vintage" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { preDelayId, 1 }, "Pre-Delay",
        juce::NormalisableRange<float> (0.0f, 120.0f, 0.1f), 22.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 1) + " ms"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { decayId, 1 }, "Decay",
        juce::NormalisableRange<float> (0.5f, 6.5f, 0.01f), 2.2f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 2) + " s"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { dampingId, 1 }, "Damping",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 45.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { earlyId, 1 }, "Early Reflections",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }));

    return layout;
}

const juce::String SpaceByNovaAudioProcessor::getName() const { return JucePlugin_Name; }

bool SpaceByNovaAudioProcessor::acceptsMidi()  const { return false; }
bool SpaceByNovaAudioProcessor::producesMidi() const { return false; }
bool SpaceByNovaAudioProcessor::isMidiEffect() const { return false; }

double SpaceByNovaAudioProcessor::getTailLengthSeconds() const { return 8.0; }

int SpaceByNovaAudioProcessor::getNumPrograms()               { return 1; }
int SpaceByNovaAudioProcessor::getCurrentProgram()            { return 0; }
void SpaceByNovaAudioProcessor::setCurrentProgram (int i)     { juce::ignoreUnused (i); }
const juce::String SpaceByNovaAudioProcessor::getProgramName (int i) { juce::ignoreUnused (i); return {}; }
void SpaceByNovaAudioProcessor::changeProgramName (int i, const juce::String& n) { juce::ignoreUnused (i, n); }

void SpaceByNovaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Read current APVTS values to seed smoothers — mirrors original prepareToPlay() behaviour
    NovaSpaceParameters init;
    init.space      = apvts.getRawParameterValue (spaceId)->load();
    init.air        = apvts.getRawParameterValue (airId)->load();
    init.depth      = apvts.getRawParameterValue (depthId)->load();
    init.mix        = apvts.getRawParameterValue (mixId)->load();
    init.width      = apvts.getRawParameterValue (widthId)->load();
    init.mode       = juce::roundToInt (apvts.getRawParameterValue (modeId)->load());
    init.preDelayMs = apvts.getRawParameterValue (preDelayId)->load();
    init.decay      = apvts.getRawParameterValue (decayId)->load();
    init.damping    = apvts.getRawParameterValue (dampingId)->load();
    init.early      = apvts.getRawParameterValue (earlyId)->load();

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)samplesPerBlock, 2 };
    spaceDSP.prepare (spec, init);
}

void SpaceByNovaAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SpaceByNovaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return mainIn == mainOut;
}
#endif

void SpaceByNovaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    // Translate APVTS → plain parameter struct (no DSP logic here)
    NovaSpaceParameters params;
    params.space      = apvts.getRawParameterValue (spaceId)->load();
    params.air        = apvts.getRawParameterValue (airId)->load();
    params.depth      = apvts.getRawParameterValue (depthId)->load();
    params.mix        = apvts.getRawParameterValue (mixId)->load();
    params.width      = apvts.getRawParameterValue (widthId)->load();
    params.mode       = juce::roundToInt (apvts.getRawParameterValue (modeId)->load());
    params.preDelayMs = apvts.getRawParameterValue (preDelayId)->load();
    params.decay      = apvts.getRawParameterValue (decayId)->load();
    params.damping    = apvts.getRawParameterValue (dampingId)->load();
    params.early      = apvts.getRawParameterValue (earlyId)->load();

    spaceDSP.setParameters (params);
    spaceDSP.process (buffer);
}

bool SpaceByNovaAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* SpaceByNovaAudioProcessor::createEditor()
{
    return new SpaceByNovaAudioProcessorEditor (*this);
}

void SpaceByNovaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SpaceByNovaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpaceByNovaAudioProcessor();
}
