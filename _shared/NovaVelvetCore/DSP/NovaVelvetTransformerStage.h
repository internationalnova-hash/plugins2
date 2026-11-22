#pragma once

#include <juce_dsp/juce_dsp.h>

namespace nova::velvet
{

// Transformer coloration model, adapting NovaTransformerStage's (_shared/
// NovaGlowCore, NovaRoyal670Core) two-effect shape: magnetic saturation
// below a configurable low split (LPF splits off the low band, drives it
// through tanh, recombines with the untouched rest of the spectrum) plus a
// gentle HF-softening filter blended by drive amount, standing in for
// transformer HF rolloff. Always followed by a DC blocker.
//
// One configurable class serves BOTH the input and output transformer
// positions in NovaVelvetEngine (two separate instances, each configured
// with its own low-split/HF-soften frequencies and drive baseline via
// NovaVelvetCalibration) rather than two near-identical classes, since the
// only real difference between the spec's "input transformer" and "output
// transformer" requirements is calibration constants, not algorithm shape -
// this catalog already establishes that same "one class, per-position
// constants" pattern implicitly (Glow uses NovaTransformerStage at both
// positions; only Royal670 omits the input position entirely).
//
// setDriveLevel() is the stage's overall amount; setLfDrive() is an
// independent extra low-end saturation depth (Weight-macro-style knob in
// the existing precedent, here driven by NovaVelvetCalibration's static
// per-position constant rather than a user knob, since Velvet's locked UI
// has no such control).
class NovaVelvetTransformerStage
{
public:
    NovaVelvetTransformerStage() = default;

    // lowSplitHz/hfSoftenHz configure this instance's role (input vs.
    // output position) - call once after construction, before prepare().
    void configure (float lowSplitHz, float hfSoftenHz) noexcept;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    void setDriveLevel (float level01) noexcept { driveLevel = juce::jlimit (0.0f, 1.0f, level01); }
    void setLfDrive (float lfDrive01) noexcept { lfDrive = juce::jlimit (0.0f, 1.0f, lfDrive01); }

    // Analog Mode's coloration scale on the HF-soften cutoff
    // (NovaVelvetAnalogModeProfile::bandwidthSofteningHzScale; <1 lowers
    // the cutoff = more audible softening). Cheap to update at runtime -
    // just recomputes a one-pole coefficient, no oversampler re-init.
    void setHfSoftenHzScale (float scale) noexcept;

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int kMaxChannels = 2;
    static constexpr float kDcBlockHz = 5.0f;

    float configuredLowSplitHz = 150.0f;
    float configuredHfSoftenHz = 9000.0f;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVelvetTransformerStage)
};

} // namespace nova::velvet
