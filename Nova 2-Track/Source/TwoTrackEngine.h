// Nova 2-Track DSP: stereo-mix shaping for a finished 2-track (no stems).
//
//   Input -> M/S shaping (Width, Vox Carve, Low End, Instruments)
//         -> transient shaping (Punch: lows, Energy: mids)
//         -> 4x-oversampled soft clipper (optional, with ceiling) -> Output
//
// Kept free of plugin code so it can be tested.
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

namespace tt
{
struct Settings
{
    float inputDb = 0.0f;        // -12 .. +12
    float width = 100.0f;        // 0 .. 200 %  (side level above the low end)
    float voxCarve = 0.0f;       // 0 .. 100 %  (pocket in the mid channel where vocals sit)
    float punch = 0.0f;          // 0 .. 100 %  (low-frequency transients)
    float lowEnd = 0.0f;         // 0 .. 100 %  (lows folded to the centre + centre weight)
    float instruments = 0.0f;    // 0 .. 100 %  (side-channel body of tonal instruments)
    float energy = 0.0f;         // 0 .. 100 %  (mid-frequency transients)
    bool  clipOn = false;
    float ceilingDb = -0.3f;     // -12 .. 0 dBFS
    float outputDb = 0.0f;       // -12 .. +6
};

// Fixed voicing.
constexpr double kLowSplitHz    = 120.0;   // below this the sides fold to the centre (Low End)
constexpr double kLowShelfHz    = 90.0;    // centre weight shelf (Low End)
constexpr float  kLowShelfMaxDb = 3.0f;
constexpr double kCarveHz       = 2000.0;  // vocal pocket in the mid channel
constexpr double kCarveQ        = 0.8;
constexpr float  kCarveMaxDb    = -6.0f;
constexpr double kInstrHz       = 900.0;   // side-channel body
constexpr double kInstrQ        = 0.5;
constexpr float  kInstrMaxDb    = 6.0f;
constexpr double kPunchHz       = 150.0;   // punch band (low-pass)
constexpr double kEnergyHz      = 2500.0;  // energy band (band-pass centre)
constexpr float  kTransientMaxGain = 2.0f; // +6 dB on the band at full attack
constexpr float  kKneeFraction  = 0.70f;

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

/** RBJ biquad, transposed direct form II, up to 2 channels. */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::array<float, 2> z1 {}, z2 {};
    void peak (double fs, double f0, double q, float gainDb) noexcept;
    void lowShelf (double fs, double f0, float gainDb) noexcept;
    void reset() noexcept { z1 = {}; z2 = {}; }
    inline float process (int ch, float x) noexcept
    {
        const float y = b0 * x + z1[(size_t) ch];
        z1[(size_t) ch] = b1 * x - a1 * y + z2[(size_t) ch];
        z2[(size_t) ch] = b2 * x - a2 * y;
        return y;
    }
};

/** Band transient shaper: boosts the band on attacks (fast envelope above slow). */
struct TransientBand
{
    float fast = 0, slow = 0, gain = 1;
    float aFastAtk = 0, aFastRel = 0, aSlowAtk = 0, aSlowRel = 0, aGain = 0;
    void prepare (double fs) noexcept;
    void reset() noexcept { fast = slow = 0; gain = 1; }
    /** Returns the smoothed gain to apply to the band for this sample. */
    inline float next (float detector, float amount) noexcept
    {
        const float d = std::abs (detector);
        fast += (d > fast ? aFastAtk : aFastRel) * (d - fast);
        slow += (d > slow ? aSlowAtk : aSlowRel) * (d - slow);
        float target = 1.0f;
        if (slow > 1.0e-6f && fast > slow)
            target = juce::jmin (kTransientMaxGain, 1.0f + amount * 3.0f * juce::jmax (0.0f, fast / slow - 1.25f));
        gain += aGain * (target - gain);
        return gain;
    }
};

class TwoTrackEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void set (const Settings& s) noexcept { target = s; }

    /** Processes stereo in place (mono: pass the same pointer twice, numChannels = 1). */
    void process (float* left, float* right, int numSamples) noexcept;

    int getLatencySamples() const noexcept { return latency; }
    float getLastOverDb() const noexcept { return lastOverDb; }
    /** Peak transient boost (dB) applied by Punch / Energy during the last process() call. */
    float getLastPunchDb() const noexcept  { return lastPunchDb; }
    float getLastEnergyDb() const noexcept { return lastEnergyDb; }

private:
    void processChunk (float* l, float* r, int n) noexcept;
    void updateFilters() noexcept;

    double fs = 44100.0;
    int maxBlock = 512, latency = 0;
    Settings target, applied;
    bool firstUpdate = true;

    Biquad lowShelfM, carveM, instrS;
    juce::dsp::LinkwitzRileyFilter<float> sideSplit; // side split (Low End / Width), LR4: low + high = allpass
    juce::dsp::LinkwitzRileyFilter<float> midAllpass; // same phase on the mid so M/S stay aligned
    juce::dsp::LinkwitzRileyFilter<float> punchLow;  // punch band
    juce::dsp::StateVariableTPTFilter<float> energyBand;
    TransientBand punchEnv, energyEnv;

    juce::LinearSmoothedValue<float> inGain, outGain, widthGain, lowFold, ceilingGain;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> work;
    float lastOverDb = 0.0f, lastPunchDb = 0.0f, lastEnergyDb = 0.0f;
};
} // namespace tt
