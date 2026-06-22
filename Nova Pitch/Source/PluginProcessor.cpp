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

    // YIN reset
    yinBuf.assign    (yinBufferSize, 0.0f);
    yinLinear.assign (yinBufferSize, 0.0f);
    yinD.assign      (yinBufferSize / 2, 0.0f);
    yinCmnd.assign   (yinBufferSize / 2, 0.0f);

    yinWritePos           = 0;
    smoothedDetectedHz    = 0.0f;
    blocksSinceValidPitch = 0;
    correctionActive      = false;
    lastTargetMidi        = -1;
    noteTargetRatio       = 1.0f;
    historyIndex          = 0;
    ph5index              = 0;
    pitchHistory5.fill (0.0f);

    // Phase vocoder init
    pvFFT = std::make_unique<juce::dsp::FFT> (kPVOrder);

    pvWindow.resize (kPVN);
    for (int i = 0; i < kPVN; ++i)
        pvWindow[i] = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi
                                                * static_cast<float> (i) / static_cast<float> (kPVN)));

    for (int ch = 0; ch < 2; ++ch)
    {
        pvInBuf[ch].assign    (kPVBufSz, 0.0f);
        pvOutBuf[ch].assign   (kPVBufSz, 0.0f);
        pvLastPhase[ch].assign  (kPVBins, 0.0f);
        pvSynthPhase[ch].assign (kPVBins, 0.0f);
    }

    pvAnaMag.assign (kPVBins, 0.0f);
    pvAnaFreq.assign (kPVBins, 0.0f);
    pvSynMag.assign (kPVBins, 0.0f);
    pvSynFreq.assign (kPVBins, 0.0f);
    pvWork.assign (kPVBufSz, 0.0f);   // 2*kPVN interleaved complex floats

    pvInWrite  = 0;
    pvOutWrite = 0;
    // Read pointer lags write pointer by kPVN — this is the plugin latency
    pvOutRead  = kPVBufSz - kPVN;
    pvHopCount = kPVN;   // wait one full frame before first process

    setLatencySamples (kPVN);  // 2048 samples at 48 kHz ≈ 42 ms
}

void NovaPitchAudioProcessor::releaseResources() {}

// ---------------------------------------------------------------------------
// Phase vocoder frame processor
//   ratio > 1 → shift pitch up, ratio < 1 → shift pitch down
//   Called every kPVHop samples.
// ---------------------------------------------------------------------------
void NovaPitchAudioProcessor::processPhaseVocoderFrame (float ratio)
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    constexpr float pi    = juce::MathConstants<float>::pi;
    constexpr float twoPiOverN = twoPi / static_cast<float> (kPVN);

    // OLA normalization: Hann window at 4x overlap sums to ~2.0 per point,
    // JUCE IFFT divides by N, so net scale per frame needs to be 2*H/N = 0.5
    constexpr float kNorm = 2.0f * static_cast<float> (kPVHop) / static_cast<float> (kPVN);

    for (int ch = 0; ch < 2; ++ch)
    {
        // ── 1. Build windowed analysis frame ───────────────────────────────
        // performRealOnlyForwardTransform expects the real signal packed into
        // the FIRST N positions, with the second N positions zeroed.
        for (int i = 0; i < kPVN; ++i)
        {
            const int ri = (pvInWrite - kPVN + i) & kPVBufMsk;
            pvWork[i] = pvInBuf[ch][ri] * pvWindow[i];
        }
        std::fill (pvWork.begin() + kPVN, pvWork.end(), 0.0f);

        // ── 2. Forward FFT ─────────────────────────────────────────────────
        pvFFT->performRealOnlyForwardTransform (pvWork.data());
        // Output: interleaved complex in pvWork[0..2*kPVN-1]

        // ── 3. Analysis: compute magnitude + instantaneous frequency ────────
        for (int k = 0; k < kPVBins; ++k)
        {
            const float re  = pvWork[2 * k];
            const float im  = pvWork[2 * k + 1];
            const float mag = std::sqrt (re * re + im * im);
            const float phi = std::atan2 (im, re);

            // Phase difference from previous frame
            float delta = phi - pvLastPhase[ch][k];
            pvLastPhase[ch][k] = phi;

            // Remove expected phase advance for bin k (= k * 2π/N * H)
            const float expected = static_cast<float> (k) * twoPiOverN * static_cast<float> (kPVHop);
            delta -= expected;

            // Wrap to [-π, π]
            delta -= twoPi * std::round (delta / twoPi);

            // True instantaneous frequency in rad/sample
            pvAnaFreq[k] = static_cast<float> (k) * twoPiOverN + delta / static_cast<float> (kPVHop);
            pvAnaMag[k]  = mag;
        }

        // ── 4. Spectral pitch shift ────────────────────────────────────────
        std::fill (pvSynMag.begin(),  pvSynMag.end(),  0.0f);
        std::fill (pvSynFreq.begin(), pvSynFreq.end(), 0.0f);

        for (int k = 0; k < kPVBins; ++k)
        {
            // Map analysis bin k to synthesis bin j = k * ratio
            const float fj = static_cast<float> (k) * ratio;
            const int   j  = static_cast<int> (fj);

            // Linear interpolation into adjacent bins for smoother shifting
            if (j >= 0 && j < kPVBins - 1)
            {
                const float frac = fj - static_cast<float> (j);
                const float mag  = pvAnaMag[k];
                const float freq = pvAnaFreq[k] * ratio;

                if (mag > pvSynMag[j])
                {
                    pvSynMag[j]  = mag * (1.0f - frac);
                    pvSynFreq[j] = freq;
                }
                if (mag * frac > pvSynMag[j + 1])
                {
                    pvSynMag[j + 1]  = mag * frac;
                    pvSynFreq[j + 1] = freq;
                }
            }
            else if (j == kPVBins - 1 && pvAnaMag[k] > pvSynMag[j])
            {
                pvSynMag[j]  = pvAnaMag[k];
                pvSynFreq[j] = pvAnaFreq[k] * ratio;
            }
        }

        // ── 5. Synthesis: accumulate phase, build IFFT input ───────────────
        for (int j = 0; j < kPVBins; ++j)
        {
            pvSynthPhase[ch][j] += pvSynFreq[j] * static_cast<float> (kPVHop);
            pvWork[2 * j]     = pvSynMag[j] * std::cos (pvSynthPhase[ch][j]);
            pvWork[2 * j + 1] = pvSynMag[j] * std::sin (pvSynthPhase[ch][j]);
        }

        // Fill conjugate symmetric bins for IFFT (bins N/2+1 .. N-1)
        for (int k = 1; k < kPVN / 2; ++k)
        {
            pvWork[2 * (kPVN - k)]     =  pvWork[2 * k];
            pvWork[2 * (kPVN - k) + 1] = -pvWork[2 * k + 1];
        }

        // ── 6. Inverse FFT ─────────────────────────────────────────────────
        pvFFT->performRealOnlyInverseTransform (pvWork.data());
        // Real output at pvWork[2*i] for i = 0..kPVN-1 (JUCE divides by N)

        // ── 7. Overlap-add ────────────────────────────────────────────────
        // performRealOnlyInverseTransform writes real output to the FIRST N
        // positions (packed, not interleaved).  Read pvWork[i], not pvWork[2*i].
        // Analysis window is baked in (IFFT(FFT(w·x))=w·x); no synthesis window.
        // Hann at 4× overlap sums to ≈2.0 per point → kNorm = 0.5 for unity gain.
        for (int i = 0; i < kPVN; ++i)
        {
            const int wi = (pvOutWrite + i) & kPVBufMsk;
            pvOutBuf[ch][wi] += pvWork[i] * kNorm;
        }
    }

    pvOutWrite = (pvOutWrite + kPVHop) & kPVBufMsk;
}

// ---------------------------------------------------------------------------

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

    const float* chL = buffer.getReadPointer (0);
    const float* chR = (numChannels > 1) ? buffer.getReadPointer (1) : chL;

    // ── Pitch detection (YIN on mono mix) ──────────────────────────────────
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

    // ── Correction ratio ────────────────────────────────────────────────────
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

    float* outL = buffer.getWritePointer (0);
    float* outR = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

    // YIN already has its own IIR (smoothedDetectedHz).  Adding another smoothing
    // layer on the ratio causes double-filtering → overshoot → audible wobble.
    // Pass ratio directly; the phase vocoder's phase accumulation handles gradual
    // transitions naturally.
    const float frameRatio = juce::jlimit (0.5f, 2.0f, ratio);

    // ── Per-sample phase vocoder I/O ────────────────────────────────────────
    for (int s = 0; s < numSamples; ++s)
    {
        // Write stereo input to ring buffers
        const int wi = pvInWrite & kPVBufMsk;
        pvInBuf[0][wi] = chL[s];
        pvInBuf[1][wi] = chR[s];
        pvInWrite++;

        // Fire a new phase vocoder frame every kPVHop input samples
        if (--pvHopCount <= 0)
        {
            processPhaseVocoderFrame (frameRatio);
            pvHopCount = kPVHop;
        }

        // Read output and clear so OLA accumulation stays clean
        const int ro = pvOutRead & kPVBufMsk;
        outL[s] = pvOutBuf[0][ro];
        if (outR) outR[s] = pvOutBuf[1][ro];
        pvOutBuf[0][ro] = 0.0f;
        pvOutBuf[1][ro] = 0.0f;
        pvOutRead++;
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
