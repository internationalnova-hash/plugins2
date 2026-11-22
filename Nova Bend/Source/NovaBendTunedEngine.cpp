#include "NovaBendTunedEngine.h"

#include <cmath>

void NovaBendTunedEngine::allocate (double sampleRate, int maxBlockSize, bool highQuality)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    maxBlock = juce::jmax (64, maxBlockSize);
    highQ = highQuality;

    tuner.prepare (sr, maxBlock, ! highQ); // Live = low-latency tracker
    formantL.prepare (sr);
    formantR.prepare (sr);
    formantL.setFormantAmount (1.f);
    formantR.setFormantAmount (1.f);

    tL.assign ((size_t) maxBlock, 0.f);
    tR.assign ((size_t) maxBlock, 0.f);
    drySize = juce::jmax (1, tuner.getLatencySamples() + 1);
    dryL.assign ((size_t) drySize, 0.f);
    dryR.assign ((size_t) drySize, 0.f);
    dryWrite = 0;

    mixSm.reset (sr, 0.02);
    activeSm.reset (sr, 0.03);
    effectSm.reset (sr, 0.03);
}

void NovaBendTunedEngine::prepare (double sampleRate, int maxBlockSize, bool highQuality)
{
    allocate (sampleRate, maxBlockSize, highQuality);
    reset();
}

void NovaBendTunedEngine::reconfigureIfNeeded (double sampleRate, bool desiredHighQuality)
{
    if (desiredHighQuality == highQ && std::abs (sampleRate - sr) < 1.0) return;
    allocate (sampleRate, maxBlock, desiredHighQuality);
    reset();
}

void NovaBendTunedEngine::reset() noexcept
{
    tuner.reset();
    formantL.reset();
    formantR.reset();
    std::fill (dryL.begin(), dryL.end(), 0.f);
    std::fill (dryR.begin(), dryR.end(), 0.f);
    dryWrite = 0;
    mixSm.setCurrentAndTargetValue (mixSm.getTargetValue());
    activeSm.setCurrentAndTargetValue (activeSm.getTargetValue());
    effectSm.setCurrentAndTargetValue (effectSm.getTargetValue());
}

void NovaBendTunedEngine::setParams (const Params& p) noexcept
{
    params = p;

    NovaVocalTuner::Params tp;
    tp.enabled  = false;  // no scale snapping in Nova Bend
    tp.amount   = 1.f;
    tp.retuneMs = 0.f;    // Pitch and Flatten act immediately
    tp.fixedShiftSemis = p.flatten ? 0.f : juce::jlimit (-12.f, 12.f, p.pitchSemitones);
    tp.flattenMidi = p.flatten ? juce::jlimit (12, 108, (p.flattenOctave + 1) * 12 + juce::jlimit (0, 11, p.flattenNoteIndex)) : -1;
    tuner.setParams (tp);

    // At Pitch 0 / Formant 0 / Flatten off there is nothing to do: output
    // the latency-matched dry signal exactly (not a resynthesis of it).
    const bool neutral = ! p.flatten && std::abs (p.pitchSemitones) < 0.005f && std::abs (p.formantSemitones) < 0.005f;
    effectSm.setTargetValue (neutral ? 0.f : 1.f);
    mixSm.setTargetValue (juce::jlimit (0.f, 1.f, p.mixPercent / 100.f));
    activeSm.setTargetValue (p.bypassed ? 0.f : 1.f);
}

void NovaBendTunedEngine::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
{
    const int lat = tuner.getLatencySamples();

    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, numSamples - start);

        for (int i = 0; i < n; ++i)
        {
            tL[(size_t) i] = inL[start + i];
            tR[(size_t) i] = inR[start + i];
        }

        tuner.process (tL.data(), tR.data(), n);

        // Formants: the shifter moved them by the applied shift; move them
        // to where they should be (own character, or following the pitch
        // when Link is on, plus the Formant knob).
        const float applied = tuner.getCurrentShiftSemis();
        const float wanted  = params.formantSemitones + (params.link ? applied : 0.f);
        const float warp    = juce::jlimit (-24.f, 24.f, wanted - applied);
        const bool  formantOn = std::abs (warp) > 0.05f;
        formantL.setFormantWarpSemitones (warp);
        formantR.setFormantWarpSemitones (warp);

        for (int i = 0; i < n; ++i)
        {
            dryL[(size_t) dryWrite] = inL[start + i];
            dryR[(size_t) dryWrite] = inR[start + i];
            int rd = dryWrite - lat;
            if (rd < 0) rd += drySize;
            const float dL = dryL[(size_t) rd], dR = dryR[(size_t) rd];
            if (++dryWrite >= drySize) dryWrite = 0;

            float wL = tL[(size_t) i], wR = tR[(size_t) i];
            if (formantOn)
            {
                wL = formantL.processPost (formantL.processPre (wL));
                wR = formantR.processPost (formantR.processPre (wR));
            }

            const float wet = effectSm.getNextValue() * mixSm.getNextValue() * activeSm.getNextValue();
            const float oL = dL + (wL - dL) * wet;
            const float oR = dR + (wR - dR) * wet;
            outL[start + i] = oL;
            if (outR != outL) outR[start + i] = oR;
        }
    }
}

float NovaBendTunedEngine::getDetectedHz() const noexcept
{
    const float m = tuner.getInputMidi();
    return m > 0.f ? 440.f * std::pow (2.f, (m - 69.f) / 12.f) : 0.f;
}
