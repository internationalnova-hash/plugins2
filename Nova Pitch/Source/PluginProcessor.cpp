#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>

constexpr std::array<int, 12> NovaPitchAudioProcessor::chromaticScale;
constexpr std::array<int, 7> NovaPitchAudioProcessor::majorScale;
constexpr std::array<int, 7> NovaPitchAudioProcessor::minorScale;
constexpr std::array<int, 5> NovaPitchAudioProcessor::pentatonicScale;
constexpr std::array<int, 6> NovaPitchAudioProcessor::bluesScale;

juce::AudioProcessorValueTreeState::ParameterLayout NovaPitchAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        "key", "Key",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" },
        0));

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        "scale", "Scale",
        juce::StringArray { "Chromatic", "Major", "Minor", "Pentatonic", "Blues" },
        0));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "tolerance", "Tolerance",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        50.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "amount", "Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        85.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "confidenceThreshold", "Confidence",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        70.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "vibrato", "Vibrato",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        10.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "formant", "Formant",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        50.0f));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        "lowLatency", "Low Latency",
        false));

    return layout;
}

NovaPitchAudioProcessor::NovaPitchAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    yinBuffer.resize (yinBufferSize, 0.0f);
    for (auto& p : pitchHistory)
        p.store (0.0f);
}

NovaPitchAudioProcessor::~NovaPitchAudioProcessor()
{
}

const juce::String NovaPitchAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NovaPitchAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool NovaPitchAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool NovaPitchAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double NovaPitchAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NovaPitchAudioProcessor::getNumPrograms()
{
    return 1;
}

int NovaPitchAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NovaPitchAudioProcessor::setCurrentProgram (int)
{
}

const juce::String NovaPitchAudioProcessor::getProgramName (int)
{
    return {};
}

void NovaPitchAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void NovaPitchAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::ignoreUnused (samplesPerBlock);
    initializePitchShift();
    
    minPitchHz = 50.0f;
    maxPitchHz = 400.0f;
    analysisInterval = static_cast<int>(sampleRate / 20.0); // Analyze ~20 times per second
    smoothedDetectedHz = 0.0f;
    retuneLfoPhase = 0.0f;
    retuneLfoJitter = 0.0f;
    outputCompGain = 1.0f;
}

void NovaPitchAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaPitchAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
   #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
   #endif
}
#endif

void NovaPitchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    int numSamples = buffer.getNumSamples();
    auto* channelL = buffer.getWritePointer (0);
    auto* channelR = totalNumInputChannels > 1 ? buffer.getWritePointer (1) : nullptr;
    const bool lowLatencyMode = apvts.getRawParameterValue ("lowLatency")->load() >= 0.5f;

    // Mono-focused detection, stereo-safe processing.
    std::vector<float> analysisBuffer (static_cast<size_t> (numSamples), 0.0f);
    if (channelR != nullptr)
    {
        for (int i = 0; i < numSamples; ++i)
            analysisBuffer[static_cast<size_t> (i)] = 0.5f * (channelL[i] + channelR[i]);
    }
    else
    {
        std::copy (channelL, channelL + numSamples, analysisBuffer.begin());
    }

    const float inputRms = computeBufferRms (buffer);

    // Hybrid detection: low-latency mode tracks faster, default mode tracks smoother.
    const int intervalDivider = lowLatencyMode ? 2 : 4;

    // Perform pitch detection periodically
    if (blockCount % intervalDivider == 0)
    {
        float detectedHz = detectPitchYIN (analysisBuffer.data(), numSamples);
        detectedHz = smoothDetectedPitch (detectedHz, inputRms, lowLatencyMode);
        detectedPitch.store (detectedHz);

        if (detectedHz > minPitchHz - 10.0f && detectedHz < maxPitchHz + 10.0f)
        {
            int scaleDegree = quantizeToScale (detectedHz);
            float targetHz = getTargetPitchHz (scaleDegree);

            float pitchRatio = computeRetuneRatio (detectedHz, targetHz, inputRms, lowLatencyMode);
            applyVibrato (pitchRatio, static_cast<float> (currentSampleRate), numSamples,
                          apvts.getRawParameterValue ("vibrato")->load());

            const float correctedHz = detectedHz * pitchRatio;
            correctedPitch.store (correctedHz);

            if (std::abs (pitchRatio - 1.0f) > 0.0005f)
            {
                processCircularBufferPitchShift (channelL, numSamples, pitchRatio, 0);
                if (channelR != nullptr)
                    processCircularBufferPitchShift (channelR, numSamples, pitchRatio, 1);
            }

            const float formantValue = apvts.getRawParameterValue ("formant")->load();
            applyFormantShaper (channelL, numSamples, formantValue, 0);
            if (channelR != nullptr)
                applyFormantShaper (channelR, numSamples, formantValue, 1);
        }
        else
        {
            correctedPitch.store (0.0f);
        }
    }

    applyOutputManagement (buffer, inputRms);

    blockCount++;
}

float NovaPitchAudioProcessor::smoothDetectedPitch (float rawDetectedHz, float signalRms, bool lowLatencyMode)
{
    if (rawDetectedHz <= 0.0f)
        return smoothedDetectedHz;

    if (smoothedDetectedHz <= 0.0f)
    {
        smoothedDetectedHz = rawDetectedHz;
        return smoothedDetectedHz;
    }

    const float energy = juce::jlimit (0.0f, 1.0f, signalRms * 4.0f);
    const float delta = std::abs (rawDetectedHz - smoothedDetectedHz);
    const float movement = juce::jlimit (0.0f, 1.0f, delta / 35.0f);

    // Auto behavior: track fast on unstable/live-like movement, smooth on steady playback-like content.
    float alphaFast = lowLatencyMode ? 0.68f : 0.54f;
    float alphaSlow = lowLatencyMode ? 0.28f : 0.14f;
    float alpha = juce::jlimit (alphaSlow, alphaFast, alphaSlow + movement * 0.45f + energy * 0.15f);

    smoothedDetectedHz += (rawDetectedHz - smoothedDetectedHz) * alpha;
    return smoothedDetectedHz;
}

float NovaPitchAudioProcessor::computeRetuneRatio (float detectedHz, float targetHz, float signalRms, bool lowLatencyMode)
{
    const float amountNorm = apvts.getRawParameterValue ("amount")->load() / 100.0f;
    const float toleranceNorm = apvts.getRawParameterValue ("tolerance")->load() / 100.0f;
    float humanizeNorm = apvts.getRawParameterValue ("confidenceThreshold")->load() / 100.0f;

    // Parameter ranges lock: Retune interpreted as 0..80 ms with UI convention:
    // left (0%) = slow, right (100%) = fast.
    const float retuneMs = (1.0f - amountNorm) * 80.0f;
    float retuneStrength = 1.0f - juce::jlimit (0.0f, 1.0f, retuneMs / 80.0f);

    // Intelligent interaction rules.
    if (retuneMs <= 14.0f)
        humanizeNorm *= 0.65f; // fast retune auto-tightens humanize
    retuneStrength *= (1.0f - toleranceNorm * 0.5f); // high flex/tolerance softens correction
    if (lowLatencyMode)
        retuneStrength = juce::jmin (1.0f, retuneStrength * 1.08f);

    // Internal retune curve type from behavior bands (soft/medium/hard).
    float curveExponent = 1.0f; // medium
    if (retuneMs >= 45.0f)      curveExponent = 1.4f; // soft
    else if (retuneMs <= 15.0f) curveExponent = 0.72f; // hard
    const float curveStrength = std::pow (juce::jlimit (0.0f, 1.0f, retuneStrength), curveExponent);

    // Scale lock mode: soft lock default, hard lock when aggressive setup is detected.
    const bool hardLock = (retuneMs <= 10.0f && toleranceNorm <= 0.2f && humanizeNorm <= 0.2f);
    float lockStrength = hardLock ? 1.0f : (0.38f + 0.62f * curveStrength);
    lockStrength *= (1.0f - humanizeNorm * 0.42f);

    // Noise-aware correction reduction to avoid artifacts.
    const float signalWeight = juce::jlimit (0.0f, 1.0f, signalRms * 3.2f);
    lockStrength *= (0.55f + 0.45f * signalWeight);
    lockStrength = juce::jlimit (0.0f, 1.0f, lockStrength);

    const float targetRatio = targetHz / juce::jmax (1.0f, detectedHz);
    return 1.0f + (targetRatio - 1.0f) * lockStrength;
}

void NovaPitchAudioProcessor::applyVibrato (float& pitchRatio, float sampleRate, int numSamples, float vibratoParam)
{
    // Effective vibrato range lock: 0..30 usable, mapped from 0..100 UI value.
    const float vibEffective = juce::jlimit (0.0f, 30.0f, vibratoParam * 0.3f);
    if (vibEffective <= 0.001f)
        return;

    const float depth = (vibEffective / 30.0f);
    const float rateHz = 4.0f + depth * 2.6f; // natural low, stylized high
    const float randomness = 0.015f * depth;
    retuneLfoJitter = 0.94f * retuneLfoJitter + randomness * juce::Random::getSystemRandom().nextFloat();

    const float phaseAdvance = juce::MathConstants<float>::twoPi * rateHz * (static_cast<float> (numSamples) / sampleRate);
    retuneLfoPhase = std::fmod (retuneLfoPhase + phaseAdvance, juce::MathConstants<float>::twoPi);

    const float lfo = std::sin (retuneLfoPhase + retuneLfoJitter * juce::MathConstants<float>::twoPi);
    const float centsDepth = 4.0f + depth * 18.0f;
    const float ratioDepth = std::pow (2.0f, (centsDepth / 1200.0f) * lfo) - 1.0f;
    pitchRatio *= (1.0f + ratioDepth);
}

void NovaPitchAudioProcessor::applyFormantShaper (float* channelData, int numSamples, float formantParam, int channelIndex)
{
    // Effective formant behavior lock: 45..60 region, neutral at 50.
    const float effective = juce::jlimit (45.0f, 60.0f, 45.0f + (formantParam / 100.0f) * 15.0f);
    const float centered = (effective - 50.0f) / 10.0f; // -0.5 .. +1.0
    const float a = juce::jlimit (-0.38f, 0.38f, centered * 0.42f);

    auto& s1 = formantAllPassState[static_cast<size_t> (channelIndex)][0];
    auto& s2 = formantAllPassState[static_cast<size_t> (channelIndex)][1];

    for (int i = 0; i < numSamples; ++i)
    {
        float x = channelData[i];
        // Two-stage all-pass vocal-tract shaper (phase/formant emphasis, not simple EQ gain).
        float y1 = -a * x + s1;
        s1 = x + a * y1;

        float y2 = -a * y1 + s2;
        s2 = y1 + a * y2;

        channelData[i] = 0.65f * x + 0.35f * y2;
    }
}

void NovaPitchAudioProcessor::applyOutputManagement (juce::AudioBuffer<float>& buffer, float inputRms)
{
    const float outRms = computeBufferRms (buffer);
    if (inputRms > 1.0e-6f && outRms > 1.0e-6f)
    {
        const float targetGain = juce::jlimit (0.78f, 1.28f, inputRms / outRms);
        outputCompGain = 0.92f * outputCompGain + 0.08f * targetGain;
    }

    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < samples; ++i)
        {
            float y = data[i] * outputCompGain;
            // Soft clip to avoid overs while preserving dynamics.
            data[i] = std::tanh (y * 1.12f) * 0.92f;
        }
    }
}

float NovaPitchAudioProcessor::computeBufferRms (const juce::AudioBuffer<float>& buffer) const
{
    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();
    if (channels <= 0 || samples <= 0)
        return 0.0f;

    double sum = 0.0;
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < samples; ++i)
            sum += static_cast<double> (data[i] * data[i]);
    }

    const double denom = static_cast<double> (channels * samples);
    return static_cast<float> (std::sqrt (sum / juce::jmax (1.0, denom)));
}

float NovaPitchAudioProcessor::detectPitchYIN (const float* samples, int numSamples)
{
    // Fill YIN buffer with incoming samples
    for (int i = 0; i < numSamples && yinWriteIndex < yinBufferSize; ++i)
    {
        yinBuffer[yinWriteIndex++] = samples[i];
    }

    if (yinWriteIndex < yinBufferSize)
        return detectedPitch.load();

    yinWriteIndex = 0;

    // YIN Algorithm: Autocorrelation via difference function
    std::vector<float> differenceFunction (yinBufferSize / 2, 0.0f);

    // Compute autocorrelation difference (lag-based)
    for (int lag = 1; lag < static_cast<int>(yinBufferSize / 2); ++lag)
    {
        float sum = 0.0f;
        for (int i = 0; i < static_cast<int>(yinBufferSize) - lag; ++i)
        {
            float diff = yinBuffer[i] - yinBuffer[i + lag];
            sum += diff * diff;
        }
        differenceFunction[lag] = sum;
    }

    // Normalize
    float min_fn = differenceFunction[1];
    float threshold = getYINThreshold();

    int tauMin = static_cast<int>(currentSampleRate / maxPitchHz);
    int tauMax = static_cast<int>(currentSampleRate / minPitchHz);

    // Find first minimum below threshold
    int foundTau = -1;
    for (int tau = tauMin; tau < tauMax && tau < static_cast<int>(differenceFunction.size()); ++tau)
    {
        if (differenceFunction[tau] < threshold * min_fn)
        {
            foundTau = tau;
            break;
        }
    }

    if (foundTau <= 0)
        return detectedPitch.load();

    // Interpolate for fine-grained pitch
    float detectedHz = static_cast<float>(currentSampleRate) / static_cast<float>(foundTau);
    pitchConfidence.store (1.0f - (differenceFunction[foundTau] / min_fn));

    // Store in history
    pitchHistory[historyIndex].store (detectedHz);
    historyIndex = (historyIndex + 1) % pitchHistorySize;

    return detectedHz;
}

float NovaPitchAudioProcessor::getYINThreshold() const noexcept
{
    float confidence = apvts.getRawParameterValue ("confidenceThreshold")->load() / 100.0f;
    return 0.1f + (0.9f * confidence);
}

int NovaPitchAudioProcessor::quantizeToScale (float pitchHz)
{
    // Convert to semitones relative to A0 (27.5 Hz)
    float semitones = 12.0f * std::log2 (pitchHz / 27.5f);
    int semitonesInt = static_cast<int>(std::round (semitones)) % 12;
    if (semitonesInt < 0) semitonesInt += 12;

    auto scaleParam = static_cast<Scale>(apvts.getRawParameterValue ("scale")->load());
    int index = 0;

    switch (scaleParam)
    {
        case Chromatic:
            index = semitonesInt;
            break;
        case Major:
        {
            int degree = semitonesInt % 12;
            for (size_t i = 0; i < majorScale.size(); ++i)
                if (majorScale[i] == degree) index = i;
            break;
        }
        case Minor:
        {
            int degree = semitonesInt % 12;
            for (size_t i = 0; i < minorScale.size(); ++i)
                if (minorScale[i] == degree) index = i;
            break;
        }
        case Pentatonic:
        {
            int degree = semitonesInt % 12;
            for (size_t i = 0; i < pentatonicScale.size(); ++i)
                if (pentatonicScale[i] == degree) index = i;
            break;
        }
        case Blues:
        {
            int degree = semitonesInt % 12;
            for (size_t i = 0; i < bluesScale.size(); ++i)
                if (bluesScale[i] == degree) index = i;
            break;
        }
        default:
            break;
    }

    return index;
}

float NovaPitchAudioProcessor::getTargetPitchHz (int scaleDegree) const
{
    auto scaleParam = static_cast<Scale>(apvts.getRawParameterValue ("scale")->load());
    auto keyParam = static_cast<int>(apvts.getRawParameterValue ("key")->load());

    int semitonesFromC = keyParam;

    switch (scaleParam)
    {
        case Chromatic:
            semitonesFromC += scaleDegree;
            break;
        case Major:
            semitonesFromC += majorScale[scaleDegree % majorScale.size()];
            break;
        case Minor:
            semitonesFromC += minorScale[scaleDegree % minorScale.size()];
            break;
        case Pentatonic:
            semitonesFromC += pentatonicScale[scaleDegree % pentatonicScale.size()];
            break;
        case Blues:
            semitonesFromC += bluesScale[scaleDegree % bluesScale.size()];
            break;
        default:
            break;
    }

    // C4 = 261.63 Hz
    return 261.63f * std::pow (2.0f, static_cast<float>(semitonesFromC) / 12.0f);
}

void NovaPitchAudioProcessor::initializePitchShift()
{
    for (auto& channel : pitchDelay)
        for (auto& s : channel)
            s = 0.0f;

    pitchReadPos = { 0.0f, 0.0f };
    pitchWriteIndex = { 0, 0 };
    formantAllPassState = {};
}

void NovaPitchAudioProcessor::processCircularBufferPitchShift (float* channelData, int numSamples, float pitchRatio, int channelIndex)
{
    const int channel = juce::jlimit (0, 1, channelIndex);
    auto& channelDelay = pitchDelay[static_cast<size_t> (channel)];
    auto& readPos = pitchReadPos[static_cast<size_t> (channel)];
    auto& writeIdx = pitchWriteIndex[static_cast<size_t> (channel)];

    const float clampedRatio = juce::jlimit (0.5f, 2.0f, pitchRatio);

    for (int i = 0; i < numSamples; ++i)
    {
        // Write
        channelDelay[static_cast<size_t> (writeIdx)] = channelData[i];
        int nextWriteIndex = (writeIdx + 1) % yinBufferSize;

        // Read with linear interpolation
        float readIndex = std::fmod (readPos, static_cast<float> (yinBufferSize));
        int readIndexInt = static_cast<int>(readIndex);
        float frac = readIndex - readIndexInt;
        int nextReadIndex = (readIndexInt + 1) % yinBufferSize;

        float sample1 = channelDelay[static_cast<size_t> (readIndexInt)];
        float sample2 = channelDelay[static_cast<size_t> (nextReadIndex)];
        float shifted = sample1 + frac * (sample2 - sample1);

        channelData[i] = shifted;

        // Update positions
        writeIdx = nextWriteIndex;
        readPos += clampedRatio;
    }
}

void NovaPitchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaPitchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaPitchAudioProcessor();
}

//==============================================================================
juce::AudioProcessorEditor* NovaPitchAudioProcessor::createEditor()
{
    return new NovaPitchAudioProcessorEditor (*this);
}

bool NovaPitchAudioProcessor::hasEditor() const
{
    return true;
}
