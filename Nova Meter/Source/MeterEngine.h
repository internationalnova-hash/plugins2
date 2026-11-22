#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <vector>

namespace tz
{

enum MeterId
{
    Peak = 0,
    TruePeak,
    LufsInt,
    LufsShort,
    Rms,
    DynRange,
    MidDensity,
    NumMeters
};

constexpr float kNoValue = -std::numeric_limits<float>::infinity();

inline bool hasValue (float v) noexcept { return std::isfinite (v); }

//==============================================================================
/** Biquad in transposed direct form II, with independent state per channel. */
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::vector<std::array<double, 2>> state;

    void setNumChannels (int n) { state.assign ((size_t) n, { 0.0, 0.0 }); }
    void reset()                { for (auto& s : state) s = { 0.0, 0.0 }; }

    inline double process (double x, int ch) noexcept
    {
        auto& s = state[(size_t) ch];
        const double y = b0 * x + s[0];
        s[0] = b1 * x - a1 * y + s[1];
        s[1] = b2 * x - a2 * y;
        return y;
    }

    void setHighPass (double fs, double f0, double q);
    void setLowPass  (double fs, double f0, double q);
};

//==============================================================================
/** 4x polyphase oversampling true-peak detector (ITU-R BS.1770-4 Annex 2 style). */
class TruePeakDetector
{
public:
    static constexpr int kFactor = 4;
    static constexpr int kTapsPerPhase = 16;

    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Returns the absolute inter-sample peak estimate for this input sample. */
    float process (float x, int ch) noexcept;

private:
    std::array<std::array<float, kTapsPerPhase>, kFactor> phases {};
    std::vector<std::array<float, kTapsPerPhase * 2>> history; // doubled ring to avoid wrap
    std::vector<int> writePos;
    int factor = kFactor;
};

//==============================================================================
/**
    Real-time measurement engine. process() is called on the audio thread; the
    read/reset methods are safe to call from any thread.
*/
class MeterEngine
{
public:
    MeterEngine();

    void prepare (double sampleRate, int numChannels);
    void process (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** Clears held values and the integrated measurement on the next audio block. */
    void requestReset() noexcept { resetRequested.store (true); }

    /** Latest reading in display units (dB / LUFS / LU). Peak meters return the
        maximum since the last call with clear=true. */
    float current (MeterId id) const noexcept { return currentVals[(size_t) id].load (std::memory_order_relaxed); }
    float takePeakSinceLastRead (MeterId id) noexcept; // Peak / TruePeak only

    float heldMax (MeterId id) const noexcept { return heldMaxVals[(size_t) id].load (std::memory_order_relaxed); }
    float heldMin (MeterId id) const noexcept { return heldMinVals[(size_t) id].load (std::memory_order_relaxed); }

    static float gainToDb (double g)      noexcept { return g > 0.0 ? (float) (20.0 * std::log10 (g)) : kNoValue; }
    static float powerToDb (double p)     noexcept { return p > 0.0 ? (float) (10.0 * std::log10 (p)) : kNoValue; }
    static float energyToLufs (double e)  noexcept { return e > 0.0 ? (float) (-0.691 + 10.0 * std::log10 (e)) : kNoValue; }

private:
    void doReset() noexcept;
    void finishSubBlock() noexcept;
    void updateHeld (MeterId id, float v, float minFloor) noexcept;
    void computeIntegrated() noexcept;

    double sampleRate = 48000.0;
    int numCh = 2;
    int subBlockLen = 4800;  // 100 ms
    int subBlockPos = 0;

    Biquad kShelf, kHighPass, midHp, midLp;
    TruePeakDetector truePeak;

    // per-100ms accumulators
    double accK = 0, accRaw = 0, accMid = 0;
    float accTp = 0;

    static constexpr int kRingLen = 30; // 3 s of 100 ms sub-blocks
    std::array<double, kRingLen> ringK {}, ringRaw {}, ringMid {};
    std::array<float, kRingLen> ringTp {};
    int ringPos = 0;
    int ringFilled = 0;

    // Integrated loudness gating histogram (-70 .. +10 LUFS, 0.01 LU bins)
    static constexpr double kHistMin = -70.0, kHistStep = 0.01;
    static constexpr int kHistBins = 8000;
    std::vector<unsigned int> histCount;
    std::vector<double> histEnergy;
    unsigned long long totalBlocks = 0;
    double totalEnergy = 0;

    std::atomic<bool> resetRequested { false };
    std::array<std::atomic<float>, NumMeters> currentVals, heldMaxVals, heldMinVals;
    std::atomic<float> peakSinceRead { 0.0f }, truePeakSinceRead { 0.0f };
};

} // namespace tz
