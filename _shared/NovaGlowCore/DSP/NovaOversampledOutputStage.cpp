#include "NovaOversampledOutputStage.h"
#include <array>

namespace nova::glow
{

void NovaOversampledOutputStage::prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages)
{
    preparedNumChannels = (int) spec.numChannels;

    if (oversamplingStages <= 0)
    {
        oversampler.reset();
        tubeStage.prepare (spec);
        transformerStage.prepare (spec);
        reset();
        return;
    }

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        spec.numChannels, oversamplingStages, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
    oversampler->initProcessing (spec.maximumBlockSize);

    const int factor = 1 << oversamplingStages;
    juce::dsp::ProcessSpec oversampledSpec = spec;
    oversampledSpec.sampleRate = spec.sampleRate * (double) factor;
    oversampledSpec.maximumBlockSize = spec.maximumBlockSize * (juce::uint32) factor;

    tubeStage.prepare (oversampledSpec);
    transformerStage.prepare (oversampledSpec);
    reset();
}

void NovaOversampledOutputStage::reset() noexcept
{
    if (oversampler) oversampler->reset();
    tubeStage.reset();
    transformerStage.reset();
}

int NovaOversampledOutputStage::getLatencySamples() const noexcept
{
    return oversampler != nullptr ? (int) oversampler->getLatencyInSamples() : 0;
}

void NovaOversampledOutputStage::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumSamples() == 0)
        return;

    if (oversampler == nullptr)
    {
        tubeStage.process (buffer);
        transformerStage.process (buffer);
        return;
    }

    const int nCh = juce::jmin (buffer.getNumChannels(), preparedNumChannels);
    const int N = buffer.getNumSamples();
    if (nCh == 0)
        return;

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) nCh, (size_t) N);
    auto upBlock = oversampler->processSamplesUp (block);

    const size_t upNumCh = upBlock.getNumChannels();
    const size_t upNumSamples = upBlock.getNumSamples();

    std::array<float*, 2> channelPtrs { nullptr, nullptr };
    for (size_t ch = 0; ch < upNumCh && ch < channelPtrs.size(); ++ch)
        channelPtrs[ch] = upBlock.getChannelPointer (ch);

    juce::AudioBuffer<float> tempBuffer (channelPtrs.data(), (int) upNumCh, (int) upNumSamples);
    tubeStage.process (tempBuffer);
    transformerStage.process (tempBuffer);

    oversampler->processSamplesDown (block);
}

} // namespace nova::glow
