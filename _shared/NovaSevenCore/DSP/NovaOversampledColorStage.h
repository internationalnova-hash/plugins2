#pragma once

#include <juce_dsp/juce_dsp.h>
#include "NovaTransformerStage.h"
#include "NovaClassAAmp.h"

namespace nova::seven
{

// Wraps NovaTransformerStage + NovaClassAAmp (processed back-to-back)
// inside one oversampled block - NOVA_7EVEN_PLAN.md section 5.6. Reuses
// _shared/NovaAIMasterCore/DSP/NovaSaturation.h's proven
// juce::dsp::Oversampling wrapper shape (the only oversampled-nonlinearity
// precedent in the catalog, see plan section 3.4).
//
class NovaOversampledColorStage
{
public:
    NovaOversampledColorStage() = default;

    // oversamplingStages: JUCE's Oversampling "numStages" - 0 = Off
    // (wrapped stages run directly at the base rate, zero added latency),
    // 1 = 2x, 2 = 4x.
    void prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages = 1);
    void reset() noexcept;

    void setMode (NovaCharacterMode mode) noexcept { classAAmp.setMode (mode); }
    void setPreampGain (float gain010) noexcept { classAAmp.setPreampGain (gain010); }
    void setTransformerDriveLevel (float level01) noexcept { transformerStage.setDriveLevel (level01); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept;

private:
    NovaTransformerStage transformerStage;
    NovaClassAAmp classAAmp;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int preparedNumChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaOversampledColorStage)
};

} // namespace nova::seven
