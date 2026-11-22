#pragma once

#include <juce_dsp/juce_dsp.h>
#include "NovaTransformerStage.h"
#include "NovaTubeStage.h"

namespace nova::glow
{

// Wraps the input-position NovaTransformerStage + NovaTubeStage (processed
// back-to-back, matching the signal flow's "Input Transformer -> Tube Gain
// Stage" ordering) inside one oversampled block - these two sit adjacent
// with nothing linear in between. Same wrapper mechanics as
// NovaOversampledOutputStage (and, further back, Royal 670's identically-
// shaped stage), just reordered and repositioned to the input side.
class NovaOversampledInputStage
{
public:
    NovaOversampledInputStage() = default;

    void prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages);
    void reset() noexcept;

    void setTransformerDrive (float level01) noexcept { transformerStage.setDriveLevel (level01); }
    void setTransformerLfDrive (float lfDrive01) noexcept { transformerStage.setLfDrive (lfDrive01); }
    void setTubeDrive (float drive01) noexcept { tubeStage.setDrive (drive01); }
    void setTubeEvenHarmonicBias (float bias01) noexcept { tubeStage.setEvenHarmonicBias (bias01); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept;

private:
    NovaTransformerStage transformerStage;
    NovaTubeStage tubeStage;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int preparedNumChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaOversampledInputStage)
};

} // namespace nova::glow
