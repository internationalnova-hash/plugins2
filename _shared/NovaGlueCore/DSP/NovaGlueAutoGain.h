#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace nova::glue
{

// Slow/smoothed makeup-gain compensation, separate from the manual MAKEUP
// knob. Tracks a slow running average of gain reduction and feeds back a
// capped fraction as makeup gain - deliberately never chases instantaneous
// GR (that would pump), and deliberately not a loudness maximizer (the
// compensation fraction is capped well below 1:1), per spec's explicit
// "should make A/B evaluation easier, not create a mastering loudness
// maximizer" requirement.
class NovaGlueAutoGain
{
public:
    NovaGlueAutoGain() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

    // Call once per sample with the current gain-reduction dB (>= 0);
    // returns a linear makeup-gain multiplier to apply alongside the
    // manual MAKEUP trim.
    float process (float gainReductionDb) noexcept;

private:
    static constexpr float kSmoothingSeconds = 2.5f;
    static constexpr float kCompensationFraction = 0.5f; // never fully 1:1 makeup

    double sampleRate = 44100.0;
    float coeff = 0.0f;
    float averageGrDb = 0.0f;
    bool enabled = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaGlueAutoGain)
};

} // namespace nova::glue
