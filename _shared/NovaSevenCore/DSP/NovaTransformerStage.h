#pragma once

#include <juce_dsp/juce_dsp.h>

namespace nova::seven
{

// Models the input transformer's frequency-dependent, mildly-asymmetric
// core saturation (NOVA_7EVEN_PLAN.md section 5.2). Always-on, not a
// separate user-facing "amount" control - real transformers don't have
// one. Intended to be felt as glue, not heard as distortion.
//
// Signal path per process() call:
//   1. Pre-emphasis low-shelf (150Hz, +3dB) biases the nonlinearity
//      toward saturating bass content first, the way iron core actually
//      behaves.
//   2. Nonlinearity: dominant odd-harmonic tanh core plus a small
//      even-harmonic (x^2) term for the slight 2nd-harmonic "iron"
//      coloration real transformers add.
//   3. Inverse low-shelf cancels step 1's tonal shift so only the
//      harmonic content added during saturation carries the bass bias -
//      the audible fundamental's frequency response is unaffected.
class NovaTransformerStage
{
public:
    NovaTransformerStage() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    // level01: how "hot" the incoming signal is (0 = quiet/no extra
    // drive, 1 = hottest expected level) - drives k from 1.0 to 1.6.
    // Driven by Input Trim in the processor, not a standalone parameter.
    void setDriveLevel (float level01) noexcept;

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, juce::dsp::IIR::Coefficients<float>>;

    void updateFilters() noexcept;

    StereoFilter preEmphasis;
    StereoFilter postCorrection;

    double sampleRate = 44100.0;
    float driveLevel01 = 0.5f;

    static constexpr float kPreEmphasisFreqHz = 150.0f;
    static constexpr float kPreEmphasisQ = 0.6f;
    static constexpr float kPreEmphasisGainDb = 3.0f;
    static constexpr float kEvenHarmonicAmount = 0.04f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaTransformerStage)
};

} // namespace nova::seven
