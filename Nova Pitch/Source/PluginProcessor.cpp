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
    correctionActive      = false;
    lastTargetMidi        = -1;
    noteTargetRatio       = 1.0f;
    pitchLockBlocks       = 0;
    historyIndex          = 0;
    ph5index              = 0;
    pitchHistory5.fill (0.0f);

    // Phase vocoder init
    pvFFT = std::make_unique<juce::dsp::FFT> (11);  // 2^11 = 2048

    pvInBufL.fill  (0.0f);  pvInBufR.fill  (0.0f);
    pvOutBufL.fill (0.0f);  pvOutBufR.fill (0.0f);
    pvWork.fill    (0.0f);

    for (int i = 0; i < kPvN; ++i)
        pvWin[i] = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi
                                             * static_cast<float> (i) / kPvN));

    pvLastPhL.fill (0.0f);  pvLastPhR.fill (0.0f);
    pvSynthPhL.fill(0.0f);  pvSynthPhR.fill(0.0f);
    pvTmpMag.fill  (0.0f);  pvTmpPh.fill   (0.0f);
    pvTmpFreq.fill (0.0f);  pvOutMag.fill  (0.0f);  pvOutFreq.fill (0.0f);

    pvInWrite   = 0;
    pvOutWrite  = kPvN;  // write kPvN ahead of read → declared latency
    pvOutRead   = 0;
    pvHopCount  = 0;
    pvFirstFrame = true;

    setLatencySamples (kPvN);  // 2048 samples
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

    const float safeRatio = juce::jlimit (0.5f, 2.0f, ratio);
    const float twoPi     = juce::MathConstants<float>::twoPi;
    const float pi        = juce::MathConstants<float>::pi;

    // ── Phase Vocoder pitch shift ───────────────────────────────────────────
    // FFT size kPvN=2048, hop kPvH=512 (4× Hann overlap).
    // kPvNorm=0.5 normalises the OLA sum back to unity (Hann 4× sums to 2.0).
    // Latency declared as kPvN = 2048 samples; host PDC compensates.
    for (int s = 0; s < numSamples; ++s)
    {
        pvInBufL[pvInWrite] = chL[s];
        pvInBufR[pvInWrite] = chR[s];
        pvInWrite = (pvInWrite + 1) & kPvMask;

        if (--pvHopCount <= 0)
        {
            pvHopCount = kPvH;
            const bool isFirst = pvFirstFrame;
            pvFirstFrame = false;

            for (int ch = 0; ch < 2; ++ch)
            {
                auto& inBuf   = (ch == 0) ? pvInBufL  : pvInBufR;
                auto& outBuf  = (ch == 0) ? pvOutBufL : pvOutBufR;
                auto& lastPh  = (ch == 0) ? pvLastPhL  : pvLastPhR;
                auto& synthPh = (ch == 0) ? pvSynthPhL : pvSynthPhR;

                // Analysis: window the most-recent kPvN samples → pvWork
                for (int i = 0; i < kPvN; ++i)
                {
                    int idx = (pvInWrite - kPvN + i + kPvN * 2) & kPvMask;
                    pvWork[i] = inBuf[idx] * pvWin[i];
                }
                std::fill (pvWork.begin() + kPvN, pvWork.end(), 0.0f);

                // Forward FFT — onlyNonNeg=true fills pvWork[0..2*kPvBins-1]
                pvFFT->performRealOnlyForwardTransform (pvWork.data(), true);

                // Extract magnitude, phase, instantaneous frequency per bin
                for (int k = 0; k < kPvBins; ++k)
                {
                    float re = pvWork[2 * k];
                    float im = pvWork[2 * k + 1];
                    pvTmpMag[k] = std::sqrt (re * re + im * im);
                    pvTmpPh[k]  = std::atan2 (im, re);

                    float delta = pvTmpPh[k] - lastPh[k];
                    lastPh[k]   = pvTmpPh[k];
                    float omega  = twoPi * static_cast<float> (k) / kPvN;
                    delta       -= omega * kPvH;
                    delta       -= twoPi * std::floor ((delta + pi) / twoPi);  // wrap to [-π,π]
                    pvTmpFreq[k] = omega + delta / kPvH;
                }

                // Spectral shift — reverse interpolated mapping eliminates spectral holes:
                // For each output bin j, pull from fractional input bin k = j / ratio.
                // Linear interpolation of magnitude and true frequency avoids the
                // comb-filter "toilet bowl" artefact that forward round() mapping causes.
                pvOutMag.fill  (0.0f);
                pvOutFreq.fill (0.0f);
                for (int j = 0; j < kPvBins; ++j)
                {
                    float kf   = static_cast<float> (j) / safeRatio;
                    int   k0   = static_cast<int> (kf);
                    float frac = kf - static_cast<float> (k0);
                    int   k1   = std::min (k0 + 1, kPvBins - 1);

                    if (k0 >= 0 && k0 < kPvBins)
                    {
                        pvOutMag[j]  = pvTmpMag[k0]  * (1.0f - frac) + pvTmpMag[k1]  * frac;
                        pvOutFreq[j] = (pvTmpFreq[k0] * (1.0f - frac) + pvTmpFreq[k1] * frac)
                                       * safeRatio;
                    }
                }

                // Phase accumulation with Laroche-Dolson identity phase locking.
                // Without locking each bin accumulates phase independently, so harmonics
                // drift in/out of phase → "choir / multiple voices" artefact.
                // Locking forces all bins in a spectral peak's region to evolve at the
                // peak's instantaneous frequency, preserving harmonic phase relationships.
                if (isFirst)
                {
                    // Seed synthesis phase via same reverse interpolation of analysis phase
                    for (int j = 0; j < kPvBins; ++j)
                    {
                        float kf   = static_cast<float> (j) / safeRatio;
                        int   k0   = static_cast<int> (kf);
                        float frac = kf - static_cast<float> (k0);
                        int   k1   = std::min (k0 + 1, kPvBins - 1);
                        synthPh[j] = k0 < kPvBins
                                     ? pvTmpPh[k0] * (1.0f - frac) + pvTmpPh[k1] * frac
                                     : 0.0f;
                    }
                }
                else
                {
                    // Standard per-bin phase accumulation (candidate phases)
                    for (int j = 0; j < kPvBins; ++j)
                        synthPh[j] += pvOutFreq[j] * kPvH;

                    // ── Identity phase locking ──────────────────────────────────────────
                    // For each bin j find its nearest spectral peak p and override:
                    //   synthPh[j] = synthPh[p] + (j-p) * pvOutFreq[p] * kPvH
                    // so every bin in the peak's cluster advances at the same rate.
                    //
                    // Temp arrays repurposed (analysis data no longer needed):
                    //   pvTmpMag[j]  = distance to nearest left peak (or kPvBins if none)
                    //   pvTmpPh[j]   = phase locked from nearest left peak
                    //   pvTmpFreq[j] = final locked phase (closer of left / right peak)

                    constexpr float kPeakThresh = 1e-8f;

                    // Left pass: propagate each peak's phase rightward
                    {
                        int   lp   = -1;
                        float lpF  = 0.0f;
                        float lpPh = 0.0f;
                        for (int j = 0; j < kPvBins; ++j)
                        {
                            bool isPeak = pvOutMag[j] > kPeakThresh
                                       && (j == 0         || pvOutMag[j] >= pvOutMag[j - 1])
                                       && (j == kPvBins-1 || pvOutMag[j] >  pvOutMag[j + 1]);
                            if (isPeak) { lp = j; lpPh = synthPh[j]; lpF = pvOutFreq[j]; }

                            if (lp >= 0)
                            {
                                pvTmpMag[j] = static_cast<float> (j - lp);
                                pvTmpPh[j]  = lpPh + static_cast<float> (j - lp) * lpF * kPvH;
                            }
                            else
                            {
                                pvTmpMag[j] = static_cast<float> (kPvBins);
                                pvTmpPh[j]  = synthPh[j];
                            }
                        }
                    }

                    // Right pass: propagate each peak's phase leftward; take closer peak
                    {
                        int   rp   = -1;
                        float rpF  = 0.0f;
                        float rpPh = 0.0f;
                        for (int j = kPvBins - 1; j >= 0; --j)
                        {
                            bool isPeak = pvOutMag[j] > kPeakThresh
                                       && (j == kPvBins-1 || pvOutMag[j] >= pvOutMag[j + 1])
                                       && (j == 0         || pvOutMag[j] >  pvOutMag[j - 1]);
                            if (isPeak) { rp = j; rpPh = synthPh[j]; rpF = pvOutFreq[j]; }

                            if (rp >= 0 && static_cast<float> (rp - j) < pvTmpMag[j])
                                pvTmpFreq[j] = rpPh + static_cast<float> (j - rp) * rpF * kPvH;
                            else
                                pvTmpFreq[j] = pvTmpPh[j];
                        }
                    }

                    // Apply locked phases
                    for (int j = 0; j < kPvBins; ++j)
                        synthPh[j] = pvTmpFreq[j];
                }

                // Synthesise: build complex spectrum from output magnitudes + accumulated phases
                for (int j = 0; j < kPvBins; ++j)
                {
                    pvWork[2 * j]     = pvOutMag[j] * std::cos (synthPh[j]);
                    pvWork[2 * j + 1] = pvOutMag[j] * std::sin (synthPh[j]);
                }
                pvWork[1]              = 0.0f;  // DC imaginary must be zero (real signal)
                pvWork[2 * kPvBins - 1] = 0.0f; // Nyquist imaginary must be zero

                // Inverse FFT — writes real to pvWork[0..kPvN-1]
                pvFFT->performRealOnlyInverseTransform (pvWork.data());

                // OLA accumulate (kPvNorm=0.5 compensates for 4× Hann sum = 2.0)
                for (int i = 0; i < kPvN; ++i)
                {
                    int idx = (pvOutWrite + i) & kPvMask;
                    outBuf[idx] += pvWork[i] * kPvNorm;
                }
            }

            pvOutWrite = (pvOutWrite + kPvH) & kPvMask;
        }

        // Read output sample and clear the slot
        outL[s] = pvOutBufL[pvOutRead];
        if (outR) outR[s] = pvOutBufR[pvOutRead];
        pvOutBufL[pvOutRead] = 0.0f;
        pvOutBufR[pvOutRead] = 0.0f;
        pvOutRead = (pvOutRead + 1) & kPvMask;
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
