#pragma once

#include <juce_dsp/juce_dsp.h>
#include "NovaTubeStage.h"

namespace nova::royal670
{

// Wraps just the input-position NovaTubeStage (driven by HRM Drive) in
// oversampling - this runs on the raw input BEFORE the detector reads it,
// so it can't share an oversampling wrapper with the output-side stages
// (the gain-reduction multiply, a linear operation the spec says not to
// oversample, sits between the two). Same juce::dsp::Oversampling wrapper
// shape as _shared/NovaSevenCore/DSP/NovaOversampledColorStage - the only
// established oversampled-nonlinearity precedent in this catalog.
class NovaOversampledInputStage
{
public:
    NovaOversampledInputStage() = default;

    // oversamplingStages: 0 = Off (no Oversampling object at all - true
    // zero added latency/CPU), 1 = 2x, 2 = 4x, 3 = 8x.
    void prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages);
    void reset() noexcept;

    void setDrive (float drive01) noexcept { tubeStage.setDrive (drive01); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept;

private:
    NovaTubeStage tubeStage;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int preparedNumChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaOversampledInputStage)
};

} // namespace nova::royal670
