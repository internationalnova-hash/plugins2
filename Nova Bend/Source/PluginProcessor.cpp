#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

#include <cmath>

NovaBendAudioProcessor::NovaBendAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // Startup state is the factory "Default" preset, single-sourced from
    // the preset data rather than duplicating its values into
    // createParameterLayout() a second time. Found by id rather than
    // assuming index 0, so reordering the factory library can't silently
    // change the plugin's startup sound.
    for (int i = 0; i < presetManager.getNumPresets(); ++i)
    {
        if (presetManager.getPreset (i).id == "default")
        {
            presetManager.applyToApvts (i, apvts);
            break;
        }
    }
}

NovaBendAudioProcessor::~NovaBendAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout
NovaBendAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::pitch, "Pitch",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, "st"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::formant, "Formant",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, "st"));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::flatten, "Flatten", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::flattenNote, "Flatten Note",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::flattenOctave, "Flatten Octave",
        juce::StringArray { "1", "2", "3", "4", "5", "6" }, 2)); // default octave 3
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::link, "Link", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::mix, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::quality, "Quality", juce::StringArray { "Live", "Studio" }, 1)); // default Studio
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::bypass, "Bypass", false));

    return { params.begin(), params.end() };
}

void NovaBendAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    lastAppliedHighQuality = (int) apvts.getRawParameterValue (ParameterIDs::quality)->load() >= 1;

    engine.prepare (sampleRate, samplesPerBlock, lastAppliedHighQuality);
    setLatencySamples (engine.getLatencySamples());

    startTimer (250);
}

void NovaBendAudioProcessor::releaseResources()
{
    stopTimer();
}

void NovaBendAudioProcessor::timerCallback()
{
    // Message-thread-only reconfigure for Quality (LIVE/STUDIO) changes -
    // resizes the detector's analysis window AND the shifter's overlap
    // buffers (both reallocate) - mirrors Nova Pitch's own proven
    // lowLatency-reconfigure pattern.
    const bool desired = (int) apvts.getRawParameterValue (ParameterIDs::quality)->load() >= 1;
    if (desired != lastAppliedHighQuality)
    {
        suspendProcessing (true);  // reallocates - keep the audio thread out
        engine.reconfigureIfNeeded (currentSampleRate, desired);
        suspendProcessing (false);
        lastAppliedHighQuality = desired;
        setLatencySamples (engine.getLatencySamples());
    }
}

void NovaBendAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    const float* chL = buffer.getReadPointer (0);
    const float* chR = (numChannels > 1) ? buffer.getReadPointer (1) : chL;

    NovaBendTunedEngine::Params p;
    p.pitchSemitones = apvts.getRawParameterValue (ParameterIDs::pitch)->load();
    p.formantSemitones = apvts.getRawParameterValue (ParameterIDs::formant)->load();
    p.flatten = apvts.getRawParameterValue (ParameterIDs::flatten)->load() >= 0.5f;
    p.flattenNoteIndex = (int) apvts.getRawParameterValue (ParameterIDs::flattenNote)->load();
    p.flattenOctave = 1 + (int) apvts.getRawParameterValue (ParameterIDs::flattenOctave)->load();
    p.link = apvts.getRawParameterValue (ParameterIDs::link)->load() >= 0.5f;
    p.mixPercent = apvts.getRawParameterValue (ParameterIDs::mix)->load();
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
        // Mono bus: engine always runs stereo internally with identical L/R
        // input (chR==chL above), so both outputs are deterministically
        // identical - write both into the same mono buffer, R after L.
        engine.process (chL, chR, outL, outL, numSamples);
    }

    detectedHz.store (engine.getDetectedHz());
    effectivePitchSemitonesDebug.store (engine.getEffectivePitchSemitonesDebug());

    float sumSq = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        sumSq += outL[i] * outL[i];
    const float blockRms = numSamples > 0 ? std::sqrt (sumSq / (float) numSamples) : 0.0f;
    const int writeIdx = levelHistoryWrite.load();
    levelHistory[(size_t) writeIdx].store (blockRms);
    levelHistoryWrite.store ((writeIdx + 1) % kLevelHistorySize);
}

float NovaBendAudioProcessor::getLevelHistoryAt (int indexFromOldest) const noexcept
{
    const int oldest = levelHistoryWrite.load();
    const int idx = (oldest + juce::jlimit (0, kLevelHistorySize - 1, indexFromOldest)) % kLevelHistorySize;
    return levelHistory[(size_t) idx].load();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaBendAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}
#endif

juce::AudioProcessorEditor* NovaBendAudioProcessor::createEditor()
{
    return new NovaBendAudioProcessorEditor (*this);
}

bool NovaBendAudioProcessor::hasEditor() const { return true; }
const juce::String NovaBendAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaBendAudioProcessor::acceptsMidi()  const { return false; }
bool NovaBendAudioProcessor::producesMidi() const { return false; }
bool NovaBendAudioProcessor::isMidiEffect() const { return false; }
double NovaBendAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int NovaBendAudioProcessor::getNumPrograms()    { return 1; }
int NovaBendAudioProcessor::getCurrentProgram() { return 0; }
void NovaBendAudioProcessor::setCurrentProgram (int) {}
const juce::String NovaBendAudioProcessor::getProgramName (int) { return {}; }
void NovaBendAudioProcessor::changeProgramName (int, const juce::String&) {}

void NovaBendAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    // Selected preset index, so the UI can show the correct preset
    // name/highlight after reload. The underlying param values are already
    // saved by apvts.copyState() above.
    xml->setAttribute ("novaBendSelectedPresetIndex", presetManager.getCurrentIndex());
    copyXmlToBinary (*xml, destData);
}

void NovaBendAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xmlState));

    // Restore the SELECTION only (which preset is highlighted) - do NOT
    // re-apply its values, since apvts.replaceState above already restored
    // the user's actual (possibly since-tweaked) parameter values;
    // re-applying would discard edits made after loading the preset.
    if (xmlState->hasAttribute ("novaBendSelectedPresetIndex"))
        presetManager.setSelectedIndexOnly (xmlState->getIntAttribute ("novaBendSelectedPresetIndex", 0));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaBendAudioProcessor();
}
