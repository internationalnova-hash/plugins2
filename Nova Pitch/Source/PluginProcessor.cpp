#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <algorithm>
#include <numeric>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static float hzToMidiF (float hz) { return 69.0f + 12.0f * std::log2f (hz / 440.0f); }
static float midiToHzF (float m)  { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
NovaPitchAudioProcessor::NovaPitchAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    yinBuf.assign (yinBufferSize, 0.0f);
    pitchHistory5.fill (0.0f);
}

NovaPitchAudioProcessor::~NovaPitchAudioProcessor() {}

// ---------------------------------------------------------------------------
// Parameter layout
// ---------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout
NovaPitchAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterInt>   ("key",                "Key",                0, 11,  0));
    params.push_back (std::make_unique<juce::AudioParameterInt>   ("scale",              "Scale",              0, 4,   0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tolerance",          "Tolerance",          0.0f, 100.0f, 50.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("amount",             "Amount",             0.0f, 100.0f, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("confidenceThreshold","Confidence Thresh",  0.0f, 100.0f, 40.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("vibrato",            "Vibrato",            0.0f, 100.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("formant",            "Formant Preserve",   true));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("lowLatency",         "Low Latency",        false));

    return { params.begin(), params.end() };
}

// ---------------------------------------------------------------------------
// prepareToPlay
// ---------------------------------------------------------------------------
void NovaPitchAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    spL.reset();
    spR.reset();

    yinBuf.assign (yinBufferSize, 0.0f);
    yinWritePos           = 0;
    smoothedDetectedHz    = 0.0f;
    blocksSinceValidPitch = 0;
    pitchRatioSmoothed    = 1.0f;
    historyIndex          = 0;
    ph5index              = 0;
    pitchHistory5.fill (0.0f);

    // The grain shifter introduces ~N/2 = 4096 samples of latency
    setLatencySamples (SimplePitchShifter::N / 2);
}

void NovaPitchAudioProcessor::releaseResources() {}

// ---------------------------------------------------------------------------
// YIN pitch detection
// ---------------------------------------------------------------------------
float NovaPitchAudioProcessor::detectYIN (const float* samples, int n)
{
    int halfN = n / 2;

    // difference function
    std::vector<float> d ((size_t)halfN, 0.0f);
    for (int tau = 1; tau < halfN; ++tau)
    {
        for (int j = 0; j < halfN; ++j)
        {
            float diff = samples[j] - samples[j + tau];
            d[(size_t)tau] += diff * diff;
        }
    }

    // cumulative mean normalized difference
    std::vector<float> cmnd ((size_t)halfN, 0.0f);
    cmnd[0] = 1.0f;
    float runSum = 0.0f;
    for (int tau = 1; tau < halfN; ++tau)
    {
        runSum += d[(size_t)tau];
        cmnd[(size_t)tau] = d[(size_t)tau] * (float)tau / (runSum + 1e-9f);
    }

    // threshold + parabolic interpolation
    const float threshold = 0.15f;
    const int   tauMin    = (int)std::ceil  (currentSampleRate / 1000.0);
    const int   tauMax    = (int)std::floor (currentSampleRate / 80.0);
    const int   tauMaxClamped = std::min (tauMax, halfN - 2);

    for (int tau = std::max (1, tauMin); tau <= tauMaxClamped; ++tau)
    {
        if (cmnd[(size_t)tau] < threshold)
        {
            float s0 = cmnd[(size_t)(tau - 1)];
            float s1 = cmnd[(size_t)tau];
            float s2 = cmnd[(size_t)(tau + 1)];
            float fTau = (float)tau + 0.5f * (s0 - s2) / (s0 - 2.0f * s1 + s2 + 1e-9f);
            return (float)(currentSampleRate / (double)fTau);
        }
    }
    return -1.0f;
}

// ---------------------------------------------------------------------------
// Scale quantization
// ---------------------------------------------------------------------------
int NovaPitchAudioProcessor::quantizeToScale (float hz)
{
    int key   = (int)apvts.getRawParameterValue ("key")->load();
    int scale = (int)apvts.getRawParameterValue ("scale")->load();

    float midi  = hzToMidiF (hz);
    int   midiR = (int)std::round (midi);
    int   pc    = ((midiR - key) % 12 + 12) % 12;

    auto nearest = [&](const int* degrees, int count) -> int {
        int best = degrees[0], bestDist = 99;
        for (int i = 0; i < count; ++i)
        {
            int d = ((pc - degrees[i]) % 12 + 12) % 12;
            int d2 = 12 - d;
            int dist = std::min (d, d2);
            if (dist < bestDist) { bestDist = dist; best = degrees[i]; }
        }
        int octave = midiR - pc;
        int candidate = octave + best + key % 12;
        if (candidate < midiR - 6) candidate += 12;
        if (candidate > midiR + 6) candidate -= 12;
        return candidate;
    };

    switch (scale)
    {
        case Chromatic:  return midiR;
        case Major:      { int deg[] = {0,2,4,5,7,9,11}; return nearest (deg, 7); }
        case Minor:      { int deg[] = {0,2,3,5,7,8,10}; return nearest (deg, 7); }
        case Pentatonic: { int deg[] = {0,2,4,7,9};       return nearest (deg, 5); }
        case Blues:      { int deg[] = {0,3,5,6,7,10};    return nearest (deg, 6); }
        default:         return midiR;
    }
}

float NovaPitchAudioProcessor::midiToHz (int midi) const { return midiToHzF ((float)midi); }
float NovaPitchAudioProcessor::hzToMidi (float hz)  const { return hzToMidiF (hz); }

// ---------------------------------------------------------------------------
// processBlock
// ---------------------------------------------------------------------------
void NovaPitchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Feed mono sum into YIN ring buffer (from input, before processing)
    const float* chL = buffer.getReadPointer (0);
    const float* chR = (numChannels > 1) ? buffer.getReadPointer (1) : chL;
    for (int s = 0; s < numSamples; ++s)
    {
        yinBuf[(size_t)yinWritePos] = (chL[s] + chR[s]) * 0.5f;
        yinWritePos = (yinWritePos + 1) % yinBufferSize;
    }

    // YIN pitch detection on the last yinBufferSize samples
    float detectedHz = -1.0f;
    {
        std::vector<float> linear (yinBufferSize);
        for (int i = 0; i < yinBufferSize; ++i)
            linear[(size_t)i] = yinBuf[(size_t)((yinWritePos + i) % yinBufferSize)];
        detectedHz = detectYIN (linear.data(), yinBufferSize);
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
        // Collect non-zero history entries
        std::array<float, 5> tmp = pitchHistory5;
        // Sort and pick middle
        std::sort (tmp.begin(), tmp.end());
        // Use the middle non-zero value
        int nonZeroCount = 0;
        for (auto v : tmp) if (v > 0.0f) ++nonZeroCount;
        if (nonZeroCount >= 3)
            medianHz = tmp[(size_t)(nonZeroCount / 2 + (5 - nonZeroCount))]; // median of valid ones
        else if (nonZeroCount > 0)
            medianHz = tmp[(size_t)(5 - nonZeroCount)]; // take the smallest non-zero (most recent)
    }

    // Smooth the median-filtered detection
    if (medianHz > 0.0f)
    {
        if (smoothedDetectedHz > 0.0f)
        {
            float semiDist = std::abs (hzToMidiF (medianHz) - hzToMidiF (smoothedDetectedHz));

            // Octave-jump rejection
            float hzRatio = medianHz / (smoothedDetectedHz + 1e-9f);
            bool octaveJump = (hzRatio > 1.88f && hzRatio < 2.12f)
                           || (hzRatio > 0.47f && hzRatio < 0.53f);

            float alpha = octaveJump    ? 0.02f
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
    float amount    = apvts.getRawParameterValue ("amount")->load() / 100.0f;
    float tolerance = apvts.getRawParameterValue ("tolerance")->load() / 100.0f;
    float vibratoAmt = apvts.getRawParameterValue ("vibrato")->load() / 100.0f;
    float confThresh = apvts.getRawParameterValue ("confidenceThreshold")->load() / 100.0f;

    float ratio = 1.0f;

    if (smoothedDetectedHz > 0.0f)
    {
        int   targetMidi = quantizeToScale (smoothedDetectedHz);
        float targetHz   = midiToHz (targetMidi);
        correctedPitch.store (targetHz);
        pitchConfidence.store (confThresh);

        float detectedMidi = hzToMidiF (smoothedDetectedHz);
        float centsDiff    = (hzToMidiF (targetHz) - detectedMidi) * 100.0f;

        float deadbandCents = tolerance * 50.0f;

        if (std::abs (centsDiff) <= deadbandCents)
        {
            // Inside deadband — glide back to unity
            pitchRatioSmoothed += (1.0f - pitchRatioSmoothed) * 0.05f;
        }
        else
        {
            // Absolute pitch ratio needed to reach targetHz
            float fullRatio = targetHz / (smoothedDetectedHz + 1e-9f);

            // Soft deadband scale
            float absCents  = std::abs (centsDiff);
            float bandScale = juce::jlimit (0.0f, 1.0f,
                                (absCents - deadbandCents) / (deadbandCents + 1.0f));
            float targetRatio = 1.0f + (fullRatio - 1.0f) * bandScale;

            // Exponential smoothing — amount controls speed
            // amount=0 → α=0.005 (very slow), amount=1 → α=0.08 (fast but smooth)
            float alpha = 0.005f + amount * amount * 0.075f;
            pitchRatioSmoothed += (targetRatio - pitchRatioSmoothed) * alpha;
        }

        if (vibratoAmt > 0.0f)
            pitchRatioSmoothed = pitchRatioSmoothed * (1.0f - vibratoAmt) + 1.0f * vibratoAmt;

        ratio = pitchRatioSmoothed;
    }
    else
    {
        pitchRatioSmoothed += (1.0f - pitchRatioSmoothed) * 0.05f;
        ratio = pitchRatioSmoothed;
    }

    // Clamp to ±2 semitones max correction (safer range for grain shifter)
    ratio = juce::jlimit (0.89f, 1.12f, ratio);

    pitchHistory[(size_t)historyIndex].store (smoothedDetectedHz);
    historyIndex = (historyIndex + 1) % pitchHistorySize;

    // Apply grain pitch shifter sample by sample
    float* bufL = buffer.getWritePointer (0);
    float* bufR = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

    for (int s = 0; s < numSamples; ++s)
    {
        bufL[s] = spL.process (bufL[s], ratio);
        if (bufR != nullptr)
            bufR[s] = spR.process (bufR[s], ratio);
    }
}

// ---------------------------------------------------------------------------
// Bus layout
// ---------------------------------------------------------------------------
#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaPitchAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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
juce::AudioProcessorEditor* NovaPitchAudioProcessor::createEditor()
{
    return new NovaPitchAudioProcessorEditor (*this);
}

bool NovaPitchAudioProcessor::hasEditor() const { return true; }

const juce::String NovaPitchAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaPitchAudioProcessor::acceptsMidi()  const { return false; }
bool NovaPitchAudioProcessor::producesMidi() const { return false; }
bool NovaPitchAudioProcessor::isMidiEffect() const { return false; }
double NovaPitchAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int NovaPitchAudioProcessor::getNumPrograms()                             { return 1; }
int NovaPitchAudioProcessor::getCurrentProgram()                          { return 0; }
void NovaPitchAudioProcessor::setCurrentProgram (int)                     {}
const juce::String NovaPitchAudioProcessor::getProgramName (int)          { return {}; }
void NovaPitchAudioProcessor::changeProgramName (int, const juce::String&) {}

void NovaPitchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaPitchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaPitchAudioProcessor();
}
