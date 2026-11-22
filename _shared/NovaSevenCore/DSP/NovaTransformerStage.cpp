#include "NovaTransformerStage.h"
#include <cmath>

namespace nova::seven
{

void NovaTransformerStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

    preEmphasis.prepare (spec);
    postCorrection.prepare (spec);

    updateFilters();
    reset();
}

void NovaTransformerStage::reset() noexcept
{
    preEmphasis.reset();
    postCorrection.reset();
}

void NovaTransformerStage::setDriveLevel (float level01) noexcept
{
    driveLevel01 = juce::jlimit (0.0f, 1.0f, level01);
}

void NovaTransformerStage::updateFilters() noexcept
{
    *preEmphasis.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, kPreEmphasisFreqHz, kPreEmphasisQ, juce::Decibels::decibelsToGain (kPreEmphasisGainDb));

    *postCorrection.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, kPreEmphasisFreqHz, kPreEmphasisQ, juce::Decibels::decibelsToGain (-kPreEmphasisGainDb));
}

void NovaTransformerStage::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumSamples() == 0)
        return;

    updateFilters();

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    preEmphasis.process (context);

    const float k = juce::jmap (driveLevel01, 1.0f, 1.6f);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* samples = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float x = samples[sample];
            const float sign = x >= 0.0f ? 1.0f : -1.0f;
            samples[sample] = std::tanh (x * k) + kEvenHarmonicAmount * x * x * sign;
        }
    }

    postCorrection.process (context);
}

} // namespace nova::seven
