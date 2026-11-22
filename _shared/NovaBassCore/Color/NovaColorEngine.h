#pragma once

#include <juce_dsp/juce_dsp.h>
#include <memory>

namespace nova::bass
{

enum class NovaColorMode { Clean, Warm, Gritty };

// Harmonic saturation stage for Nova Bass's CLEAN/WARM/GRITTY color
// selector, applied once on the engine's summed output bus (not per
// voice - oversampling per voice would be wasteful CPU for no audible
// benefit here, and this sits after the voice sum in the documented
// signal flow: Voice -> PitchEnv -> AmpEnv -> Filter -> Color -> Stereo).
//
// This is an extraction+generalization job, not a reuse-as-is: no
// standalone saturation class existed anywhere in the catalog before this.
// Combines two existing, separately-proven pieces:
//  - The oversampling wrapper shape from _shared/NovaAIMasterCore/DSP/
//    NovaSaturation.h (2x FIR-equiripple juce::dsp::Oversampling), here
//    decoupled from NovaMasteringPlan.
//  - Nova Heat's asymmetric-tanh mode-profile curve (Source/
//    PluginProcessor.cpp's ModeProfile/applySaturation, Soft/Medium/Hard
//    - structurally already CLEAN/WARM/GRITTY).
//
// setParams(mode, drive01) makes CLEAN+50% drive sound different from
// GRITTY+50% drive (spec section 21): the mode selects asymmetry/tone-
// smoothing character, drive selects intensity within that character -
// they are independent axes feeding the same curve, not two presets.
class NovaColorEngine
{
public:
    NovaColorEngine() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    // drive01: the customer-facing DRIVE knob, 0-1.
    void setParams (NovaColorMode mode, float drive01) noexcept;

    // Processes buffer in place. Oversampling is skipped when the stage
    // barely engages (drive near zero), matching Nova Apex's precedent
    // for controlling CPU cost when a nonlinear stage isn't doing
    // meaningful work.
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    struct ModeProfile
    {
        float driveMultiplier;
        float asymmetry;
        float topSmoothing;
        float makeupGainDbAtFullDrive;
    };

    static ModeProfile getModeProfile (NovaColorMode mode) noexcept;
    static float applySaturation (float sample, float k, const ModeProfile& profile) noexcept;

    NovaColorMode currentMode = NovaColorMode::Clean;
    float currentDrive01 = 0.0f;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int preparedNumChannels = 2;
    std::vector<float> smoothingState;
    float smoothingCoeff = 0.5f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaColorEngine)
};

} // namespace nova::bass
