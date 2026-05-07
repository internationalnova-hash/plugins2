#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    constexpr auto driveId = "drive";
    constexpr auto shapeId = "clip_shape";
    constexpr auto toneId = "tone";
    constexpr auto punchId = "punch";
    constexpr auto ceilingId = "ceiling";
    constexpr auto mixId = "mix";

    constexpr auto modeId = "mode";
    constexpr auto oversamplingId = "oversampling";
    constexpr auto lowLatencyId = "low_latency";
    constexpr auto safeModeId = "safe_mode";
    constexpr auto linkLRId = "link_lr";

    inline float clampUnit (float value) noexcept
    {
        return juce::jlimit (0.0f, 1.0f, value);
    }

    inline float fastLerp (float a, float b, float t) noexcept
    {
        return a + (b - a) * t;
    }
}

NovaClipAudioProcessor::NovaClipAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts (*this, nullptr, juce::Identifier ("NovaClip"), createParameterLayout())
{
}

NovaClipAudioProcessor::~NovaClipAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaClipAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveId, 1 },
        "Drive",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f),
        6.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " dB"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shapeId, 1 },
        "Clip Shape",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        52.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { toneId, 1 },
        "Tone",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { punchId, 1 },
        "Punch",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        65.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ceilingId, 1 },
        "Ceiling",
        juce::NormalisableRange<float> (-12.0f, 0.0f, 0.1f),
        -0.3f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " dB"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        100.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { modeId, 1 },
        "Mode",
        juce::StringArray { "Smooth", "Punch", "Loud", "Hard" },
        0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { oversamplingId, 1 },
        "Oversampling",
        juce::StringArray { "1x", "2x", "4x" },
        2));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { lowLatencyId, 1 },
        "Low Latency",
        false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { safeModeId, 1 },
        "Safe Mode",
        true));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { linkLRId, 1 },
        "Link L/R",
        true));

    return layout;
}

const juce::String NovaClipAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaClipAudioProcessor::acceptsMidi() const { return false; }
bool NovaClipAudioProcessor::producesMidi() const { return false; }
bool NovaClipAudioProcessor::isMidiEffect() const { return false; }
double NovaClipAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int NovaClipAudioProcessor::getNumPrograms() { return 1; }
int NovaClipAudioProcessor::getCurrentProgram() { return 0; }
void NovaClipAudioProcessor::setCurrentProgram (int) {}
const juce::String NovaClipAudioProcessor::getProgramName (int) { return {}; }
void NovaClipAudioProcessor::changeProgramName (int, const juce::String&) {}

void NovaClipAudioProcessor::resetDynamicsState() noexcept
{
    transientFastEnv = { 0.0f, 0.0f };
    transientSlowEnv = { 0.0f, 0.0f };
    toneLowpassState = { 0.0f, 0.0f };
    postSmoothState = { 0.0f, 0.0f };
    safeHfLowpassState = { 0.0f, 0.0f };
}

void NovaClipAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    dryBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock);

    oversampler2x.reset();
    oversampler2x.initProcessing (static_cast<size_t> (samplesPerBlock));

    oversampler4x.reset();
    oversampler4x.initProcessing (static_cast<size_t> (samplesPerBlock));

    resetDynamicsState();
}

void NovaClipAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaClipAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}
#endif

float NovaClipAudioProcessor::processClippingSample (float sample,
                                                     int channel,
                                                     float driveDb,
                                                     float shapeNorm,
                                                     float toneNorm,
                                                     float punchNorm,
                                                     int mode,
                                                     bool safeMode,
                                                     bool linkLR) noexcept
{
    float modeDriveScale = 1.0f;
    float modeShapeBias = 0.0f;
    float modePunchBoost = 0.0f;
    float modeHarmonicLift = 0.0f;

    switch (mode)
    {
        case 0: modeDriveScale = 0.86f; modeShapeBias = -0.22f; modeHarmonicLift = -0.04f; break; // Smooth
        case 1: modeDriveScale = 0.97f; modeShapeBias = -0.04f; modePunchBoost = 0.35f; modeHarmonicLift = -0.02f; break; // Punch
        case 2: modeDriveScale = 1.12f; modeShapeBias = 0.10f; modeHarmonicLift = 0.08f; break;  // Loud
        case 3: modeDriveScale = 1.26f; modeShapeBias = 0.24f; modePunchBoost = 0.10f; modeHarmonicLift = 0.14f; break;  // Hard
        default: break;
    }

    const int detectorIndex = linkLR ? 0 : channel;

    const float driveNorm = clampUnit (driveDb / 24.0f);
    const float driveLinear = juce::Decibels::decibelsToGain (driveDb * modeDriveScale);
    float effectiveShape = clampUnit (shapeNorm + modeShapeBias);
    const float effectivePunch = clampUnit (punchNorm + modePunchBoost);
    const float toneSigned = (toneNorm * 2.0f) - 1.0f;

    if (safeMode)
        effectiveShape = fastLerp (effectiveShape, effectiveShape * 0.74f, 0.55f);

    const float absInput = std::abs (sample);

    // Dual-time envelope tracks body vs. edge; this keeps punch while shaving only spikes.
    const float fastAttackCoeff = std::exp (-1.0f / (0.00042f * static_cast<float> (currentSampleRate)));
    const float fastReleaseCoeff = std::exp (-1.0f / (0.0038f * static_cast<float> (currentSampleRate)));
    const float slowAttackCoeff = std::exp (-1.0f / (0.0045f * static_cast<float> (currentSampleRate)));
    const float slowReleaseCoeff = std::exp (-1.0f / (0.045f * static_cast<float> (currentSampleRate)));

    const float fastCoeff = absInput > transientFastEnv[detectorIndex] ? fastAttackCoeff : fastReleaseCoeff;
    const float slowCoeff = absInput > transientSlowEnv[detectorIndex] ? slowAttackCoeff : slowReleaseCoeff;

    transientFastEnv[detectorIndex] = fastCoeff * transientFastEnv[detectorIndex] + (1.0f - fastCoeff) * absInput;
    transientSlowEnv[detectorIndex] = slowCoeff * transientSlowEnv[detectorIndex] + (1.0f - slowCoeff) * absInput;

    const float signedBody = std::copysign (transientSlowEnv[detectorIndex], sample);
    const float transient = sample - signedBody;
    const float transientEdge = juce::jmax (0.0f, transientFastEnv[detectorIndex] - transientSlowEnv[detectorIndex]);

    // Stage 1: pre-saturation adds glue and tone-dependent harmonic tilt before clipping.
    const float preDrive = 1.0f + driveNorm * 1.55f + effectivePunch * 0.42f;
    const float preNorm = std::tanh (sample * preDrive) / std::tanh (preDrive);

    const float tiltCoeff = std::exp (-1.0f / (0.00066f * static_cast<float> (currentSampleRate)));
    toneLowpassState[channel] = tiltCoeff * toneLowpassState[channel] + (1.0f - tiltCoeff) * preNorm;
    const float lowBand = toneLowpassState[channel];
    const float highBand = preNorm - lowBand;

    float tilted = preNorm;
    if (toneSigned >= 0.0f)
        tilted = preNorm + highBand * toneSigned * 0.48f + highBand * driveNorm * 0.08f;
    else
        tilted = fastLerp (preNorm + lowBand * (-toneSigned) * 0.18f,
                           lowBand,
                           (-toneSigned) * (0.54f + driveNorm * 0.16f));

    const float pushed = tilted * driveLinear;

    // Stage 2: knee-morph clipper (soft -> mastering clip -> hard) driven by Clip Shape.
    const float softAmount = 1.12f + std::pow (1.0f - effectiveShape, 1.25f) * 11.5f;
    const float softClip = std::tanh (softAmount * pushed) / std::tanh (softAmount);

    const float masterKnee = 0.62f + (1.0f - effectiveShape) * 0.92f;
    const float masteringClip = pushed / (1.0f + std::abs (pushed) * masterKnee);

    const float hardCeiling = safeMode ? 0.96f : 1.0f;
    const float hardClip = juce::jlimit (-hardCeiling, hardCeiling, pushed);

    float clipped = 0.0f;
    if (effectiveShape <= 0.5f)
    {
        const float t = std::pow (effectiveShape * 2.0f, 1.12f);
        clipped = fastLerp (softClip, masteringClip, t);
    }
    else
    {
        const float t = std::pow ((effectiveShape - 0.5f) * 2.0f, 1.18f);
        clipped = fastLerp (masteringClip, hardClip, t);
    }

    // Preserve transient edge even under clipping to keep kick/snare impact.
    float preserveAmount = effectivePunch * (0.12f + (1.0f - effectiveShape) * 0.24f);
    if (safeMode)
        preserveAmount += 0.08f + (1.0f - effectiveShape) * 0.06f;
    const float preserveGate = clampUnit (transientEdge * (3.2f + effectivePunch * 2.2f));
    clipped += transient * preserveAmount * preserveGate;

    // Harmonic density bloom: odd/even shaping increases with drive but avoids fizz.
    const float odd = clipped * clipped * clipped;
    const float even = clipped * std::abs (clipped);

    const float harmonicBase = clampUnit (0.05f + driveNorm * (0.18f + effectiveShape * 0.08f) + modeHarmonicLift);
    const float harmonicTiltBright = clampUnit (toneSigned * 0.5f + 0.5f);
    float oddAmount = harmonicBase * (0.52f + harmonicTiltBright * 0.36f);
    float evenAmount = harmonicBase * (0.24f + (1.0f - harmonicTiltBright) * 0.22f);

    if (safeMode)
    {
        oddAmount *= 0.74f;
        evenAmount *= 1.08f;
    }

    clipped += odd * oddAmount + even * evenAmount;

    if (safeMode)
    {
        // HF protection around the harsh upper band keeps cymbals/vocals smoother.
        const float hfProtectCoeff = std::exp (-1.0f / (0.000016f * static_cast<float> (currentSampleRate))); // ~10 kHz
        safeHfLowpassState[channel] = hfProtectCoeff * safeHfLowpassState[channel]
                                    + (1.0f - hfProtectCoeff) * clipped;

        const float harshBand = clipped - safeHfLowpassState[channel];
        const float harshReduction = 0.18f + driveNorm * 0.12f + clampUnit (toneSigned) * 0.08f;
        clipped -= harshBand * harshReduction;
    }

    // Stage 3: tiny post smoothing glues hard edges and makes high-drive behavior premium.
    const float postCoeff = std::exp (-1.0f / (0.00034f * static_cast<float> (currentSampleRate)));
    postSmoothState[channel] = postCoeff * postSmoothState[channel] + (1.0f - postCoeff) * clipped;

    float postBlend = 0.022f + driveNorm * 0.036f + effectiveShape * 0.030f;
    if (safeMode)
        postBlend += 0.072f;
    clipped = fastLerp (clipped, postSmoothState[channel], clampUnit (postBlend));

    if (safeMode)
        clipped = std::tanh (clipped * 0.86f);

    return clipped;
}

void NovaClipAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    dryBuffer.makeCopyOf (buffer, true);

    const float driveDb = apvts.getRawParameterValue (driveId)->load();
    const float shapeNorm = apvts.getRawParameterValue (shapeId)->load() / 100.0f;
    const float toneNorm = apvts.getRawParameterValue (toneId)->load() / 100.0f;
    const float punchNorm = apvts.getRawParameterValue (punchId)->load() / 100.0f;
    const float ceilingDb = apvts.getRawParameterValue (ceilingId)->load();
    const float mixNorm = apvts.getRawParameterValue (mixId)->load() / 100.0f;

    const int mode = juce::roundToInt (apvts.getRawParameterValue (modeId)->load());
    const int oversampling = juce::roundToInt (apvts.getRawParameterValue (oversamplingId)->load());
    const bool lowLatency = apvts.getRawParameterValue (lowLatencyId)->load() > 0.5f;
    const bool safeMode = apvts.getRawParameterValue (safeModeId)->load() > 0.5f;
    const bool linkLR = apvts.getRawParameterValue (linkLRId)->load() > 0.5f;

    const int channels = juce::jmin (2, buffer.getNumChannels());
    const int samples = buffer.getNumSamples();

    const float safeDriveDb = safeMode ? juce::jmin (driveDb, 18.0f) : driveDb;
    const float safetyCeilingDb = safeMode ? (ceilingDb - 0.2f) : ceilingDb;
    const float ceilingGain = juce::Decibels::decibelsToGain (ceilingDb);
    const float safetyCeilingGain = juce::Decibels::decibelsToGain (safetyCeilingDb);

    float inputPeakLeft = 0.0f;
    float inputPeakRight = 0.0f;
    float outputPeakLeft = 0.0f;
    float outputPeakRight = 0.0f;

    float reductionAccum = 0.0f;
    float heatAccum = 0.0f;
    int reductionSamples = 0;

    auto processBufferBlock = [&] (juce::dsp::AudioBlock<float> block, double localSampleRate)
    {
        juce::ignoreUnused (localSampleRate);

        const int blockChannels = juce::jmin (channels, static_cast<int> (block.getNumChannels()));
        const int blockSamples = static_cast<int> (block.getNumSamples());

        for (int sampleIndex = 0; sampleIndex < blockSamples; ++sampleIndex)
        {
            if (linkLR)
            {
                const float inL = block.getSample (0, (size_t) sampleIndex);
                const float inR = blockChannels > 1 ? block.getSample (1, (size_t) sampleIndex) : inL;
                const float linkedInput = 0.5f * (inL + inR);
                const float linkedAbs = std::abs (linkedInput);
                transientFastEnv[0] = 0.92f * transientFastEnv[0] + 0.08f * linkedAbs;
                transientSlowEnv[0] = 0.985f * transientSlowEnv[0] + 0.015f * linkedAbs;
            }

            for (int channel = 0; channel < blockChannels; ++channel)
            {
                const float drySample = block.getSample ((size_t) channel, (size_t) sampleIndex);
                const float wetSample = processClippingSample (drySample,
                                                               channel,
                                                               safeDriveDb,
                                                               shapeNorm,
                                                               toneNorm,
                                                               punchNorm,
                                                               mode,
                                                               safeMode,
                                                               linkLR);

                const float inputAbs = std::abs (drySample);
                const float outputAbs = std::abs (wetSample);
                const float reductionDb = juce::Decibels::gainToDecibels ((inputAbs + 1.0e-6f) / (outputAbs + 1.0e-6f));
                reductionAccum += juce::jmax (0.0f, reductionDb);

                const float clipAmount = juce::jmax (0.0f, inputAbs - outputAbs);
                heatAccum += juce::jlimit (0.0f, 1.0f, clipAmount * 2.3f);
                ++reductionSamples;

                block.setSample ((size_t) channel, (size_t) sampleIndex, wetSample);
            }
        }
    };

    const bool useOversampling = (! lowLatency) && oversampling > 0;

    if (useOversampling && oversampling == 1)
    {
        juce::dsp::AudioBlock<float> baseBlock (buffer);
        auto upBlock = oversampler2x.processSamplesUp (baseBlock);
        processBufferBlock (upBlock, currentSampleRate * 2.0);
        oversampler2x.processSamplesDown (baseBlock);
    }
    else if (useOversampling && oversampling >= 2)
    {
        juce::dsp::AudioBlock<float> baseBlock (buffer);
        auto upBlock = oversampler4x.processSamplesUp (baseBlock);
        processBufferBlock (upBlock, currentSampleRate * 4.0);
        oversampler4x.processSamplesDown (baseBlock);
    }
    else
    {
        juce::dsp::AudioBlock<float> directBlock (buffer);
        processBufferBlock (directBlock, currentSampleRate);
    }

    for (int sampleIndex = 0; sampleIndex < samples; ++sampleIndex)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            const float dry = dryBuffer.getSample (channel, sampleIndex);
            const float wet = buffer.getSample (channel, sampleIndex) * ceilingGain;
            float mixed = (dry * (1.0f - mixNorm)) + (wet * mixNorm);

            if (safeMode)
            {
                // Gentle true-peak softening for streaming/mastering safety.
                const float safeSat = std::tanh (mixed / (safetyCeilingGain + 1.0e-6f)) * safetyCeilingGain;
                mixed = fastLerp (mixed, safeSat, 0.34f);
            }

            buffer.setSample (channel, sampleIndex, mixed);

            if (channel == 0)
            {
                inputPeakLeft = juce::jmax (inputPeakLeft, std::abs (dry));
                outputPeakLeft = juce::jmax (outputPeakLeft, std::abs (mixed));
            }
            else
            {
                inputPeakRight = juce::jmax (inputPeakRight, std::abs (dry));
                outputPeakRight = juce::jmax (outputPeakRight, std::abs (mixed));
            }
        }
    }

    if (channels < 2)
    {
        inputPeakRight = inputPeakLeft;
        outputPeakRight = outputPeakLeft;
    }

    const float avgReductionDb = reductionSamples > 0 ? (reductionAccum / static_cast<float> (reductionSamples)) : 0.0f;
    const float avgHeat = reductionSamples > 0 ? (heatAccum / static_cast<float> (reductionSamples)) : 0.0f;

    inputPeakL.store (inputPeakLeft);
    inputPeakR.store (inputPeakRight);
    outputPeakL.store (outputPeakLeft);
    outputPeakR.store (outputPeakRight);
    clipReductionDb.store (avgReductionDb);
    heatAmount.store (avgHeat);
}

bool NovaClipAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* NovaClipAudioProcessor::createEditor() { return new NovaClipAudioProcessorEditor (*this); }

void NovaClipAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaClipAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaClipAudioProcessor();
}
