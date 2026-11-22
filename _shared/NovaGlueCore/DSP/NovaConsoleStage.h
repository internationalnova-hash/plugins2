#pragma once

#include <juce_dsp/juce_dsp.h>

namespace nova::glue
{

// Subtle console/VCA nonlinearity stage - adapted from the tanh+DC-blocker
// idiom in _shared/NovaGlowCore/DSP/NovaTubeStage.cpp, but NOT a tube
// model: drive range and even-harmonic bias are both capped far lower, so
// this reads as "console input-stage character" / "VCA nonlinear response"
// per Nova Glue's spec, never as a distortion or saturation-plugin stage.
// Used twice in the engine (input position and output position, where
// DRIVE lives) - same reuse shape as NovaTubeStage itself.
//
// evenHarmonicBias blends symmetric tanh() (odd harmonics only) toward a
// small asymmetric, DC-corrected shifted tanh() (adds even harmonics) -
// capped internally so the asymmetry stays "extremely subtle" even at
// bias01=1, per spec. A one-pole DC blocker (~5Hz) always runs after the
// waveshaper.
class NovaConsoleStage
{
public:
    NovaConsoleStage() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    // drive01: 0 = bypass-transparent (unity gain, no coloration).
    // Capped range is deliberately much narrower than NovaTubeStage's -
    // this is a subtle console stage, not a saturation effect.
    void setDrive (float drive01) noexcept { drive = juce::jlimit (0.0f, 1.0f, drive01); }
    void setEvenHarmonicBias (float bias01) noexcept { evenHarmonicBias = juce::jlimit (0.0f, 1.0f, bias01) * kMaxEvenHarmonicBias; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int kMaxChannels = 2;
    static constexpr float kAsymmetryShift = 0.12f; // much smaller than NovaTubeStage's 0.35 - "extremely subtle asymmetry"
    static constexpr float kMaxEvenHarmonicBias = 0.2f;
    static constexpr float kDriveGainMax = 1.3f; // 0..1 drive01 maps to a gentle 1.0x..1.3x drive gain
    static constexpr float kDcBlockHz = 5.0f;

    float drive = 0.0f;
    float evenHarmonicBias = 0.0f;
    double sampleRate = 44100.0;

    float dcBlockR = 0.0f;
    float dcX1[kMaxChannels] { 0.0f, 0.0f };
    float dcY1[kMaxChannels] { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaConsoleStage)
};

} // namespace nova::glue
