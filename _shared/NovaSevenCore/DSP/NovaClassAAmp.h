#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

namespace nova::seven
{

enum class NovaCharacterMode { Clean, Classic, Hot };

// The main coloration stage, driven by Preamp Gain and Clean/Classic/Hot
// (NOVA_7EVEN_PLAN.md section 5.3/5.4). Same two-piece asymmetric-tanh
// shape as Nova Heat's applySaturation (structure reused, see plan
// section 3.3), plus a per-mode "sag" envelope for the dynamic knee-
// softening quality real Class-A stages have under sustained drive that
// Nova Heat's doesn't model.
class NovaClassAAmp
{
public:
    NovaClassAAmp() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    void setMode (NovaCharacterMode mode) noexcept { currentMode = mode; }
    NovaCharacterMode getMode() const noexcept { return currentMode; }

    // 0-10, matches the preamp_gain parameter's raw UI range.
    void setPreampGain (float gain010) noexcept { preampGain010 = juce::jlimit (0.0f, 10.0f, gain010); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    struct ModeProfile
    {
        float driveMultiplier;
        float baseCurve;
        float asymmetry;
        float sagAmount;
        float sagTimeMs;
    };
    static ModeProfile getModeProfile (NovaCharacterMode mode) noexcept;

private:
    NovaCharacterMode currentMode = NovaCharacterMode::Classic;
    float preampGain010 = 3.0f;
    double sampleRate = 44100.0;

    // Fixed at 2 - every plugin in this catalog only supports mono/stereo
    // buses (isBusesLayoutSupported), so a per-channel sag envelope never
    // needs more than 2 slots.
    std::array<float, 2> sagEnv { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaClassAAmp)
};

} // namespace nova::seven
