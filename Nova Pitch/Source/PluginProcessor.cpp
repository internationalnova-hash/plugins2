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
        0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "amount", "Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "confidenceThreshold", "Confidence",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "vibrato", "Vibrato",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "formant", "Formant",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        0.0f));

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
    analysisScratch.clear();
    analysisScratch.reserve (static_cast<size_t> (juce::jmax (samplesPerBlock, 512)));
    
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
    lockedTargetMidi = -1;
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
    const float amountValue = apvts.getRawParameterValue ("amount")->load();
    const float toleranceValue = apvts.getRawParameterValue ("tolerance")->load();
    const float confidenceValue = apvts.getRawParameterValue ("confidenceThreshold")->load();
    const float vibratoValue = apvts.getRawParameterValue ("vibrato")->load();
    const float formantValue = apvts.getRawParameterValue ("formant")->load();
    const float amountNorm = juce::jlimit (0.0f, 1.0f, amountValue / 100.0f);
    const float retuneSpeedNorm = amountNorm; // 100 knob = fastest retune
    juce::ignoreUnused (toleranceValue, confidenceValue);

    // Debug: Log parameter values every 100 blocks to diagnose engine engagement
    static int debugCounter = 0;
    if (++debugCounter % 100 == 0)
    {
        DBG("Nova Pitch params: amount=" << amountValue << " tolerance=" << toleranceValue 
            << " confidence=" << confidenceValue << " vibrato=" << vibratoValue << " formant=" << formantValue);
    }

    // Mono-focused detection, stereo-safe processing.
    if (analysisScratch.size() < static_cast<size_t> (numSamples))
        analysisScratch.resize (static_cast<size_t> (numSamples), 0.0f);

    float* analysisData = analysisScratch.data();
    if (channelR != nullptr)
    {
        for (int i = 0; i < numSamples; ++i)
            analysisData[i] = 0.5f * (channelL[i] + channelR[i]);
    }
    else
    {
        std::copy (channelL, channelL + numSamples, analysisData);
    }

    const float inputRms = computeBufferRms (buffer);

    // Track faster when Retune is set fast (right side / amount near 100).
    const bool fastRetuneTracking = retuneSpeedNorm > 0.70f;
    const int intervalDivider = (lowLatencyMode || fastRetuneTracking) ? 1 : 2;

    // Perform pitch detection periodically
    if (blockCount % intervalDivider == 0)
    {
        float detectedHz = detectPitchYIN (analysisData, numSamples);
        detectedHz = smoothDetectedPitch (detectedHz, inputRms, lowLatencyMode || fastRetuneTracking);
        detectedPitch.store (detectedHz);

        if (detectedHz > minPitchHz - 10.0f && detectedHz < maxPitchHz + 10.0f)
        {
            const int candidateMidiNote = quantizeToScale (detectedHz);
            const float detectedMidi = 69.0f + 12.0f * std::log2 (juce::jmax (1.0f, detectedHz) / 440.0f);

            if (lockedTargetMidi < 0)
            {
                lockedTargetMidi = candidateMidiNote;
            }
            else if (candidateMidiNote != lockedTargetMidi)
            {
                // Dynamic hysteresis: hard-tune (fast retune) should switch notes quicker,
                // while slower retune keeps more stability.
                const float switchHysteresis = juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 0.50f, 0.12f);
                const bool switchUp = candidateMidiNote > lockedTargetMidi
                                   && detectedMidi > static_cast<float> (lockedTargetMidi) + switchHysteresis;
                const bool switchDown = candidateMidiNote < lockedTargetMidi
                                     && detectedMidi < static_cast<float> (lockedTargetMidi) - switchHysteresis;
                if (switchUp || switchDown)
                    lockedTargetMidi = candidateMidiNote;
            }

            const int targetMidiNote = lockedTargetMidi;
            const float targetHz = getTargetPitchHz (targetMidiNote);

            float pitchRatio = computeRetuneRatio (detectedHz, targetHz, inputRms, lowLatencyMode);
            applyVibrato (pitchRatio, static_cast<float> (currentSampleRate), numSamples,
                          vibratoValue);
            // Keep correction bounded while allowing strong hard-tune behavior.
            targetPitchRatio = juce::jlimit (0.50f, 2.00f, pitchRatio);
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
            lockedTargetMidi = -1;
        }
    }

    // AutoTune/MetaTune architecture:
    // - Retune knob = SPEED only (LP filter glide time toward target)
    // - Correction is always 100% toward target note — no depth scaling
    // - Shifted audio replaces input entirely — no wet/dry blend (blend causes doubling)
    const float trackingConfidence = juce::jlimit (0.0f, 1.0f, pitchConfidence.load());
    const bool trackingLost = trackingConfidence < 0.03f || inputRms < 0.002f;

    // Retune stays active across the full knob range; knob controls speed only.
    {
        const float speedCoeff = lowLatencyMode
            ? juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 0.035f, 0.998f)
            : juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 0.020f, 0.992f);

        if (! trackingLost)
            activePitchRatio += (targetPitchRatio - activePitchRatio) * speedCoeff;
        else
            activePitchRatio += (1.0f - activePitchRatio) * 0.03f;

        activePitchRatio = juce::jlimit (0.50f, 2.00f, activePitchRatio);

        // Apply shift: corrected audio replaces input — no dry blend, no doubling.
        // When ratio ≈ 1.0 (singer already on pitch), shifter bypasses internally → clean passthrough.
        processCircularBufferPitchShift (channelL, numSamples, activePitchRatio, 0);
        if (channelR != nullptr)
            processCircularBufferPitchShift (channelR, numSamples, activePitchRatio, 1);
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

float NovaPitchAudioProcessor::computeRetuneRatio (float detectedHz, float targetHz, float /*signalRms*/, bool /*lowLatencyMode*/)
{
    const float amountNorm = apvts.getRawParameterValue ("amount")->load() / 100.0f;
    const float retuneSpeedNorm = juce::jlimit (0.0f, 1.0f, amountNorm);

    const float toleranceNorm = apvts.getRawParameterValue ("tolerance")->load() / 100.0f;

    // Full correction toward target — retune SPEED is handled by the LP filter in processBlock.
    // This mirrors AutoTune/MetaTune: correction is always 100%, speed is the only variable.
    const float fullRatio = targetHz / juce::jmax (1.0f, detectedHz);

    // Tolerance window: if singer is already within toleranceCents, leave them alone.
    const float centsError = std::abs (1200.0f * std::log2 (juce::jmax (0.001f, std::abs (fullRatio))));
    const float toleranceScale = juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 1.0f, 0.05f);
    const float toleranceCents = toleranceNorm * 45.0f * toleranceScale;
    if (centsError < toleranceCents)
        return 1.0f;

    return juce::jlimit (0.50f, 2.00f, fullRatio);
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
    // Formant knob semantics: 0 must be neutral (no coloration/artifacts).
    const float amount = juce::jlimit (0.0f, 1.0f, formantParam / 100.0f);
    if (amount < 0.01f)
        return;

    const float a = juce::jlimit (-0.16f, 0.16f, amount * 0.16f);
    const float mix = juce::jlimit (0.0f, 0.22f, amount * 0.22f);

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
    const float threshold = 0.22f + confidenceParam * 0.08f; // 0.22 to 0.30

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
    // This creates an immediate pitchShiftBufferSize/2 sample separation so the
    // cross-fading dual-head algorithm works from the very first block.
    // Without this offset both heads start at 0 — the reader reads the
    // just-written sample every cycle and the pitch shift has no effect.
    pitchReadPos[0] = 0.0f;
    pitchReadPos[1] = 0.0f;
    pitchCrossfadePhase[0] = 0.0f;
    pitchCrossfadePhase[1] = 0.5f;
    pitchWriteIndex[0] = pitchShiftBufferSize / 2;
    pitchWriteIndex[1] = pitchShiftBufferSize / 2;
    pitchShiftRatioSmoothed[0] = 1.0f;
    pitchShiftRatioSmoothed[1] = 1.0f;
    pitchOutputSmoother[0] = 0.0f;
    pitchOutputSmoother[1] = 0.0f;
    formantAllPassState = {};
}

void NovaPitchAudioProcessor::processCircularBufferPitchShift (float* channelData, int numSamples, float pitchRatio, int channelIndex)
{
    const int channel = juce::jlimit (0, 1, channelIndex);
    auto& channelDelay = pitchDelay[static_cast<size_t> (channel)];
    auto& readPos = pitchReadPos[static_cast<size_t> (channel)];
    auto& crossfadePhase = pitchCrossfadePhase[static_cast<size_t> (channel)];
    auto& writeIdx = pitchWriteIndex[static_cast<size_t> (channel)];
    auto& ratioSmoothed = pitchShiftRatioSmoothed[static_cast<size_t> (channel)];
    auto& outputSmoother = pitchOutputSmoother[static_cast<size_t> (channel)];

    const int bufferSize = pitchShiftBufferSize;
    const float clampedRatio = juce::jlimit (0.50f, 2.00f, pitchRatio);
    ratioSmoothed += (clampedRatio - ratioSmoothed) * 0.06f;
    const float effectiveRatio = juce::jlimit (0.50f, 2.00f, ratioSmoothed);

    // Granular OLA core.
    // A 2-grain Hann overlap reduces zipper/skip artifacts compared to simple delay modulation.
    constexpr int hopSamples = 512;
    constexpr int grainSamples = hopSamples * 2;
    constexpr int baseDelaySamples = 3072;

    auto wrapPos = [bufferSize] (float p)
    {
        while (p < 0.0f)
            p += static_cast<float> (bufferSize);
        while (p >= static_cast<float> (bufferSize))
            p -= static_cast<float> (bufferSize);
        return p;
    };

    auto sampleAt = [&] (float pos)
    {
        const float wrapped = wrapPos (pos);
        const int i0 = static_cast<int> (wrapped);
        const int i1 = (i0 + 1) % bufferSize;
        const int im1 = (i0 - 1 + bufferSize) % bufferSize;
        const int i2 = (i0 + 2) % bufferSize;
        const float frac = wrapped - static_cast<float> (i0);
        const float ym1 = channelDelay[static_cast<size_t> (im1)];
        const float y0 = channelDelay[static_cast<size_t> (i0)];
        const float y1 = channelDelay[static_cast<size_t> (i1)];
        const float y2 = channelDelay[static_cast<size_t> (i2)];

        // Cubic Hermite interpolation lowers aliasy/grainy texture vs linear interpolation.
        const float c0 = y0;
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    };

    auto hannAt = [] (float n)
    {
        const float clamped = juce::jlimit (0.0f, static_cast<float> (grainSamples - 1), n);
        return 0.5f - 0.5f * std::cos ((juce::MathConstants<float>::twoPi * clamped)
                                       / static_cast<float> (grainSamples - 1));
    };

    // Grain phase advances in output time. On each hop boundary we move source grain anchor.
    float phaseSamples = crossfadePhase * static_cast<float> (hopSamples);
    const float analysisHop = static_cast<float> (hopSamples) * effectiveRatio;

    for (int i = 0; i < numSamples; ++i)
    {
        channelDelay[static_cast<size_t> (writeIdx)] = channelData[i];

        // Keep grain anchor in a valid latency region behind writer.
        const float desiredAnchor = static_cast<float> (writeIdx - baseDelaySamples);
        const float latency = std::fmod (static_cast<float> (writeIdx) - readPos + static_cast<float> (bufferSize), static_cast<float> (bufferSize));
        if (latency < 512.0f || latency > static_cast<float> (bufferSize - 512))
            readPos = wrapPos (desiredAnchor);

        const float nA = phaseSamples;
        const float nB = phaseSamples + static_cast<float> (hopSamples);
        const float wA = hannAt (nA);
        const float wB = hannAt (nB);
        const float wSum = juce::jmax (1.0e-5f, wA + wB);

        const float srcA = readPos + nA * effectiveRatio;
        const float srcB = (readPos - analysisHop) + nB * effectiveRatio;

        const float shifted = (sampleAt (srcA) * wA + sampleAt (srcB) * wB) / wSum;

        // Gentle de-zipper for residual crossover grain.
        outputSmoother += (shifted - outputSmoother) * 0.35f;
        const float unityDelta = std::abs (effectiveRatio - 1.0f);
        const float shiftWet = juce::jlimit (0.0f, 1.0f, (unityDelta - 0.002f) / 0.018f);
        channelData[i] = channelData[i] * (1.0f - shiftWet) + outputSmoother * shiftWet;

        ++phaseSamples;
        if (phaseSamples >= static_cast<float> (hopSamples))
        {
            phaseSamples -= static_cast<float> (hopSamples);
            readPos = wrapPos (readPos + analysisHop);
        }

        writeIdx = (writeIdx + 1) % bufferSize;
    }

    crossfadePhase = phaseSamples / static_cast<float> (hopSamples);
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
