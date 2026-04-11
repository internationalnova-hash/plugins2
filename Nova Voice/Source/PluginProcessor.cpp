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
    subOctavePolarityLeft = false;
    subOctavePolarityRight = false;
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
    const float textureDrive = 1.0f + textureNorm * 4.8f * mode.texture;
    const float textureMix = juce::jlimit (0.14f, 0.92f, 0.22f + textureNorm * 0.62f * mode.texture);
    const float airEnhance = juce::jlimit (0.00f, 0.22f, 0.03f + airNorm * 0.16f * mode.brightness);
    const float motionDepth = juce::jlimit (0.0f, 0.22f, 0.02f + morphNorm * 0.10f * mode.motion + textureNorm * 0.06f);
    const float phaseStep = juce::MathConstants<float>::twoPi * (0.22f + 0.35f * mode.motion) / static_cast<float> (currentSampleRate);
    const float morphCharacter = juce::jlimit (0.0f, 1.0f, morphNorm * (0.55f + 0.55f * mode.morph));
    const float formBias = juce::jlimit (-0.75f, 0.75f, formNorm * 0.70f);
    const float morphDrive = 1.0f + 6.2f * morphCharacter;
    const float foldMix = juce::jlimit (0.20f, 0.86f, 0.32f + textureNorm * 0.34f + morphCharacter * 0.26f);

    // Dedicated voice-transform layer for obvious creative morphing.
    const float octaveLayer = juce::jlimit (0.0f, 1.0f, morphCharacter * (0.65f + 0.35f * mode.morph));
    const float highVoiceMix = 0.10f + octaveLayer * 0.65f;
    const float lowVoiceMix = 0.08f + octaveLayer * 0.58f;

    const float formantShift = std::pow (2.0f, formNorm * (0.55f + 0.20f * mode.morph));
    const float f1 = juce::jlimit (220.0f, 2200.0f, 520.0f * formantShift);
    const float f2 = juce::jlimit (700.0f, 4200.0f, 1450.0f * formantShift);
    const float f3 = juce::jlimit (1300.0f, 7000.0f, 2850.0f * formantShift);
    const float formGain1 = juce::Decibels::decibelsToGain (juce::jmap (formNorm, -6.0f, 7.0f));
    const float formGain2 = juce::Decibels::decibelsToGain (juce::jmap (formNorm, -4.0f, 8.0f));
    const float formGain3 = juce::Decibels::decibelsToGain (juce::jmap (formNorm, -2.5f, 5.0f));

    formantFiltersLeft[0].coefficients  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, f1, 1.15f, formGain1);
    formantFiltersLeft[1].coefficients  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, f2, 1.05f, formGain2);
    formantFiltersLeft[2].coefficients  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, f3, 0.90f, formGain3);
    formantFiltersRight[0].coefficients = formantFiltersLeft[0].coefficients;
    formantFiltersRight[1].coefficients = formantFiltersLeft[1].coefficients;
    formantFiltersRight[2].coefficients = formantFiltersLeft[2].coefficients;

    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        const float inL = dryLeft[s];
        const float inR = dryRight != nullptr ? dryRight[s] : dryLeft[s];
        float wetL = inL;
        float wetR = inR;

        for (size_t i = 0; i < smoothingBandCount; ++i)
        {
            wetL = leftReductionFilters[i].processSample (wetL);
            wetR = rightReductionFilters[i].processSample (wetR);
        }

        const float satL = std::tanh (wetL * textureDrive);
        const float satR = std::tanh (wetR * textureDrive);
        wetL = juce::jmap (textureMix, wetL, satL);
        wetR = juce::jmap (textureMix, wetR, satR);

        // Stronger vocal morph stage: asymmetric shaping + fold blend for audible timbre transformation.
        const float rectL = (2.0f * std::abs (wetL) - 1.0f);
        const float rectR = (2.0f * std::abs (wetR) - 1.0f);
        const float asymL = std::tanh ((wetL + rectL * formBias) * morphDrive);
        const float asymR = std::tanh ((wetR + rectR * formBias) * morphDrive);
        const float foldL = std::sin (wetL * (1.0f + 3.8f * morphCharacter));
        const float foldR = std::sin (wetR * (1.0f + 3.8f * morphCharacter));
        const float morphedL = juce::jmap (foldMix, asymL, foldL);
        const float morphedR = juce::jmap (foldMix, asymR, foldR);
        wetL = juce::jmap (morphCharacter, wetL, morphedL);
        wetR = juce::jmap (morphCharacter, wetR, morphedR);

        // Octave-up via full-wave shaping and sub-octave via divide-by-two style polarity toggling.
        const float octaveUpL = std::tanh ((2.0f * std::abs (inL) - 0.70f) * (1.4f + 2.0f * octaveLayer));
        const float octaveUpR = std::tanh ((2.0f * std::abs (inR) - 0.70f) * (1.4f + 2.0f * octaveLayer));

        if ((inL >= 0.0f) != (previousInputLeft >= 0.0f))
            subOctavePolarityLeft = !subOctavePolarityLeft;
        if ((inR >= 0.0f) != (previousInputRight >= 0.0f))
            subOctavePolarityRight = !subOctavePolarityRight;

        const float subRawL = subOctavePolarityLeft ? std::abs (inL) : -std::abs (inL);
        const float subRawR = subOctavePolarityRight ? std::abs (inR) : -std::abs (inR);
        const float subOctL = std::tanh (subRawL * (1.8f + 1.5f * octaveLayer));
        const float subOctR = std::tanh (subRawR * (1.8f + 1.5f * octaveLayer));

        previousInputLeft = inL;
        previousInputRight = inR;

        wetL += highVoiceMix * octaveUpL + lowVoiceMix * subOctL;
        wetR += highVoiceMix * octaveUpR + lowVoiceMix * subOctR;

        // Formant emphasis after octave layering to make voice identity shifts obvious.
        wetL = formantFiltersLeft[0].processSample (wetL);
        wetL = formantFiltersLeft[1].processSample (wetL);
        wetL = formantFiltersLeft[2].processSample (wetL);
        wetR = formantFiltersRight[0].processSample (wetR);
        wetR = formantFiltersRight[1].processSample (wetR);
        wetR = formantFiltersRight[2].processSample (wetR);

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
