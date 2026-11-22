#pragma once

#include <random>
#include <cmath>
#include <cstdint>

// Slow, bounded, non-periodic modulation source (spec: "extremely slow,
// decorrelated, organic modulation" - explicitly NOT a sine LFO, since a
// sine is periodic enough to read as chorus). Algorithm shape borrowed
// from NovaDSP/Console/NovaConsoleDSP's channel-strip drift (leaky
// integrator fed by uniform random draws), but that reference hardcodes
// its leak coefficient (e.g. 0.999f) which is implicitly tied to a
// specific sample rate. Here the time constant is expressed in seconds
// and converted to a coefficient via exp(-1/(tau*sampleRate)) - the same
// technique Nova One uses for its widener crossover - so behavior is
// identical across 44.1/48/88.2/96kHz.
//
// Uses mt19937, not minstd_rand: this class needs multiple SIMULTANEOUS
// decorrelated instances (up to 4 voices x 3 modulators = 12), and
// measured minstd_rand (a simple LCG - every seed just starts at a
// different point in one shared cyclic sequence) producing 0.74
// correlation between two nearby-but-arbitrary seeds after smoothing.
// mt19937's much larger state gives reliably independent sequences
// across seeds instead of gambling on which integers happen to land far
// apart in an LCG's cycle.
class NovaOrganicModulator
{
public:
    NovaOrganicModulator() = default;

    // seed: distinct per instance so multiple modulators (per voice, per
    // parameter) are never correlated with each other.
    void prepare (double sampleRate, float timeConstantSeconds, uint32_t seed) noexcept
    {
        leakCoeff = std::exp (-1.0f / (timeConstantSeconds * (float) sampleRate));
        rng.seed (seed);
        state = 0.0f;

        // A leaky integrator's STATIONARY VARIANCE from fixed-amplitude
        // i.i.d. noise is Var(x) * (1-a)/(1+a), which shrinks as sample
        // rate rises (more, smaller-relative-influence kicks land within
        // one time constant at a higher rate) - so an unscaled version of
        // this class would wobble with visibly smaller amplitude at 96kHz
        // than at 44.1kHz for the same tau. Scaling the injected noise by
        // sqrt((1+a)/(1-a)) exactly cancels that factor, so the walk's
        // amplitude - not just its speed - is sample-rate invariant.
        noiseScale = std::sqrt ((1.0f + leakCoeff) / (1.0f - leakCoeff));
    }

    // Advances the random walk by one sample and returns the current
    // value, bounded to [-1, 1]. Realtime-safe: no allocation.
    float getNextSample() noexcept
    {
        const float draw = dist (rng) * noiseScale;
        state = state * leakCoeff + (1.0f - leakCoeff) * draw;
        if (state > 1.0f) state = 1.0f;
        else if (state < -1.0f) state = -1.0f;
        return state;
    }

    void reset() noexcept { state = 0.0f; }

private:
    std::mt19937 rng;
    std::uniform_real_distribution<float> dist { -1.0f, 1.0f };
    float leakCoeff = 0.999f;
    float noiseScale = 1.0f;
    float state = 0.0f;
};
