#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

#include <cmath>

namespace
{
    // Circuit modes: Neon (bright, odd-harmonic), Heat (warm, even-harmonic via
    // asymmetry), Nova (symmetric hard-kneed clip, "edge"), Galaxy
    // (harmonic-multiplying wavefolder, extreme/metallic).
    enum CircuitMode { Neon = 0, Heat = 1, Nova = 2, Galaxy = 3 };

    struct ModeProfile
    {
        float baseK;         // starting harmonic intensity ("how hot the circuit runs")
        float envelopeBoost; // how much the input envelope adds on top of baseK
    };

    ModeProfile getModeProfile (int modeIndex) noexcept
    {
        switch (juce::jlimit (0, 3, modeIndex))
        {
            case Heat:   return { 2.4f, 1.4f };
            case Nova:   return { 2.8f, 1.6f };
            case Galaxy: return { 2.0f, 3.0f };
            default:     return { 3.2f, 1.8f }; // Neon
        }
    }

    float applySaturation (float x, float k, int modeIndex) noexcept
    {
        switch (juce::jlimit (0, 3, modeIndex))
        {
            case Heat:
            {
                // Asymmetric tanh: softer on the negative half generates even
                // harmonics, read as warmer/fuller than a symmetric curve.
                if (x >= 0.0f)
                    return std::tanh (k * x);
                return std::tanh (k * 0.65f * x);
            }

            case Nova:
            {
                // Classic cubic soft-clip (x - x^3/3), normalised so |x|=1 hits
                // the ceiling. Harder-kneed than tanh - symmetric, odd-harmonic,
                // but a different, edgier timbre.
                const auto driven = juce::jlimit (-1.0f, 1.0f, k * x);
                const auto shaped = driven - (driven * driven * driven) / 3.0f;
                return shaped / (2.0f / 3.0f);
            }

            case Galaxy:
            {
                // Harmonic-multiplying wavefolder: sin(k * asin(x)) for x in
                // [-1,1]. Always bounded regardless of k (asin/sin domains are
                // safe), and produces dense, inharmonic-leaning overtones as k
                // grows - the "extreme, metallic" character.
                const auto clamped = juce::jlimit (-1.0f, 1.0f, x);
                return std::sin (k * std::asin (clamped));
            }

            default: // Neon
                // Clean symmetric tanh, high drive - odd-harmonic-dominant,
                // bright and aggressive.
                return std::tanh (k * x);
        }
    }
}

NovaSubAudioProcessor::NovaSubAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("NovaSub"), createParameterLayout())
{
}

NovaSubAudioProcessor::~NovaSubAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaSubAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::color, 1 },
        "Color",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        5.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::filterKnob, 1 },
        "Filter",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        5.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::mix, 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        40.0f,
        "%",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 0) + "%"; }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::circuitMode, 1 },
        "Mode",
        juce::StringArray { "Neon", "Heat", "Nova", "Galaxy" },
        0));

    return layout;
}

const juce::String NovaSubAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NovaSubAudioProcessor::acceptsMidi() const
{
    return false;
}

bool NovaSubAudioProcessor::producesMidi() const
{
    return false;
}

bool NovaSubAudioProcessor::isMidiEffect() const
{
    return false;
}

double NovaSubAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NovaSubAudioProcessor::getNumPrograms()
{
    return 1;
}

int NovaSubAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NovaSubAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String NovaSubAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void NovaSubAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void NovaSubAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (juce::jmax (1, getTotalNumOutputChannels()));

    colorLowShelf.prepare (spec);
    colorHighShelf.prepare (spec);
    sweepFilter.prepare (spec);

    colorLowShelf.reset();
    colorHighShelf.reset();
    sweepFilter.reset();

    dryBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock);
    dryBuffer.clear();

    envelopeState = 0.0f;
    peakHold = 0.0f;
    inputPeakHold = 0.0f;
    inputPeakLevel.store (0.0f);
    outputPeakLevel.store (0.0f);
    outputIsHot.store (false);
}

void NovaSubAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaSubAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainOutput == layouts.getMainInputChannelSet();
}
#endif

void NovaSubAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    const auto color = apvts.getRawParameterValue (ParameterIDs::color)->load();
    const auto filterKnob = apvts.getRawParameterValue (ParameterIDs::filterKnob)->load();
    const auto mix = apvts.getRawParameterValue (ParameterIDs::mix)->load();
    const auto modeIndex = static_cast<int> (apvts.getRawParameterValue (ParameterIDs::circuitMode)->load());

    const auto mode = getModeProfile (modeIndex);
    const auto mixNorm = juce::jlimit (0.0f, 1.0f, mix / 100.0f);

    // ── Color: pre-saturation tilt EQ (dark <-> bright) ──────────────────
    const auto colorNorm = juce::jlimit (0.0f, 1.0f, color / 10.0f);
    const auto tiltDb = -6.0f + (colorNorm * 12.0f);
    const auto lowGainDb = -tiltDb * 0.5f;
    const auto highGainDb = tiltDb * 0.5f;

    *colorLowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        currentSampleRate, 200.0f, 0.7f, juce::Decibels::decibelsToGain (lowGainDb));
    *colorHighShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        currentSampleRate, 3000.0f, 0.7f, juce::Decibels::decibelsToGain (highGainDb));

    // ── Filter: single-knob sweep, center = open ─────────────────────────
    // Below center: rising-cutoff high-pass (thins the low end).
    // Above center: descending-cutoff low-pass (tames harshness).
    const auto filterNorm = juce::jlimit (0.0f, 1.0f, filterKnob / 10.0f);
    if (filterNorm <= 0.5f)
    {
        const auto t = (0.5f - filterNorm) / 0.5f;
        const auto hpCutoff = juce::jmap (t, 0.0f, 1.0f, 20.0f, 1000.0f);
        *sweepFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, hpCutoff, 0.707f);
    }
    else
    {
        const auto t = (filterNorm - 0.5f) / 0.5f;
        const auto lpCutoff = juce::jmap (t, 0.0f, 1.0f, 20000.0f, 900.0f);
        *sweepFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, lpCutoff, 0.707f);
    }

    dryBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    dryBuffer.makeCopyOf (buffer, true);

    float inputPeak = 0.0f;
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const auto* samples = dryBuffer.getReadPointer (channel);
        for (int sample = 0; sample < dryBuffer.getNumSamples(); ++sample)
            inputPeak = juce::jmax (inputPeak, std::abs (samples[sample]));
    }
    inputPeakHold = inputPeak > inputPeakHold ? inputPeak : inputPeakHold * 0.92f;
    inputPeakLevel.store (juce::jlimit (0.0f, 1.0f, inputPeakHold));

    juce::dsp::AudioBlock<float> wetBlock (buffer);
    juce::dsp::ProcessContextReplacing<float> wetContext (wetBlock);

    colorLowShelf.process (wetContext);
    colorHighShelf.process (wetContext);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float magnitude = 0.0f;
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
            magnitude += std::abs (buffer.getSample (channel, sample));
        magnitude /= static_cast<float> (juce::jmax (1, totalNumInputChannels));

        envelopeState = (0.985f * envelopeState) + (0.015f * magnitude);
        const auto k = mode.baseK + (envelopeState * mode.envelopeBoost);

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* samples = buffer.getWritePointer (channel);
            samples[sample] = applySaturation (samples[sample], k, modeIndex);
        }
    }

    sweepFilter.process (wetContext);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* wet = buffer.getWritePointer (channel);
        const auto* dry = dryBuffer.getReadPointer (channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto blended = (dry[sample] * (1.0f - mixNorm)) + (wet[sample] * mixNorm);
            // Safety soft-ceiling: transparent near zero, smoothly approaches
            // +/-ceiling for any input, so no DSP path here can produce an
            // uncontrolled hard clip or runaway level regardless of mode/knob
            // extremes.
            constexpr float ceiling = 0.98f;
            wet[sample] = ceiling * std::tanh (blended / ceiling);
        }
    }

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

bool NovaSubAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NovaSubAudioProcessor::createEditor()
{
    return new NovaSubAudioProcessorEditor (*this);
}

void NovaSubAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NovaSubAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaSubAudioProcessor();
}
