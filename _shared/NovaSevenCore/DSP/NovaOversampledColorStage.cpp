#include "NovaOversampledColorStage.h"
#include <array>

namespace nova::seven
{

void NovaOversampledColorStage::prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages)
{
    preparedNumChannels = (int) spec.numChannels;

    // stages <= 0 = Off: no Oversampling block at all (not a 1-stage
    // Oversampling asked to be a no-op - that path was never written or
    // tested). The wrapped stages just run directly at the base rate,
    // giving true zero added latency and zero oversampling CPU cost.
    if (oversamplingStages <= 0)
    {
        oversampler.reset();
        transformerStage.prepare (spec);
        classAAmp.prepare (spec);
        reset();
        return;
    }

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        spec.numChannels, oversamplingStages, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
    oversampler->initProcessing (spec.maximumBlockSize);

    // The wrapped stages run at the OVERSAMPLED rate - their filter
    // coefficients (transformer's 150Hz shelf) and time constants (Class-A
    // sag envelope) must be computed against the rate they actually
    // process at, not the plugin's base rate.
    const int factor = 1 << oversamplingStages;
    juce::dsp::ProcessSpec oversampledSpec = spec;
    oversampledSpec.sampleRate = spec.sampleRate * (double) factor;
    oversampledSpec.maximumBlockSize = spec.maximumBlockSize * (juce::uint32) factor;

    transformerStage.prepare (oversampledSpec);
    classAAmp.prepare (oversampledSpec);

    reset();
}

void NovaOversampledColorStage::reset() noexcept
{
    if (oversampler) oversampler->reset();
    transformerStage.reset();
    classAAmp.reset();
}

int NovaOversampledColorStage::getLatencySamples() const noexcept
{
    return oversampler != nullptr ? (int) oversampler->getLatencyInSamples() : 0;
}

void NovaOversampledColorStage::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumSamples() == 0)
        return;

    if (oversampler == nullptr)
    {
        // Off: run the wrapped stages directly at the base rate, no
        // oversampling block, no added latency.
        transformerStage.process (buffer);
        classAAmp.process (buffer);
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

    // Wrap the oversampled block's own memory in a temporary AudioBuffer
    // (zero-copy - NovaTransformerStage/NovaClassAAmp both already take
    // AudioBuffer<float>& and are proven standalone; this lets them run
    // in-place on the oversampled data without changing their interface).
    std::array<float*, 2> channelPtrs { nullptr, nullptr };
    for (size_t ch = 0; ch < upNumCh && ch < channelPtrs.size(); ++ch)
        channelPtrs[ch] = upBlock.getChannelPointer (ch);

    juce::AudioBuffer<float> tempBuffer (channelPtrs.data(), (int) upNumCh, (int) upNumSamples);

    transformerStage.process (tempBuffer);
    classAAmp.process (tempBuffer);

    oversampler->processSamplesDown (block);
}

} // namespace nova::seven
