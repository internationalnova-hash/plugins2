#include "NovaTubeStage.h"
#include <cmath>

namespace nova::glow
{

void NovaTubeStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    dcBlockR = std::exp (-2.0f * juce::MathConstants<float>::pi * kDcBlockHz / (float) sampleRate);
    reset();
}

void NovaTubeStage::reset() noexcept
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        dcX1[ch] = 0.0f;
        dcY1[ch] = 0.0f;
    }
}

void NovaTubeStage::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    const int N = buffer.getNumSamples();
    if (nCh == 0 || N == 0)
        return;

    if (drive <= 1.0e-4f)
        return; // 0 drive = bypass-transparent, no waveshaping, no DC-block coloration either.

    // Effective drive gain: 0..4 maps to a modest-to-heavy tanh drive.
    // Deliberately NO drive-dependent makeup gain: tanh's derivative at
    // the origin is already 1, so small-signal gain stays near-unity
    // without compensation, and letting the output settle toward the
    // tanh asymptote as drive rises is what makes distortion (the
    // difference from a linear pass) grow monotonically with drive - the
    // property the engine's GR-coupling depends on. An earlier version
    // divided by tanh(driveGain*0.3), which shrank the whole output fast
    // enough as drive rose to make "more drive" measure as *less*
    // distortion once the signal was already in the saturating region -
    // exactly backwards from the "heavier limiting -> more harmonic
    // coloration" requirement (caught by TubeTransformerValidation.cpp's
    // monotonicity check).
    const float driveGain = 1.0f + drive * 3.0f;

    for (int ch = 0; ch < nCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        float x1 = dcX1[ch];
        float y1 = dcY1[ch];

        for (int i = 0; i < N; ++i)
        {
            const float x = data[i];
            const float driven = driveGain * x;

            const float symmetric = std::tanh (driven);
            const float asymmetric = std::tanh (driven + kAsymmetryShift) - std::tanh (kAsymmetryShift);

            float y = symmetric * (1.0f - evenHarmonicBias) + asymmetric * evenHarmonicBias;

            // One-pole DC blocker.
            const float blocked = y - x1 + dcBlockR * y1;
            x1 = y;
            y1 = blocked;

            data[i] = blocked;
        }

        dcX1[ch] = x1;
        dcY1[ch] = y1;
    }
}

} // namespace nova::glow
