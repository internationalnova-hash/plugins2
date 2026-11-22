#pragma once

#include <juce_dsp/juce_dsp.h>

namespace nova::velvet
{

// Push-pull tube amplifier model. Adapts NovaTubeStage's (_shared/
// NovaGlowCore, NovaRoyal670Core) symmetric/asymmetric-tanh blend + DC
// blocker shape, with a push-pull-specific addition: real push-pull stages
// cancel even harmonics at idle (unlike a single-ended stage), so the
// baseline even-harmonic bias here is deliberately low, and grows slightly
// with signal level (setLevelEnvelope01()) to model a real push-pull
// stage's bias imbalance under load - "level-dependent harmonic evolution,
// no sudden threshold" per spec, made concrete as two named, independently
// tunable quantities (baseline + growth) rather than one static knob.
//
// No makeup gain inside this class, for the same reason as NovaTubeStage:
// an earlier version of that class that divided by tanh(driveGain*0.3)
// made heavier drive measure as LESS distortion once already saturating -
// backwards from the "heavier drive -> more harmonic coloration"
// requirement, caught by this catalog's own TubeTransformerValidation.cpp
// monotonicity check. Tube Drive's required "approximate output
// compensation" instead lives as a separate, deliberately partial gain
// stage in NovaVelvetEngine - see its process() for why.
class NovaVelvetTubeStage
{
public:
    NovaVelvetTubeStage() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    // drive01: 0 = bypass-transparent (unity gain, no coloration).
    void setDrive (float drive01) noexcept;
    void setLevelEnvelope01 (float level01) noexcept { levelEnvelope01 = juce::jlimit (0.0f, 1.0f, level01); }

    // Analog Mode's coloration scale on the even-harmonic bias baseline
    // (see NovaVelvetAnalogModeProfile::tubeEvenHarmonicBiasScale) - 1.0 at
    // the reference Tube mode.
    void setEvenHarmonicBiasScale (float scale) noexcept { evenHarmonicBiasScale = juce::jmax (0.0f, scale); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int kMaxChannels = 2;
    static constexpr float kAsymmetryShift = 0.35f;
    static constexpr float kDcBlockHz = 5.0f;

    float drive = 0.0f;
    float levelEnvelope01 = 0.0f;
    float evenHarmonicBiasScale = 1.0f;
    double sampleRate = 44100.0;

    float dcBlockR = 0.0f;
    float dcX1[kMaxChannels] { 0.0f, 0.0f };
    float dcY1[kMaxChannels] { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVelvetTubeStage)
};

} // namespace nova::velvet
