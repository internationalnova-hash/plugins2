#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <algorithm>
#include <numeric>

#ifdef HAVE_RUBBERBAND
#include <rubberband/RubberBandStretcher.h>
using RubberBand::RubberBandStretcher;
#endif

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static float hzToMidiF (float hz) { return 69.0f + 12.0f * std::log2f (hz / 440.0f); }
static float midiToHzF (float m)  { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
NovaAutotuneAudioProcessor::NovaAutotuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    mpmBuf.assign (mpmBufferSize, 0.0f);
    pitchHistory5.fill (0.0f);
}

NovaAutotuneAudioProcessor::~NovaAutotuneAudioProcessor() {}

// ---------------------------------------------------------------------------
// Parameter layout
// ---------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout
NovaAutotuneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterInt>   ("key",         "Key",          0, 11,  0));
    params.push_back (std::make_unique<juce::AudioParameterInt>   ("scale",       "Scale",        0, 5,   0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("retuneSpeed", "Retune Speed", 0.0f, 100.0f, 20.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tolerance",   "Tolerance",    0.0f, 50.0f,  0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("amount",      "Amount",       0.0f, 100.0f, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("formant",     "Formant Preserve", true));

    return { params.begin(), params.end() };
}

// ---------------------------------------------------------------------------
// prepareToPlay
// ---------------------------------------------------------------------------
void NovaAutotuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    mpmBuf.assign (mpmBufferSize, 0.0f);
    mpmWritePos           = 0;
    smoothedDetectedHz    = 0.0f;
    blocksSinceValidPitch = 0;
    pitchRatioSmoothed    = 1.0f;
    historyIndex          = 0;
    ph5index              = 0;
    pitchHistory5.fill (0.0f);

#ifdef HAVE_RUBBERBAND
    const int numCh = std::max (1, getTotalNumInputChannels());
    auto options = RubberBandStretcher::OptionProcessRealTime
                 | RubberBandStretcher::OptionPitchHighConsistency
                 | RubberBandStretcher::OptionFormantPreserved;

    stretcher = std::make_unique<RubberBandStretcher> (
        (size_t)sampleRate, (size_t)numCh, options);
    stretcher->setPitchScale (1.0);

    const int latency = static_cast<int> (stretcher->getLatency());
    setLatencySamples (latency);

    // Prime with silence to fill internal latency buffer
    std::vector<float> silence (static_cast<size_t> (latency), 0.0f);
    const float* ptrs[2] = { silence.data(), silence.data() };
    stretcher->process (ptrs, static_cast<size_t> (latency), false);
#else
    setLatencySamples (0);
    juce::ignoreUnused (samplesPerBlock);
#endif
}

void NovaAutotuneAudioProcessor::releaseResources()
{
#ifdef HAVE_RUBBERBAND
    stretcher.reset();
#endif
}

// ---------------------------------------------------------------------------
// MPM pitch detection (McLeod Pitch Method)
// ---------------------------------------------------------------------------
float NovaAutotuneAudioProcessor::detectMPM (const float* samples, int n)
{
    // Compute NSDF
    std::vector<float> nsdf ((size_t)n, 0.0f);
    for (int tau = 0; tau < n; ++tau)
    {
        float acf = 0.0f, m = 0.0f;
        for (int j = 0; j < n - tau; ++j)
        {
            acf += samples[j] * samples[j + tau];
            m   += samples[j] * samples[j] + samples[j + tau] * samples[j + tau];
        }
        nsdf[(size_t)tau] = (m > 1e-8f) ? 2.0f * acf / m : 0.0f;
    }

    // Find key maxima (peaks after positive zero crossings, above threshold)
    const float threshold = 0.8f;
    std::vector<int> keyMaxima;
    bool lookingForMax = false;

    for (int i = 1; i < n - 1; ++i)
    {
        if (nsdf[(size_t)(i - 1)] < 0.0f && nsdf[(size_t)i] >= 0.0f)
            lookingForMax = true;

        if (lookingForMax && nsdf[(size_t)i] > nsdf[(size_t)(i - 1)] && nsdf[(size_t)i] >= nsdf[(size_t)(i + 1)])
        {
            if (nsdf[(size_t)i] > threshold)
                keyMaxima.push_back (i);
            lookingForMax = false;
        }
    }

    if (keyMaxima.empty())
        return -1.0f;

    // Parabolic interpolation on first key maximum
    int tau = keyMaxima[0];
    if (tau < 1 || tau >= n - 1)
        return -1.0f;

    float s0 = nsdf[(size_t)(tau - 1)];
    float s1 = nsdf[(size_t)tau];
    float s2 = nsdf[(size_t)(tau + 1)];
    float fTau = (float)tau + 0.5f * (s0 - s2) / (s0 - 2.0f * s1 + s2 + 1e-9f);

    return (fTau > 0.0f) ? (float)(currentSampleRate / (double)fTau) : -1.0f;
}

// ---------------------------------------------------------------------------
// Scale quantization
// ---------------------------------------------------------------------------
float NovaAutotuneAudioProcessor::quantizeToScale (float hz)
{
    int key   = (int)apvts.getRawParameterValue ("key")->load();
    int scale = (int)apvts.getRawParameterValue ("scale")->load();

    float midi  = hzToMidiF (hz);
    int   midiR = (int)std::round (midi);
    int   pc    = ((midiR - key) % 12 + 12) % 12;

    auto nearest = [&](const int* degrees, int count) -> float
    {
        int best = degrees[0], bestDist = 99;
        for (int i = 0; i < count; ++i)
        {
            int d    = ((pc - degrees[i]) % 12 + 12) % 12;
            int d2   = 12 - d;
            int dist = std::min (d, d2);
            if (dist < bestDist) { bestDist = dist; best = degrees[i]; }
        }
        int candidate = (midiR - pc) + best;
        if (candidate < midiR - 6) candidate += 12;
        if (candidate > midiR + 6) candidate -= 12;
        return midiToHzF ((float)candidate);
    };

    switch (scale)
    {
        case Chromatic:  return midiToHzF ((float)midiR);
        case Major:      { int deg[] = {0,2,4,5,7,9,11};  return nearest (deg, 7); }
        case Minor:      { int deg[] = {0,2,3,5,7,8,10};  return nearest (deg, 7); }
        case Pentatonic: { int deg[] = {0,2,4,7,9};        return nearest (deg, 5); }
        case Blues:      { int deg[] = {0,3,5,6,7,10};     return nearest (deg, 6); }
        case Dorian:     { int deg[] = {0,2,3,5,7,9,10};  return nearest (deg, 7); }
        default:         return midiToHzF ((float)midiR);
    }
}

float NovaAutotuneAudioProcessor::midiToHz (float midi) const { return midiToHzF (midi); }
float NovaAutotuneAudioProcessor::hzToMidi (float hz)   const { return hzToMidiF (hz); }

// ---------------------------------------------------------------------------
// processBlock
// ---------------------------------------------------------------------------
void NovaAutotuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Feed mono sum into MPM ring buffer
    const float* chL = buffer.getReadPointer (0);
    const float* chR = (numChannels > 1) ? buffer.getReadPointer (1) : chL;
    for (int s = 0; s < numSamples; ++s)
    {
        mpmBuf[(size_t)mpmWritePos] = (chL[s] + chR[s]) * 0.5f;
        mpmWritePos = (mpmWritePos + 1) % mpmBufferSize;
    }

    // MPM pitch detection on the last mpmBufferSize samples
    float detectedHz = -1.0f;
    {
        std::vector<float> linear ((size_t)mpmBufferSize);
        for (int i = 0; i < mpmBufferSize; ++i)
            linear[(size_t)i] = mpmBuf[(size_t)((mpmWritePos + i) % mpmBufferSize)];
        detectedHz = detectMPM (linear.data(), mpmBufferSize);
    }

    // Update 5-block pitch history (for median filter)
    if (detectedHz > 0.0f)
    {
        pitchHistory5[(size_t)ph5index] = detectedHz;
        ph5index = (ph5index + 1) % 5;
    }

    // Compute median of last 5 valid detections
    float medianHz = 0.0f;
    {
        std::array<float, 5> tmp = pitchHistory5;
        std::sort (tmp.begin(), tmp.end());
        int nonZeroCount = 0;
        for (auto v : tmp) if (v > 0.0f) ++nonZeroCount;
        if (nonZeroCount >= 3)
            medianHz = tmp[(size_t)(nonZeroCount / 2 + (5 - nonZeroCount))];
        else if (nonZeroCount > 0)
            medianHz = tmp[(size_t)(5 - nonZeroCount)];
    }

    // Exponential smoothing with octave-jump rejection
    if (medianHz > 0.0f)
    {
        if (smoothedDetectedHz > 0.0f)
        {
            float hzRatio = medianHz / (smoothedDetectedHz + 1e-9f);
            bool octaveJump = (hzRatio > 1.88f && hzRatio < 2.12f)
                           || (hzRatio > 0.47f && hzRatio < 0.53f);

            float semiDist = std::abs (hzToMidiF (medianHz) - hzToMidiF (smoothedDetectedHz));
            float alpha = octaveJump      ? 0.02f
                        : semiDist > 2.0f ? 0.15f
                        : semiDist > 0.5f ? 0.08f
                                          : 0.04f;
            smoothedDetectedHz = alpha * medianHz + (1.0f - alpha) * smoothedDetectedHz;
        }
        else
        {
            smoothedDetectedHz = medianHz;
        }
        blocksSinceValidPitch = 0;
    }
    else
    {
        if (++blocksSinceValidPitch > maxHoldBlocks)
            smoothedDetectedHz = 0.0f;
    }

    detectedPitch.store (smoothedDetectedHz);

    // Read parameters
    float retuneSpeed = apvts.getRawParameterValue ("retuneSpeed")->load(); // 0–100
    float tolerance   = apvts.getRawParameterValue ("tolerance")->load();   // semitones, 0–50
    float amount      = apvts.getRawParameterValue ("amount")->load() / 100.0f; // 0–1
    bool  formantOn   = apvts.getRawParameterValue ("formant")->load() > 0.5f;

    juce::ignoreUnused (formantOn); // passed to RubberBand via options at init

    float correctionRatio = 1.0f;

    if (smoothedDetectedHz > 0.0f)
    {
        float targetHz = quantizeToScale (smoothedDetectedHz);
        correctedPitch.store (targetHz);
        pitchConfidence.store (1.0f);

        float detectedMidi = hzToMidiF (smoothedDetectedHz);
        float targetMidi   = hzToMidiF (targetHz);
        float semiError    = targetMidi - detectedMidi;

        if (std::abs (semiError) <= tolerance + 1e-4f)
        {
            // Inside deadband — glide back to unity
            pitchRatioSmoothed += (1.0f - pitchRatioSmoothed) * 0.05f;
        }
        else
        {
            // Octave-align detected pitch to target octave before computing ratio
            float alignedHz = smoothedDetectedHz;
            if (targetHz > 1.0f)
            {
                while (alignedHz < targetHz * 0.70710678f) alignedHz *= 2.0f;
                while (alignedHz > targetHz * 1.41421356f) alignedHz *= 0.5f;
            }

            float fullRatio = targetHz / (alignedHz + 1e-9f);
            float targetRatio = 1.0f + (fullRatio - 1.0f) * amount;

            // Retune speed: 0 = instant (dt→0 → alpha→1), 100 = very slow (500ms)
            // Map retuneSpeed 0–100 → retune_time 0ms–500ms
            // alpha = 1 - exp(-blockDuration / retune_time)
            float blockDuration = (float)numSamples / (float)currentSampleRate;
            float alpha;
            if (retuneSpeed < 0.5f)
            {
                alpha = 1.0f; // instant snap
            }
            else
            {
                float retuneTime = (retuneSpeed / 100.0f) * 0.5f; // 0–500ms in seconds
                alpha = 1.0f - std::exp (-blockDuration / (retuneTime + 1e-9f));
            }
            alpha = juce::jlimit (0.001f, 1.0f, alpha);

            pitchRatioSmoothed += (targetRatio - pitchRatioSmoothed) * alpha;
        }

        correctionRatio = pitchRatioSmoothed;
    }
    else
    {
        pitchRatioSmoothed += (1.0f - pitchRatioSmoothed) * 0.05f;
        correctionRatio = pitchRatioSmoothed;
    }

    // Clamp to ±3 semitones — prevents octave-error blowouts
    correctionRatio = juce::jlimit (0.841f, 1.189f, correctionRatio);

    // Store pitch history for UI
    pitchHistory[(size_t)historyIndex].store (smoothedDetectedHz);
    historyIndex = (historyIndex + 1) % pitchHistorySize;

    // Apply pitch shifting
    float* bufL = buffer.getWritePointer (0);
    float* bufR = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

#ifdef HAVE_RUBBERBAND
    stretcher->setPitchScale ((double)correctionRatio);
    const float* inPtrs[2] = { bufL, bufR ? bufR : bufL };
    stretcher->process (inPtrs, (size_t)numSamples, false);
    int avail = stretcher->available();
    if (avail >= numSamples)
    {
        float* outPtrs[2] = { bufL, bufR ? bufR : bufL };
        stretcher->retrieve (outPtrs, (size_t)numSamples);
    }
    else
    {
        buffer.clear();
    }
#else
    // Fallback: pass-through when RubberBand not available
    juce::ignoreUnused (bufL, bufR, correctionRatio);
#endif
}

// ---------------------------------------------------------------------------
// Bus layout
// ---------------------------------------------------------------------------
#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaAutotuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

// ---------------------------------------------------------------------------
// Editor / boilerplate
// ---------------------------------------------------------------------------
juce::AudioProcessorEditor* NovaAutotuneAudioProcessor::createEditor()
{
    return new NovaAutotuneAudioProcessorEditor (*this);
}

bool NovaAutotuneAudioProcessor::hasEditor() const { return true; }

const juce::String NovaAutotuneAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaAutotuneAudioProcessor::acceptsMidi()  const { return false; }
bool NovaAutotuneAudioProcessor::producesMidi() const { return false; }
bool NovaAutotuneAudioProcessor::isMidiEffect() const { return false; }
double NovaAutotuneAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int NovaAutotuneAudioProcessor::getNumPrograms()                             { return 1; }
int NovaAutotuneAudioProcessor::getCurrentProgram()                          { return 0; }
void NovaAutotuneAudioProcessor::setCurrentProgram (int)                     {}
const juce::String NovaAutotuneAudioProcessor::getProgramName (int)          { return {}; }
void NovaAutotuneAudioProcessor::changeProgramName (int, const juce::String&) {}

void NovaAutotuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaAutotuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaAutotuneAudioProcessor();
}
