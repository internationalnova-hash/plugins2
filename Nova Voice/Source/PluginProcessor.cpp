#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <limits>

namespace
{
    constexpr auto morphId = "morph";
    constexpr auto textureId = "texture";
    constexpr auto formId = "form";
    constexpr auto airId = "air";
    constexpr auto blendId = "blend";
    constexpr auto modeId = "voice_mode";

    float clampUnit (float value) noexcept
    {
        return juce::jlimit (0.0f, 1.0f, value);
    }
}

NovaVoiceAudioProcessor::NovaVoiceAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts (*this, nullptr, juce::Identifier ("NovaVoice"), createParameterLayout())
{
    initialiseBandLayout();

    for (auto& bucket : inputSpectrum)
        bucket.store (0.0f);

    for (auto& bucket : problemSpectrum)
        bucket.store (0.0f);

    for (auto& bucket : reductionSpectrum)
        bucket.store (0.0f);
}

NovaVoiceAudioProcessor::~NovaVoiceAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaVoiceAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { morphId, 1 },
        "Morph",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        5.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value * 10.0f)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureId, 1 },
        "Texture",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        4.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value * 10.0f)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { formId, 1 },
        "Form",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        5.5f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value * 10.0f)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { airId, 1 },
        "Air",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        5.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value * 10.0f)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { blendId, 1 },
        "Blend",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        55.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { modeId, 1 },
        "Voice Mode",
        juce::StringArray { "Clean", "Digital", "Hybrid", "Extreme" },
        2));

    return layout;
}

void NovaVoiceAudioProcessor::initialiseBandLayout()
{
    constexpr float logMinF = 4.9068905963f;   // log2 (30)
    constexpr float logMaxF = 14.1292830167f;  // log2 (18000)

    for (size_t i = 0; i < smoothingBandCount; ++i)
    {
        const float t    = static_cast<float> (i) / static_cast<float> (smoothingBandCount - 1);
        const float freq = std::pow (2.0f, logMinF + t * (logMaxF - logMinF));

        bandFrequencies[i] = freq;
        bandQValues[i]     = 1.10f;
        bandMaxReductionDb[i] = juce::jmap (t, 3.5f, 10.0f);
        bandAttackMs[i] = juce::jmap (t, 8.0f, 4.0f);
        bandReleaseMs[i] = juce::jmap (t, 130.0f, 90.0f);
    }
}

const juce::String NovaVoiceAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaVoiceAudioProcessor::acceptsMidi() const  { return false; }
bool NovaVoiceAudioProcessor::producesMidi() const { return false; }
bool NovaVoiceAudioProcessor::isMidiEffect() const { return false; }
double NovaVoiceAudioProcessor::getTailLengthSeconds() const { return 0.20; }
int NovaVoiceAudioProcessor::getNumPrograms() { return 1; }
int NovaVoiceAudioProcessor::getCurrentProgram() { return 0; }
void NovaVoiceAudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String NovaVoiceAudioProcessor::getProgramName (int index) { juce::ignoreUnused (index); return {}; }
void NovaVoiceAudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName); }

void NovaVoiceAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    dryBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock);

    analyzerFifoIndex = 0;
    nextFFTBlockReady = false;
    std::fill (analyzerFifo.begin(), analyzerFifo.end(), 0.0f);
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::fill (bandEnvelopes.begin(), bandEnvelopes.end(), 0.0f);

    for (size_t index = 0; index < smoothingBandCount; ++index)
    {
        const float sr = static_cast<float> (sampleRate);
        bandAttackCoeff[index]  = std::exp (-1.0f / (0.001f * bandAttackMs[index]  * sr));
        bandReleaseCoeff[index] = std::exp (-1.0f / (0.001f * bandReleaseMs[index] * sr));

        juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 1 };
        leftProbeFilters[index].coefficients  = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, bandFrequencies[index], 0.7071f);
        rightProbeFilters[index].coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, bandFrequencies[index], 0.7071f);
        leftProbeFilters[index].prepare (spec);
        rightProbeFilters[index].prepare (spec);

        auto neutral = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, bandFrequencies[index], bandQValues[index], 1.0f);
        leftReductionFilters[index].coefficients  = neutral;
        rightReductionFilters[index].coefficients = neutral;
        leftReductionFilters[index].prepare (spec);
        rightReductionFilters[index].prepare (spec);

        leftProbeFilters[index].reset();
        rightProbeFilters[index].reset();
        leftReductionFilters[index].reset();
        rightReductionFilters[index].reset();
    }

    previousWetLeft = 0.0f;
    previousWetRight = 0.0f;
    modulationPhase = 0.0f;
}

void NovaVoiceAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaVoiceAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}
#endif

void NovaVoiceAudioProcessor::pushNextSampleIntoAnalyzer (float sample) noexcept
{
    if (analyzerFifoIndex == fftSize)
        return;

    analyzerFifo[analyzerFifoIndex++] = sample;

    if (analyzerFifoIndex == fftSize)
    {
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        std::copy (analyzerFifo.begin(), analyzerFifo.end(), fftData.begin());
        nextFFTBlockReady = true;
        analyzerFifoIndex = 0;
    }
}

void NovaVoiceAudioProcessor::refreshAnalyzerData()
{
    const float morphNorm = apvts.getRawParameterValue (morphId)->load() / 10.0f;
    const float textureNorm = apvts.getRawParameterValue (textureId)->load() / 10.0f;

    if (nextFFTBlockReady)
    {
        nextFFTBlockReady = false;
        window.multiplyWithWindowingTable (fftData.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        for (int b = 0; b < spectrumBins; ++b)
        {
            const auto startNorm = static_cast<float> (b) / static_cast<float> (spectrumBins);
            const auto endNorm = static_cast<float> (b + 1) / static_cast<float> (spectrumBins);

            auto startBin = juce::jlimit (1, (fftSize / 2) - 2,
                                          static_cast<int> (std::pow (startNorm, 1.8f) * ((fftSize / 2) - 2)) + 1);
            auto endBin = juce::jlimit (startBin + 1, (fftSize / 2) - 1,
                                        static_cast<int> (std::pow (endNorm, 1.8f) * ((fftSize / 2) - 1)) + 1);

            float peakValue = 0.0f;
            float localAvg = 0.0f;
            for (int bin = startBin; bin <= endBin; ++bin)
            {
                peakValue = juce::jmax (peakValue, fftData[bin]);
                localAvg += fftData[bin];
            }
            localAvg /= static_cast<float> ((endBin - startBin) + 1);

            const float inputNorm = clampUnit ((juce::Decibels::gainToDecibels (peakValue + 1.0e-5f) + 78.0f) / 78.0f);
            const float problemNorm = clampUnit ((peakValue - localAvg * 0.86f) * 7.0f) * (0.45f + morphNorm * 0.75f);

            inputSpectrum[static_cast<size_t> (b)].store (inputNorm);
            problemSpectrum[static_cast<size_t> (b)].store (problemNorm);
        }
    }

    for (int b = 0; b < spectrumBins; ++b)
    {
        const float tNorm = static_cast<float> (b) / static_cast<float> (spectrumBins - 1);
        const float displayHz = 20.0f * std::pow (1000.0f, tNorm);

        float minLogDist = std::numeric_limits<float>::max();
        float reductionVal = 0.0f;

        for (size_t bi = 0; bi < smoothingBandCount; ++bi)
        {
            const float dist = std::abs (std::log2 (displayHz / juce::jmax (1.0f, bandFrequencies[bi])));
            if (dist < minLogDist)
            {
                minLogDist = dist;
                reductionVal = bandSmoothedReductionDb[bi];
            }
        }

        const float norm = clampUnit ((-reductionVal / 12.0f) * (0.7f + textureNorm * 0.7f));
        reductionSpectrum[static_cast<size_t> (b)].store (norm);
    }
}

void NovaVoiceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float morph = apvts.getRawParameterValue (morphId)->load();
    const float texture = apvts.getRawParameterValue (textureId)->load();
    const float form = apvts.getRawParameterValue (formId)->load();
    const float air = apvts.getRawParameterValue (airId)->load();
    const float blend = apvts.getRawParameterValue (blendId)->load();
    const int modeIndex = juce::roundToInt (apvts.getRawParameterValue (modeId)->load());

    const float morphNorm = morph / 10.0f;
    const float textureNorm = texture / 10.0f;
    const float formNorm = (form - 5.0f) / 5.0f;
    const float airNorm = air / 10.0f;
    const float blendNorm = blend / 100.0f;

    struct ModeBias { float morph; float texture; float formRange; float brightness; float motion; };
    ModeBias mode = { 0.85f, 0.90f, 2.2f, 1.0f, 1.0f }; // Hybrid default
    if (modeIndex == 0) mode = { 0.60f, 0.55f, 1.5f, 0.90f, 0.70f };      // Clean
    else if (modeIndex == 1) mode = { 1.00f, 1.15f, 3.0f, 1.15f, 1.10f }; // Digital
    else if (modeIndex == 3) mode = { 1.25f, 1.35f, 5.0f, 1.20f, 1.35f }; // Extreme

    const auto* dryLeft = dryBuffer.getReadPointer (0);
    const auto* dryRight = dryBuffer.getNumChannels() > 1 ? dryBuffer.getReadPointer (1) : nullptr;

    const float focusShiftOct = formNorm * (mode.formRange / 4.5f);
    const float focusHz = 1700.0f * std::pow (2.0f, focusShiftOct);
    const float focusWidthOct = juce::jlimit (0.9f, 2.8f, 2.2f - (0.8f * std::abs (formNorm)));
    const float detectorSens = 0.45f + morphNorm * 0.65f * mode.morph;
    const float threshold = 0.014f;

    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        const float left = dryLeft[s];
        const float right = dryRight != nullptr ? dryRight[s] : left;
        pushNextSampleIntoAnalyzer (0.5f * (left + right));

        for (size_t i = 0; i < smoothingBandCount; ++i)
        {
            const float lp = std::abs (leftProbeFilters[i].processSample (left));
            const float rp = std::abs (rightProbeFilters[i].processSample (right));
            const float level = juce::jmax (lp, rp);

            if (level > bandEnvelopes[i])
                bandEnvelopes[i] = bandAttackCoeff[i] * bandEnvelopes[i] + (1.0f - bandAttackCoeff[i]) * level;
            else
                bandEnvelopes[i] = bandReleaseCoeff[i] * bandEnvelopes[i] + (1.0f - bandReleaseCoeff[i]) * level;
        }
    }

    for (size_t i = 0; i < smoothingBandCount; ++i)
    {
        const float freq = bandFrequencies[i];
        const float envelope = bandEnvelopes[i];
        const float harshness = clampUnit ((envelope - threshold) * (20.0f * detectorSens));

        const float logDist = std::log2 (juce::jmax (1.0f, freq) / focusHz);
        const float focusWeight = juce::jlimit (0.26f, 1.0f,
            0.26f + 0.74f * std::exp (-0.5f * (logDist / focusWidthOct) * (logDist / focusWidthOct)));

        float protection = 1.0f;
        if (freq > 6000.0f)
            protection *= juce::jmap (airNorm, 1.0f, 0.56f);
        if (freq < 180.0f)
            protection *= 0.78f;

        bandRawReductionDb[i] = -bandMaxReductionDb[i]
                              * (0.24f + morphNorm * 0.95f * mode.morph)
                              * focusWeight
                              * harshness
                              * protection;
    }

    const float crossBlend = juce::jlimit (0.25f, 0.75f, 0.64f - (textureNorm * 0.20f));
    for (size_t i = 0; i < smoothingBandCount; ++i)
    {
        const float prev = (i > 0) ? bandRawReductionDb[i - 1] : bandRawReductionDb[i];
        const float next = (i < smoothingBandCount - 1) ? bandRawReductionDb[i + 1] : bandRawReductionDb[i];
        const float soft = 0.25f * prev + 0.5f * bandRawReductionDb[i] + 0.25f * next;
        bandSmoothedReductionDb[i] = juce::jmap (crossBlend, bandRawReductionDb[i], soft);

        auto coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            currentSampleRate, bandFrequencies[i], bandQValues[i], juce::Decibels::decibelsToGain (bandSmoothedReductionDb[i]));
        leftReductionFilters[i].coefficients = coeffs;
        rightReductionFilters[i].coefficients = coeffs;
    }

    auto* leftChannel = buffer.getWritePointer (0);
    auto* rightChannel = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    float blockPeak = 0.0f;
    const float textureDrive = 1.0f + textureNorm * 3.2f * mode.texture;
    const float textureMix = juce::jlimit (0.08f, 0.82f, 0.12f + textureNorm * 0.52f * mode.texture);
    const float airEnhance = juce::jlimit (0.00f, 0.22f, 0.03f + airNorm * 0.16f * mode.brightness);
    const float motionDepth = juce::jlimit (0.0f, 0.22f, 0.02f + morphNorm * 0.10f * mode.motion + textureNorm * 0.06f);
    const float phaseStep = juce::MathConstants<float>::twoPi * (0.22f + 0.35f * mode.motion) / static_cast<float> (currentSampleRate);

    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        float wetL = dryLeft[s];
        float wetR = dryRight != nullptr ? dryRight[s] : dryLeft[s];

        for (size_t i = 0; i < smoothingBandCount; ++i)
        {
            wetL = leftReductionFilters[i].processSample (wetL);
            wetR = rightReductionFilters[i].processSample (wetR);
        }

        const float satL = std::tanh (wetL * textureDrive);
        const float satR = std::tanh (wetR * textureDrive);
        wetL = juce::jmap (textureMix, wetL, satL);
        wetR = juce::jmap (textureMix, wetR, satR);

        const float diffL = wetL - previousWetLeft;
        const float diffR = wetR - previousWetRight;
        previousWetLeft = wetL;
        previousWetRight = wetR;

        wetL += diffL * airEnhance;
        wetR += diffR * airEnhance;

        const float motion = std::sin (modulationPhase);
        modulationPhase += phaseStep;
        if (modulationPhase > juce::MathConstants<float>::twoPi)
            modulationPhase -= juce::MathConstants<float>::twoPi;

        wetL *= (1.0f + motionDepth * motion);
        wetR *= (1.0f - motionDepth * motion);

        const float outL = dryLeft[s] * (1.0f - blendNorm) + wetL * blendNorm;
        const float dryRs = dryRight != nullptr ? dryRight[s] : dryLeft[s];
        const float outR = dryRs * (1.0f - blendNorm) + wetR * blendNorm;

        leftChannel[s] = outL;
        if (rightChannel != nullptr)
            rightChannel[s] = outR;

        blockPeak = juce::jmax (blockPeak, std::abs (outL), std::abs (outR));
    }

    outputPeakLevel.store (blockPeak);
    refreshAnalyzerData();
}

bool NovaVoiceAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* NovaVoiceAudioProcessor::createEditor() { return new NovaVoiceAudioProcessorEditor (*this); }

void NovaVoiceAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaVoiceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaVoiceAudioProcessor();
}
