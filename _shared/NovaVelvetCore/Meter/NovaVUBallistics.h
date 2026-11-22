#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace nova::velvet
{

// True VU ballistics - symmetric attack and release, ~300ms integration
// time to reach ~99% of a step input (ANSI C16.5), NOT the fast-attack/
// slow-release peak-style behavior a digital peak meter would use. Copied
// per this catalog's own per-Core-copy convention (see NovaGlowCore/
// NovaRoyal670Core/NovaSevenCore/Meter/NovaVUBallistics) rather than
// shared-included, so each plugin owns its own Core dir.
//
// Reference level: 0 VU = -18dBFS, matching NovaVelvetCalibration::kReferenceDbfs.
class NovaVUBallistics
{
public:
    NovaVUBallistics() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept { displayLevel = 0.0f; }

    // Call once per audio block, on the audio thread. For the GR meter,
    // feed already-in-dB gain-reduction values directly (see
    // NovaVelvetEngine::getGrDisplayDb()); for Input/Output, feed linear
    // audio amplitude.
    void process (const float* channelData, int numSamples) noexcept;

    // Linear (0-1ish) smoothed rectified level, OR (for GR) the smoothed
    // dB value directly if fed already-in-dB data.
    float getDisplayLevel() const noexcept { return displayLevel; }

    // VU-scale reading for linear-amplitude input: 0.0 = 0VU = -18dBFS.
    float getDisplayLevelVu() const noexcept;

private:
    double sampleRate = 44100.0;
    float coeff = 0.0f;
    float displayLevel = 0.0f;

    static constexpr float kVuReferenceDbfs = -18.0f;

    // Solving for the tau that puts the ANSI C16.5 99%-in-300ms point
    // correctly (a naive "coeff = 1-exp(-1/(0.3*fs))" only reaches 63.2%
    // at 300ms).
    static constexpr float kTimeConstantSeconds = 0.3f / 4.605170186f; // 0.3 / ln(100)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVUBallistics)
};

} // namespace nova::velvet
