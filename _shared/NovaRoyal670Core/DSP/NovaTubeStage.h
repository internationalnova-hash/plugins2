#pragma once

#include <juce_dsp/juce_dsp.h>

namespace nova::royal670
{

// Asymmetric tanh-family waveshaper standing in for a tube gain stage.
// Used twice in the engine's signal chain: once at the input position
// (driven by HRM Drive) and once at the output position (driven by Tube
// Saturation, additionally boosted by the detector's gain-reduction
// coupling - see NovaRoyal670Engine). Not a generic fixed-amount
// saturator: the caller re-drives it every block via setDrive(), which is
// exactly how the GR-coupling requirement is satisfied without this class
// needing to know anything about gain reduction itself.
//
// evenHarmonicBias blends between a symmetric tanh() (odd harmonics only)
// and an asymmetric, DC-corrected shifted tanh() (adds even harmonics) -
// this is what the Weight macro's "harmonic density" contribution drives.
// A one-pole DC blocker (~5Hz) always runs after the waveshaper, since the
// asymmetric path is not perfectly DC-free on its own for all bias/drive
// combinations.
class NovaTubeStage
{
public:
    NovaTubeStage() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    // drive01: 0 = bypass-transparent (unity gain, no coloration).
    void setDrive (float drive01) noexcept { drive = juce::jlimit (0.0f, 4.0f, drive01); }
    void setEvenHarmonicBias (float bias01) noexcept { evenHarmonicBias = juce::jlimit (0.0f, 1.0f, bias01); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int kMaxChannels = 2;
    static constexpr float kAsymmetryShift = 0.35f;
    static constexpr float kDcBlockHz = 5.0f;

    float drive = 0.0f;
    float evenHarmonicBias = 0.0f;
    double sampleRate = 44100.0;

    float dcBlockR = 0.0f;
    float dcX1[kMaxChannels] { 0.0f, 0.0f };
    float dcY1[kMaxChannels] { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaTubeStage)
};

} // namespace nova::royal670
