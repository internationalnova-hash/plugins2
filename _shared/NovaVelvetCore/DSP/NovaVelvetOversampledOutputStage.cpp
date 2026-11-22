#include "NovaVelvetOversampledOutputStage.h"
#include "NovaVelvetCalibration.h"
#include <array>

namespace nova::velvet
{

void NovaVelvetOversampledOutputStage::prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages)
{
    preparedNumChannels = (int) spec.numChannels;
    transformerStage.configure (NovaVelvetCalibration::kOutputTransformerLowSplitHz,
                                 NovaVelvetCalibration::kOutputTransformerHfSoftenHz);

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

void NovaVelvetOversampledOutputStage::reset() noexcept
{
    if (oversampler) oversampler->reset();
    tubeStage.reset();
    transformerStage.reset();
}

int NovaVelvetOversampledOutputStage::getLatencySamples() const noexcept
{
    return oversampler != nullptr ? (int) oversampler->getLatencyInSamples() : 0;
}

void NovaVelvetOversampledOutputStage::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumSamples() == 0)
        return;

    const auto runInnerStages = [this] (juce::AudioBuffer<float>& buf) noexcept
    {
        tubeStage.process (buf);

        if (! juce::approximatelyEqual (tubeDriveMakeupGain, 1.0f))
        {
            const int nCh = buf.getNumChannels();
            const int N = buf.getNumSamples();
            for (int ch = 0; ch < nCh; ++ch)
                buf.applyGain (ch, 0, N, tubeDriveMakeupGain);
        }

        transformerStage.process (buf);
    };

    if (oversampler == nullptr)
    {
        runInnerStages (buffer);
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
    runInnerStages (tempBuffer);

    oversampler->processSamplesDown (block);
}

} // namespace nova::velvet
