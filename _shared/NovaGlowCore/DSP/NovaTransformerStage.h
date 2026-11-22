#pragma once

#include <juce_dsp/juce_dsp.h>

namespace nova::glow
{

// Models output-transformer coloration as two independent, frequency-
// scoped effects (distinct in character from NovaTubeStage's broadband
// harmonic waveshaping):
//   1. Magnetic saturation, concentrated below ~150Hz (a one-pole LPF
//      splits off the low band, which alone is driven through a tanh
//      soft-clip and recombined with the untouched rest of the spectrum).
//   2. A gentle one-pole high-frequency softening filter, blended in by
//      the same drive amount, standing in for transformer HF rolloff.
//
// setLfDrive() is the Weight macro's extra low-end saturation depth,
// independent of the main Transformer knob's overall amount.
class NovaTransformerStage
{
public:
    NovaTransformerStage() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    void setDriveLevel (float level01) noexcept { driveLevel = juce::jlimit (0.0f, 1.0f, level01); }
    void setLfDrive (float lfDrive01) noexcept { lfDrive = juce::jlimit (0.0f, 1.0f, lfDrive01); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int kMaxChannels = 2;
    static constexpr float kLowSplitHz = 150.0f;
    static constexpr float kHfSoftenHz = 9000.0f;
    static constexpr float kDcBlockHz = 5.0f;

    float driveLevel = 0.0f;
    float lfDrive = 0.0f;
    double sampleRate = 44100.0;

    float lowSplitCoeff = 0.0f;
    float hfSoftenCoeff = 0.0f;
    float dcBlockR = 0.0f;

    float lowState[kMaxChannels] { 0.0f, 0.0f };
    float hfState[kMaxChannels] { 0.0f, 0.0f };
    float dcX1[kMaxChannels] { 0.0f, 0.0f };
    float dcY1[kMaxChannels] { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaTransformerStage)
};

} // namespace nova::glow
