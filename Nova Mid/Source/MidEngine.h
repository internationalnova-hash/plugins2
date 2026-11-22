// Nova Mid DSP: a wide midrange bell ("Push") into a 4x-oversampled soft clipper
// whose ceiling caps the output peak. Kept free of plugin code so it can be tested.
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

namespace nm
{
// Where the weight lives: a broad bell centred in the midrange, wide enough to
// add body and presence without piling up low end or harsh top.
constexpr double kBellFreqHz = 850.0;
constexpr double kBellQ      = 0.5;

constexpr float kPushMaxDb      = 20.0f;
constexpr float kCeilingMinDb   = -24.0f;
constexpr float kCeilingMaxDb   = 0.0f;
// Soft-clip knee: the curve starts bending this fraction below the ceiling.
constexpr float kKneeFraction   = 0.70f;

/** Smooth clipper: linear below knee*c, then eases into c and never passes it. */
inline float softClip (float x, float ceiling) noexcept
{
    const float t = ceiling * kKneeFraction;
    const float a = std::abs (x);
    if (a <= t)
        return x;
    const float span = ceiling - t;
    const float y = t + span * std::tanh ((a - t) / span);
    return x < 0.0f ? -y : y;
}

/** RBJ peaking bell, transposed direct form II. */
struct Bell
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::array<float, 2> z1 {}, z2 {};

    void setGainDb (double sampleRate, float gainDb) noexcept;
    void reset() noexcept { z1 = {}; z2 = {}; }

    inline float process (int ch, float x) noexcept
    {
        const float y = b0 * x + z1[(size_t) ch];
        z1[(size_t) ch] = b1 * x - a1 * y + z2[(size_t) ch];
        z2[(size_t) ch] = b2 * x - a2 * y;
        return y;
    }
};

class MidEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setPushDb (float db)     { pushTarget.setTargetValue (juce::jlimit (0.0f, kPushMaxDb, db)); }
    void setCeilingDb (float db)  { ceilingDbTarget = juce::jlimit (kCeilingMinDb, kCeilingMaxDb, db); }
    void setOutputDb (float db)   { outGain.setTargetValue (juce::Decibels::decibelsToGain (db)); }

    /** Processes in place. numChannels must be <= the prepared channel count (1 or 2). */
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    int getLatencySamples() const noexcept { return latency; }

    /** Largest amount (dB) the signal was driven past the ceiling during the last process() call. */
    float getLastOverDb() const noexcept { return lastOverDb; }

private:
    void processChunk (float* const* channels, int numChannels, int start, int numSamples) noexcept;

    double sampleRate = 44100.0;
    int channels = 2, maxBlock = 512, latency = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    Bell bell;
    float bellGainDb = -1000.0f;
    juce::LinearSmoothedValue<float> pushTarget, ceilingGain, outGain;
    float ceilingDbTarget = 0.0f;
    float lastOverDb = 0.0f;
};
} // namespace nm
