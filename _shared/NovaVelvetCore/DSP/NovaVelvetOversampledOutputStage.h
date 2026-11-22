#pragma once

#include <juce_dsp/juce_dsp.h>
#include "NovaVelvetTubeStage.h"
#include "NovaVelvetTransformerStage.h"

namespace nova::velvet
{

// Wraps the output-position NovaVelvetTubeStage, a Tube-Drive makeup-gain
// multiply, then NovaVelvetTransformerStage - processed back-to-back inside
// one oversampled block, matching the signal path's "Tube Push-Pull
// Amplifier -> Output Transformer" ordering. The makeup-gain multiply sits
// HERE (inside the oversampled block, between the two nonlinear stages)
// rather than as a separate plain gain stage in the engine, so it
// participates in the same oversampled domain as the nonlinearities it's
// compensating for - a plain post-downsample gain multiply would be
// equivalent here since gain is linear and commutes with the downsampling
// filter, but keeping it in this class keeps the "Tube Drive's
// compensation is deliberately partial" logic co-located with the tube
// stage it compensates.
class NovaVelvetOversampledOutputStage
{
public:
    NovaVelvetOversampledOutputStage() = default;

    void prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages);
    void reset() noexcept;

    void setTubeDrive (float drive01) noexcept { tubeStage.setDrive (drive01); }
    void setTubeLevelEnvelope01 (float level01) noexcept { tubeStage.setLevelEnvelope01 (level01); }
    void setTubeEvenHarmonicBiasScale (float scale) noexcept { tubeStage.setEvenHarmonicBiasScale (scale); }
    void setTubeDriveMakeupGainLinear (float gain) noexcept { tubeDriveMakeupGain = gain; }
    void setTransformerDrive (float level01) noexcept { transformerStage.setDriveLevel (level01); }
    void setTransformerLfDrive (float lfDrive01) noexcept { transformerStage.setLfDrive (lfDrive01); }
    void setTransformerHfSoftenHzScale (float scale) noexcept { transformerStage.setHfSoftenHzScale (scale); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept;

private:
    NovaVelvetTubeStage tubeStage;
    NovaVelvetTransformerStage transformerStage;
    float tubeDriveMakeupGain = 1.0f;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int preparedNumChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVelvetOversampledOutputStage)
};

} // namespace nova::velvet
