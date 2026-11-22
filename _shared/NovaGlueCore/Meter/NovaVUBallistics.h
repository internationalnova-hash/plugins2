#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace nova::glue
{

// True VU ballistics - symmetric attack and release, ~300ms integration
// time to reach ~99% of a step input (ANSI C16.5), NOT the fast-attack/
// slow-release peak-style behavior a digital peak meter would use. Ported
// from _shared/NovaSevenCore/DSP/NovaVUBallistics (copied rather than
// shared-included, matching the catalog convention of each plugin owning
// its own Core dir), but driven per-channel here rather than L/R-averaged
// into one needle, since Royal 670 has two independent VU meters (Left/Mid
// and Right/Side depending on stereo mode).
//
// Reference level: 0 VU = -18dBFS.
class NovaVUBallistics
{
public:
    NovaVUBallistics() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept { displayLevel = 0.0f; }

    // Call once per audio block, on the audio thread, with this meter's
    // single channel of the metered (post Output Trim) signal.
    void process (const float* channelData, int numSamples) noexcept;

    // Linear (0-1ish) smoothed rectified level.
    float getDisplayLevel() const noexcept { return displayLevel; }

    // VU-scale reading: 0.0 = 0VU = -18dBFS, +N/-N in VU-equivalent dB.
    float getDisplayLevelVu() const noexcept;

private:
    double sampleRate = 44100.0;
    float coeff = 0.0f;
    float displayLevel = 0.0f;

    static constexpr float kVuReferenceDbfs = -18.0f;

    // See NovaSevenCore's original for the derivation: solving for the
    // tau that puts the ANSI C16.5 99%-in-300ms point correctly (a naive
    // "coeff = 1-exp(-1/(0.3*fs))" only reaches 63.2% at 300ms).
    static constexpr float kTimeConstantSeconds = 0.3f / 4.605170186f; // 0.3 / ln(100)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVUBallistics)
};

} // namespace nova::glue
