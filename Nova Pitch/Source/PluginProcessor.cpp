#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <limits>

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
    
        // Cover full vocal range: bass to soprano head voice (60 Hz – 1 kHz).
        // Old cap of 400 Hz made tauMin=110 samples, making every note above G4 undetectable.
        minPitchHz = 60.0f;
        maxPitchHz = 1000.0f;
    analysisInterval = static_cast<int>(sampleRate / 20.0); // Analyze ~20 times per second
    smoothedDetectedHz = 0.0f;
    retuneLfoPhase = 0.0f;
    retuneLfoJitter = 0.0f;
    outputCompGain = 1.0f;
    targetPitchRatio = 1.0f;
    activePitchRatio = 1.0f;
    wetMixSmoothed = 0.0f;
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

    std::vector<float> dryLeft (static_cast<size_t> (numSamples), 0.0f);
    std::copy (channelL, channelL + numSamples, dryLeft.begin());
    std::vector<float> dryRight;
    if (channelR != nullptr)
    {
        dryRight.resize (static_cast<size_t> (numSamples), 0.0f);
        std::copy (channelR, channelR + numSamples, dryRight.begin());
    }
    const bool lowLatencyMode = apvts.getRawParameterValue ("lowLatency")->load() >= 0.5f;
    const float amountValue = apvts.getRawParameterValue ("amount")->load();
    const float toleranceValue = apvts.getRawParameterValue ("tolerance")->load();
    const float confidenceValue = apvts.getRawParameterValue ("confidenceThreshold")->load();
    const float vibratoValue = apvts.getRawParameterValue ("vibrato")->load();
    const float formantValue = apvts.getRawParameterValue ("formant")->load();

    // Debug: Log parameter values every 100 blocks to diagnose engine engagement
    static int debugCounter = 0;
    if (++debugCounter % 100 == 0)
    {
        DBG("Nova Pitch params: amount=" << amountValue << " tolerance=" << toleranceValue 
            << " confidence=" << confidenceValue << " vibrato=" << vibratoValue << " formant=" << formantValue);
    }

    // True clean baseline mode for troubleshooting and dry testing.
    const bool baselineBypass = amountValue <= 0.5f
                             && toleranceValue <= 0.5f
                             && confidenceValue <= 0.5f
                             && vibratoValue <= 0.5f
                             && formantValue <= 0.5f;
    
    if (baselineBypass && debugCounter % 100 == 1)
    {
        DBG("Nova Pitch: Baseline bypass active - no processing");
    }

    if (baselineBypass)
    {
        targetPitchRatio = 1.0f;
        activePitchRatio = 1.0f;
        correctedPitch.store (0.0f);
        
        // CRITICAL: Clear all DSP state to prevent artifacts when returning to baseline.
        // This ensures the circular buffer and LFO state don't retain stale data.
        initializePitchShift();
        smoothedDetectedHz = 0.0f;
        retuneLfoPhase = 0.0f;
        retuneLfoJitter = 0.0f;
        outputCompGain = 1.0f;
        wetMixSmoothed = 0.0f;
        
        blockCount++;
        return;
    }

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
    const int intervalDivider = lowLatencyMode ? 1 : 2;

    // Perform pitch detection periodically
    if (blockCount % intervalDivider == 0)
    {
        float detectedHz = detectPitchYIN (analysisBuffer.data(), numSamples);
        detectedHz = smoothDetectedPitch (detectedHz, inputRms, lowLatencyMode);
        detectedPitch.store (detectedHz);

        if (detectedHz > minPitchHz - 10.0f && detectedHz < maxPitchHz + 10.0f)
        {
            const int targetMidiNote = quantizeToScale (detectedHz);
            const float targetHz = getTargetPitchHz (targetMidiNote);

            float pitchRatio = computeRetuneRatio (detectedHz, targetHz, inputRms, lowLatencyMode);
            applyVibrato (pitchRatio, static_cast<float> (currentSampleRate), numSamples,
                          vibratoValue);
            targetPitchRatio = juce::jlimit (0.7f, 1.4f, pitchRatio);
            const float correctedHz = detectedHz * pitchRatio;
            correctedPitch.store (correctedHz);

            if (debugCounter % 100 == 2)
            {
                DBG("Nova Pitch: detectedHz=" << detectedHz << " targetHz=" << targetHz << " pitchRatio=" << pitchRatio);
            }
        }
        else
        {
            if (debugCounter % 100 == 2)
            {
                DBG("Nova Pitch: detectedHz=" << detectedHz << " out of range=[" << (minPitchHz - 10.0f) << "-" << (maxPitchHz + 10.0f) << "]");
            }

            targetPitchRatio = 1.0f;
            correctedPitch.store (0.0f);
            pitchConfidence.store (0.0f);
        }
    }

    const float trackingConfidence = juce::jlimit (0.0f, 1.0f, pitchConfidence.load());
    const bool trackingLost = trackingConfidence < 0.12f || inputRms < 0.004f;
    if (trackingLost)
        targetPitchRatio += (1.0f - targetPitchRatio) * 0.18f;

    const float ratioSmoothing = juce::jmap (amountValue, 0.18f, lowLatencyMode ? 0.62f : 0.44f) * (trackingLost ? 0.6f : 1.0f);
    activePitchRatio += (targetPitchRatio - activePitchRatio) * ratioSmoothing;
    activePitchRatio = juce::jlimit (0.78f, 1.28f, activePitchRatio);

    if (std::abs (activePitchRatio - 1.0f) > 0.006f)
    {
        processCircularBufferPitchShift (channelL, numSamples, activePitchRatio, 0);
        if (channelR != nullptr)
            processCircularBufferPitchShift (channelR, numSamples, activePitchRatio, 1);
    }

    const float amountNorm = juce::jlimit (0.0f, 1.0f, amountValue / 100.0f);
    const float correctionDepth = juce::jlimit (0.0f, 1.0f, std::abs (activePitchRatio - 1.0f) / 0.18f);
    const float confidenceGate = juce::jlimit (0.0f, 1.0f, (trackingConfidence - 0.05f) / 0.95f);

    // Keep the corrected path clearly present, but never fully expose the shifter on weak tracking.
    float wetMix = juce::jmap (amountNorm, 0.38f, 0.92f);
    wetMix *= juce::jmax (0.80f, confidenceGate);
    wetMix *= juce::jmax (0.65f, 0.45f + 0.55f * correctionDepth);
    wetMix = juce::jlimit (0.25f, 0.92f, wetMix);
    
    wetMixSmoothed = 0.92f * wetMixSmoothed + 0.08f * wetMix;
    wetMix = wetMixSmoothed;

    for (int i = 0; i < numSamples; ++i)
        channelL[i] = dryLeft[static_cast<size_t> (i)] * (1.0f - wetMix) + channelL[i] * wetMix;

    if (channelR != nullptr)
    {
        for (int i = 0; i < numSamples; ++i)
            channelR[i] = dryRight[static_cast<size_t> (i)] * (1.0f - wetMix) + channelR[i] * wetMix;
    }

    applyFormantShaper (channelL, numSamples, formantValue, 0);
    if (channelR != nullptr)
        applyFormantShaper (channelR, numSamples, formantValue, 1);

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

    // Retune at 0% must behave as dry/no correction.
    if (amountNorm <= 0.0001f)
        return 1.0f;

    // Parameter ranges lock: Retune interpreted as 0..80 ms with UI convention:
    // left (0%) = slow, right (100%) = fast.
    const float retuneMs = (1.0f - amountNorm) * 80.0f;
    float retuneStrength = juce::jlimit (0.0f, 1.0f, amountNorm);

    // Intelligent interaction rules.
    if (retuneMs <= 14.0f)
        humanizeNorm *= 0.65f; // fast retune auto-tightens humanize
    retuneStrength *= (1.0f - toleranceNorm * 0.18f); // keep retune authoritative even with tolerance up
    if (lowLatencyMode)
        retuneStrength = juce::jmin (1.0f, retuneStrength * 1.18f); // boost low latency mode more

    // Internal retune curve type from behavior bands (soft/medium/hard).
    float curveExponent = 1.0f; // medium
    if (retuneMs >= 45.0f)      curveExponent = 1.4f; // soft
    else if (retuneMs <= 15.0f) curveExponent = 0.72f; // hard
    const float curveStrength = std::pow (juce::jlimit (0.0f, 1.0f, retuneStrength), curveExponent);

    // Scale lock mode: soft lock default, hard lock when aggressive setup is detected.
    const bool hardLock = (retuneMs <= 10.0f && toleranceNorm <= 0.2f && humanizeNorm <= 0.2f);
    float lockStrength = hardLock ? juce::jmax (curveStrength, 0.98f) : curveStrength;
    lockStrength *= juce::jlimit (0.65f, 1.0f, 1.0f - humanizeNorm * 0.35f); // never reduce below 65% for humanize

    // Noise-aware correction reduction to avoid artifacts - but maintain strong tuning.
    const float signalWeight = juce::jlimit (0.0f, 1.0f, signalRms * 3.2f);
    // Scale by signal weight but ensure minimum strength: strong signal = full, quiet = at least 60% correction.
    lockStrength *= (0.60f + 0.40f * signalWeight);

    // At near-max retune, correction must be audibly engaged even if other knobs are relaxed.
    if (amountNorm >= 0.99f)
    {
        const float centsError = std::abs (1200.0f * std::log2 (targetHz / juce::jmax (1.0f, detectedHz)));
        const float errorNorm = juce::jlimit (0.0f, 1.0f, centsError / 40.0f);
        const float forcedStrength = 0.80f + 0.20f * errorNorm;
        lockStrength = juce::jmax (lockStrength, forcedStrength);
    }

    lockStrength = juce::jlimit (0.0f, 1.0f, lockStrength);

    const float targetRatio = targetHz / juce::jmax (1.0f, detectedHz);
    const float centsError = std::abs (1200.0f * std::log2 (targetRatio));
    const float snapStrength = juce::jlimit (0.0f, 1.0f, centsError / (18.0f + toleranceNorm * 48.0f));
    lockStrength = juce::jmax (lockStrength, 0.42f + 0.58f * snapStrength * amountNorm);

    return 1.0f + (targetRatio - 1.0f) * juce::jlimit (0.0f, 1.0f, lockStrength);
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
    const float centered = (formantParam - 50.0f) / 50.0f; // -1..1
    if (std::abs (centered) < 0.01f)
        return;

    const float a = juce::jlimit (-0.22f, 0.22f, centered * 0.22f);
    const float mix = juce::jlimit (0.0f, 0.35f, std::abs (centered) * 0.35f);

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

        channelData[i] = x * (1.0f - mix) + y2 * mix;
    }
}

void NovaPitchAudioProcessor::applyOutputManagement (juce::AudioBuffer<float>& buffer, float inputRms)
{
    const float outRms = computeBufferRms (buffer);
    if (inputRms > 1.0e-6f && outRms > 1.0e-6f)
    {
        // Clean path: attenuation-only management to avoid added drive/clipping.
        const float targetGain = juce::jlimit (0.75f, 1.0f, inputRms / outRms);
        outputCompGain = 0.92f * outputCompGain + 0.08f * targetGain;
    }
    else
    {
        outputCompGain = 0.98f * outputCompGain + 0.02f;
    }

    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < samples; ++i)
            data[i] *= outputCompGain;
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
    // Accumulate samples into analysis window.
    for (int i = 0; i < numSamples && yinWriteIndex < yinBufferSize; ++i)
        yinBuffer[static_cast<size_t> (yinWriteIndex++)] = samples[i];

    if (yinWriteIndex < yinBufferSize)
        return detectedPitch.load();

    yinWriteIndex = 0;

    const int halfBuf = yinBufferSize / 2;
    const int tauMin  = juce::jmax (2, static_cast<int> (currentSampleRate / maxPitchHz));
    const int tauMax  = juce::jmin (halfBuf - 1, static_cast<int> (currentSampleRate / minPitchHz));

    if (tauMin >= tauMax)
        return detectedPitch.load();

    // Step 1: Difference function d(τ).
    std::vector<float> d (static_cast<size_t> (tauMax + 1), 0.0f);
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        float sum = 0.0f;
        for (int i = 0; i < halfBuf; ++i)
        {
            const float diff = yinBuffer[static_cast<size_t> (i)]
                             - yinBuffer[static_cast<size_t> (i + tau)];
            sum += diff * diff;
        }
        d[static_cast<size_t> (tau)] = sum;
    }

    // Step 2: Cumulative mean normalised difference d'(τ).
    d[0] = 1.0f;
    float runningSum = 0.0f;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        runningSum += d[static_cast<size_t> (tau)];
        d[static_cast<size_t> (tau)] = (runningSum > 0.0f)
            ? d[static_cast<size_t> (tau)] * static_cast<float> (tau) / runningSum
            : 1.0f;
    }

    // Step 3: Absolute threshold — first dip below threshold is the vocal period.
    const float confidenceParam = juce::jlimit (0.0f, 1.0f,
        apvts.getRawParameterValue ("confidenceThreshold")->load() / 100.0f);
    const float threshold = 0.10f + confidenceParam * 0.20f; // 0.10 to 0.30

    int foundTau = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
    {
        if (d[static_cast<size_t> (tau)] < threshold)
        {
            // Walk to the local minimum within this dip.
            while (tau + 1 < tauMax
                   && d[static_cast<size_t> (tau + 1)] < d[static_cast<size_t> (tau)])
                ++tau;
            foundTau = tau;
            break;
        }
    }

    if (foundTau < 0)
    {
        // Fallback: global minimum over search range. More permissive for real vocal pitch.
        foundTau = tauMin;
        float bestVal = d[static_cast<size_t> (tauMin)];
        for (int tau = tauMin + 1; tau < tauMax; ++tau)
        {
            if (d[static_cast<size_t> (tau)] < bestVal)
            {
                bestVal = d[static_cast<size_t> (tau)];
                foundTau = tau;
            }
        }
        if (bestVal > 0.60f)
            return detectedPitch.load(); // Raised from 0.45 to allow weaker but real detections.
    }

    // Step 4: Parabolic interpolation for sub-sample period accuracy.
    float betterTau = static_cast<float> (foundTau);
    if (foundTau > tauMin && foundTau < tauMax - 1)
    {
        const float y0 = d[static_cast<size_t> (foundTau - 1)];
        const float y1 = d[static_cast<size_t> (foundTau)];
        const float y2 = d[static_cast<size_t> (foundTau + 1)];
        const float parabDen = y0 - 2.0f * y1 + y2;
        if (std::abs (parabDen) > 1.0e-8f)
            betterTau += 0.5f * (y0 - y2) / parabDen;
    }

    if (betterTau <= 0.5f)
        return detectedPitch.load();

    const float detectedHz = static_cast<float> (currentSampleRate) / betterTau;
    
    // Improved confidence: based on how pronounced the dip is and whether it's from threshold search (more confident)
    // vs fallback search (less confident). Score reflects actual periodicity clarity.
    const float baseConfidence = 1.0f - juce::jlimit (0.0f, 0.90f, d[static_cast<size_t> (foundTau)] * 1.35f);
    const bool fromThresholdSearch = (d[static_cast<size_t> (foundTau)] < threshold);
    const float searchBoost = fromThresholdSearch ? 0.10f : 0.0f; // Threshold hits are more reliable.
    pitchConfidence.store (juce::jlimit (0.15f, 0.95f, baseConfidence + searchBoost));
    pitchHistory[static_cast<size_t> (historyIndex)].store (detectedHz);
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
    const float safeHz = juce::jmax (1.0f, pitchHz);
    const float detectedMidi = 69.0f + 12.0f * std::log2 (safeHz / 440.0f);
    const int centerMidi = static_cast<int> (std::round (detectedMidi));
    const int key = static_cast<int> (apvts.getRawParameterValue ("key")->load());
    const auto scale = static_cast<Scale> (apvts.getRawParameterValue ("scale")->load());

    auto inScale = [&] (int semitoneFromKey)
    {
        const int degree = ((semitoneFromKey % 12) + 12) % 12;

        switch (scale)
        {
            case Chromatic:  return true;
            case Major:      return std::find (majorScale.begin(), majorScale.end(), degree) != majorScale.end();
            case Minor:      return std::find (minorScale.begin(), minorScale.end(), degree) != minorScale.end();
            case Pentatonic: return std::find (pentatonicScale.begin(), pentatonicScale.end(), degree) != pentatonicScale.end();
            case Blues:      return std::find (bluesScale.begin(), bluesScale.end(), degree) != bluesScale.end();
            default:         return true;
        }
    };

    int bestMidi = centerMidi;
    float bestDistance = std::numeric_limits<float>::max();

    for (int midi = centerMidi - 24; midi <= centerMidi + 24; ++midi)
    {
        const int semitoneFromKey = midi - key;
        if (! inScale (semitoneFromKey))
            continue;

        const float distance = std::abs (static_cast<float> (midi) - detectedMidi);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestMidi = midi;
        }
    }

    return bestMidi;
}

float NovaPitchAudioProcessor::getTargetPitchHz (int midiNote) const
{
    return 440.0f * std::pow (2.0f, (static_cast<float> (midiNote) - 69.0f) / 12.0f);
}

void NovaPitchAudioProcessor::initializePitchShift()
{
    for (auto& channel : pitchDelay)
        for (auto& s : channel)
            s = 0.0f;

    // Read starts at 0; write starts at the half-buffer mark.
    // This creates an immediate yinBufferSize/2 sample separation so the
    // cross-fading dual-head algorithm works from the very first block.
    // Without this offset both heads start at 0 — the reader reads the
    // just-written sample every cycle and the pitch shift has no effect.
    pitchReadPos[0] = 0.0f;
    pitchReadPos[1] = 0.0f;
    pitchWriteIndex[0] = yinBufferSize / 2;
    pitchWriteIndex[1] = yinBufferSize / 2;
    pitchOutputSmoother[0] = 0.0f;
    pitchOutputSmoother[1] = 0.0f;
    formantAllPassState = {};
}

void NovaPitchAudioProcessor::processCircularBufferPitchShift (float* channelData, int numSamples, float pitchRatio, int channelIndex)
{
    const int channel = juce::jlimit (0, 1, channelIndex);
    auto& channelDelay = pitchDelay[static_cast<size_t> (channel)];
    auto& readPos = pitchReadPos[static_cast<size_t> (channel)];
    auto& writeIdx = pitchWriteIndex[static_cast<size_t> (channel)];

    const float clampedRatio = juce::jlimit (0.5f, 2.0f, pitchRatio);

    // If ratio is essentially 1.0, just pass through without circular buffer distortion
    if (std::abs (clampedRatio - 1.0f) < 0.001f)
    {
        return;
    }

    auto sampleAt = [&] (float pos)
    {
        const float wrapped = std::fmod (pos + static_cast<float> (yinBufferSize), static_cast<float> (yinBufferSize));
        const int i0 = static_cast<int> (wrapped);
        const int i1 = (i0 + 1) % yinBufferSize;
        const float frac = wrapped - static_cast<float> (i0);
        const float s0 = channelDelay[static_cast<size_t> (i0)];
        const float s1 = channelDelay[static_cast<size_t> (i1)];
        return s0 + frac * (s1 - s0);
    };

    for (int i = 0; i < numSamples; ++i)
    {
        channelDelay[static_cast<size_t> (writeIdx)] = channelData[i];

        const float phase = std::fmod (readPos + static_cast<float> (yinBufferSize), static_cast<float> (yinBufferSize));
        const float phaseNorm = phase / static_cast<float> (yinBufferSize);

        const float headA = phase;
        const float headB = std::fmod (phase + static_cast<float> (yinBufferSize) * 0.5f,
                                       static_cast<float> (yinBufferSize));

        const float crossfade = 0.5f - 0.5f * std::cos (phaseNorm * juce::MathConstants<float>::twoPi);
        const float fadeA = std::sqrt (juce::jlimit (0.0f, 1.0f, 1.0f - crossfade));
        const float fadeB = std::sqrt (juce::jlimit (0.0f, 1.0f, crossfade));
        const float shifted = sampleAt (headA) * fadeA + sampleAt (headB) * fadeB;

        const float smoother = juce::jlimit (0.18f, 0.34f, 0.20f + 0.22f * std::abs (clampedRatio - 1.0f));
        auto& outputSmoother = pitchOutputSmoother[static_cast<size_t> (channel)];
        outputSmoother += (shifted - outputSmoother) * smoother;
        channelData[i] = outputSmoother;

        writeIdx = (writeIdx + 1) % yinBufferSize;
        readPos += clampedRatio;
        if (readPos >= static_cast<float> (yinBufferSize))
            readPos -= static_cast<float> (yinBufferSize);
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
