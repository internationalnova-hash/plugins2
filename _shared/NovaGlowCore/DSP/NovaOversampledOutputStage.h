#pragma once

#include <juce_dsp/juce_dsp.h>
#include "NovaTubeStage.h"
#include "NovaTransformerStage.h"

namespace nova::glow
{

// Wraps the output-position NovaTubeStage + NovaTransformerStage
// (processed back-to-back, matching the signal flow's "output tube stage
// -> output transformer" ordering) inside one oversampled block - these
// two sit adjacent to each other with nothing linear in between, unlike
// the input tube stage (see NovaOversampledInputStage). Same wrapper
// shape as _shared/NovaSevenCore/DSP/NovaOversampledColorStage.
class NovaOversampledOutputStage
{
public:
    NovaOversampledOutputStage() = default;

    void prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages);
    void reset() noexcept;

    void setTubeDrive (float drive01) noexcept { tubeStage.setDrive (drive01); }
    void setTubeEvenHarmonicBias (float bias01) noexcept { tubeStage.setEvenHarmonicBias (bias01); }
    void setTransformerDrive (float level01) noexcept { transformerStage.setDriveLevel (level01); }
    void setTransformerLfDrive (float lfDrive01) noexcept { transformerStage.setLfDrive (lfDrive01); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept;

private:
    NovaTubeStage tubeStage;
    NovaTransformerStage transformerStage;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int preparedNumChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaOversampledOutputStage)
};

} // namespace nova::glow
