#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

#include <cmath>

namespace
{
    struct ModeProfile
    {
        float driveMultiplier;
        float heatMultiplier;
        float dynamicSensitivity;
        float widthAmount;
        float focusStrength;
        float topSmoothing;
        float asymmetry;
        float transientSoftening;
    };

    ModeProfile getModeProfile (int modeIndex) noexcept
    {
        switch (juce::jlimit (0, 2, modeIndex))
        {
            case 0: return { 0.70f, 0.60f, 0.20f, 0.06f, 0.08f, 0.10f, 0.05f, 0.05f };
            case 2: return { 1.25f, 1.30f, 0.60f, 0.15f, 0.20f, 0.35f, 0.20f, 0.30f };
            default: return { 1.00f, 1.00f, 0.40f, 0.10f, 0.14f, 0.20f, 0.10f, 0.15f };
        }
    }

    float getMagicIntensity (int modeIndex, bool magicOn) noexcept
    {
        if (! magicOn)
            return 0.0f;

        switch (juce::jlimit (0, 2, modeIndex))
        {
            case 0: return 0.7f;
            case 2: return 1.15f;
            default: return 1.0f;
        }
    }

    float getCurveAmount (int modeIndex, float heatNorm) noexcept
    {
        switch (juce::jlimit (0, 2, modeIndex))
        {
            case 0: return 0.8f + (heatNorm * 0.4f);
            case 2: return 1.8f + (heatNorm * 1.5f);
            default: return 1.2f + (heatNorm * 1.0f);
        }
    }

    float applySaturation (float sample, float k, const ModeProfile& mode) noexcept
    {
        const auto cubic = sample * sample * sample;
        const auto skewed = sample + (mode.asymmetry * 0.35f * cubic);

        if (sample >= 0.0f)
            return std::tanh (skewed * (k + mode.asymmetry));

        return std::tanh (skewed * juce::jmax (0.2f, k - (mode.asymmetry * 0.5f)));
    }
}

NovaHeatAudioProcessor::NovaHeatAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("NovaHeat"), createParameterLayout())
{
}

NovaHeatAudioProcessor::~NovaHeatAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaHeatAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::drive, 1 },
        "Drive",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        2.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::tone, 1 },
        "Tone",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        5.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::heat, 1 },
        "Heat",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        2.5f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::mix, 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        18.0f,
        "%",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 0) + "%"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::outputGain, 1 },
        "Output Gain",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f),
        0.0f,
        "dB",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int)
        {
            const auto sign = value >= 0.0f ? "+" : "";
            return sign + juce::String (value, 1) + " dB";
        }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::characterMode, 1 },
        "Mode",
        juce::StringArray { "Soft", "Medium", "Hard" },
        1));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::magicMode, 1 },
        "Magic",
        false));

    return layout;
}

const juce::String NovaHeatAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NovaHeatAudioProcessor::acceptsMidi() const
{
    return false;
}

bool NovaHeatAudioProcessor::producesMidi() const
{
    return false;
}

bool NovaHeatAudioProcessor::isMidiEffect() const
{
    return false;
}

double NovaHeatAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NovaHeatAudioProcessor::getNumPrograms()
{
    return 1;
}

int NovaHeatAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NovaHeatAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String NovaHeatAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void NovaHeatAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void NovaHeatAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (juce::jmax (1, getTotalNumOutputChannels()));

    toneLowShelf.prepare (spec);
    toneHighShelf.prepare (spec);
    focusPreFilter.prepare (spec);
    focusPostFilter.prepare (spec);
    topSmoothFilter.prepare (spec);
    outputTrim.prepare (spec);
    outputTrim.setRampDurationSeconds (0.03);

    toneLowShelf.reset();
    toneHighShelf.reset();
    focusPreFilter.reset();
    focusPostFilter.reset();
    topSmoothFilter.reset();
    outputTrim.reset();

    dryBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock);
    dryBuffer.clear();

    envelopeState = 0.0f;
    peakHold = 0.0f;
    outputPeakLevel.store (0.0f);
    outputIsHot.store (false);
}

void NovaHeatAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaHeatAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainOutput == layouts.getMainInputChannelSet();
}
#endif

void NovaHeatAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    const auto drive = apvts.getRawParameterValue (ParameterIDs::drive)->load();
    const auto tone = apvts.getRawParameterValue (ParameterIDs::tone)->load();
    const auto heat = apvts.getRawParameterValue (ParameterIDs::heat)->load();
    const auto mix = apvts.getRawParameterValue (ParameterIDs::mix)->load();
    const auto outputGainDb = apvts.getRawParameterValue (ParameterIDs::outputGain)->load();
    const auto modeIndex = static_cast<int> (apvts.getRawParameterValue (ParameterIDs::characterMode)->load());
    const auto magicOn = apvts.getRawParameterValue (ParameterIDs::magicMode)->load() > 0.5f;

    const auto mode = getModeProfile (modeIndex);
    const auto magicIntensity = getMagicIntensity (modeIndex, magicOn);

    const auto driveNorm = juce::jlimit (0.0f, 1.0f, drive / 10.0f);
    const auto toneNorm = juce::jlimit (0.0f, 1.0f, tone / 10.0f);
    const auto heatNorm = juce::jlimit (0.0f, 1.0f, heat / 10.0f);
    const auto mixNorm = juce::jlimit (0.0f, 1.0f, mix / 100.0f);

    const auto tiltDb = -4.0f + (toneNorm * 8.0f);
    const auto lowGainDb = -tiltDb * 0.5f;
    const auto highGainDb = tiltDb * 0.5f;

    const auto focusPreDb = 0.5f + (mode.focusStrength * 5.0f) + (magicOn ? 1.0f * magicIntensity : 0.0f);
    const auto focusPostDb = -(focusPreDb * 0.5f);
    const auto topSmoothing = mode.topSmoothing + (heatNorm * 0.18f) + (magicOn ? 0.12f * magicIntensity : 0.0f);
    const auto smoothingCutoff = juce::jlimit (3500.0f, 18000.0f, 18000.0f - (topSmoothing * 22000.0f));

    *toneLowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, 220.0f, 0.68f, juce::Decibels::decibelsToGain (lowGainDb));
    *toneHighShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 2800.0f, 0.68f, juce::Decibels::decibelsToGain (highGainDb));
    *focusPreFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 2500.0f, 0.72f, juce::Decibels::decibelsToGain (focusPreDb));
    *focusPostFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 2500.0f, 0.78f, juce::Decibels::decibelsToGain (focusPostDb));
    *topSmoothFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, smoothingCutoff, 0.75f);

    dryBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    dryBuffer.makeCopyOf (buffer, true);

    juce::dsp::AudioBlock<float> wetBlock (buffer);
    juce::dsp::ProcessContextReplacing<float> wetContext (wetBlock);

    toneLowShelf.process (wetContext);
    toneHighShelf.process (wetContext);
    focusPreFilter.process (wetContext);

    const auto dynamicSensitivity = mode.dynamicSensitivity + (magicOn ? 0.15f * magicIntensity : 0.0f);
    const auto curveBase = getCurveAmount (modeIndex, heatNorm * mode.heatMultiplier) + (magicOn ? 0.10f * magicIntensity : 0.0f);
    const auto saturationBlend = juce::jlimit (0.0f, 1.0f, 0.58f + (heatNorm * 0.18f) + mode.transientSoftening);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float magnitude = 0.0f;
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
            magnitude += std::abs (buffer.getSample (channel, sample));

        magnitude /= static_cast<float> (juce::jmax (1, totalNumInputChannels));
        envelopeState = (0.985f * envelopeState) + (0.015f * magnitude);

        const auto dynamicBoost = envelopeState * dynamicSensitivity;
        const auto driveDb = juce::jlimit (0.0f, 30.0f,
                                           (driveNorm * 24.0f * mode.driveMultiplier)
                                             + (dynamicBoost * 12.0f));
        const auto driveGain = juce::Decibels::decibelsToGain (driveDb);
        const auto kDynamic = curveBase + (magnitude * 0.45f);

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* samples = buffer.getWritePointer (channel);
            const auto driven = samples[sample] * driveGain;
            const auto saturated = applySaturation (driven, kDynamic, mode);
            samples[sample] = juce::jmap (saturationBlend, driven, saturated);
        }
    }

    topSmoothFilter.process (wetContext);
    focusPostFilter.process (wetContext);

    if (totalNumInputChannels >= 2)
    {
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        const auto widthAmount = mode.widthAmount + (heatNorm * 0.05f) + (magicOn ? 0.08f * magicIntensity : 0.0f);
        const auto sideBoost = 1.0f + widthAmount;

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto mid = 0.5f * (left[sample] + right[sample]);
            const auto side = 0.5f * (left[sample] - right[sample]) * sideBoost;
            left[sample] = mid + side;
            right[sample] = mid - side;
        }
    }

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* wet = buffer.getWritePointer (channel);
        const auto* dry = dryBuffer.getReadPointer (channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            wet[sample] = (dry[sample] * (1.0f - mixNorm)) + (wet[sample] * mixNorm);
    }

    const auto autoTrimDb = -(drive * 0.3f) + (magicOn ? -0.5f * magicIntensity : 0.0f);
    outputTrim.setGainDecibels (outputGainDb + autoTrimDb);
    outputTrim.process (wetContext);

    float peak = 0.0f;
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const auto* samples = buffer.getReadPointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            peak = juce::jmax (peak, std::abs (samples[sample]));
    }

    peakHold = peak > peakHold ? peak : peakHold * 0.92f;
    outputPeakLevel.store (juce::jlimit (0.0f, 1.0f, peakHold));
    outputIsHot.store (peak >= 0.98f);
}

bool NovaHeatAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NovaHeatAudioProcessor::createEditor()
{
    return new NovaHeatAudioProcessorEditor (*this);
}

void NovaHeatAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NovaHeatAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaHeatAudioProcessor();
}
