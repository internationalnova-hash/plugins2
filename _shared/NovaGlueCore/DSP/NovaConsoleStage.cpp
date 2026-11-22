#include "NovaConsoleStage.h"
#include <cmath>

namespace nova::glue
{

void NovaConsoleStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    dcBlockR = std::exp (-2.0f * juce::MathConstants<float>::pi * kDcBlockHz / (float) sampleRate);
    reset();
}

void NovaConsoleStage::reset() noexcept
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        dcX1[ch] = 0.0f;
        dcY1[ch] = 0.0f;
    }
}

void NovaConsoleStage::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    const int N = buffer.getNumSamples();
    if (nCh == 0 || N == 0)
        return;

    if (drive <= 1.0e-4f)
        return; // 0 drive = bypass-transparent, no waveshaping, no DC-block coloration either.

    // Deliberately no drive-dependent makeup gain here either (same
    // reasoning as NovaTubeStage: tanh's derivative at the origin is 1, so
    // small-signal gain stays near-unity without compensation, keeping
    // distortion growth monotonic with drive). Overall output loudness is
    // instead compensated by the engine's separate, centralized Drive
    // compensation curve (see NovaGlueEngine), not baked into this stage.
    const float driveGain = 1.0f + drive * (kDriveGainMax - 1.0f);

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

} // namespace nova::glue
