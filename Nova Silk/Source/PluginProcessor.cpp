#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    constexpr auto smoothId = "smooth";
    constexpr auto focusId = "focus";
    constexpr auto airPreserveId = "air_preserve";
    constexpr auto bodyId = "body";
    constexpr auto outputId = "output";
    constexpr auto magicId = "magic";

    constexpr float qSqrtHalf = 0.70710678f;

    // Frequency boundaries for the five behaviour zones
    constexpr float zone2Hz = 120.0f;    // Zone 1 ends / Zone 2 starts
    constexpr float zone3Hz = 700.0f;    // Zone 2 ends / Zone 3 starts
    constexpr float zone4Hz = 2500.0f;   // Zone 3 ends / Zone 4 starts (MAIN)
    constexpr float zone5Hz = 6000.0f;   // Zone 4 ends / Zone 5 starts (Air)

    float clampUnit (float value) noexcept
    {
        return juce::jlimit (0.0f, 1.0f, value);
    }

    float lerp (float start, float end, float amount) noexcept
    {
        return start + ((end - start) * amount);
    }
}

NovaSilkAudioProcessor::NovaSilkAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts (*this, nullptr, juce::Identifier ("NovaSilk"), createParameterLayout())
{
    initialiseBandLayout();

    for (auto& bucket : inputSpectrum)
        bucket.store (0.0f);

    for (auto& bucket : problemSpectrum)
        bucket.store (0.0f);

    for (auto& bucket : reductionSpectrum)
        bucket.store (0.0f);
}

NovaSilkAudioProcessor::~NovaSilkAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaSilkAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { smoothId, 1 },
        "Smooth",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        4.2f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { focusId, 1 },
        "Focus",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        6.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { airPreserveId, 1 },
        "Air Preserve",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        7.2f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { bodyId, 1 },
        "Body",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        6.2f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputId, 1 },
        "Output",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " dB"; }));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { magicId, 1 },
        "Magic",
        true));

    return layout;
}

void NovaSilkAudioProcessor::initialiseBandLayout()
{
    // 24 bands logarithmically spaced from 30 Hz to 18 kHz
    constexpr float logMinF = 4.9068905963f;   // log2 (30)
    constexpr float logMaxF = 14.1292830167f;  // log2 (18000)

    for (size_t i = 0; i < smoothingBandCount; ++i)
    {
        const float t    = static_cast<float> (i) / static_cast<float> (smoothingBandCount - 1);
        const float freq = std::pow (2.0f, logMinF + t * (logMaxF - logMinF));

        bandFrequencies[i] = freq;
        bandQValues[i]     = 1.2f;   // broad, musical Q — no surgical cuts

        // Zone 1 (20–120 Hz)  — low body, very gentle
        if (freq < zone2Hz)
        {
            bandMaxReductionDb[i] = 1.5f;
            bandAttackMs[i]       = 25.0f;
            bandReleaseMs[i]      = 215.0f;
        }
        // Zone 2 (120–700 Hz) — low-mid body, body-controlled
        else if (freq < zone3Hz)
        {
            bandMaxReductionDb[i] = 2.5f;
            bandAttackMs[i]       = 20.0f;
            bandReleaseMs[i]      = 185.0f;
        }
        // Zone 3 (700–2500 Hz) — mid boxiness, moderate
        else if (freq < zone4Hz)
        {
            bandMaxReductionDb[i] = 5.0f;
            bandAttackMs[i]       = 14.0f;
            bandReleaseMs[i]      = 150.0f;
        }
        // Zone 4 (2.5–6 kHz)  — presence / harshness, MAIN target
        else if (freq < zone5Hz)
        {
            bandMaxReductionDb[i] = 8.0f;
            bandAttackMs[i]       = 7.0f;
            bandReleaseMs[i]      = 110.0f;
        }
        // Zone 5 (6–18 kHz)   — air / top end, delicate
        else
        {
            bandMaxReductionDb[i] = 3.5f;
            bandAttackMs[i]       = 5.0f;
            bandReleaseMs[i]      = 90.0f;
        }
    }
}

const juce::String NovaSilkAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NovaSilkAudioProcessor::acceptsMidi() const  { return false; }
bool NovaSilkAudioProcessor::producesMidi() const { return false; }
bool NovaSilkAudioProcessor::isMidiEffect() const { return false; }

double NovaSilkAudioProcessor::getTailLengthSeconds() const
{
    return 0.15;
}

int NovaSilkAudioProcessor::getNumPrograms()                { return 1; }
int NovaSilkAudioProcessor::getCurrentProgram()            { return 0; }
void NovaSilkAudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }

const juce::String NovaSilkAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void NovaSilkAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void NovaSilkAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    dryBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock);

    analyzerFifoIndex = 0;
    nextFFTBlockReady = false;
    std::fill (analyzerFifo.begin(), analyzerFifo.end(), 0.0f);
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::fill (bandEnvelopes.begin(), bandEnvelopes.end(), 0.0f);
    std::fill (lastBandReductionDb.begin(), lastBandReductionDb.end(), 0.0f);

    for (size_t index = 0; index < smoothingBandCount; ++index)
    {
        const float sr = static_cast<float> (sampleRate);
        bandAttackCoeff[index]  = std::exp (-1.0f / (0.001f * bandAttackMs[index]  * sr));
        bandReleaseCoeff[index] = std::exp (-1.0f / (0.001f * bandReleaseMs[index] * sr));

        juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 1 };

        leftProbeFilters[index].coefficients  = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, bandFrequencies[index], qSqrtHalf);
        rightProbeFilters[index].coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, bandFrequencies[index], qSqrtHalf);
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
}

void NovaSilkAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaSilkAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}
#endif

void NovaSilkAudioProcessor::pushNextSampleIntoAnalyzer (float sample) noexcept
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

void NovaSilkAudioProcessor::refreshAnalyzerData()
{
    const float smoothNorm  = apvts.getRawParameterValue (smoothId)->load() / 100.0f;
    const bool  magicEnabled = apvts.getRawParameterValue (magicId)->load() > 0.5f;

    // ── Input + problem spectra from FFT (only when a new block is available) ─
    if (nextFFTBlockReady)
    {
        nextFFTBlockReady = false;
        window.multiplyWithWindowingTable (fftData.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        for (int b = 0; b < spectrumBins; ++b)
        {
            const auto startNorm = static_cast<float> (b)     / static_cast<float> (spectrumBins);
            const auto endNorm   = static_cast<float> (b + 1) / static_cast<float> (spectrumBins);

            auto startBin = juce::jlimit (1, (fftSize / 2) - 2,
                                          static_cast<int> (std::pow (startNorm, 1.8f) * ((fftSize / 2) - 2)) + 1);
            auto endBin   = juce::jlimit (startBin + 1, (fftSize / 2) - 1,
                                          static_cast<int> (std::pow (endNorm, 1.8f) * ((fftSize / 2) - 1)) + 1);

            float peakValue   = 0.0f;
            float localAvg    = 0.0f;

            for (int bin = startBin; bin <= endBin; ++bin)
            {
                peakValue = juce::jmax (peakValue, fftData[bin]);
                localAvg += fftData[bin];
            }
            localAvg /= static_cast<float> ((endBin - startBin) + 1);

            const float inputNorm   = clampUnit ((juce::Decibels::gainToDecibels (peakValue + 1.0e-5f) + 78.0f) / 78.0f);
            // Problem: local peak excess — scaled by smooth and magic so the display
            // tracks the actual DSP activity level
            const float problemNorm = clampUnit ((peakValue - localAvg * 0.84f) * 7.0f)
                                    * smoothNorm
                                    * (magicEnabled ? 1.2f : 0.9f);

            inputSpectrum[static_cast<size_t> (b)].store   (inputNorm);
            problemSpectrum[static_cast<size_t> (b)].store (problemNorm);
        }
    }

    // ── Reduction spectrum from band data (always fresh each block) ───────────
    // Map each display bin to a display frequency and interpolate from processing bands
    for (int b = 0; b < spectrumBins; ++b)
    {
        const float tNorm     = static_cast<float> (b) / static_cast<float> (spectrumBins - 1);
        const float displayHz = 20.0f * std::pow (1000.0f, tNorm);  // 20 Hz → 20 kHz log

        float minLogDist   = std::numeric_limits<float>::max();
        float reductionVal = 0.0f;

        for (size_t bi = 0; bi < smoothingBandCount; ++bi)
        {
            const float dist = std::abs (std::log2 (displayHz / juce::jmax (1.0f, bandFrequencies[bi])));
            if (dist < minLogDist)
            {
                minLogDist   = dist;
                reductionVal = bandSmoothedReductionDb[bi];
            }
        }

        // Normalise: Zone-4 maximum (8 dB) maps to 1.0
        reductionSpectrum[static_cast<size_t> (b)].store (clampUnit (-reductionVal / 8.0f));
    }
}

void NovaSilkAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels  = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    dryBuffer.makeCopyOf (buffer, true);

    const float smoothNorm   = apvts.getRawParameterValue (smoothId)->load()      / 100.0f;
    const float focusNorm    = apvts.getRawParameterValue (focusId)->load()       / 100.0f;
    const float airNorm      = apvts.getRawParameterValue (airPreserveId)->load() / 100.0f;
    const float bodyNorm     = apvts.getRawParameterValue (bodyId)->load()        / 100.0f;
    const float outputLinear = juce::Decibels::decibelsToGain (apvts.getRawParameterValue (outputId)->load());
    const bool  magicEnabled = apvts.getRawParameterValue (magicId)->load() > 0.5f;

    const auto* dryLeft  = dryBuffer.getReadPointer (0);
    const auto* dryRight = dryBuffer.getNumChannels() > 1 ? dryBuffer.getReadPointer (1) : nullptr;

    // Focus centre frequency — log-mapped: 250 * 2^(focusNorm * 6)
    // focusNorm = 0.60 (default) → ~3024 Hz, squarely in Zone 4
    const float focusHz           = 250.0f * std::pow (2.0f, focusNorm * 6.0f);
    constexpr float focusWidthOct = 2.5f;   // Gaussian half-width in octaves

    // Detection sensitivity scales with Smooth so low settings are gentle
    const float detectorSens     = 0.4f + smoothNorm * 0.6f;
    constexpr float kThreshold   = 0.015f;  // ~-36 dBFS

    // ── Per-sample: probe bandpass filters + per-band envelope followers ──────
    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        const float left  = dryLeft[s];
        const float right = dryRight != nullptr ? dryRight[s] : left;

        pushNextSampleIntoAnalyzer (0.5f * (left + right));

        for (size_t i = 0; i < smoothingBandCount; ++i)
        {
            const float lp    = std::abs (leftProbeFilters[i].processSample  (left));
            const float rp    = std::abs (rightProbeFilters[i].processSample (right));
            const float level = juce::jmax (lp, rp);

            // Proper attack / release envelope — faster attack, slower release
            if (level > bandEnvelopes[i])
                bandEnvelopes[i] = bandAttackCoeff[i] * bandEnvelopes[i]
                                 + (1.0f - bandAttackCoeff[i]) * level;
            else
                bandEnvelopes[i] = bandReleaseCoeff[i] * bandEnvelopes[i];
        }
    }

    // ── Post-block: compute raw per-band reductions ──────────────────────────
    for (size_t i = 0; i < smoothingBandCount; ++i)
    {
        const float freq     = bandFrequencies[i];
        const float envelope = bandEnvelopes[i];

        // Harshness: how far above threshold the band is (shaped by detector sensitivity)
        const float harshness = clampUnit ((envelope - kThreshold) * (20.0f * detectorSens));

        // Focus weighting — broad log-Gaussian centred on focusHz
        // Floor of 0.30 ensures every band gets some processing
        const float logDist     = std::log2 (juce::jmax (1.0f, freq) / focusHz);
        const float focusWeight = juce::jlimit (0.30f, 1.0f,
            0.30f + 0.70f * std::exp (-0.5f * (logDist / focusWidthOct) * (logDist / focusWidthOct)));

        // Zone protection modifiers
        float protection = 1.0f;

        // Body: reduces max reduction in Zones 1 & 2 (low-mid warmth preservation)
        if (freq < zone3Hz)
            protection *= lerp (1.0f, 0.20f, bodyNorm);

        // Air: reduces max reduction in Zone 5 (high-frequency sparkle preservation)
        if (freq >= zone5Hz)
            protection *= lerp (1.0f, 0.15f, airNorm);

        // Magic adds 15 % reach; without it backs off slightly for a more precise feel
        const float magicMult = magicEnabled ? 1.15f : 0.88f;

        bandRawReductionDb[i] = -bandMaxReductionDb[i]
                                  * smoothNorm
                                  * focusWeight
                                  * protection
                                  * harshness
                                  * magicMult;
    }

    // ── Cross-band smoothing — prevents sharp inter-band transitions ──────────
    if (magicEnabled)
    {
        // 5-tap Gaussian: wider, more "silk blanket" feel
        auto get = [&] (int idx) -> float
        {
            return bandRawReductionDb[static_cast<size_t> (juce::jlimit (0, (int) smoothingBandCount - 1, idx))];
        };

        for (size_t i = 0; i < smoothingBandCount; ++i)
        {
            const int idx = static_cast<int> (i);
            bandSmoothedReductionDb[i] = 0.07f * get (idx - 2)
                                       + 0.22f * get (idx - 1)
                                       + 0.42f * get (idx)
                                       + 0.22f * get (idx + 1)
                                       + 0.07f * get (idx + 2);
        }
    }
    else
    {
        // 3-tap Gaussian: tighter when magic is off
        for (size_t i = 0; i < smoothingBandCount; ++i)
        {
            const float prev = (i > 0)                       ? bandRawReductionDb[i - 1] : bandRawReductionDb[i];
            const float next = (i < smoothingBandCount - 1) ? bandRawReductionDb[i + 1] : bandRawReductionDb[i];
            bandSmoothedReductionDb[i] = 0.25f * prev + 0.50f * bandRawReductionDb[i] + 0.25f * next;
        }
    }

    // ── Apply smoothed reductions as peak-filter coefficients ────────────────
    for (size_t i = 0; i < smoothingBandCount; ++i)
    {
        const float reductionGain = juce::Decibels::decibelsToGain (bandSmoothedReductionDb[i]);
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            currentSampleRate, bandFrequencies[i], bandQValues[i], reductionGain);
        leftReductionFilters[i].coefficients  = coeffs;
        rightReductionFilters[i].coefficients = coeffs;
    }

    // ── Per-sample: run reduction filter chain + output gain ─────────────────
    auto*       leftChannel  = buffer.getWritePointer (0);
    auto* const rightChannel = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    float blockPeak = 0.0f;

    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        float leftSample  = dryLeft[s];
        float rightSample = dryRight != nullptr ? dryRight[s] : dryLeft[s];

        for (size_t i = 0; i < smoothingBandCount; ++i)
        {
            leftSample  = leftReductionFilters[i].processSample  (leftSample);
            rightSample = rightReductionFilters[i].processSample (rightSample);
        }

        leftSample  *= outputLinear;
        rightSample *= outputLinear;

        leftChannel[s] = leftSample;
        if (rightChannel != nullptr)
            rightChannel[s] = rightSample;

        blockPeak = juce::jmax (blockPeak, std::abs (leftSample), std::abs (rightSample));
    }

    outputPeakLevel.store (blockPeak);
    refreshAnalyzerData();
}

bool NovaSilkAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NovaSilkAudioProcessor::createEditor()
{
    return new NovaSilkAudioProcessorEditor (*this);
}

void NovaSilkAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaSilkAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaSilkAudioProcessor();
}