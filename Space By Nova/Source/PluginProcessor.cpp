#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    constexpr auto spaceId = "space";
    constexpr auto airId = "air";
    constexpr auto depthId = "depth";
    constexpr auto mixId = "mix";
    constexpr auto widthId = "width";
    constexpr auto modeId = "nova_mode";
    constexpr auto preDelayId = "pre_delay_ms";
    constexpr auto decayId = "decay";
    constexpr auto dampingId = "damping";
    constexpr auto earlyId = "early_reflections";

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
        juce::ParameterID { spaceId, 1 },
        "Space",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        1.8f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (percentText (value, 10.0f))) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { airId, 1 },
        "Air",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        3.2f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (percentText (value, 10.0f))) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { depthId, 1 },
        "Depth",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        2.6f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (percentText (value, 10.0f))) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        16.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { widthId, 1 },
        "Width",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        3.8f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (percentText (value, 10.0f))) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { modeId, 1 },
        "Nova Mode",
        juce::StringArray { "Studio", "Arena", "Dream", "Vintage" },
        0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { preDelayId, 1 },
        "Pre-Delay",
        juce::NormalisableRange<float> (0.0f, 120.0f, 0.1f),
        28.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " ms"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { decayId, 1 },
        "Decay",
        juce::NormalisableRange<float> (0.5f, 6.5f, 0.01f),
        2.2f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 2) + " s"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { dampingId, 1 },
        "Damping",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        45.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { earlyId, 1 },
        "Early Reflections",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        35.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    return layout;
}

const juce::String SpaceByNovaAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SpaceByNovaAudioProcessor::acceptsMidi() const  { return false; }
bool SpaceByNovaAudioProcessor::producesMidi() const { return false; }
bool SpaceByNovaAudioProcessor::isMidiEffect() const { return false; }

double SpaceByNovaAudioProcessor::getTailLengthSeconds() const
{
    return 8.0;
}

int SpaceByNovaAudioProcessor::getNumPrograms()                   { return 1; }
int SpaceByNovaAudioProcessor::getCurrentProgram()               { return 0; }
void SpaceByNovaAudioProcessor::setCurrentProgram (int index)    { juce::ignoreUnused (index); }
const juce::String SpaceByNovaAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}
void SpaceByNovaAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void SpaceByNovaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 1 };
    preDelayLeft.prepare (spec);
    preDelayRight.prepare (spec);
    decorrelationDelay.prepare (spec);
    preDelayLeft.reset();
    preDelayRight.reset();
    decorrelationDelay.reset();

    wetToneLeft.prepare (spec);
    wetToneRight.prepare (spec);
    wetBodyLeft.prepare (spec);
    wetBodyRight.prepare (spec);
    wetToneLeft.reset();
    wetToneRight.reset();
    wetBodyLeft.reset();
    wetBodyRight.reset();
    wetToneLeft.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    wetToneRight.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    wetBodyLeft.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    wetBodyRight.setType (juce::dsp::StateVariableTPTFilterType::highpass);

    dryBuffer.setSize (2, samplesPerBlock);
    wetBuffer.setSize (2, samplesPerBlock);

    for (auto* smoother : { &smoothedSpace, &smoothedAir, &smoothedDepth, &smoothedMix, &smoothedWidth })
        smoother->reset (sampleRate, 0.20);

    smoothedSpace.setCurrentAndTargetValue (apvts.getRawParameterValue (spaceId)->load());
    smoothedAir.setCurrentAndTargetValue (apvts.getRawParameterValue (airId)->load());
    smoothedDepth.setCurrentAndTargetValue (apvts.getRawParameterValue (depthId)->load());
    smoothedMix.setCurrentAndTargetValue (apvts.getRawParameterValue (mixId)->load());
    smoothedWidth.setCurrentAndTargetValue (apvts.getRawParameterValue (widthId)->load());

    reverb.reset();
    motionPhase = 0.0f;
    haloPhase = 1.7f;
}

void SpaceByNovaAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SpaceByNovaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return mainIn == mainOut;
}
#endif

float SpaceByNovaAudioProcessor::clamp01 (float value) noexcept
{
    return juce::jlimit (0.0f, 1.0f, value);
}

void SpaceByNovaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    dryBuffer.setSize (juce::jmax (1, numChannels), numSamples, false, false, true);
    wetBuffer.setSize (juce::jmax (1, numChannels), numSamples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);
    wetBuffer.makeCopyOf (buffer, true);

    smoothedSpace.setTargetValue (apvts.getRawParameterValue (spaceId)->load());
    smoothedAir.setTargetValue (apvts.getRawParameterValue (airId)->load());
    smoothedDepth.setTargetValue (apvts.getRawParameterValue (depthId)->load());
    smoothedMix.setTargetValue (apvts.getRawParameterValue (mixId)->load());
    smoothedWidth.setTargetValue (apvts.getRawParameterValue (widthId)->load());

    const float space = smoothedSpace.skip (numSamples);
    const float air = smoothedAir.skip (numSamples);
    const float depth = smoothedDepth.skip (numSamples);
    const float mix = smoothedMix.skip (numSamples);
    const float width = smoothedWidth.skip (numSamples);

    const float preDelayMsBase = apvts.getRawParameterValue (preDelayId)->load();
    const float decaySeconds = apvts.getRawParameterValue (decayId)->load();
    const float dampingControl = apvts.getRawParameterValue (dampingId)->load() / 100.0f;
    const float earlyControl = apvts.getRawParameterValue (earlyId)->load() / 100.0f;
    const int modeIndex = juce::roundToInt (apvts.getRawParameterValue (modeId)->load());

    const float spaceNorm = clamp01 (space / 10.0f);
    const float airNorm = clamp01 (air / 10.0f);
    const float depthNorm = clamp01 (depth / 10.0f);
    const float mixNorm = clamp01 (mix / 100.0f);
    const float widthNorm = clamp01 (width / 10.0f);
    const float decayNorm = clamp01 ((decaySeconds - 0.5f) / 6.0f);
    const float airCurve = 1.0f - ((1.0f - airNorm) * (1.0f - airNorm));
    const float airSafety = airNorm * airNorm;
    const float bloomDelayMs = juce::jmap (spaceNorm, 0.0f, 20.0f);
    const float attackSoftness = juce::jlimit (0.0f, 0.35f,
                                               0.04f + (0.16f * depthNorm) + (0.10f * spaceNorm));

    float roomSize = clamp01 (0.21f + (0.43f * spaceNorm) + (0.22f * decayNorm));
    float damping = juce::jlimit (0.16f, 0.95f,
                                  (0.76f - (0.26f * airCurve))
                                  + (0.12f * spaceNorm)
                                  + (0.18f * dampingControl)
                                  + (0.08f * airSafety));

    float preDelayMs = juce::jlimit (0.0f, 180.0f,
                                     preDelayMsBase
                                     + juce::jmap (depthNorm, 8.0f, 56.0f)
                                     + bloomDelayMs);

    float earlyAmount = juce::jlimit (0.03f, 0.66f,
                                      (0.15f + (0.26f * earlyControl))
                                      - (0.18f * depthNorm)
                                      - (0.07f * spaceNorm));

    float wetTrim = juce::jlimit (0.65f, 1.08f,
                                  juce::jmap (mixNorm, 0.0f, 1.0f, 1.0f, 0.93f)
                                  + (0.06f * decayNorm)
                                  + (0.02f * spaceNorm));

    float sideGain = juce::jmap (widthNorm, 0.72f, 1.78f);
    float decorrelationMs = juce::jmap (widthNorm, 0.0f, 12.0f);
    float toneCutoffHz = juce::jlimit (3200.0f, 16000.0f,
                                       4600.0f + (airCurve * 9000.0f) - (spaceNorm * 1600.0f));
    float lowCutHz = juce::jlimit (90.0f, 420.0f,
                                   120.0f + (spaceNorm * 120.0f) + (depthNorm * 55.0f));
    float brightnessGain = juce::jlimit (0.94f, 1.08f,
                                         0.97f + (0.08f * airCurve) - (0.03f * airSafety));
    float internalWidth = juce::jlimit (0.25f, 1.0f, 0.50f + (0.38f * widthNorm));
    float duckAmount = juce::jlimit (0.05f, 0.30f, 0.18f + (0.08f * (1.0f - depthNorm)));
    float glueAmount = juce::jlimit (0.02f, 0.14f,
                                     0.03f + (0.03f * spaceNorm) + (0.03f * depthNorm));
    float haloBlend = juce::jlimit (0.01f, 0.16f,
                                    0.03f + (0.05f * widthNorm) + (0.03f * airNorm));
    float motionDepth = juce::jlimit (0.004f, 0.08f,
                                      0.008f + (0.020f * spaceNorm) + (0.012f * depthNorm));
    float tailLift = juce::jlimit (0.01f, 0.12f,
                                   0.02f + (0.04f * airNorm) + (0.03f * decayNorm));

    switch (modeIndex)
    {
        case 1: // Arena
            roomSize = clamp01 (roomSize + 0.09f);
            preDelayMs += 12.0f;
            earlyAmount *= 0.80f;
            sideGain += 0.16f;
            decorrelationMs += 2.1f;
            wetTrim += 0.02f;
            toneCutoffHz = juce::jmin (toneCutoffHz * 1.02f, 13200.0f);
            lowCutHz += 25.0f;
            brightnessGain *= 1.01f;
            duckAmount = 0.17f;
            glueAmount = 0.08f;
            haloBlend += 0.025f;
            motionDepth += 0.010f;
            tailLift += 0.020f;
            internalWidth = juce::jlimit (0.25f, 1.0f, internalWidth + 0.10f);
            break;
        case 2: // Dream
            roomSize = clamp01 (roomSize + 0.12f);
            preDelayMs += 22.0f;
            earlyAmount *= 0.46f;
            damping = juce::jlimit (0.16f, 0.95f, damping + 0.10f);
            sideGain += 0.18f;
            decorrelationMs += 4.0f;
            wetTrim += 0.02f;
            toneCutoffHz *= 0.84f;
            lowCutHz += 42.0f;
            brightnessGain *= 0.95f;
            duckAmount = 0.14f;
            glueAmount = 0.10f;
            haloBlend += 0.040f;
            motionDepth += 0.018f;
            tailLift += 0.035f;
            internalWidth = juce::jlimit (0.25f, 1.0f, internalWidth + 0.12f);
            break;
        case 3: // Vintage
            roomSize = clamp01 (roomSize - 0.01f);
            preDelayMs = juce::jmax (8.0f, preDelayMs - 5.0f);
            earlyAmount = juce::jlimit (0.10f, 0.60f, earlyAmount + 0.05f);
            damping = juce::jlimit (0.16f, 0.95f, damping + 0.18f);
            sideGain *= 0.72f;
            decorrelationMs *= 0.58f;
            toneCutoffHz *= 0.70f;
            lowCutHz += 10.0f;
            brightnessGain *= 0.91f;
            wetTrim *= 0.96f;
            duckAmount = 0.12f;
            glueAmount = 0.07f;
            haloBlend *= 0.65f;
            motionDepth *= 0.55f;
            tailLift *= 0.55f;
            internalWidth *= 0.74f;
            break;
        default: // Studio
            roomSize *= 0.74f;
            preDelayMs = juce::jmax (12.0f, preDelayMs - 6.0f);
            earlyAmount = juce::jlimit (0.12f, 0.58f, earlyAmount + 0.08f);
            wetTrim *= 0.90f;
            sideGain *= 0.74f;
            decorrelationMs *= 0.58f;
            toneCutoffHz = juce::jmin (toneCutoffHz, 9000.0f);
            lowCutHz += 60.0f;
            brightnessGain *= 0.97f;
            duckAmount = 0.30f;
            glueAmount = 0.05f;
            haloBlend *= 0.55f;
            motionDepth *= 0.50f;
            tailLift *= 0.45f;
            internalWidth *= 0.78f;
            break;
    }

    juce::Reverb::Parameters reverbParams;
    reverbParams.roomSize = roomSize;
    reverbParams.damping = damping;
    reverbParams.wetLevel = 1.0f;
    reverbParams.dryLevel = 0.0f;
    reverbParams.width = internalWidth;
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters (reverbParams);

    wetToneLeft.setCutoffFrequency (toneCutoffHz);
    wetToneRight.setCutoffFrequency (toneCutoffHz);
    wetBodyLeft.setCutoffFrequency (lowCutHz);
    wetBodyRight.setCutoffFrequency (lowCutHz);

    const float preDelaySamples = juce::jlimit (0.0f,
                                                0.4f * static_cast<float> (currentSampleRate),
                                                static_cast<float> (currentSampleRate) * preDelayMs * 0.001f);
    preDelayLeft.setDelay (preDelaySamples);
    preDelayRight.setDelay (preDelaySamples);

    const float decorSamples = juce::jlimit (0.0f, 2048.0f,
                                             static_cast<float> (currentSampleRate) * decorrelationMs * 0.001f);
    decorrelationDelay.setDelay (decorSamples);

    auto* wetL = wetBuffer.getWritePointer (0);
    auto* wetR = wetBuffer.getNumChannels() > 1 ? wetBuffer.getWritePointer (1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float inputL = wetL[sample];
        const float inputR = wetR != nullptr ? wetR[sample] : inputL;

        const float delayedL = preDelayLeft.popSample (0);
        const float delayedR = preDelayRight.popSample (0);

        preDelayLeft.pushSample (0, inputL);
        preDelayRight.pushSample (0, inputR);

        const float bloomL = juce::jmap (attackSoftness,
                                         delayedL,
                                         (delayedL * 0.72f) + (inputL * 0.28f));
        wetL[sample] = bloomL + (delayedL * earlyAmount * 0.28f);

        if (wetR != nullptr)
        {
            const float bloomR = juce::jmap (attackSoftness,
                                             delayedR,
                                             (delayedR * 0.72f) + (inputR * 0.28f));
            wetR[sample] = bloomR + (delayedR * earlyAmount * 0.28f);
        }
    }

    if (wetR != nullptr)
        reverb.processStereo (wetL, wetR, numSamples);
    else
        reverb.processMono (wetL, numSamples);

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const auto* dryL = dryBuffer.getReadPointer (0);
    const auto* dryR = dryBuffer.getNumChannels() > 1 ? dryBuffer.getReadPointer (1) : nullptr;

    auto smoothWetSample = [glueAmount] (float sample)
    {
        const auto saturated = std::tanh (sample * (1.0f + (glueAmount * 2.4f)));
        return juce::jmap (glueAmount, sample, saturated);
    };

    const auto twoPi = juce::MathConstants<float>::twoPi;
    const float motionRateHz = juce::jmap (depthNorm, 0.11f, 0.33f);
    const float haloRateHz = juce::jmap (spaceNorm, 0.07f, 0.19f);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float dryRight = dryR != nullptr ? dryR[sample] : dryL[sample];
        const float vocalEnergy = 0.5f * (std::abs (dryL[sample]) + std::abs (dryRight));
        const float vocalSense = clamp01 (std::sqrt (vocalEnergy * 3.2f));
        const float gapBloom = 1.0f + ((1.0f - vocalSense) * tailLift);
        const float motionLfo = std::sin (motionPhase);
        const float haloLfo = 0.5f + (0.5f * std::sin (haloPhase));

        motionPhase += (twoPi * motionRateHz) / static_cast<float> (currentSampleRate);
        haloPhase += (twoPi * haloRateHz) / static_cast<float> (currentSampleRate);

        if (motionPhase > twoPi)
            motionPhase -= twoPi;

        if (haloPhase > twoPi)
            haloPhase -= twoPi;

        float processedL = wetBodyLeft.processSample (0,
                                                      wetToneLeft.processSample (0, wetL[sample] * brightnessGain));
        float processedR = wetR != nullptr
            ? wetBodyRight.processSample (0,
                                          wetToneRight.processSample (0, wetR[sample] * brightnessGain))
            : processedL;

        processedL = smoothWetSample (processedL * (gapBloom + (motionLfo * motionDepth)));
        processedR = smoothWetSample (processedR * (gapBloom - (motionLfo * motionDepth)));

        if (wetR != nullptr)
        {
            const float delayedRight = decorrelationDelay.popSample (0);
            decorrelationDelay.pushSample (0, processedR);
            processedR = (processedR * (1.0f - (0.45f * widthNorm))) + (delayedRight * 0.45f * widthNorm);

            const float mid = 0.5f * (processedL + processedR);
            const float side = 0.5f * (processedL - processedR) * sideGain;
            processedL = (mid + side) * wetTrim;
            processedR = (mid - side) * wetTrim;

            const float halo = mid * haloBlend * haloLfo;
            processedL += halo;
            processedR += halo;
        }
        else
        {
            processedL *= wetTrim * (1.0f + (0.5f * tailLift));
        }

        const float duckFactor = 1.0f - (duckAmount * vocalSense);
        processedL *= duckFactor;
        processedR *= duckFactor;

        outL[sample] = (dryL[sample] * (1.0f - mixNorm)) + (processedL * mixNorm);

        if (outR != nullptr)
            outR[sample] = (dryRight * (1.0f - mixNorm)) + (processedR * mixNorm);
    }
}

bool SpaceByNovaAudioProcessor::hasEditor() const
{
    return true;
}

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

    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpaceByNovaAudioProcessor();
}
