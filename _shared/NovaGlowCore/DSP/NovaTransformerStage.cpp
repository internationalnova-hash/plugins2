#include "NovaTransformerStage.h"
#include <cmath>

namespace nova::glow
{

void NovaTransformerStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    const float sr = (float) sampleRate;
    lowSplitCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * kLowSplitHz / sr);
    hfSoftenCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * kHfSoftenHz / sr);
    dcBlockR = std::exp (-2.0f * juce::MathConstants<float>::pi * kDcBlockHz / sr);
    reset();
}

void NovaTransformerStage::reset() noexcept
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        lowState[ch] = 0.0f;
        hfState[ch] = 0.0f;
        dcX1[ch] = 0.0f;
        dcY1[ch] = 0.0f;
    }
}

void NovaTransformerStage::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    const int N = buffer.getNumSamples();
    if (nCh == 0 || N == 0)
        return;

    if (driveLevel <= 1.0e-4f)
        return; // Bypass-transparent at zero drive.

    // LF saturation depth: base amount from the Transformer knob, extra
    // push from the Weight macro's lfDrive contribution. No drive-
    // dependent makeup gain, for the same reason as NovaTubeStage - see
    // its process() comment.
    const float lfSatDrive = 1.0f + driveLevel * (1.0f + lfDrive * 2.0f) * 3.0f;

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
            const float satLow = std::tanh (low * lfSatDrive);
            const float withLfSat = satLow + (x - low);

            // Gentle HF softening: blend toward a lowpassed version by
            // driveLevel (transformer HF rolloff character).
            hf = hfSoftenCoeff * hf + (1.0f - hfSoftenCoeff) * withLfSat;
            const float softened = withLfSat + (hf - withLfSat) * (driveLevel * 0.5f);

            // One-pole DC blocker (defensive - the LF tanh path is odd/
            // symmetric so shouldn't add DC, but this keeps the stage
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

} // namespace nova::glow
