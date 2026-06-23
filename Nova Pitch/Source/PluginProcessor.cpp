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
    correctionActive = false;
    lastTargetMidi   = -1;
    noteTargetRatio  = 1.0f;
    historyIndex     = 0;
    ph5index         = 0;
    pitchHistory5.fill (0.0f);

    // Dual-head pitch shifter init
    dlBufL.fill (0.0f);
    dlBufR.fill (0.0f);
    dlWrite    = kDlLatency;   // read starts kDlLatency samples behind write
    dlReadA    = 0.0f;
    dlReadB    = 0.0f;
    dlXfading  = false;
    dlXfade    = 0.0f;
    dlXfadeInc = 0.0f;

    setLatencySamples (kDlLatency);
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

    // ── Dual-head TD-PSOLA pitch shift ─────────────────────────────────────
    // Head A reads the delay line at `safeRatio` samples per output sample
    // (Doppler effect → pitch shift).  When the read/write gap drifts more
    // than one pitch period from kDlLatency, we crossfade to head B which is
    // one pitch period away.  For a periodic signal that position is
    // perceptually identical, so the jump is inaudible.
    // No FFT, no phase accumulation, no choir effect.
    // Pitch period for crossfade grain size — clamp to sane range
    const int pitchPeriod = (smoothedDetectedHz >= 80.0f && smoothedDetectedHz <= 1200.0f)
                            ? juce::jlimit (40, 600,
                                  (int) std::round ((float) currentSampleRate / smoothedDetectedHz))
                            : 256;

    for (int s = 0; s < numSamples; ++s)
    {
        // Write new input sample to delay line
        dlBufL[dlWrite & kDlMask] = chL[s];
        dlBufR[dlWrite & kDlMask] = chR[s];
        dlWrite++;

        // ── Read head A (always active) ─────────────────────────────────────
        {
            int   i0  = static_cast<int> (dlReadA) & kDlMask;
            int   i1  = (i0 + 1) & kDlMask;
            float fr  = dlReadA - std::floor (dlReadA);
            outL[s]   = dlBufL[i0] + fr * (dlBufL[i1] - dlBufL[i0]);
            if (outR)
                outR[s] = dlBufR[i0] + fr * (dlBufR[i1] - dlBufR[i0]);
        }

        // ── Crossfade with head B when active ───────────────────────────────
        if (dlXfading)
        {
            int   i0  = static_cast<int> (dlReadB) & kDlMask;
            int   i1  = (i0 + 1) & kDlMask;
            float fr  = dlReadB - std::floor (dlReadB);
            float bL  = dlBufL[i0] + fr * (dlBufL[i1] - dlBufL[i0]);

            // Hann-shaped crossfade (0→1 as dlXfade goes 0→1)
            float w   = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * dlXfade));
            outL[s]   = outL[s] * (1.0f - w) + bL * w;

            if (outR)
            {
                float bR = dlBufR[i0] + fr * (dlBufR[i1] - dlBufR[i0]);
                outR[s]  = outR[s] * (1.0f - w) + bR * w;
            }

            dlReadB   += safeRatio;
            dlXfade   += dlXfadeInc;

            if (dlXfade >= 1.0f)
            {
                // Crossfade complete — B becomes the new A
                dlReadA   = dlReadB;
                dlXfading = false;
                dlXfade   = 0.0f;
            }
        }

        dlReadA += safeRatio;

        // ── Gap check: trigger crossfade when drift exceeds one pitch period ─
        if (!dlXfading)
        {
            float gap = static_cast<float> (dlWrite) - dlReadA;

            if (gap < static_cast<float> (kDlLatency - pitchPeriod))
            {
                // Read caught up to write (upshift) → jump back one period
                dlReadB    = dlReadA - static_cast<float> (pitchPeriod);
                dlXfade    = 0.0f;
                dlXfadeInc = 1.0f / static_cast<float> (pitchPeriod);
                dlXfading  = true;
                // Advance B for this sample (A was already advanced above)
                dlReadB   += safeRatio;
                dlXfade   += dlXfadeInc;
            }
            else if (gap > static_cast<float> (kDlLatency + pitchPeriod))
            {
                // Write lapped read (downshift) → jump forward one period
                dlReadB    = dlReadA + static_cast<float> (pitchPeriod);
                dlXfade    = 0.0f;
                dlXfadeInc = 1.0f / static_cast<float> (pitchPeriod);
                dlXfading  = true;
                dlReadB   += safeRatio;
                dlXfade   += dlXfadeInc;
            }
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
