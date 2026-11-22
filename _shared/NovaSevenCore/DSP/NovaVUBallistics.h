#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace nova::seven
{

// True VU ballistics (NOVA_7EVEN_PLAN.md section 5.7) - symmetric attack
// and release, ~300ms integration time to reach ~99% of a step input
// (ANSI C16.5), NOT the fast-attack/slow-release peak-style behavior
// Nova Vox's meter has (plan section 3.6 - explicitly not reusable here).
//
// Reference level: 0 VU = -18dBFS (plan section 5.7's fixed reference).
class NovaVUBallistics
{
public:
    NovaVUBallistics() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept { displayLevel = 0.0f; }

    // Call once per audio block, on the audio thread, with the metered
    // signal (post Out Trim, plan section 5.1). Averages |L|/|R| per
    // sample so a stereo mix drives a single needle.
    void process (const juce::AudioBuffer<float>& buffer) noexcept;

    // Linear (0-1ish) smoothed rectified level.
    float getDisplayLevel() const noexcept { return displayLevel; }

    // VU-scale reading: 0.0 = 0VU = -18dBFS, +N/-N in VU-equivalent dB.
    float getDisplayLevelVu() const noexcept;

private:
    double sampleRate = 44100.0;
    float coeff = 0.0f;
    float displayLevel = 0.0f;

    static constexpr float kVuReferenceDbfs = -18.0f;

    // The plan's own docstring target - "300ms to reach 99% of a step"
    // (ANSI C16.5) - and its shorthand "coeff = 1-exp(-1/(0.3*fs))"
    // comment are mutually inconsistent: a one-pole with tau=300ms only
    // reaches 1-exp(-1)=63.2% at t=300ms, not 99% (99% needs t=ln(100)*tau
    // = ~4.6 time constants). Solving for the tau that actually puts the
    // 99% point at 300ms: 0.99 = 1-exp(-0.3/tau) => tau = 0.3/ln(100).
    // Trusting the real external standard (99% in 300ms) over the
    // planning doc's arithmetic slip.
    static constexpr float kTimeConstantSeconds = 0.3f / 4.605170186f; // 0.3 / ln(100)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVUBallistics)
};

} // namespace nova::seven
