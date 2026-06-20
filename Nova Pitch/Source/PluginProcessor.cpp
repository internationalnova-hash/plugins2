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

void NovaPitchAudioProcessor::initStretcher (double sampleRate, int maxBlockSize, bool formantPreserve)
{
#ifdef HAVE_RUBBERBAND
    using RBS = RubberBand::RubberBandStretcher;

    // Bare-minimum real-time options — no HighConsistency (which can inflate latency
    // to thousands of samples and requires huge priming chunks that exceed maxBlockSize,
    // corrupting RubberBand's internal state).
    int options = RBS::OptionProcessRealTime;

    if (formantPreserve)
        options |= RBS::OptionFormantPreserved;

    stretcher = std::make_unique<RBS> (
        static_cast<size_t> (sampleRate), 2, options, 1.0, 1.0);

    // Must be called before any process() — sets internal buffer allocation.
    // Default is 512; DAW buffers can be 1024, 1632, 2048, etc.
    stretcher->setMaxProcessSize (static_cast<size_t> (maxBlockSize));

    stretcher->setTimeRatio (1.0);
    stretcher->setPitchScale (1.0);

    const int latency = static_cast<int> (stretcher->getLatency());
    setLatencySamples (latency);

    // Prime in chunks no larger than maxBlockSize.
    // Previously we fed ALL latency samples in one call, which violates
    // setMaxProcessSize when latency > maxBlockSize → corrupted internal state.
    if (latency > 0)
    {
        std::vector<float> silence (static_cast<size_t> (maxBlockSize), 0.0f);
        const float* ptrs[2] = { silence.data(), silence.data() };

        int remaining = latency;
        while (remaining > 0)
        {
            int chunk = std::min (remaining, maxBlockSize);
            stretcher->process (ptrs, static_cast<size_t> (chunk), false);
            remaining -= chunk;
        }
    }
#else
    juce::ignoreUnused (sampleRate, maxBlockSize, formantPreserve);
    setLatencySamples (0);
#endif
}

void NovaPitchAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock > 0 ? samplesPerBlock : 512;

    yinBuf.assign    (yinBufferSize, 0.0f);
    yinLinear.assign (yinBufferSize, 0.0f);
    yinD.assign      (yinBufferSize / 2, 0.0f);
    yinCmnd.assign   (yinBufferSize / 2, 0.0f);

    yinWritePos           = 0;
    smoothedDetectedHz    = 0.0f;
    blocksSinceValidPitch = 0;
    pitchRatioSmoothed    = 1.0f;
    lastPitchScale        = 1.0f;
    correctionActive      = false;
    lastTargetMidi        = -1;
    noteTargetRatio       = 1.0f;
    historyIndex          = 0;
    ph5index              = 0;
    pitchHistory5.fill (0.0f);

    bool formant = apvts.getRawParameterValue ("formant")->load() > 0.5f;
    lastFormantSetting = formant;
    initStretcher (sampleRate, currentBlockSize, formant);
}

void NovaPitchAudioProcessor::releaseResources()
{
#ifdef HAVE_RUBBERBAND
    stretcher.reset();
#endif
}

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

    bool formant = apvts.getRawParameterValue ("formant")->load() > 0.5f;
    if (formant != lastFormantSetting)
    {
        lastFormantSetting = formant;
        initStretcher (currentSampleRate, currentBlockSize, formant);
    }

    // --- Pitch detection ---

    const float* chL = buffer.getReadPointer (0);
    const float* chR = (numChannels > 1) ? buffer.getReadPointer (1) : chL;
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
            bool  octaveJump = (hzRatio > 1.88f && hzRatio < 2.12f)
                            || (hzRatio > 0.47f && hzRatio < 0.53f);
            float semiDist = std::abs (hzToMidiF (medianHz) - hzToMidiF (smoothedDetectedHz));
            float alpha    = octaveJump ? 0.02f
                                        : juce::jlimit (0.05f, 0.15f, semiDist * 0.03f);
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

    // --- Compute correction ratio ---

    float amount     = apvts.getRawParameterValue ("amount")->load() / 100.0f;
    float tolerance  = apvts.getRawParameterValue ("tolerance")->load() / 100.0f;
    float vibratoAmt = apvts.getRawParameterValue ("vibrato")->load() / 100.0f;

    float ratio = 1.0f;

    if (smoothedDetectedHz > 0.0f)
    {
        int   targetMidi = quantizeToScale (smoothedDetectedHz);
        float targetHz   = midiToHz (targetMidi);
        correctedPitch.store (targetHz);
        pitchConfidence.store (apvts.getRawParameterValue ("confidenceThreshold")->load() / 100.0f);

        float centsDiff     = (hzToMidiF (targetHz) - hzToMidiF (smoothedDetectedHz)) * 100.0f;
        float deadbandCents = tolerance * 50.0f;

        if (!correctionActive && std::abs (centsDiff) > deadbandCents)
            correctionActive = true;
        if (correctionActive  && std::abs (centsDiff) < deadbandCents * 0.4f)
            correctionActive = false;

        if (correctionActive)
        {
            // Recompute ratio only when the target note changes — not every block.
            // Continuous per-block recomputation causes micro-modulation from
            // pitch detection jitter, which sounds like wobble/chorus/delay.
            if (targetMidi != lastTargetMidi)
            {
                lastTargetMidi = targetMidi;

                float alignedHz = smoothedDetectedHz;
                if (targetHz > 1.0f)
                {
                    while (alignedHz < targetHz * 0.70710678f) alignedHz *= 2.0f;
                    while (alignedHz > targetHz * 1.41421356f) alignedHz *= 0.5f;
                }
                noteTargetRatio = targetHz / (alignedHz + 1e-9f);
                noteTargetRatio = juce::jlimit (0.841f, 1.189f, noteTargetRatio);
            }

            float scaledRatio = 1.0f + (noteTargetRatio - 1.0f) * amount;
            pitchRatioSmoothed += (scaledRatio - pitchRatioSmoothed) * 0.12f;
        }
        else
        {
            lastTargetMidi = -1;
            pitchRatioSmoothed += (1.0f - pitchRatioSmoothed) * 0.08f;
        }

        if (vibratoAmt > 0.0f)
            pitchRatioSmoothed = pitchRatioSmoothed * (1.0f - vibratoAmt) + 1.0f * vibratoAmt;

        ratio = pitchRatioSmoothed;
    }
    else
    {
        lastTargetMidi = -1;
        pitchRatioSmoothed += (1.0f - pitchRatioSmoothed) * 0.05f;
        ratio = pitchRatioSmoothed;
    }

    ratio = juce::jlimit (0.841f, 1.189f, ratio);

    pitchHistory[(size_t)historyIndex].store (smoothedDetectedHz);
    historyIndex = (historyIndex + 1) % pitchHistorySize;

    // --- Pitch shift ---

    float* bufL = buffer.getWritePointer (0);
    float* bufR = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

#ifdef HAVE_RUBBERBAND
    if (stretcher != nullptr)
    {
        if (std::abs (ratio - lastPitchScale) > 0.0005f)
        {
            stretcher->setPitchScale (static_cast<double> (ratio));
            lastPitchScale = ratio;
        }

        const float* inPtrs[2] = { bufL, bufR != nullptr ? bufR : bufL };
        stretcher->process (inPtrs, static_cast<size_t> (numSamples), false);

        const int avail = stretcher->available();

        if (avail >= numSamples)
        {
            // Drain any excess before retrieving the current block
            if (avail > numSamples * 3)
            {
                const int excess = std::min (avail - numSamples, drainBufSize);
                float* dp[2] = { drainL.data(), drainR.data() };
                stretcher->retrieve (dp, static_cast<size_t> (excess));
            }

            float* outPtrs[2] = { bufL, bufR != nullptr ? bufR : bufL };
            stretcher->retrieve (outPtrs, static_cast<size_t> (numSamples));
        }
        else if (avail > 0)
        {
            // Partial output — retrieve what we have, zero-fill the rest.
            // Outputs zeros instead of dry audio to avoid mix of processed/unprocessed.
            float* outPtrs[2] = { bufL, bufR != nullptr ? bufR : bufL };
            stretcher->retrieve (outPtrs, static_cast<size_t> (avail));
            for (int i = avail; i < numSamples; ++i)
            {
                bufL[i] = 0.0f;
                if (bufR != nullptr) bufR[i] = 0.0f;
            }
        }
        else
        {
            // No output yet (startup) — output silence
            buffer.clear();
        }
    }
#else
    juce::ignoreUnused (bufL, bufR);
#endif
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
