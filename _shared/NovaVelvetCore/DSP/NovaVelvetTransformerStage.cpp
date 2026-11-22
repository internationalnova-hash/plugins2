#include "NovaVelvetTransformerStage.h"
#include <cmath>

namespace nova::velvet
{

void NovaVelvetTransformerStage::configure (float lowSplitHz, float hfSoftenHz) noexcept
{
    configuredLowSplitHz = lowSplitHz;
    configuredHfSoftenHz = hfSoftenHz;
}

void NovaVelvetTransformerStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    const float sr = (float) sampleRate;
    lowSplitCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * configuredLowSplitHz / sr);
    hfSoftenCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * configuredHfSoftenHz / sr);
    dcBlockR = std::exp (-2.0f * juce::MathConstants<float>::pi * kDcBlockHz / sr);
    reset();
}

void NovaVelvetTransformerStage::setHfSoftenHzScale (float scale) noexcept
{
    scale = juce::jmax (0.05f, scale);
    const float effectiveHz = configuredHfSoftenHz * scale;
    hfSoftenCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * effectiveHz / (float) sampleRate);
}

void NovaVelvetTransformerStage::reset() noexcept
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        lowState[ch] = 0.0f;
        hfState[ch] = 0.0f;
        dcX1[ch] = 0.0f;
        dcY1[ch] = 0.0f;
    }
}

void NovaVelvetTransformerStage::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    const int N = buffer.getNumSamples();
    if (nCh == 0 || N == 0)
        return;

    if (driveLevel <= 1.0e-4f)
        return; // Bypass-transparent at zero drive.

    // lfSatDrive scales the LF band into the tanh curve. Dividing the
    // shaped low band by this SAME factor restores exact unity small-
    // signal gain (see NovaVelvetTubeStage's process() comment for the
    // full derivation) - without this, tanh(low*lfSatDrive) alone applies
    // a lfSatDrive-times LINEAR BOOST to quiet low-frequency content
    // specifically (since the "rest of spectrum" high-pass residual stays
    // at pure unity), which is exactly the "exaggerated bass boost" the
    // spec explicitly prohibits - measured directly via
    // NovaVelvetFrequencyResponseValidation as an 8dB+ LF lift at quiet
    // levels before this fix.
    const float lfSatDrive = 1.0f + driveLevel * (1.0f + lfDrive * 2.0f) * 3.0f;
    const float invLfSatDrive = 1.0f / lfSatDrive;

    for (int ch = 0; ch < nCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        float low = lowState[ch];
        float hf = hfState[ch];
        float x1 = dcX1[ch];
        float y1 = dcY1[ch];

        for (int i = 0; i < N; ++i)
        {
            const float x = data[i];

            // Split off the low band, soft-clip it, recombine.
            low = lowSplitCoeff * low + (1.0f - lowSplitCoeff) * x;
            const float satLow = std::tanh (low * lfSatDrive) * invLfSatDrive;
            const float withLfSat = satLow + (x - low);

            // Gentle HF softening: blend toward a lowpassed version by
            // driveLevel (transformer HF rolloff character).
            hf = hfSoftenCoeff * hf + (1.0f - hfSoftenCoeff) * withLfSat;
            const float softened = withLfSat + (hf - withLfSat) * (driveLevel * 0.5f);

            // One-pole DC blocker (defensive - the LF tanh path is
            // odd/symmetric so shouldn't add DC, but this keeps the stage
            // guaranteed DC-free per spec regardless of future tuning).
            const float blocked = softened - x1 + dcBlockR * y1;
            x1 = softened;
            y1 = blocked;

            data[i] = blocked;
        }

        lowState[ch] = low;
        hfState[ch] = hf;
        dcX1[ch] = x1;
        dcY1[ch] = y1;
    }
}

} // namespace nova::velvet
