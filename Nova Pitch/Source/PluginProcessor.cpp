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

    bool ll = static_cast<bool> (apvts.getRawParameterValue ("lowLatency")->load());
    initPV (ll);

    yinBuf.assign (yinBufferSize, 0.0f);
    yinWritePos          = 0;
    smoothedDetectedHz   = 0.0f;
    blocksSinceValidPitch = 0;
    pitchRatioSmoothed   = 1.0f;
    historyIndex         = 0;
}

void NovaPitchAudioProcessor::releaseResources() {}

// ---------------------------------------------------------------------------
// PV init
// ---------------------------------------------------------------------------
void NovaPitchAudioProcessor::initPV (bool lowLatency)
{
    if (lowLatency)
    {
        currentFftOrder = 9;   // 512
        currentHopSize  = 128;
    }
    else
    {
        currentFftOrder = 11;  // 2048
        currentHopSize  = 512;
    }
    currentFftSize = 1 << currentFftOrder;

    fft = std::make_unique<juce::dsp::FFT> (currentFftOrder);

    hannWindow.resize ((size_t)currentFftSize);
    for (int i = 0; i < currentFftSize; ++i)
        hannWindow[(size_t)i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float)i / (float)(currentFftSize - 1));

    pvL.reset (currentFftSize);
    pvR.reset (currentFftSize);

    setLatencySamples (currentFftSize);
}

// ---------------------------------------------------------------------------
// YIN pitch detector
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
        // snap to nearest octave relative to midiR
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
// Spectral envelope for formant preservation
// ---------------------------------------------------------------------------
void NovaPitchAudioProcessor::computeSpectralEnvelope (
    const std::vector<float>& mag, std::vector<float>& env, int smoothBins)
{
    env.resize (mag.size());
    std::vector<float> logMag (mag.size());
    for (size_t i = 0; i < mag.size(); ++i)
        logMag[i] = std::log (mag[i] + 1e-9f);

    int half = smoothBins / 2;
    for (int i = 0; i < (int)logMag.size(); ++i)
    {
        int lo = std::max (0, i - half);
        int hi = std::min ((int)logMag.size() - 1, i + half);
        float sum = 0.0f;
        for (int k = lo; k <= hi; ++k) sum += logMag[(size_t)k];
        env[(size_t)i] = std::exp (sum / (float)(hi - lo + 1));
    }
}

// ---------------------------------------------------------------------------
// Phase vocoder frame
// ---------------------------------------------------------------------------
void NovaPitchAudioProcessor::runPVFrame (PVChannel& ch, float ratio)
{
    const int N     = currentFftSize;
    const int bins  = N / 2 + 1;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // Windowed input frame
    std::vector<float> fftData (N * 2, 0.0f);
    for (int i = 0; i < N; ++i)
    {
        int idx = (ch.inputWritePos - N + i + (int)ch.inputBuf.size()) % (int)ch.inputBuf.size();
        fftData[(size_t)i] = ch.inputBuf[(size_t)idx] * hannWindow[(size_t)i];
    }

    fft->performRealOnlyForwardTransform (fftData.data(), true);

    // Analysis: magnitude + true frequency
    std::vector<float> mag  (bins, 0.0f);
    std::vector<float> freq (bins, 0.0f);
    for (int k = 0; k < bins; ++k)
    {
        float re = fftData[(size_t)(k * 2)];
        float im = fftData[(size_t)(k * 2 + 1)];
        mag[(size_t)k] = std::sqrt (re * re + im * im);

        float phase     = std::atan2 (im, re);
        float phaseDiff = phase - ch.lastAnalysisPhase[(size_t)k];
        ch.lastAnalysisPhase[(size_t)k] = phase;

        float expected  = twoPi * (float)k * (float)currentHopSize / (float)N;
        float deltaPhi  = phaseDiff - expected;
        deltaPhi -= twoPi * std::round (deltaPhi / twoPi);
        freq[(size_t)k] = ((float)k + deltaPhi * (float)N / (twoPi * (float)currentHopSize))
                          * (float)currentSampleRate / (float)N;
    }

    // Optional formant envelope capture
    bool doFormant = static_cast<bool> (apvts.getRawParameterValue ("formant")->load());
    std::vector<float> envOrig;
    if (doFormant)
        computeSpectralEnvelope (mag, envOrig, 30);

    // Bin scatter by ratio
    std::vector<float> magOut  (bins, 0.0f);
    std::vector<float> freqOut (bins, 0.0f);
    for (int k = 0; k < bins; ++k)
    {
        int kNew = (int)std::round ((float)k * ratio);
        if (kNew >= 0 && kNew < bins && mag[(size_t)k] > magOut[(size_t)kNew])
        {
            magOut [(size_t)kNew] = mag [(size_t)k];
            freqOut[(size_t)kNew] = freq[(size_t)k] * ratio;
        }
    }

    // Formant correction
    if (doFormant)
    {
        std::vector<float> envShifted;
        computeSpectralEnvelope (magOut, envShifted, 30);
        for (int k = 0; k < bins; ++k)
            magOut[(size_t)k] *= envOrig[(size_t)k] / (envShifted[(size_t)k] + 1e-9f);
    }

    // Synthesis phase accumulation + IFFT
    std::vector<float> synthData (N * 2, 0.0f);
    for (int k = 0; k < bins; ++k)
    {
        float expected  = twoPi * (float)k * (float)currentHopSize / (float)N;
        float deltaPhi  = (freqOut[(size_t)k] * (float)N / (float)currentSampleRate - (float)k)
                          * twoPi * (float)currentHopSize / (float)N;
        ch.synthesisPhase[(size_t)k] += expected + deltaPhi;
        float sp = ch.synthesisPhase[(size_t)k];
        synthData[(size_t)(k * 2)]     = magOut[(size_t)k] * std::cos (sp);
        synthData[(size_t)(k * 2 + 1)] = magOut[(size_t)k] * std::sin (sp);
    }
    fft->performRealOnlyInverseTransform (synthData.data());

    // Overlap-add
    float gain = (float)currentHopSize / ((float)N * 0.5f);
    for (int i = 0; i < N; ++i)
    {
        int idx = (ch.outputWritePos + i) % (int)ch.outputBuf.size();
        ch.outputBuf[(size_t)idx] += synthData[(size_t)i] * hannWindow[(size_t)i] * gain;
    }
    ch.outputWritePos = (ch.outputWritePos + currentHopSize) % (int)ch.outputBuf.size();
}

void NovaPitchAudioProcessor::processPVChannel (
    PVChannel& ch, const float* in, float* out, int numSamples, float ratio)
{
    for (int s = 0; s < numSamples; ++s)
    {
        ch.inputBuf[(size_t)ch.inputWritePos] = in[s];
        ch.inputWritePos = (ch.inputWritePos + 1) % (int)ch.inputBuf.size();
        ch.samplesSinceLastHop++;

        if (ch.samplesSinceLastHop >= currentHopSize)
        {
            ch.samplesSinceLastHop = 0;
            runPVFrame (ch, ratio);
        }

        out[s] = ch.outputBuf[(size_t)ch.outputReadPos];
        ch.outputBuf[(size_t)ch.outputReadPos] = 0.0f;
        ch.outputReadPos = (ch.outputReadPos + 1) % (int)ch.outputBuf.size();
    }
}

// ---------------------------------------------------------------------------
// processBlock
// ---------------------------------------------------------------------------
void NovaPitchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Reinit if low-latency mode changed
    bool llWanted  = static_cast<bool> (apvts.getRawParameterValue ("lowLatency")->load());
    bool llCurrent = (currentFftSize == 512);
    if (llWanted != llCurrent)
        initPV (llWanted);

    // Feed mono sum into YIN ring buffer
    const float* chL = buffer.getReadPointer (0);
    const float* chR = (numChannels > 1) ? buffer.getReadPointer (1) : chL;
    for (int s = 0; s < numSamples; ++s)
    {
        yinBuf[(size_t)yinWritePos] = (chL[s] + chR[s]) * 0.5f;
        yinWritePos = (yinWritePos + 1) % yinBufferSize;
    }

    // YIN detection
    float detectedHz = -1.0f;
    {
        std::vector<float> linear (yinBufferSize);
        for (int i = 0; i < yinBufferSize; ++i)
            linear[(size_t)i] = yinBuf[(size_t)((yinWritePos + i) % yinBufferSize)];
        detectedHz = detectYIN (linear.data(), yinBufferSize);
    }

    if (detectedHz > 0.0f)
    {
        if (smoothedDetectedHz > 0.0f)
        {
            // Adaptive smoothing: faster on large pitch jumps (new note),
            // slower on small deviations (vibrato/drift) — like MetaTune's detector
            float semiDist = std::abs (hzToMidiF (detectedHz) - hzToMidiF (smoothedDetectedHz));
            float alpha = (semiDist > 2.0f) ? 0.55f   // big jump → snap quickly to new note
                        : (semiDist > 0.5f) ? 0.25f   // moderate → track moderately
                                            : 0.10f;  // small wobble → smooth it out
            smoothedDetectedHz = alpha * detectedHz + (1.0f - alpha) * smoothedDetectedHz;
        }
        else
        {
            smoothedDetectedHz = detectedHz;
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

        // Cents deviation from target
        float detectedMidi = hzToMidiF (smoothedDetectedHz);
        float centsDiff    = (hzToMidiF (targetHz) - detectedMidi) * 100.0f;

        // Deadband (tolerance): if within N cents, don't correct at all
        // tolerance=0 → 0 cents deadband (always correct), tolerance=1 → 50 cents deadband
        float deadbandCents = tolerance * 50.0f;

        if (std::abs (centsDiff) <= deadbandCents)
        {
            // Inside deadband — glide ratio back toward 1.0 gently
            pitchRatioSmoothed += (1.0f - pitchRatioSmoothed) * 0.08f;
        }
        else
        {
            // Outside deadband — MetaTune-style speed-based correction:
            // Amount controls how fast (semitones/second) we move toward the target.
            // amount=1.0 → instant snap, amount=0 → ~0.3 semitones/sec
            float targetRatio = targetHz / (smoothedDetectedHz + 1e-9f);

            // Absolute pitch ratio — what the vocoder needs to bring voice to targetHz
            float fullRatio = targetHz / (smoothedDetectedHz + 1e-9f);

            // Soft deadband: scale correction to zero inside the band, full outside
            float absCents  = std::abs (centsDiff);
            float bandScale = juce::jlimit (0.0f, 1.0f,
                                (absCents - deadbandCents) / (deadbandCents + 1.0f));

            float targetRatio = 1.0f + (fullRatio - 1.0f) * bandScale;

            // Exponential smoothing — amount controls speed (MetaTune feel)
            // amount=0 → α≈0.005 (slow glide), amount=1 → α=0.7 (fast, near-instant)
            float alpha = 0.005f + amount * amount * 0.695f;
            pitchRatioSmoothed += (targetRatio - pitchRatioSmoothed) * alpha;
        }

        // Vibrato preservation: blend some of the original (uncorrected) ratio back in
        // vibrato=0 → full correction, vibrato=1 → no correction (pass-through)
        if (vibratoAmt > 0.0f)
            pitchRatioSmoothed = pitchRatioSmoothed * (1.0f - vibratoAmt) + 1.0f * vibratoAmt;

        ratio = pitchRatioSmoothed;
    }
    else
    {
        // No pitch detected — glide back to unity
        pitchRatioSmoothed += (1.0f - pitchRatioSmoothed) * 0.08f;
        ratio = pitchRatioSmoothed;
    }

    ratio = juce::jlimit (0.5f, 2.0f, ratio);

    pitchHistory[(size_t)historyIndex].store (smoothedDetectedHz);
    historyIndex = (historyIndex + 1) % pitchHistorySize;

    // Phase vocoder
    if (numChannels >= 2)
    {
        juce::AudioBuffer<float> outBuf (2, numSamples);
        outBuf.clear();
        processPVChannel (pvL, buffer.getReadPointer (0), outBuf.getWritePointer (0), numSamples, ratio);
        processPVChannel (pvR, buffer.getReadPointer (1), outBuf.getWritePointer (1), numSamples, ratio);
        buffer.copyFrom (0, 0, outBuf, 0, 0, numSamples);
        buffer.copyFrom (1, 0, outBuf, 1, 0, numSamples);
    }
    else if (numChannels == 1)
    {
        juce::AudioBuffer<float> outBuf (1, numSamples);
        outBuf.clear();
        processPVChannel (pvL, buffer.getReadPointer (0), outBuf.getWritePointer (0), numSamples, ratio);
        buffer.copyFrom (0, 0, outBuf, 0, 0, numSamples);
    }
}

// ---------------------------------------------------------------------------
// Bus layout
// ---------------------------------------------------------------------------
bool NovaPitchAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

// ---------------------------------------------------------------------------
// Standard boilerplate
// ---------------------------------------------------------------------------
const juce::String NovaPitchAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaPitchAudioProcessor::acceptsMidi()  const { return false; }
bool NovaPitchAudioProcessor::producesMidi() const { return false; }
bool NovaPitchAudioProcessor::isMidiEffect() const { return false; }
double NovaPitchAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int NovaPitchAudioProcessor::getNumPrograms()                             { return 1; }
int NovaPitchAudioProcessor::getCurrentProgram()                          { return 0; }
void NovaPitchAudioProcessor::setCurrentProgram (int)                    {}
const juce::String NovaPitchAudioProcessor::getProgramName (int)         { return {}; }
void NovaPitchAudioProcessor::changeProgramName (int, const juce::String&) {}

bool NovaPitchAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* NovaPitchAudioProcessor::createEditor()
{
    return new NovaPitchAudioProcessorEditor (*this);
}

void NovaPitchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaPitchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaPitchAudioProcessor();
}
