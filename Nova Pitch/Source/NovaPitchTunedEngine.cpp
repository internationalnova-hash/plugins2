#include "NovaPitchTunedEngine.h"

#include <cmath>

namespace
{
    int maskForScale (nova::pitch::Scale s) noexcept
    {
        switch (s)
        {
            case nova::pitch::Scale::Major:      return 0xAB5; // 0 2 4 5 7 9 11
            case nova::pitch::Scale::Minor:      return 0x5AD; // 0 2 3 5 7 8 10
            case nova::pitch::Scale::Pentatonic: return 0x295; // 0 2 4 7 9
            case nova::pitch::Scale::Blues:      return 0x4E9; // 0 3 5 6 7 10
            case nova::pitch::Scale::Chromatic:
            default:                             return 0xFFF;
        }
    }

    float midiToHz (float m) noexcept { return m > 0.f ? 440.f * std::pow (2.f, (m - 69.f) / 12.f) : 0.f; }
}

void NovaPitchTunedEngine::allocate (double sampleRate, int maxBlockSize, bool lowLatencyMode)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    maxBlock = juce::jmax (64, maxBlockSize);
    lowLatency = lowLatencyMode;

    tuner.prepare (sr, maxBlock, lowLatency);
    warpL.prepare (sr);
    warpR.prepare (sr);

    tL.assign ((size_t) maxBlock, 0.f);
    tR.assign ((size_t) maxBlock, 0.f);
    drySize = juce::jmax (1, tuner.getLatencySamples() + 1);
    dryL.assign ((size_t) drySize, 0.f);
    dryR.assign ((size_t) drySize, 0.f);
    dryWrite = 0;

    mixSm.reset (sr, 0.02);
    outSm.reset (sr, 0.02);
    activeSm.reset (sr, 0.03);
}

void NovaPitchTunedEngine::prepare (double sampleRate, int maxBlockSize, bool lowLatencyMode)
{
    allocate (sampleRate, maxBlockSize, lowLatencyMode);
    reset();
}

void NovaPitchTunedEngine::reconfigureIfNeeded (double sampleRate, bool desiredLowLatencyMode)
{
    if (desiredLowLatencyMode == lowLatency && std::abs (sampleRate - sr) < 1.0) return;
    allocate (sampleRate, maxBlock, desiredLowLatencyMode);
    reset();
}

void NovaPitchTunedEngine::reset() noexcept
{
    tuner.reset();
    warpL.reset();
    warpR.reset();
    std::fill (dryL.begin(), dryL.end(), 0.f);
    std::fill (dryR.begin(), dryR.end(), 0.f);
    dryWrite = 0;
    mixSm.setCurrentAndTargetValue (mixSm.getTargetValue());
    outSm.setCurrentAndTargetValue (outSm.getTargetValue());
    activeSm.setCurrentAndTargetValue (activeSm.getTargetValue());
}

void NovaPitchTunedEngine::setParams (const Params& p) noexcept
{
    NovaVocalTuner::Params tp;
    tp.enabled   = ! p.bypassed;
    tp.amount    = 1.f;
    const float speed = juce::jlimit (0.f, 1.f, p.retuneSpeedPercent / 100.f);
    tp.retuneMs  = 200.f * speed * speed;                       // 0 = instant hard tune, 50 = 50 ms, 100 = 200 ms
    tp.keyPc     = juce::jlimit (0, 11, p.keyPitchClass);
    tp.scaleMask = maskForScale (p.scale);
    tp.humanize  = juce::jlimit (0.f, 1.f, p.amountPercent / 100.f);
    tp.vibratoPreserve = juce::jlimit (0.f, 1.f, p.vibratoPreservePercent / 100.f);
    tp.vibratoRateHz   = juce::jlimit (2.f, 8.f, p.vibratoRateHz);
    tp.transientHold   = juce::jlimit (0.f, 1.f, p.transientsPercent / 100.f) * 0.8f;
    tp.voicingThreshold = juce::jlimit (0.1f, 0.5f, 0.5f - 0.0036f * p.confidenceThresholdPercent); // 70 % -> 0.25
    tp.maxHz = 350.f + 6.5f * juce::jlimit (0.f, 100.f, p.rangePercent);                             // 350..1000 Hz
    tuner.setParams (tp);
    lastTunerParams = tp;

    formantWarp   = juce::jlimit (-1.f, 1.f, p.formantWarpPercent / 100.f);
    formantAmount = std::abs (formantWarp) > 0.001f ? juce::jlimit (0.f, 1.f, p.formantAmountPercent / 100.f) : 0.f;

    mixSm.setTargetValue (juce::jlimit (0.f, 1.f, p.mixPercent / 100.f));
    outSm.setTargetValue (juce::Decibels::decibelsToGain (p.outputGainDb));
    activeSm.setTargetValue (p.bypassed ? 0.f : 1.f);
}

void NovaPitchTunedEngine::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
{
    warpL.setFormantAmount (formantAmount);
    warpR.setFormantAmount (formantAmount);
    warpL.setFormantWarp (formantWarp);
    warpR.setFormantWarp (formantWarp);
    const bool warpOn = formantAmount > 0.f;
    const int lat = tuner.getLatencySamples();

    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, numSamples - start);

        // Copy first: outL may alias inL (in-place hosts, mono buses)
        for (int i = 0; i < n; ++i)
        {
            tL[(size_t) i] = inL[start + i];
            tR[(size_t) i] = inR[start + i];
        }

        tuner.process (tL.data(), tR.data(), n);

        for (int i = 0; i < n; ++i)
        {
            // Latency-matched dry for mix/bypass
            dryL[(size_t) dryWrite] = inL[start + i];
            dryR[(size_t) dryWrite] = inR[start + i];
            int rd = dryWrite - lat;
            if (rd < 0) rd += drySize;
            const float dL = dryL[(size_t) rd], dR = dryR[(size_t) rd];
            if (++dryWrite >= drySize) dryWrite = 0;

            float wL = tL[(size_t) i], wR = tR[(size_t) i];
            if (warpOn)
            {
                wL = warpL.processPost (warpL.processPre (wL));
                wR = warpR.processPost (warpR.processPre (wR));
            }

            const float wet = mixSm.getNextValue() * activeSm.getNextValue();
            const float g = outSm.getNextValue();
            const float oL = (dL + (wL - dL) * wet) * g;
            const float oR = (dR + (wR - dR) * wet) * g;
            outL[start + i] = oL;
            if (outR != outL) outR[start + i] = oR;
        }
    }
}

float NovaPitchTunedEngine::getDetectedHz() const noexcept  { return midiToHz (tuner.getInputMidi()); }
float NovaPitchTunedEngine::getCorrectedHz() const noexcept { return midiToHz (tuner.getTargetMidi()); }
