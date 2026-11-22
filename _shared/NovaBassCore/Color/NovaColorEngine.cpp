#include "NovaColorEngine.h"
#include <algorithm>
#include <cmath>

namespace nova::bass
{

void NovaColorEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    preparedNumChannels = (int) spec.numChannels;

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        spec.numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
    oversampler->initProcessing (spec.maximumBlockSize);

    const double oversampledRate = spec.sampleRate * (double) oversampler->getOversamplingFactor();
    constexpr float topSmoothingCutoffHz = 5000.0f;
    smoothingCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * topSmoothingCutoffHz / (float) oversampledRate);

    smoothingState.assign ((size_t) preparedNumChannels, 0.0f);

    reset();
}

void NovaColorEngine::reset() noexcept
{
    if (oversampler) oversampler->reset();
    std::fill (smoothingState.begin(), smoothingState.end(), 0.0f);
}

void NovaColorEngine::setParams (NovaColorMode mode, float drive01) noexcept
{
    currentMode = mode;
    currentDrive01 = juce::jlimit (0.0f, 1.0f, drive01);
}

NovaColorEngine::ModeProfile NovaColorEngine::getModeProfile (NovaColorMode mode) noexcept
{
    switch (mode)
    {
        case NovaColorMode::Clean:  return { 0.6f, 0.04f, 0.35f, 1.5f };
        case NovaColorMode::Gritty: return { 1.6f, 0.25f, 0.02f, 5.0f };
        case NovaColorMode::Warm:
        default:                    return { 1.0f, 0.12f, 0.15f, 3.0f };
    }
}

// Nova Heat's asymmetric-tanh curve (Source/PluginProcessor.cpp), ported
// verbatim in shape - odd-order asymmetric skew before the tanh gives even
// harmonics (the "warmth"), and the positive/negative branches use
// slightly different effective drive so the curve isn't symmetric about
// zero (matches how real analog saturation behaves).
float NovaColorEngine::applySaturation (float sample, float k, const ModeProfile& profile) noexcept
{
    const float cubic = sample * sample * sample;
    const float skewed = sample + (profile.asymmetry * 0.35f * cubic);
    if (sample >= 0.0f)
        return std::tanh (skewed * (k + profile.asymmetry));
    return std::tanh (skewed * juce::jmax (0.2f, k - (profile.asymmetry * 0.5f)));
}

void NovaColorEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (oversampler == nullptr) return;

    const ModeProfile profile = getModeProfile (currentMode);

    // Skip the oversampled nonlinear pass entirely when the DRIVE knob is
    // near zero - matches Nova Apex's `needsOversample` gating precedent
    // (its condition is driveVal-only, not mode-dependent: k stays close
    // to 1.0 for every mode when drive01 is near zero, so oversampling
    // buys nothing there regardless of which mode is selected).
    if (currentDrive01 < 0.01f)
        return;

    const int nCh = juce::jmin (buffer.getNumChannels(), preparedNumChannels);
    const int numSamples = buffer.getNumSamples();
    if (nCh == 0 || numSamples == 0) return;

    // Effective drive coefficient: mode's own multiplier scales how much
    // the DRIVE knob's 0-1 range pushes into the curve, so the same knob
    // position sounds different per mode (spec section 21).
    const float k = 1.0f + profile.driveMultiplier * currentDrive01 * 3.0f;
    const float makeupGainLinear = juce::Decibels::decibelsToGain (-profile.makeupGainDbAtFullDrive * currentDrive01);

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) nCh, (size_t) numSamples);
    auto upBlock = oversampler->processSamplesUp (block);

    for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch)
    {
        float* d = upBlock.getChannelPointer (ch);
        float& smoothState = smoothingState[juce::jmin (ch, smoothingState.size() - 1)];

        for (size_t i = 0; i < upBlock.getNumSamples(); ++i)
        {
            const float saturated = applySaturation (d[i], k, profile);

            // Top-end damping: blend the saturated sample with a one-pole
            // lowpassed version of itself, by profile.topSmoothing amount -
            // CLEAN rolls off more of the new harmonic energy at the top,
            // GRITTY leaves nearly all of it in.
            smoothState = smoothState + (1.0f - smoothingCoeff) * (saturated - smoothState);
            const float smoothed = saturated + (smoothState - saturated) * profile.topSmoothing;

            d[i] = smoothed * makeupGainLinear;
        }
    }

    oversampler->processSamplesDown (block);
}

} // namespace nova::bass
