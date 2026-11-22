#include "NovaSaturation.h"
#include <cmath>

void NovaSaturation::prepare (const juce::dsp::ProcessSpec& spec)
{
    preparedNumChannels = (int) spec.numChannels;

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        spec.numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
    oversampler->initProcessing (spec.maximumBlockSize);

    reset();
}

void NovaSaturation::reset() noexcept
{
    if (oversampler) oversampler->reset();
}

void NovaSaturation::setPlan (const NovaMasteringPlan::Saturation& p) noexcept { plan = p; }

void NovaSaturation::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! plan.active || oversampler == nullptr) return;

    // drive: 0-1 UI range -> 1.0 (clean) to 3.0 (driven) multiplier ahead
    // of tanh, matching the shape of Nova Master's harmonic-blend
    // technique (PluginProcessor.cpp:431-434) but with a wider range
    // appropriate for a dedicated saturation stage rather than Master's
    // subtle "glue" coloration.
    const float drive = 1.f + plan.drive * 2.f;
    const float blend = juce::jlimit (0.f, 1.f, plan.mix);

    const int nCh = juce::jmin (buffer.getNumChannels(), preparedNumChannels);
    const int N   = buffer.getNumSamples();
    if (nCh == 0 || N == 0) return;

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) nCh, (size_t) N);
    auto upBlock = oversampler->processSamplesUp (block);

    for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch)
    {
        float* d = upBlock.getChannelPointer (ch);
        for (size_t i = 0; i < upBlock.getNumSamples(); ++i)
        {
            const float dry = d[i];
            const float saturated = std::tanh (dry * drive);
            d[i] = dry + (saturated - dry) * blend;
        }
    }

    oversampler->processSamplesDown (block);
}
