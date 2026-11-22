// Headless end-to-end validation of NovaDoublerEngine: mono-in and
// stereo-in handling, dry/wet MIX crossfade, bypass ramp, automation
// zipper across all params, sample-rate independence. Drives the engine
// directly (mirrors this codebase's established pattern of not linking
// PluginProcessor/PluginEditor into headless validation apps).

#include "NovaDoublerEngine.h"
#include <juce_core/juce_core.h>
#include <iostream>
#include <cmath>
#include <vector>

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    float rms (const std::vector<float>& v)
    {
        double sum = 0.0;
        for (float x : v) sum += (double) x * x;
        return (float) std::sqrt (sum / (double) v.size());
    }

    std::vector<float> makeToneBurst (int n, double sr, float freq = 220.0f)
    {
        std::vector<float> sig ((size_t) n);
        for (int i = 0; i < n; ++i)
            sig[(size_t) i] = 0.5f * std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / sr);
        return sig;
    }

    // Pink noise (Paul Kellet's economy method) - for loudness/gain
    // measurements specifically. A single sine tone hits narrow-band
    // comb-filter cancellation/reinforcement at specific inter-voice delay
    // amounts, producing loudness readings that reflect this one
    // synthetic tone's frequency, not real (broadband) material - the
    // same artifact already diagnosed in EnsembleValidation.cpp's RMS
    // sweep, fixed there the same way.
    std::vector<float> makePinkNoise (int n)
    {
        std::mt19937 rng (12345);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
        std::vector<float> out ((size_t) n);
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        for (int i = 0; i < n; ++i)
        {
            const float white = dist (rng);
            b0 = 0.99886f * b0 + white * 0.0555179f;
            b1 = 0.99332f * b1 + white * 0.0750759f;
            b2 = 0.96900f * b2 + white * 0.1538520f;
            b3 = 0.86650f * b3 + white * 0.3104856f;
            b4 = 0.55000f * b4 + white * 0.5329522f;
            b5 = -0.7616f * b5 - white * 0.0168980f;
            const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
            b6 = white * 0.115926f;
            out[(size_t) i] = pink * 0.11f;
        }
        return out;
    }
}

int main()
{
    const double sr = 48000.0;
    const int n = (int) (sr * 1.0);

    std::cout << "\n[Integration] Mono input, MIX=0%: output is pure dry, duplicated to both channels\n";
    {
        NovaDoublerEngine engine;
        engine.prepare (sr);
        engine.setParams (60.0f, 40.0f, 0.0f, true, false);

        auto mono = makeToneBurst (n, sr);
        std::vector<float> outL ((size_t) n), outR ((size_t) n);
        const float* inChans[1] = { mono.data() };

        // Run several blocks so the mix/width/tightness ramps fully settle.
        const int blockLen = 512;
        for (int start = 0; start < n; start += blockLen)
        {
            const int len = juce::jmin (blockLen, n - start);
            const float* blockIn[1] = { mono.data() + start };
            engine.processBlock (blockIn, 1, outL.data() + start, outR.data() + start, len);
        }

        double maxDiff = 0.0;
        for (int i = n / 2; i < n; ++i) // skip the first half while the ramp settles
        {
            maxDiff = juce::jmax (maxDiff, (double) std::abs (outL[(size_t) i] - mono[(size_t) i]));
            maxDiff = juce::jmax (maxDiff, (double) std::abs (outR[(size_t) i] - mono[(size_t) i]));
        }
        check (maxDiff < 0.01, "MIX=0% output matches dry mono input on both channels once settled, max diff=" + juce::String (maxDiff, 5));
    }

    std::cout << "\n[Integration] Mono input, MIX=100%: wet content present, stereo image created from mono source\n";
    {
        NovaDoublerEngine engine;
        engine.prepare (sr);
        engine.setParams (80.0f, 60.0f, 100.0f, true, false);

        auto mono = makeToneBurst (n, sr);
        std::vector<float> outL ((size_t) n), outR ((size_t) n);
        const int blockLen = 512;
        for (int start = 0; start < n; start += blockLen)
        {
            const int len = juce::jmin (blockLen, n - start);
            const float* blockIn[1] = { mono.data() + start };
            engine.processBlock (blockIn, 1, outL.data() + start, outR.data() + start, len);
        }

        std::vector<float> lTail (outL.begin() + n / 2, outL.end());
        std::vector<float> rTail (outR.begin() + n / 2, outR.end());
        double diffFromMono = 0.0;
        for (size_t i = 0; i < lTail.size(); ++i)
            diffFromMono += std::abs ((double) lTail[i] - (double) mono[n / 2 + (int) i]);
        diffFromMono /= (double) lTail.size();
        check (diffFromMono > 0.01, "MIX=100% output differs from dry (wet content present), mean abs diff=" + juce::String (diffFromMono, 5));

        double lrDiff = 0.0;
        for (size_t i = 0; i < lTail.size(); ++i)
            lrDiff += std::abs ((double) lTail[i] - (double) rTail[i]);
        lrDiff /= (double) lTail.size();
        check (lrDiff > 0.005, "L and R channels differ from each other (real stereo image created from mono source), mean abs diff=" + juce::String (lrDiff, 5));
    }

    std::cout << "\n[Integration] Stereo input, MIX=0%: true stereo dry image preserved (not folded to mono)\n";
    {
        NovaDoublerEngine engine;
        engine.prepare (sr);
        engine.setParams (50.0f, 50.0f, 0.0f, false, false);

        auto left = makeToneBurst (n, sr, 220.0f);
        auto right = makeToneBurst (n, sr, 330.0f); // deliberately different so folding-to-mono would be detectable
        std::vector<float> outL ((size_t) n), outR ((size_t) n);
        const int blockLen = 512;
        for (int start = 0; start < n; start += blockLen)
        {
            const int len = juce::jmin (blockLen, n - start);
            const float* blockIn[2] = { left.data() + start, right.data() + start };
            engine.processBlock (blockIn, 2, outL.data() + start, outR.data() + start, len);
        }

        double maxDiffL = 0.0, maxDiffR = 0.0;
        for (int i = n / 2; i < n; ++i)
        {
            maxDiffL = juce::jmax (maxDiffL, (double) std::abs (outL[(size_t) i] - left[(size_t) i]));
            maxDiffR = juce::jmax (maxDiffR, (double) std::abs (outR[(size_t) i] - right[(size_t) i]));
        }
        check (maxDiffL < 0.01 && maxDiffR < 0.01,
               "MIX=0% preserves the true stereo input unfolded, maxDiffL=" + juce::String (maxDiffL, 5) + " maxDiffR=" + juce::String (maxDiffR, 5));
    }

    std::cout << "\n[Integration] Bypass: settled output equals dry input regardless of Width/Tightness/Mix\n";
    {
        NovaDoublerEngine engine;
        engine.prepare (sr);
        engine.setParams (100.0f, 0.0f, 100.0f, true, true); // extreme settings, but bypassed

        auto mono = makeToneBurst (n, sr);
        std::vector<float> outL ((size_t) n), outR ((size_t) n);
        const int blockLen = 512;
        for (int start = 0; start < n; start += blockLen)
        {
            const int len = juce::jmin (blockLen, n - start);
            const float* blockIn[1] = { mono.data() + start };
            engine.processBlock (blockIn, 1, outL.data() + start, outR.data() + start, len);
        }

        double maxDiff = 0.0;
        for (int i = n / 2; i < n; ++i)
        {
            maxDiff = juce::jmax (maxDiff, (double) std::abs (outL[(size_t) i] - mono[(size_t) i]));
            maxDiff = juce::jmax (maxDiff, (double) std::abs (outR[(size_t) i] - mono[(size_t) i]));
        }
        check (maxDiff < 0.01, "bypassed output matches dry input once the ramp settles, even at extreme Width/Tightness/Mix, max diff=" + juce::String (maxDiff, 5));
    }

    std::cout << "\n[Integration] Bypass toggle mid-stream produces no click\n";
    {
        NovaDoublerEngine engine;
        engine.prepare (sr);
        engine.setParams (70.0f, 50.0f, 100.0f, true, false);

        auto mono = makeToneBurst (n, sr);
        std::vector<float> outL ((size_t) n), outR ((size_t) n);
        const int blockLen = 64;
        for (int start = 0; start < n; start += blockLen)
        {
            if (start == n / 2) engine.setParams (70.0f, 50.0f, 100.0f, true, true); // toggle bypass mid-stream
            const int len = juce::jmin (blockLen, n - start);
            const float* blockIn[1] = { mono.data() + start };
            engine.processBlock (blockIn, 1, outL.data() + start, outR.data() + start, len);
        }

        float maxDelta = 0.0f;
        for (int i = 1; i < n; ++i)
            maxDelta = juce::jmax (maxDelta, std::abs (outL[(size_t) i] - outL[(size_t) (i - 1)]));
        // The 30ms bypass ramp spreads the dry<->wet transition over ~1440
        // samples at 48kHz, so even a full-scale transition produces a tiny
        // per-sample step; a real click would spike far above this.
        check (maxDelta < 0.2f, "max sample-to-sample delta across a mid-stream bypass toggle stays bounded (ramped, not stepped), got " + juce::String (maxDelta, 4));
    }

    std::cout << "\n[Integration] Equal-power MIX crossfade: no loudness dip at MIX=50%\n";
    {
        auto measureRms = [&] (float mixPercent) -> float
        {
            NovaDoublerEngine engine;
            engine.prepare (sr);
            engine.setParams (70.0f, 50.0f, mixPercent, true, false);

            auto mono = makePinkNoise (n);
            std::vector<float> outL ((size_t) n), outR ((size_t) n);
            const int blockLen = 512;
            for (int start = 0; start < n; start += blockLen)
            {
                const int len = juce::jmin (blockLen, n - start);
                const float* blockIn[1] = { mono.data() + start };
                engine.processBlock (blockIn, 1, outL.data() + start, outR.data() + start, len);
            }
            // Combined stereo power (L^2+R^2), not a single channel: equal-
            // power panning conserves total power across L+R, so measuring
            // one channel alone reads artificially quiet for wet content
            // that's spread across both channels (same fix already applied
            // in EnsembleValidation.cpp's loudness-consistency test).
            std::vector<float> lTail (outL.begin() + n / 2, outL.end());
            std::vector<float> rTail (outR.begin() + n / 2, outR.end());
            const float lRms = rms (lTail);
            const float rRms = rms (rTail);
            return std::sqrt (lRms * lRms + rRms * rRms);
        };

        const float rms0 = measureRms (0.0f);
        const float rms50 = measureRms (50.0f);
        const float rms100 = measureRms (100.0f);
        const float minRms = juce::jmin (rms0, juce::jmin (rms50, rms100));
        const float maxRms = juce::jmax (rms0, juce::jmax (rms50, rms100));
        const float spreadDb = 20.0f * std::log10 (juce::jmax (1.0e-6f, maxRms) / juce::jmax (1.0e-6f, minRms));
        check (spreadDb < 6.0f, "loudness stays within 6dB across MIX=0/50/100%, rms0=" + juce::String (rms0, 4)
                   + " rms50=" + juce::String (rms50, 4) + " rms100=" + juce::String (rms100, 4) + " spread=" + juce::String (spreadDb, 2) + "dB");
    }

    std::cout << "\n[Integration] Automation zipper: gradual sweep of all 4 params produces no discontinuity\n";
    {
        NovaDoublerEngine engine;
        engine.prepare (sr);

        auto mono = makeToneBurst (n, sr);
        std::vector<float> outL ((size_t) n), outR ((size_t) n);
        const int blockLen = 32;
        for (int start = 0; start < n; start += blockLen)
        {
            const float sweepPos = (float) start / (float) n;
            engine.setParams (sweepPos * 100.0f, (1.0f - sweepPos) * 100.0f, sweepPos * 100.0f, sweepPos > 0.5f, false);
            const int len = juce::jmin (blockLen, n - start);
            const float* blockIn[1] = { mono.data() + start };
            engine.processBlock (blockIn, 1, outL.data() + start, outR.data() + start, len);
        }

        float maxDelta = 0.0f;
        for (int i = 1; i < n; ++i)
            maxDelta = juce::jmax (maxDelta, std::abs (outL[(size_t) i] - outL[(size_t) (i - 1)]));
        check (maxDelta < 0.3f, "max sample-to-sample delta over a full Width/Tightness/Mix/Voices automation sweep stays bounded, got " + juce::String (maxDelta, 4));
    }

    std::cout << "\n[Integration] Sample-rate independence: finite, sane output at all 4 rates\n";
    {
        const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0 };
        for (double rate : rates)
        {
            NovaDoublerEngine engine;
            engine.prepare (rate);
            engine.setParams (60.0f, 40.0f, 80.0f, true, false);

            const int rn = (int) (rate * 0.5);
            auto mono = makeToneBurst (rn, rate);
            std::vector<float> outL ((size_t) rn), outR ((size_t) rn);
            const int blockLen = 512;
            bool finite = true;
            for (int start = 0; start < rn; start += blockLen)
            {
                const int len = juce::jmin (blockLen, rn - start);
                const float* blockIn[1] = { mono.data() + start };
                engine.processBlock (blockIn, 1, outL.data() + start, outR.data() + start, len);
            }
            for (int i = 0; i < rn; ++i)
                if (! std::isfinite (outL[(size_t) i]) || ! std::isfinite (outR[(size_t) i])) finite = false;

            check (finite, "sr=" + juce::String (rate, 0) + "Hz: output stays finite throughout");
        }
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL INTEGRATION VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
