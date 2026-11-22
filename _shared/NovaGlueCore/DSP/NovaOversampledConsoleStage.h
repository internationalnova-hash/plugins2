#pragma once

#include <juce_dsp/juce_dsp.h>
#include "NovaConsoleStage.h"

namespace nova::glue
{

// Wraps one NovaConsoleStage inside an oversampled block - used twice by
// the engine (once at the input position, once at the output position
// where DRIVE lives), each with its own instance/oversampler, matching the
// wrapper shape in _shared/NovaGlowCore/DSP/NovaOversampledOutputStage.cpp
// (simplified here to bundle a single nonlinear stage rather than a pair,
// since Glue's spec only calls for one console/VCA stage per boundary).
class NovaOversampledConsoleStage
{
public:
    NovaOversampledConsoleStage() = default;

    void prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages);
    void reset() noexcept;

    void setDrive (float drive01) noexcept { consoleStage.setDrive (drive01); }
    void setEvenHarmonicBias (float bias01) noexcept { consoleStage.setEvenHarmonicBias (bias01); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept;

private:
    NovaConsoleStage consoleStage;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int preparedNumChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaOversampledConsoleStage)
};

} // namespace nova::glue
