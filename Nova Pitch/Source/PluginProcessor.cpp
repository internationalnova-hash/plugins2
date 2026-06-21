#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <algorithm>
#include <numeric>

static float hzToMidiF (float hz) { return 69.0f + 12.0f * std::log2f (hz / 440.0f); }
static float midiToHzF (float m)  { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }

NovaPitchAudioProcessor::NovaPitchAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    yinBuf.assign    (yinBufferSize, 0.0f);
    yinLinear.assign (yinBufferSize, 0.0f);
    yinD.assign      (yinBufferSize / 2, 0.0f);
    yinCmnd.assign   (yinBufferSize / 2, 0.0f);
    pitchHistory5.fill (0.0f);
}

NovaPitchAudioProcessor::~NovaPitchAudioProcessor() {}

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

void NovaPitchAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    yinBuf.assign    (yinBufferSize, 0.0f);
    yinLinear.assign (yinBufferSize, 0.0f);
    yinD.assign      (yinBufferSize / 2, 0.0f);
    yinCmnd.assign   (yinBufferSize / 2, 0.0f);

    yinWritePos           = 0;
    smoothedDetectedHz    = 0.0f;
    blocksSinceValidPitch = 0;
    pitchRatioSmoothed    = 1.0f;
    correctionActive      = false;
    lastTargetMidi        = -1;
    noteTargetRatio       = 1.0f;
    historyIndex          = 0;
    ph5index              = 0;
    pitchHistory5.fill (0.0f);

    // Dual-head shifter reset
    shiftBufL.fill (0.0f);
    shiftBufR.fill (0.0f);
    shiftWritePos = 0;
    shiftDelay1   = static_cast<float> (kInitialDelay);
    shiftDelay2   = static_cast<float> (kInitialDelay);
    shiftXfade    = -1;

    setLatencySamples (kInitialDelay);
}

void NovaPitchAudioProcessor::releaseResources() {}

float NovaPitchAudioProcessor::detectYIN (const float* samples, int n, float* d, float* cmnd)
{
    const int halfN = n / 2;

    for (int tau = 0; tau < halfN; ++tau) d[tau] = 0.0f;
    for (int tau = 1; tau < halfN; ++tau)
        for (int j = 0; j < halfN; ++j)
        {
            float diff = samples[j] - samples[j + tau];
            d[tau] += diff * diff;
        }

    cmnd[0] = 1.0f;
    float runSum = 0.0f;
    for (int tau = 1; tau < halfN; ++tau)
    {
        runSum += d[tau];
        cmnd[tau] = d[tau] * (float)tau / (runSum + 1e-9f);
    }

    const float threshold = 0.15f;
    const int   tauMin    = (int)std::ceil  (currentSampleRate / 1000.0);
    const int   tauMax    = (int)std::floor (currentSampleRate / 80.0);
    const int   tauMaxC   = std::min (tauMax, halfN - 2);

    for (int tau = std::max (1, tauMin); tau <= tauMaxC; ++tau)
    {
        if (cmnd[tau] < threshold)
        {
            float s0  = cmnd[tau - 1];
            float s1  = cmnd[tau];
            float s2  = cmnd[tau + 1];
            float fTau = (float)tau + 0.5f * (s0 - s2) / (s0 - 2.0f * s1 + s2 + 1e-9f);
            return (float)(currentSampleRate / (double)fTau);
        }
    }
    return -1.0f;
}

int NovaPitchAudioProcessor::quantizeToScale (float hz)
{
    int key   = (int)apvts.getRawParameterValue ("key")->load();
    int scale = (int)apvts.getRawParameterValue ("scale")->load();

    float midi  = hzToMidiF (hz);
    int   midiR = (int)std::round (midi);
    int   pc    = ((midiR - key) % 12 + 12) % 12;

    auto nearest = [&](const int* degrees, int count) -> int
    {
        int best = degrees[0], bestDist = 99;
        for (int i = 0; i < count; ++i)
        {
            int d  = ((pc - degrees[i]) % 12 + 12) % 12;
            int d2 = 12 - d;
            int dist = std::min (d, d2);
            if (dist < bestDist) { bestDist = dist; best = degrees[i]; }
        }
        int tonicOfOctave = midiR - pc;
        int candidate = tonicOfOctave + best;
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

void NovaPitchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // ----------------------------------------------------------------
    // Capture input BEFORE any writes (handles separate I/O buffers)
    // ----------------------------------------------------------------
    const float* chL = buffer.getReadPointer (0);
    const float* chR = (numChannels > 1) ? buffer.getReadPointer (1) : chL;

    // ----------------------------------------------------------------
    // Pitch detection — YIN on mono mix of input
    // ----------------------------------------------------------------
    for (int s = 0; s < numSamples; ++s)
    {
        yinBuf[(size_t)yinWritePos] = (chL[s] + chR[s]) * 0.5f;
        yinWritePos = (yinWritePos + 1) % yinBufferSize;
    }

    for (int i = 0; i < yinBufferSize; ++i)
        yinLinear[(size_t)i] = yinBuf[(size_t)((yinWritePos + i) % yinBufferSize)];

    float detectedHz = detectYIN (yinLinear.data(), yinBufferSize,
                                   yinD.data(), yinCmnd.data());

    if (detectedHz > 0.0f)
    {
        pitchHistory5[(size_t)ph5index] = detectedHz;
        ph5index = (ph5index + 1) % 5;
    }

    float medianHz = 0.0f;
    {
        std::array<float, 5> tmp = pitchHistory5;
        std::sort (tmp.begin(), tmp.end());
        int nonZero = 0;
        for (auto v : tmp) if (v > 0.0f) ++nonZero;
        if (nonZero >= 3)
            medianHz = tmp[(size_t)(nonZero / 2 + (5 - nonZero))];
        else if (nonZero > 0)
            medianHz = tmp[(size_t)(5 - nonZero)];
    }

    if (medianHz > 0.0f)
    {
        if (smoothedDetectedHz > 0.0f)
        {
            float hzRatio  = medianHz / (smoothedDetectedHz + 1e-9f);
            bool  octJump  = (hzRatio > 1.88f && hzRatio < 2.12f)
                          || (hzRatio > 0.47f && hzRatio < 0.53f);
            float semiDist = std::abs (hzToMidiF (medianHz) - hzToMidiF (smoothedDetectedHz));
            float alpha    = octJump ? 0.02f : juce::jlimit (0.05f, 0.15f, semiDist * 0.03f);
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

    // ----------------------------------------------------------------
    // Correction ratio
    // ----------------------------------------------------------------
    float amount    = apvts.getRawParameterValue ("amount")->load() / 100.0f;
    float tolerance = apvts.getRawParameterValue ("tolerance")->load() / 100.0f;
    float ratio = 1.0f;

    if (smoothedDetectedHz > 0.0f)
    {
        int   targetMidi = quantizeToScale (smoothedDetectedHz);
        float targetHz   = midiToHz (targetMidi);
        correctedPitch.store (targetHz);
        pitchConfidence.store (apvts.getRawParameterValue ("confidenceThreshold")->load() / 100.0f);

        float centsDiff     = (hzToMidiF (targetHz) - hzToMidiF (smoothedDetectedHz)) * 100.0f;
        float deadbandCents = tolerance * 50.0f;

        if (!correctionActive && std::abs (centsDiff) > deadbandCents)  correctionActive = true;
        if ( correctionActive && std::abs (centsDiff) < deadbandCents * 0.4f) correctionActive = false;

        if (correctionActive)
        {
            if (targetMidi != lastTargetMidi)
            {
                lastTargetMidi = targetMidi;
                float alignedHz = smoothedDetectedHz;
                if (targetHz > 1.0f)
                {
                    while (alignedHz < targetHz * 0.70710678f) alignedHz *= 2.0f;
                    while (alignedHz > targetHz * 1.41421356f) alignedHz *= 0.5f;
                }
                noteTargetRatio = juce::jlimit (0.841f, 1.189f, targetHz / (alignedHz + 1e-9f));
            }
            ratio = 1.0f + (noteTargetRatio - 1.0f) * amount;
        }
        else
        {
            lastTargetMidi  = -1;
            noteTargetRatio = 1.0f;
            ratio = 1.0f;
        }
    }
    else
    {
        lastTargetMidi  = -1;
        noteTargetRatio = 1.0f;
        correctionActive = false;
        ratio = 1.0f;
    }

    ratio = juce::jlimit (0.841f, 1.189f, ratio);

    pitchHistory[(size_t)historyIndex].store (smoothedDetectedHz);
    historyIndex = (historyIndex + 1) % pitchHistorySize;

    // ----------------------------------------------------------------
    // Dual-head crossfade pitch shifter with pitch-period-aligned resets
    //
    // Both heads maintain a delay behind the write head.  At pitch ratio
    // 1.0 the delay is stable.  When ratio ≠ 1.0 the delay drifts; when
    // it goes out of bounds a second head is started, period-aligned so
    // the two heads are an integer number of pitch periods apart — their
    // signals are nearly identical, making the blend inaudible.
    // ----------------------------------------------------------------
    float* outL = buffer.getWritePointer (0);
    float* outR = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

    auto readLerp = [this] (const std::array<float, kShiftBufSize>& buf, float pos) -> float
    {
        int   i = static_cast<int> (pos) & kShiftBufMask;
        int   j = (i + 1) & kShiftBufMask;
        float f = pos - std::floor (pos);
        return buf[i] * (1.0f - f) + buf[j] * f;
    };

    for (int s = 0; s < numSamples; ++s)
    {
        shiftBufL[shiftWritePos & kShiftBufMask] = chL[s];
        shiftBufR[shiftWritePos & kShiftBufMask] = chR[s];
        shiftWritePos++;

        // Compute absolute read positions from write position and delays
        float pos1 = static_cast<float> (shiftWritePos) - shiftDelay1;
        float pos2 = static_cast<float> (shiftWritePos) - shiftDelay2;

        float h1L = readLerp (shiftBufL, pos1);
        float h1R = readLerp (shiftBufR, pos1);
        float h2L = readLerp (shiftBufL, pos2);
        float h2R = readLerp (shiftBufR, pos2);

        // Raised-cosine blend: 0 = head1 only, 1 = head2 only
        float blend = 0.0f;
        if (shiftXfade >= 0)
        {
            float t = static_cast<float> (shiftXfade) / static_cast<float> (kCrossfadeLen);
            blend = 0.5f * (1.0f - std::cos (t * juce::MathConstants<float>::pi));
            if (++shiftXfade >= kCrossfadeLen)
            {
                shiftDelay1 = shiftDelay2;   // head 2 becomes head 1
                shiftXfade  = -1;
            }
        }

        outL[s] = h1L * (1.0f - blend) + h2L * blend;
        if (outR != nullptr)
            outR[s] = h1R * (1.0f - blend) + h2R * blend;

        // Both delays drift by (ratio - 1) per sample
        // ratio > 1 → delay shrinks (head approaches write)
        // ratio < 1 → delay grows (head falls behind write)
        shiftDelay1 -= (ratio - 1.0f);
        shiftDelay2 -= (ratio - 1.0f);

        // Trigger a crossfade when head 1's delay goes out of range
        if (shiftXfade < 0 && (shiftDelay1 < kMinDelay || shiftDelay1 > kMaxDelay))
        {
            // Estimate pitch period; default to a safe value when no pitch detected
            float period = (smoothedDetectedHz > 50.0f)
                         ? static_cast<float> (currentSampleRate) / smoothedDetectedHz
                         : 256.0f;

            // Use the minimum N (whole periods) so head 2 stays safely in range after
            // the crossfade completes.  Jumping N=13+ periods (as a "return to centre"
            // strategy) makes the blend traverse 1400+ samples of audio, causing a
            // severe momentary pitch drop.  With N=1 the traversal is ~1 period = tiny.
            float drift = std::abs (ratio - 1.0f);
            int   N     = juce::jmax (1, static_cast<int> (std::ceil (drift * kCrossfadeLen / period)));
            float sign  = (shiftDelay1 < kMinDelay) ? 1.0f : -1.0f;
            shiftDelay2 = shiftDelay1 + sign * static_cast<float> (N) * period;
            shiftDelay2 = juce::jlimit (kMinDelay, kMaxDelay, shiftDelay2);
            shiftXfade  = 0;
        }
    }
}

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
int NovaPitchAudioProcessor::getNumPrograms()    { return 1; }
int NovaPitchAudioProcessor::getCurrentProgram() { return 0; }
void NovaPitchAudioProcessor::setCurrentProgram (int) {}
const juce::String NovaPitchAudioProcessor::getProgramName (int) { return {}; }
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
