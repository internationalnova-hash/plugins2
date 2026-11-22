#pragma once

#include <juce_dsp/juce_dsp.h>
#include "NovaVelvetTransformerStage.h"

namespace nova::velvet
{

// Wraps the input transformer ALONE inside one oversampled block - an
// intentional divergence from this catalog's usual symmetric input/output
// wrapper shape (NovaOversampledInputStage in Glow/Royal670 wraps
// Transformer+Tube together on the input side). Nova Velvet's optical
// gain-reduction element sits BETWEEN the input transformer and the tube
// stage (per the spec's explicit signal-path ordering: input transformer ->
// optical cell -> tube amplifier -> output transformer), so the input side
// has only one nonlinear stage to wrap, not two.
class NovaVelvetOversampledInputStage
{
public:
    NovaVelvetOversampledInputStage() = default;

    void prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages);
    void reset() noexcept;

    void setTransformerDrive (float level01) noexcept { transformerStage.setDriveLevel (level01); }
    void setTransformerLfDrive (float lfDrive01) noexcept { transformerStage.setLfDrive (lfDrive01); }
    void setTransformerHfSoftenHzScale (float scale) noexcept { transformerStage.setHfSoftenHzScale (scale); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept;

private:
    NovaVelvetTransformerStage transformerStage;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int preparedNumChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVelvetOversampledInputStage)
};

} // namespace nova::velvet
