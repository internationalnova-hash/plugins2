#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <limits>

namespace
{
    constexpr auto pitchId = "pitch";
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
        juce::ParameterID { pitchId, 1 },
        "Pitch",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int)
        {
            const int semitones = juce::roundToInt (value);
            if (semitones > 0)
                return "+" + juce::String (semitones) + " st";
            return juce::String (semitones) + " st";
        }));

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
        juce::StringArray { "Clean", "Digital", "Hybrid", "Extreme", "Robot" },
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
    pitchUpBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock);
    pitchDownBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock);

    analyzerFifoIndex = 0;
    nextFFTBlockReady = false;
    std::fill (analyzerFifo.begin(), analyzerFifo.end(), 0.0f);
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::fill (bandEnvelopes.begin(), bandEnvelopes.end(), 0.0f);

    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 1 };

    // Initialize analysis filters and formant filters
    for (size_t index = 0; index < smoothingBandCount; ++index)
    {
        const float sr = static_cast<float> (sampleRate);
        bandAttackCoeff[index]  = std::exp (-1.0f / (0.001f * bandAttackMs[index]  * sr));
        bandReleaseCoeff[index] = std::exp (-1.0f / (0.001f * bandReleaseMs[index] * sr));

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

    // Initialize formant filters for creative morphing
    for (size_t i = 0; i < 4; ++i)
    {
        formantFiltersLeft[i].prepare (spec);
        formantFiltersRight[i].prepare (spec);
        formantFiltersLeft[i].reset();
        formantFiltersRight[i].reset();
    }

    previousWetLeft = 0.0f;
    previousWetRight = 0.0f;
    previousInputLeft = 0.0f;
    previousInputRight = 0.0f;
    modulationPhase = 0.0f;
    pitchUpPhase = 0.0f;
    pitchDownPhase = 0.0f;
    pitchDelaySize = juce::jmax (16384, juce::nextPowerOfTwo (static_cast<int> (sampleRate * 0.35)));
    pitchDelayLeft.assign (static_cast<size_t> (pitchDelaySize), 0.0f);
    pitchDelayRight.assign (static_cast<size_t> (pitchDelaySize), 0.0f);
    pitchWriteIndex = 0;
    pitchReadPos = static_cast<float> (pitchDelaySize - juce::jmin (4096, pitchDelaySize / 4));
    subOctavePolarityLeft = false;
    subOctavePolarityRight = false;
    robotCarrierPhase = 0.0f;
    robotEnvelope = 0.0f;
    inputDcStateLeft = 0.0f;
    inputDcStateRight = 0.0f;
    inputPrevLeft = 0.0f;
    inputPrevRight = 0.0f;
    airSmoothLeft = 0.0f;
    airSmoothRight = 0.0f;
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

    const float pitch = apvts.getRawParameterValue (pitchId)->load();
    const float morph = apvts.getRawParameterValue (morphId)->load();
    const float texture = apvts.getRawParameterValue (textureId)->load();
    const float form = apvts.getRawParameterValue (formId)->load();
    const float air = apvts.getRawParameterValue (airId)->load();
    const float blend = apvts.getRawParameterValue (blendId)->load();
    const int modeIndex = juce::roundToInt (apvts.getRawParameterValue (modeId)->load());

    const float morphNormRaw = morph / 10.0f;
    const float textureNormRaw = texture / 10.0f;
    const float formNormRaw = form / 10.0f;
    const float airNormRaw = air / 10.0f;

    // Perceptual curves: make early knob movement immediately audible.
    // CRITICAL: Use steeper curves so that default/midpoint values produce MINIMAL effect (neutral baseline)
    const float morphCentered = (morphNormRaw - 0.50f) * 2.0f;
    const float morphNorm = std::pow (juce::jlimit (0.0f, 1.0f, std::abs (morphCentered)), 0.80f);  // Curves around midpoint
    const float morphSign = morphCentered >= 0.0f ? 1.0f : -1.0f;
    const float textureNorm = std::pow (juce::jlimit (0.0f, 1.0f, textureNormRaw - 0.40f) * 2.0f, 0.75f);  // Shifts to emphasize movement from defaults
    const float formCentered = (formNormRaw - 0.55f) * 2.0f;
    const float formNorm = std::pow (juce::jlimit (0.0f, 1.0f, std::abs (formCentered)), 0.85f);  // Neutral at form=5.5
    const float formSign = formCentered >= 0.0f ? 1.0f : -1.0f;
    const float airNorm = std::pow (juce::jlimit (0.0f, 1.0f, airNormRaw - 0.50f) * 2.0f, 0.80f);  // Neutral at air=5.0
    const float blendNorm = blend / 100.0f;

    struct ModeBias
    {
        float pitchSmooth;
        float pitchEdge;
        float morph;
        float texture;
        float warmWeight;
        float gritWeight;
        float digitalWeight;
        float formRange;
        float brightness;
        float motion;
        float robot;
    };

    ModeBias mode = { 0.0025f, 0.00f, 0.85f, 0.90f, 0.60f, 0.70f, 0.45f, 2.2f, 1.0f, 1.0f, 0.0f }; // Hybrid default
    if (modeIndex == 0) mode = { 0.0042f, 0.00f, 0.60f, 0.55f, 0.95f, 0.35f, 0.12f, 1.5f, 0.90f, 0.70f, 0.0f };      // Clean
    else if (modeIndex == 1) mode = { 0.0022f, 0.05f, 1.00f, 1.15f, 0.35f, 0.85f, 1.10f, 3.0f, 1.15f, 1.10f, 0.0f }; // Digital
    else if (modeIndex == 3) mode = { 0.0014f, 0.10f, 1.25f, 1.35f, 0.25f, 1.20f, 1.40f, 5.0f, 1.20f, 1.35f, 0.0f }; // Extreme
    else if (modeIndex == 4) mode = { 0.0010f, 0.22f, 1.45f, 1.20f, 0.18f, 0.90f, 1.55f, 5.4f, 0.95f, 1.45f, 1.0f }; // Robot

    const auto* dryLeft = dryBuffer.getReadPointer (0);
    const auto* dryRight = dryBuffer.getNumChannels() > 1 ? dryBuffer.getReadPointer (1) : nullptr;

    const float focusShiftOct = juce::jmap (formNorm, -0.10f, 0.55f) * (mode.formRange / 4.5f);
    const float focusHz = 1500.0f * std::pow (2.0f, focusShiftOct);
    const float focusWidthOct = juce::jlimit (1.1f, 2.8f, 2.5f - (0.45f * formNorm));
    const float detectorSens = 0.45f + morphNorm * 0.65f * mode.morph;
    const float threshold = juce::jmap (morphNorm, 0.0048f, 0.0026f);

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
        const float aboveThreshold = juce::jmax (0.0f, envelope - threshold);
        const float detectorDrive = (aboveThreshold / (threshold + 1.0e-5f)) * (1.2f + detectorSens * 2.2f);
        const float harshness = clampUnit (std::tanh (detectorDrive));

        const float logDist = std::log2 (juce::jmax (1.0f, freq) / focusHz);
        const float focusWeight = juce::jlimit (0.26f, 1.0f,
            0.26f + 0.74f * std::exp (-0.5f * (logDist / focusWidthOct) * (logDist / focusWidthOct)));

        float protection = 1.0f;
        if (freq > 6000.0f)
            protection *= juce::jmap (airNorm, 1.0f, 0.56f);
        if (freq < 180.0f)
            protection *= 0.78f;

        bandRawReductionDb[i] = -bandMaxReductionDb[i]
                      * (morphNorm * 1.05f * mode.morph)
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
    const float reductionAmount = juce::jlimit (0.0f, 0.10f, morphNorm * 0.075f + textureNorm * 0.02f);
    const float textureDrive = 1.0f + textureNorm * 1.25f * mode.texture + morphNorm * 0.14f;
    const float textureMix = juce::jlimit (0.0f, 0.50f, 0.05f + textureNorm * 0.34f * mode.texture);
    const float airEnhance = juce::jlimit (0.00f, 0.22f, 0.03f + airNorm * 0.16f * mode.brightness);
    const float airExciteMix = juce::jlimit (0.0f, 0.46f, 0.02f + airNorm * 0.38f * mode.brightness);
    const float airPolishMix = juce::jlimit (0.0f, 0.36f, 0.01f + airNorm * 0.30f * mode.brightness);
    const float airSmoothCoeff = std::exp (-juce::MathConstants<float>::twoPi * 3600.0f / static_cast<float> (currentSampleRate));
    const float motionDepth = juce::jlimit (0.0f, 0.22f, 0.02f + morphNorm * 0.10f * mode.motion + textureNorm * 0.06f);
    const float phaseStep = juce::MathConstants<float>::twoPi * (0.22f + 0.35f * mode.motion) / static_cast<float> (currentSampleRate);
    const float morphCharacter = juce::jlimit (0.0f, 1.0f, std::pow (morphNorm, 0.70f) * (0.50f + 0.58f * mode.morph));
    const float pitchAmountNorm = juce::jlimit (0.0f, 1.0f, std::pow (std::abs (pitch) / 12.0f, 0.75f));
    const float formBias = formSign * juce::jmap (formNorm, 0.03f, 0.18f);
    const float morphDrive = 1.0f + 2.4f * morphCharacter;
    const float foldMix = juce::jlimit (0.0f, 0.55f, 0.05f + morphCharacter * 0.50f);

    // Dedicated voice-transform layer for obvious creative morphing.
    const float octaveLayer = juce::jlimit (0.0f, 1.0f, std::pow (morphCharacter, 1.2f) * (0.55f + 0.45f * mode.morph));
    const float highVoiceMix = octaveLayer * 0.18f;
    const float lowVoiceMix = octaveLayer * 0.12f;

    const float formantShift = std::pow (2.0f, formSign * juce::jmap (formNorm, 0.12f, 0.45f) * (0.80f + 0.18f * mode.morph));
    const float f1 = juce::jlimit (260.0f, 1800.0f, 520.0f * formantShift);
    const float f2 = juce::jlimit (900.0f, 3300.0f, 1420.0f * formantShift);
    const float f3 = juce::jlimit (1600.0f, 5200.0f, 2550.0f * formantShift);
    // Make formant gains neutral (1.0) when formNorm is below threshold for baseline neutrality
    const float formGainMult = std::abs (formNorm) > 0.002f ? 1.0f : 0.0f;
    const float formGain1 = juce::Decibels::decibelsToGain (juce::jmap (formNorm, 0.0f, 6.0f) * formGainMult);
    const float formGain2 = juce::Decibels::decibelsToGain (juce::jmap (formNorm, 0.0f, 7.0f) * formGainMult);
    const float formGain3 = juce::Decibels::decibelsToGain (juce::jmap (formNorm, 0.0f, 5.0f) * formGainMult);

    formantFiltersLeft[0].coefficients  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, f1, 1.15f, formGain1);
    formantFiltersLeft[1].coefficients  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, f2, 1.05f, formGain2);
    formantFiltersLeft[2].coefficients  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, f3, 0.90f, formGain3);
    formantFiltersRight[0].coefficients = formantFiltersLeft[0].coefficients;
    formantFiltersRight[1].coefficients = formantFiltersLeft[1].coefficients;
    formantFiltersRight[2].coefficients = formantFiltersLeft[2].coefficients;

    const float robotAmount = mode.robot * juce::jlimit (0.0f, 1.0f, 0.35f + morphNorm * 0.65f);
    const float robotCarrierHz = 95.0f + textureNorm * 210.0f + juce::jmax (0.0f, formNorm) * 130.0f;
    const float robotPhaseStep = juce::MathConstants<float>::twoPi * robotCarrierHz / static_cast<float> (currentSampleRate);
    const float robotQuantSteps = juce::jmap (textureNorm, 7.0f, 30.0f);
    const float robotSample = juce::jmax (1.0f, static_cast<float> (currentSampleRate * (0.0012f - 0.0008f * textureNorm)));
    const float morphSigned = juce::jlimit (-1.0f, 1.0f, (morph - 5.0f) / 5.0f);
    const float morphAmtRaw = std::abs (morphSigned);
    // Keep the center region truly neutral so tiny host jitter does not create hiss/distortion.
    const float morphAmt = juce::jlimit (0.0f, 1.0f, juce::jmax (0.0f, morphAmtRaw - 0.06f) / 0.94f);
    const float textureSigned = juce::jlimit (-1.0f, 1.0f, (texture - 4.0f) / 6.0f);
    const float textureAmt = std::abs (textureSigned);
    const float formSigned = juce::jlimit (-1.0f, 1.0f, (form - 5.5f) / 4.5f);
    const float formAmt = std::abs (formSigned);
    const float airAmt = juce::jlimit (0.0f, 1.0f, (air - 5.0f) / 5.0f);

    const bool controlsNeutral = (std::abs (pitch) < 0.05f)
                              && (std::abs (morph - 5.0f) < 0.30f)
                              && (std::abs (texture - 4.0f) < 0.18f)
                              && (std::abs (form - 5.5f) < 0.18f)
                              && (std::abs (air - 5.0f) < 0.18f);

    int robotCounter = static_cast<int> (pitchDownPhase * robotSample);
    float heldRobotL = 0.0f;
    float heldRobotR = 0.0f;

    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        const float rawInL = dryLeft[s];
        const float rawInR = dryRight != nullptr ? dryRight[s] : rawInL;

        if (controlsNeutral)
        {
            // True neutral path: bypass all DSP and keep unity signal at baseline.
            const float outLNeutral = rawInL;
            const float outRNeutral = rawInR;

            leftChannel[s] = outLNeutral;
            if (rightChannel != nullptr)
                rightChannel[s] = outRNeutral;

            inputPrevLeft = rawInL;
            inputPrevRight = rawInR;
            inputDcStateLeft = 0.0f;
            inputDcStateRight = 0.0f;
            previousInputLeft = rawInL;
            previousInputRight = rawInR;
            previousWetLeft = rawInL;
            previousWetRight = rawInR;

            pitchDelayLeft[static_cast<size_t> (pitchWriteIndex)] = rawInL;
            pitchDelayRight[static_cast<size_t> (pitchWriteIndex)] = rawInR;
            ++pitchWriteIndex;
            if (pitchWriteIndex >= pitchDelaySize)
                pitchWriteIndex = 0;
            pitchReadPos = static_cast<float> (pitchWriteIndex - juce::jlimit (256, pitchDelaySize / 2, static_cast<int> (currentSampleRate * 0.018)));
            while (pitchReadPos < 0.0f)
                pitchReadPos += static_cast<float> (pitchDelaySize);

            blockPeak = juce::jmax (blockPeak, std::abs (outLNeutral), std::abs (outRNeutral));
            continue;
        }

        // Simplified guaranteed-audible path for each control to avoid knob collapse into one tone.
        {
        float inL = rawInL;
        float inR = rawInR;
        float wetL = inL;
        float wetR = inR;

        pitchDelayLeft[static_cast<size_t> (pitchWriteIndex)] = inL;
        pitchDelayRight[static_cast<size_t> (pitchWriteIndex)] = inR;

        if (std::abs (pitch) > 0.1f)
        {
            const float ratio = std::pow (2.0f, pitch / 12.0f);

            const int i0 = static_cast<int> (pitchReadPos);
            const int i1 = (i0 + 1) % pitchDelaySize;
            const float frac = pitchReadPos - static_cast<float> (i0);

            const float s0L = pitchDelayLeft[static_cast<size_t> (i0)];
            const float s1L = pitchDelayLeft[static_cast<size_t> (i1)];
            const float s0R = pitchDelayRight[static_cast<size_t> (i0)];
            const float s1R = pitchDelayRight[static_cast<size_t> (i1)];

            const float shiftedL = s0L + frac * (s1L - s0L);
            const float shiftedR = s0R + frac * (s1R - s0R);

            const float pitchMix = juce::jlimit (0.0f, 1.0f, 0.25f + 0.75f * pitchAmountNorm);
            wetL = juce::jmap (pitchMix, wetL, shiftedL);
            wetR = juce::jmap (pitchMix, wetR, shiftedR);

            pitchReadPos += ratio;
        }

        ++pitchWriteIndex;
        if (pitchWriteIndex >= pitchDelaySize)
            pitchWriteIndex = 0;

        const int targetDelay = juce::jlimit (96, pitchDelaySize / 3, static_cast<int> (currentSampleRate * 0.008));
        float distance = static_cast<float> (pitchWriteIndex) - pitchReadPos;
        if (distance < 0.0f)
            distance += static_cast<float> (pitchDelaySize);
        if (std::abs (distance - static_cast<float> (targetDelay)) > 64.0f)
            pitchReadPos = static_cast<float> (pitchWriteIndex - targetDelay);
        while (pitchReadPos < 0.0f)
            pitchReadPos += static_cast<float> (pitchDelaySize);
        while (pitchReadPos >= static_cast<float> (pitchDelaySize))
            pitchReadPos -= static_cast<float> (pitchDelaySize);

        if (formAmt > 0.01f)
        {
            float formantL = formantFiltersLeft[0].processSample (wetL);
            formantL = formantFiltersLeft[1].processSample (formantL);
            formantL = formantFiltersLeft[2].processSample (formantL);
            float formantR = formantFiltersRight[0].processSample (wetR);
            formantR = formantFiltersRight[1].processSample (formantR);
            formantR = formantFiltersRight[2].processSample (formantR);

            const float shiftedFormL = formSigned >= 0.0f ? std::tanh (formantL * (1.0f + 1.2f * formAmt))
                                                          : 0.75f * formantL + 0.25f * wetL;
            const float shiftedFormR = formSigned >= 0.0f ? std::tanh (formantR * (1.0f + 1.2f * formAmt))
                                                          : 0.75f * formantR + 0.25f * wetR;
            const float formMix = juce::jlimit (0.0f, 1.0f, 0.24f + 0.70f * formAmt);
            wetL = juce::jmap (formMix, wetL, shiftedFormL);
            wetR = juce::jmap (formMix, wetR, shiftedFormR);
        }

        if (morphAmt > 0.001f)
        {
            const float morphMix = juce::jlimit (0.0f, 1.0f, 0.10f + 0.62f * morphAmt);
            const float drive = 1.0f + 1.85f * morphAmt;

            float shapedL = wetL;
            float shapedR = wetR;

            if (morphSigned >= 0.0f)
            {
                const float satL = std::tanh (wetL * drive);
                const float satR = std::tanh (wetR * drive);
                const float edgeL = satL - std::tanh (satL * 0.74f);
                const float edgeR = satR - std::tanh (satR * 0.74f);
                shapedL = satL + edgeL * (0.12f + 0.30f * morphAmt);
                shapedR = satR + edgeR * (0.12f + 0.30f * morphAmt);
            }
            else
            {
                const float softDrive = juce::jmax (0.45f, 1.0f - 0.42f * morphAmt);
                const float softL = std::tanh (wetL * softDrive);
                const float softR = std::tanh (wetR * softDrive);
                shapedL = 0.80f * softL + 0.20f * wetL;
                shapedR = 0.80f * softR + 0.20f * wetR;
            }

            const float morphComp = 1.0f / (1.0f + 0.26f * morphAmt);
            shapedL *= morphComp;
            shapedR *= morphComp;
            wetL = juce::jmap (morphMix, wetL, shapedL);
            wetR = juce::jmap (morphMix, wetR, shapedR);
        }

        if (textureAmt > 0.01f)
        {
            const float warmL = std::tanh (wetL * (1.0f + 1.8f * textureAmt));
            const float warmR = std::tanh (wetR * (1.0f + 1.8f * textureAmt));
            const float gritL = std::sin (wetL * (1.0f + 4.6f * textureAmt));
            const float gritR = std::sin (wetR * (1.0f + 4.6f * textureAmt));
            const float steps = juce::jmap (textureAmt, 20.0f, 6.0f);
            const float digitalL = std::round (gritL * steps) / steps;
            const float digitalR = std::round (gritR * steps) / steps;
            const float softenedL = 0.82f * wetL + 0.18f * previousWetLeft;
            const float softenedR = 0.82f * wetR + 0.18f * previousWetRight;

            float texL = textureSigned >= 0.0f ? juce::jmap (juce::jlimit (0.0f, 1.0f, textureAmt * 1.10f), warmL, gritL)
                                                : softenedL;
            float texR = textureSigned >= 0.0f ? juce::jmap (juce::jlimit (0.0f, 1.0f, textureAmt * 1.10f), warmR, gritR)
                                                : softenedR;

            if (textureSigned >= 0.0f)
            {
                const float gritToDigital = juce::jlimit (0.0f, 1.0f, juce::jmax (0.0f, textureAmt - 0.50f) * 2.0f);
                texL = juce::jmap (gritToDigital, texL, digitalL);
                texR = juce::jmap (gritToDigital, texR, digitalR);
            }

            const float texMix = juce::jlimit (0.0f, 1.0f, 0.16f + 0.74f * textureAmt);
            wetL = juce::jmap (texMix, wetL, texL);
            wetR = juce::jmap (texMix, wetR, texR);
        }

        if (airAmt > 0.01f)
        {
            const float smooth = std::exp (-juce::MathConstants<float>::twoPi * 4200.0f / static_cast<float> (currentSampleRate));
            airSmoothLeft = smooth * airSmoothLeft + (1.0f - smooth) * wetL;
            airSmoothRight = smooth * airSmoothRight + (1.0f - smooth) * wetR;
            const float airBandL = wetL - airSmoothLeft;
            const float airBandR = wetR - airSmoothRight;
            const float exciteL = std::tanh (airBandL * (1.6f + 5.0f * airAmt));
            const float exciteR = std::tanh (airBandR * (1.6f + 5.0f * airAmt));
            wetL += exciteL * (0.10f + 0.30f * airAmt);
            wetR += exciteR * (0.10f + 0.30f * airAmt);
        }

        const bool effectActiveSimple = (pitchAmountNorm > 0.01f)
                                      || (morphAmt > 0.01f)
                                      || (textureAmt > 0.01f)
                                      || (formAmt > 0.01f)
                                      || (airAmt > 0.01f);

        if (effectActiveSimple)
        {
            const float wetComp = 1.0f / (1.0f + 0.16f * textureAmt + 0.10f * morphAmt + 0.01f * std::abs (pitch));
            wetL *= wetComp;
            wetR *= wetComp;
        }

        const float dryGain = std::cos (blendNorm * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin (blendNorm * juce::MathConstants<float>::halfPi);
        const float outL = rawInL * dryGain + wetL * wetGain;
        const float outR = rawInR * dryGain + wetR * wetGain;

        leftChannel[s] = outL;
        if (rightChannel != nullptr)
            rightChannel[s] = outR;

        previousWetLeft = wetL;
        previousWetRight = wetR;
        previousInputLeft = rawInL;
        previousInputRight = rawInR;
        blockPeak = juce::jmax (blockPeak, std::abs (outL), std::abs (outR));
        continue;
        }

        const float dcL = (rawInL - inputPrevLeft) + 0.9995f * inputDcStateLeft;
        const float dcR = (rawInR - inputPrevRight) + 0.9995f * inputDcStateRight;
        const float inL = 0.88f * rawInL + 0.12f * dcL;
        const float inR = 0.88f * rawInR + 0.12f * dcR;
        inputPrevLeft = rawInL;
        inputPrevRight = rawInR;
        inputDcStateLeft = dcL;
        inputDcStateRight = dcR;

        float wetL = inL;
        float wetR = inR;

        if ((inL >= 0.0f) != (previousInputLeft >= 0.0f))
            subOctavePolarityLeft = !subOctavePolarityLeft;
        if ((inR >= 0.0f) != (previousInputRight >= 0.0f))
            subOctavePolarityRight = !subOctavePolarityRight;

        // ===== PITCH STAGE =====
        // Real pitch shift via circular-buffer resampling.
        pitchDelayLeft[static_cast<size_t> (pitchWriteIndex)] = inL;
        pitchDelayRight[static_cast<size_t> (pitchWriteIndex)] = inR;

        if (std::abs (pitch) > 0.1f)
        {
            const float ratio = std::pow (2.0f, pitch / 12.0f);
            const auto readInterp = [this] (const std::vector<float>& delay, float readPos) -> float
            {
                const int i0 = static_cast<int> (readPos);
                const int i1 = (i0 + 1) % pitchDelaySize;
                const float frac = readPos - static_cast<float> (i0);
                const float s0 = delay[static_cast<size_t> (i0)];
                const float s1 = delay[static_cast<size_t> (i1)];
                return s0 + frac * (s1 - s0);
            };

            const float shiftedL = readInterp (pitchDelayLeft, pitchReadPos);
            const float shiftedR = readInterp (pitchDelayRight, pitchReadPos);
            const float pitchMix = juce::jlimit (0.0f, 1.0f, 0.18f + 0.82f * pitchAmountNorm);

            wetL = juce::jmap (pitchMix, wetL, 0.85f * shiftedL + 0.15f * inL);
            wetR = juce::jmap (pitchMix, wetR, 0.85f * shiftedR + 0.15f * inR);

            pitchReadPos += ratio;
        }

        ++pitchWriteIndex;
        if (pitchWriteIndex >= pitchDelaySize)
            pitchWriteIndex = 0;

        const int targetDelay = juce::jlimit (256, pitchDelaySize / 2, static_cast<int> (currentSampleRate * 0.018));
        float distance = static_cast<float> (pitchWriteIndex) - pitchReadPos;
        if (distance < 0.0f)
            distance += static_cast<float> (pitchDelaySize);

        if (distance < 96.0f || distance > static_cast<float> (pitchDelaySize - 256))
            pitchReadPos = static_cast<float> (pitchWriteIndex - targetDelay);

        while (pitchReadPos < 0.0f)
            pitchReadPos += static_cast<float> (pitchDelaySize);
        while (pitchReadPos >= static_cast<float> (pitchDelaySize))
            pitchReadPos -= static_cast<float> (pitchDelaySize);

        previousInputLeft = inL;
        previousInputRight = inR;

        // ===== REDUCTION FILTER BANK =====
        // Only apply if morph is meaningfully beyond neutral (very low threshold so effects activate immediately)
        if (morphNorm > 0.002f)
        {
            for (size_t i = 0; i < smoothingBandCount; ++i)
            {
                wetL = leftReductionFilters[i].processSample (wetL);
                wetR = rightReductionFilters[i].processSample (wetR);
            }
            // Keep the reduction stage characterful, but avoid collapsing every preset into the same harsh tone.
            wetL = juce::jmap (reductionAmount, inL, wetL);
            wetR = juce::jmap (reductionAmount, inR, wetR);
        }

        const float lowL = 0.93f * previousWetLeft + 0.07f * wetL;
        const float lowR = 0.93f * previousWetRight + 0.07f * wetR;
        const float highL = wetL - lowL;
        const float highR = wetR - lowR;
        const float satL = std::tanh (highL * textureDrive);
        const float satR = std::tanh (highR * textureDrive);
        // Only apply texture saturation if texture is meaningfully beyond neutral
        if (textureNorm > 0.002f)
        {
            wetL = lowL + juce::jmap (textureMix, highL, satL);
            wetR = lowR + juce::jmap (textureMix, highR, satR);
        }

        // ===== FORMANT / IDENTITY STAGE =====
        // Formant filters are updated with neutral gains when formNorm < 0.002f, so they don't affect the signal at baseline
        float formantL = formantFiltersLeft[0].processSample (wetL);
        formantL = formantFiltersLeft[1].processSample (formantL);
        formantL = formantFiltersLeft[2].processSample (formantL);
        float formantR = formantFiltersRight[0].processSample (wetR);
        formantR = formantFiltersRight[1].processSample (formantR);
        formantR = formantFiltersRight[2].processSample (formantR);

        // Only apply formant morphing if form is meaningfully beyond neutral
        if (std::abs (formNorm) > 0.002f)
        {
            const float throatL = std::tanh ((formantL + formBias * (0.10f + 0.10f * formNorm)) * (1.0f + formNorm * 0.45f));
            const float throatR = std::tanh ((formantR + formBias * (0.10f + 0.10f * formNorm)) * (1.0f + formNorm * 0.45f));
            const float formantMix = juce::jlimit (0.0f, 1.0f, std::abs (formNorm) * 0.52f);
            wetL = juce::jmap (formantMix, wetL, throatL);
            wetR = juce::jmap (formantMix, wetR, throatR);
        }

        // ===== MORPH STAGE =====
        // Only apply morphing effects if morph is meaningfully beyond neutral
        if (morphNorm > 0.002f)
        {
            const float rectL = (2.0f * std::abs (wetL) - 1.0f);
            const float rectR = (2.0f * std::abs (wetR) - 1.0f);
            const float asymL = std::tanh ((wetL + rectL * formBias * morphSign) * morphDrive);
            const float asymR = std::tanh ((wetR + rectR * formBias * morphSign) * morphDrive);
            const float foldL = std::sin (wetL * (1.0f + 1.9f * morphCharacter));
            const float foldR = std::sin (wetR * (1.0f + 1.9f * morphCharacter));
            const float morphedL = juce::jmap (foldMix, asymL, foldL);
            const float morphedR = juce::jmap (foldMix, asymR, foldR);

            const float env = 0.5f * (std::abs (inL) + std::abs (inR));
            const float dynamicMorph = juce::jlimit (0.0f, 1.0f, 0.06f + morphCharacter * (0.52f + env * 0.55f));
            wetL = juce::jmap (dynamicMorph, wetL, morphedL);
            wetR = juce::jmap (dynamicMorph, wetR, morphedR);

            // Octave-up via full-wave shaping and sub-octave via divide-by-two style polarity toggling.
            const float octaveUpL = std::tanh ((2.0f * std::abs (wetL) - 0.95f) * (0.85f + 1.55f * octaveLayer));
            const float octaveUpR = std::tanh ((2.0f * std::abs (wetR) - 0.95f) * (0.85f + 1.55f * octaveLayer));

            const float pitchedL = wetL;
            const float pitchedR = wetR;

            const float subRawL = subOctavePolarityLeft ? std::abs (pitchedL) : -std::abs (pitchedL);
            const float subRawR = subOctavePolarityRight ? std::abs (pitchedR) : -std::abs (pitchedR);
            const float subOctL = std::tanh (subRawL * (0.95f + 0.95f * octaveLayer));
            const float subOctR = std::tanh (subRawR * (0.95f + 0.95f * octaveLayer));

            wetL += highVoiceMix * octaveUpL + lowVoiceMix * subOctL;
            wetR += highVoiceMix * octaveUpR + lowVoiceMix * subOctR;
        }

        // ===== HARMONIC / TEXTURE STAGE =====
        // Only apply harmonic distortions if texture is meaningfully beyond neutral
        if (textureNorm > 0.002f)
        {
            const float preTextureL = wetL;
            const float preTextureR = wetR;
            const float warmL = std::tanh (wetL * (1.0f + textureNorm * 1.35f * mode.warmWeight));
            const float warmR = std::tanh (wetR * (1.0f + textureNorm * 1.35f * mode.warmWeight));
            const float gritL = std::sin (wetL * (1.0f + textureNorm * 3.1f * mode.gritWeight));
            const float gritR = std::sin (wetR * (1.0f + textureNorm * 3.1f * mode.gritWeight));
            const float bits = juce::jmap (textureNorm, 10.0f, 26.0f);
            const float digitalL = std::round (gritL * bits * mode.digitalWeight) / (bits * mode.digitalWeight + 1.0e-5f);
            const float digitalR = std::round (gritR * bits * mode.digitalWeight) / (bits * mode.digitalWeight + 1.0e-5f);
            const float harmonicMix = juce::jlimit (0.0f, 1.0f, 0.08f + textureNorm * (0.45f + 0.22f * mode.texture));
            const float digitalMix = juce::jlimit (0.0f, 1.0f, juce::jmax (0.0f, textureNorm - 0.45f) * 1.10f);
            float harmonicL = juce::jmap (harmonicMix, wetL, juce::jmap (0.5f, warmL, gritL));
            float harmonicR = juce::jmap (harmonicMix, wetR, juce::jmap (0.5f, warmR, gritR));
            harmonicL = juce::jmap (digitalMix, harmonicL, digitalL);
            harmonicR = juce::jmap (digitalMix, harmonicR, digitalR);

            // Prevent MORPH+TEXTURE+FORM stacks from collapsing into one generic distortion tone.
            const float comboDensity = morphNorm * textureNorm * (0.55f + 0.45f * std::abs (formNorm));
            const float antiCollapseMix = juce::jlimit (0.0f, 0.46f, comboDensity * 0.46f);
            wetL = juce::jmap (antiCollapseMix, harmonicL, 0.62f * harmonicL + 0.38f * preTextureL);
            wetR = juce::jmap (antiCollapseMix, harmonicR, 0.62f * harmonicR + 0.38f * preTextureR);
        }

        if (robotAmount > 0.0f)
        {
            const float inMono = 0.5f * (inL + inR);
            robotEnvelope = 0.994f * robotEnvelope + 0.006f * std::abs (inMono);

            const float carrier = std::sin (robotCarrierPhase);
            const float quantL = std::round (wetL * robotQuantSteps) / robotQuantSteps;
            const float quantR = std::round (wetR * robotQuantSteps) / robotQuantSteps;

            ++robotCounter;
            if (robotCounter >= static_cast<int> (robotSample))
            {
                robotCounter = 0;
                heldRobotL = quantL;
                heldRobotR = quantR;
            }

            const float robotL = std::tanh ((heldRobotL * 0.65f + quantL * 0.35f) * (0.8f + robotEnvelope * 7.0f) * carrier);
            const float robotR = std::tanh ((heldRobotR * 0.65f + quantR * 0.35f) * (0.8f + robotEnvelope * 7.0f) * carrier);

            wetL = juce::jmap (robotAmount, wetL, robotL);
            wetR = juce::jmap (robotAmount, wetR, robotR);

            robotCarrierPhase += robotPhaseStep;
            if (robotCarrierPhase > juce::MathConstants<float>::twoPi)
                robotCarrierPhase -= juce::MathConstants<float>::twoPi;
        }

        // ===== AIR & MOTION ENHANCEMENT =====
        // Only apply air/motion effects if air parameter is meaningfully beyond neutral
        if (airNorm > 0.002f || motionDepth > 0.001f)
        {
            airSmoothLeft = airSmoothCoeff * airSmoothLeft + (1.0f - airSmoothCoeff) * wetL;
            airSmoothRight = airSmoothCoeff * airSmoothRight + (1.0f - airSmoothCoeff) * wetR;
            const float airBandL = wetL - airSmoothLeft;
            const float airBandR = wetR - airSmoothRight;
            const float airExciteL = std::tanh (airBandL * (1.3f + airNorm * 6.5f));
            const float airExciteR = std::tanh (airBandR * (1.3f + airNorm * 6.5f));
            if (airNorm > 0.06f)
            {
                wetL += airExciteMix * airExciteL;
                wetR += airExciteMix * airExciteR;
            }

            const float polishedL = airSmoothLeft + airBandL * 0.62f;
            const float polishedR = airSmoothRight + airBandR * 0.62f;
            if (airNorm > 0.06f)
            {
                wetL = juce::jmap (airPolishMix, wetL, polishedL);
                wetR = juce::jmap (airPolishMix, wetR, polishedR);
            }

            const float diffL = wetL - previousWetLeft;
            const float diffR = wetR - previousWetRight;
            previousWetLeft = wetL;
            previousWetRight = wetR;

            if (airNorm > 0.06f)
            {
                wetL += diffL * airEnhance;
                wetR += diffR * airEnhance;
            }

            if (motionDepth > 0.01f)
            {
                const float motion = std::sin (modulationPhase);
                modulationPhase += phaseStep;
                if (modulationPhase > juce::MathConstants<float>::twoPi)
                    modulationPhase -= juce::MathConstants<float>::twoPi;

                wetL *= (1.0f + motionDepth * motion);
                wetR *= (1.0f - motionDepth * motion);
            }
        }
        else
        {
            previousWetLeft = wetL;
            previousWetRight = wetR;
        }

        // Loudness compensation so creative transformation is heard as character, not just level increase.
        const bool effectActive = (pitchAmountNorm > 0.01f)
                               || (morphNorm > 0.002f)
                               || (textureNorm > 0.002f)
                               || (formNorm > 0.002f)
                               || (airNorm > 0.002f);

        if (effectActive)
        {
            const float wetComp = 1.0f / (1.0f + textureNorm * 0.24f + morphNorm * 0.20f + std::abs (pitch) * 0.015f);
            wetL *= wetComp;
            wetR *= wetComp;

            // Soft output protection keeps aggressive combinations controlled.
            wetL = std::tanh (wetL * 1.03f) * 0.98f;
            wetR = std::tanh (wetR * 1.03f) * 0.98f;
        }

        const float dryGain = std::cos (blendNorm * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin (blendNorm * juce::MathConstants<float>::halfPi);

        const float outL = dryLeft[s] * dryGain + wetL * wetGain;
        const float dryRs = dryRight != nullptr ? dryRight[s] : dryLeft[s];
        const float outR = dryRs * dryGain + wetR * wetGain;

        leftChannel[s] = outL;
        if (rightChannel != nullptr)
            rightChannel[s] = outR;

        blockPeak = juce::jmax (blockPeak, std::abs (outL), std::abs (outR));
    }

    pitchDownPhase = static_cast<float> (robotCounter) / robotSample;

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
