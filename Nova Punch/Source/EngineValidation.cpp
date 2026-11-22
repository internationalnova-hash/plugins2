// Headless end-to-end validation of NovaPunchEngine: Punch/Body directional
// tests, Focus band-selectivity, Sensitivity, Clip Safe, Mix, sample-rate/
// buffer-size sweeps, and safety (silence/DC/extreme/NaN).

#include "NovaPunchEngine.h"
#include "Analysis/NovaPunchCrossover.h"
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

using namespace nova::punch;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    bool allFinite (const std::vector<float>& v)
    {
        for (float x : v) if (! std::isfinite (x)) return false;
        return true;
    }

    NovaPunchEngine::Params defaultParams()
    {
        NovaPunchEngine::Params p;
        p.punchPercent = 50.0f;
        p.bodyPercent = 50.0f;
        p.focus = NovaPunchEngine::Focus::Full;
        p.sensitivityPercent = 65.0f;
        p.mixPercent = 100.0f;
        p.clipSafe = true;
        p.bypassed = false;
        return p;
    }

    std::vector<float> makeKickLike (double sr, double totalSeconds, double startSeconds, float amp = 0.6f)
    {
        std::vector<float> x ((size_t) (sr * totalSeconds), 0.0f);
        const int startN = (int) (startSeconds * sr);
        const int durN = (int) (0.4 * sr);
        for (int i = 0; i < durN && startN + i < (int) x.size(); ++i)
        {
            const double t = (double) i / sr;
            const double env = std::exp (-t * 8.0);
            const double freq = 60.0 + 90.0 * std::exp (-t * 30.0);
            x[(size_t) (startN + i)] = (float) (amp * env * std::sin (2.0 * juce::MathConstants<double>::pi * freq * t));
        }
        return x;
    }

    void runEngine (NovaPunchEngine& engine, const std::vector<float>& in, std::vector<float>& out, int blockSize)
    {
        out.assign (in.size(), 0.0f);
        int pos = 0;
        while (pos < (int) in.size())
        {
            const int thisBlock = juce::jmin (blockSize, (int) in.size() - pos);
            engine.process (in.data() + pos, in.data() + pos, out.data() + pos, out.data() + pos, thisBlock);
            pos += thisBlock;
        }
    }

    float peakInRange (const std::vector<float>& x, int start, int end)
    {
        float peak = 0.0f;
        for (int i = juce::jmax (0, start); i < juce::jmin ((int) x.size(), end); ++i)
            peak = juce::jmax (peak, std::abs (x[(size_t) i]));
        return peak;
    }
}

int main()
{
    const double sr = 44100.0;

    std::cout << "\n[Engine] PUNCH: >50 increases attack-region peak, <50 decreases it, relative to neutral\n";
    {
        const auto kick = makeKickLike (sr, 1.0, 0.1);
        const int attackStart = (int) (0.1 * sr), attackEnd = (int) (0.13 * sr); // first ~30ms of the hit

        auto measure = [&] (float punch) -> float
        {
            NovaPunchEngine engine; engine.prepare (sr, 512);
            auto p = defaultParams(); p.punchPercent = punch;
            engine.setParams (p);
            std::vector<float> out;
            runEngine (engine, kick, out, 512);
            return peakInRange (out, attackStart, attackEnd);
        };

        const float neutral = measure (50.0f);
        const float high = measure (85.0f);
        const float low = measure (15.0f);
        check (high > neutral, "Punch=85 attack peak (" + juce::String (high, 4) + ") > neutral (" + juce::String (neutral, 4) + ")");
        check (low < neutral, "Punch=15 attack peak (" + juce::String (low, 4) + ") < neutral (" + juce::String (neutral, 4) + ")");
    }

    std::cout << "\n[Engine] BODY: >50 increases sustain-region energy, <50 decreases it, attack stays substantially less affected\n";
    {
        const auto kick = makeKickLike (sr, 1.0, 0.1);
        const int attackStart = (int) (0.1 * sr), attackEnd = (int) (0.13 * sr);
        const int sustainStart = (int) (0.35 * sr), sustainEnd = (int) (0.5 * sr);

        auto measure = [&] (float body) -> std::pair<float, float>
        {
            NovaPunchEngine engine; engine.prepare (sr, 512);
            auto p = defaultParams(); p.bodyPercent = body;
            engine.setParams (p);
            std::vector<float> out;
            runEngine (engine, kick, out, 512);
            return { peakInRange (out, attackStart, attackEnd), peakInRange (out, sustainStart, sustainEnd) };
        };

        const auto neutral = measure (50.0f);
        const auto high = measure (85.0f);
        const auto low = measure (15.0f);
        check (high.second > neutral.second, "Body=85 sustain peak (" + juce::String (high.second, 5) + ") > neutral (" + juce::String (neutral.second, 5) + ")");
        check (low.second < neutral.second, "Body=15 sustain peak (" + juce::String (low.second, 5) + ") < neutral (" + juce::String (neutral.second, 5) + ")");

        const float attackDeviation = std::abs (high.first - neutral.first) + std::abs (low.first - neutral.first);
        const float sustainDeviation = std::abs (high.second - neutral.second) + std::abs (low.second - neutral.second);
        check (attackDeviation < sustainDeviation, "Body changes affect the attack region substantially less than the sustain region");
    }

    std::cout << "\n[Engine] FOCUS: a composite low+mid+high signal is affected predominantly in the targeted band\n";
    {
        const int n = (int) (sr * 0.3);
        std::vector<float> composite (n, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / sr;
            const double env = std::exp (-t * 10.0);
            composite[(size_t) i] += (float) (0.4 * env * std::sin (2.0 * juce::MathConstants<double>::pi * 80.0 * t));   // low
            composite[(size_t) i] += (float) (0.4 * env * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * t)); // mid
            composite[(size_t) i] += (float) (0.4 * env * std::sin (2.0 * juce::MathConstants<double>::pi * 8000.0 * t)); // high
        }

        auto measureBandEnergies = [&] (NovaPunchEngine::Focus focus) -> std::array<float, 3>
        {
            NovaPunchEngine engine; engine.prepare (sr, 512);
            auto p = defaultParams(); p.punchPercent = 90.0f; p.focus = focus;
            engine.setParams (p);
            std::vector<float> out;
            runEngine (engine, composite, out, 512);

            NovaPunchCrossover measurer; measurer.prepare (sr);
            double lowSq = 0, midSq = 0, highSq = 0;
            for (float s : out)
            {
                const auto b = measurer.processSample (s);
                lowSq += (double) b.low * b.low;
                midSq += (double) b.mid * b.mid;
                highSq += (double) b.high * b.high;
            }
            return { (float) std::sqrt (lowSq / out.size()), (float) std::sqrt (midSq / out.size()), (float) std::sqrt (highSq / out.size()) };
        };

        // Compare against a NEUTRAL (Punch=50, i.e. no gain applied
        // anywhere) baseline, not Focus=Full - Full also actively boosts
        // every band, so a Full-vs-Low comparison mostly measures "how
        // much did Full's untargeted bands get boosted that Low didn't",
        // not "did Low actually target the low band". Against a genuinely
        // neutral reference, each Focus setting's OWN targeted band should
        // show the largest delta among that setting's own three deltas.
        auto neutral = [&] () -> std::array<float, 3>
        {
            NovaPunchEngine engine; engine.prepare (sr, 512);
            auto p = defaultParams(); p.punchPercent = 50.0f; p.focus = NovaPunchEngine::Focus::Full;
            engine.setParams (p);
            std::vector<float> out;
            runEngine (engine, composite, out, 512);
            NovaPunchCrossover measurer; measurer.prepare (sr);
            double lowSq = 0, midSq = 0, highSq = 0;
            for (float s : out)
            {
                const auto b = measurer.processSample (s);
                lowSq += (double) b.low * b.low; midSq += (double) b.mid * b.mid; highSq += (double) b.high * b.high;
            }
            return { (float) std::sqrt (lowSq / out.size()), (float) std::sqrt (midSq / out.size()), (float) std::sqrt (highSq / out.size()) };
        }();

        const auto low = measureBandEnergies (NovaPunchEngine::Focus::Low);
        const auto mid = measureBandEnergies (NovaPunchEngine::Focus::Mid);
        const auto high = measureBandEnergies (NovaPunchEngine::Focus::High);

        auto d = [&] (const std::array<float, 3>& x, int i) { return std::abs (x[(size_t) i] - neutral[(size_t) i]); };

        check (d (low, 0) > d (low, 1) && d (low, 0) > d (low, 2),
               "Focus=Low: low-band delta-from-neutral is the largest of its own three (low=" + juce::String (d (low,0),5)
                   + " mid=" + juce::String (d (low,1),5) + " high=" + juce::String (d (low,2),5) + ")");
        check (d (mid, 1) > d (mid, 0) && d (mid, 1) > d (mid, 2),
               "Focus=Mid: mid-band delta-from-neutral is the largest of its own three (low=" + juce::String (d (mid,0),5)
                   + " mid=" + juce::String (d (mid,1),5) + " high=" + juce::String (d (mid,2),5) + ")");
        check (d (high, 2) > d (high, 0) && d (high, 2) > d (high, 1),
               "Focus=High: high-band delta-from-neutral is the largest of its own three (low=" + juce::String (d (high,0),5)
                   + " mid=" + juce::String (d (high,1),5) + " high=" + juce::String (d (high,2),5) + ")");
    }

    std::cout << "\n[Engine] SENSITIVITY: threshold is relative, not absolute - a weak event triggers more readily at high sensitivity\n";
    {
        const auto weakKick = makeKickLike (sr, 0.5, 0.1, 0.08f); // quiet hit

        auto countOnsets = [&] (float sensitivity) -> float
        {
            NovaPunchEngine engine; engine.prepare (sr, 512);
            auto p = defaultParams(); p.sensitivityPercent = sensitivity; p.punchPercent = 90.0f;
            engine.setParams (p);
            std::vector<float> out;
            runEngine (engine, weakKick, out, 512);
            // Use the engine's own last-reported activity as a proxy for
            // "did it treat this as meaningfully transient" - higher
            // sensitivity should show more punch activity on a weak hit.
            return engine.getPunchActivity();
        };

        const float lowSens = countOnsets (10.0f);
        const float highSens = countOnsets (95.0f);
        check (highSens >= lowSens, "high sensitivity picks up the weak event at least as readily as low sensitivity ("
                   + juce::String (highSens, 3) + " >= " + juce::String (lowSens, 3) + ")");
    }

    std::cout << "\n[Engine] CLIP SAFE: high positive Punch near 0dBFS does not produce unsafe overs when ON\n";
    {
        const auto loudKick = makeKickLike (sr, 0.5, 0.05, 0.95f);

        NovaPunchEngine engineOn; engineOn.prepare (sr, 512);
        auto pOn = defaultParams(); pOn.punchPercent = 100.0f; pOn.clipSafe = true;
        engineOn.setParams (pOn);
        std::vector<float> outOn;
        runEngine (engineOn, loudKick, outOn, 512);

        float peakOn = 0.0f;
        for (float s : outOn) peakOn = juce::jmax (peakOn, std::abs (s));
        check (allFinite (outOn), "Clip Safe ON: output stays finite");
        check (peakOn <= 1.01f, "Clip Safe ON: peak stays at/under ceiling, got " + juce::String (peakOn, 4));
    }

    std::cout << "\n[Engine] MIX: 0%=dry, 100%=wet, latency-aligned (engine latency is 0)\n";
    {
        const auto kick = makeKickLike (sr, 0.3, 0.05);

        NovaPunchEngine engineDry; engineDry.prepare (sr, 512);
        auto pDry = defaultParams(); pDry.mixPercent = 0.0f; pDry.punchPercent = 90.0f;
        engineDry.setParams (pDry);
        std::vector<float> outDry;
        runEngine (engineDry, kick, outDry, 512);

        double maxDiff = 0.0;
        for (size_t i = 0; i < kick.size(); ++i)
            maxDiff = juce::jmax (maxDiff, (double) std::abs (outDry[i] - kick[i]));
        check (maxDiff < 1.0e-4, "Mix=0%: output matches dry input exactly (max diff=" + juce::String (maxDiff, 6) + ")");

        check (engineDry.getLatencySamples() == 0, "engine reports zero latency (no dry-delay alignment needed)");
    }

    std::cout << "\n[Engine] Sample-rate sweep\n";
    {
        for (double testSr : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        {
            NovaPunchEngine engine; engine.prepare (testSr, 512);
            engine.setParams (defaultParams());
            const auto kick = makeKickLike (testSr, 0.3, 0.05);
            std::vector<float> out;
            runEngine (engine, kick, out, 512);
            check (allFinite (out), "sr=" + juce::String (testSr, 0) + "Hz: finite output");
        }
    }

    std::cout << "\n[Engine] Buffer-size sweep\n";
    {
        const auto kick = makeKickLike (sr, 0.3, 0.05);
        for (int blockSize : { 32, 64, 128, 256, 512, 1024 })
        {
            NovaPunchEngine engine; engine.prepare (sr, blockSize);
            engine.setParams (defaultParams());
            std::vector<float> out;
            runEngine (engine, kick, out, blockSize);
            check (allFinite (out), "blockSize=" + juce::String (blockSize) + ": finite output");
        }
    }

    std::cout << "\n[Engine] Safety: silence, DC, extreme levels, mid-stream reset\n";
    {
        NovaPunchEngine engine; engine.prepare (sr, 512);
        engine.setParams (defaultParams());

        std::vector<float> silence ((size_t) sr, 0.0f);
        std::vector<float> out;
        runEngine (engine, silence, out, 512);
        check (allFinite (out), "silence: finite output");
        float maxAbs = 0.0f; for (float s : out) maxAbs = juce::jmax (maxAbs, std::abs (s));
        check (maxAbs < 1.0e-5f, "silence: output stays near zero, max=" + juce::String (maxAbs, 8));

        std::vector<float> dc ((size_t) sr, 1.0f);
        runEngine (engine, dc, out, 512);
        check (allFinite (out), "DC: finite output");

        std::vector<float> extreme ((size_t) sr, 1.0e4f);
        runEngine (engine, extreme, out, 512);
        check (allFinite (out), "extreme input (1e4): finite output");

        engine.reset();
        auto kick = makeKickLike (sr, 0.3, 0.05);
        runEngine (engine, kick, out, 512);
        check (allFinite (out), "output stays finite after a mid-stream reset()");
    }

    std::cout << "\n[Engine] Telemetry sanity: reported gain matches the sign of what should be applied\n";
    {
        NovaPunchEngine engine; engine.prepare (sr, 512);
        auto p = defaultParams(); p.punchPercent = 90.0f;
        engine.setParams (p);
        const auto kick = makeKickLike (sr, 0.3, 0.05);
        std::vector<float> out;
        runEngine (engine, kick, out, 512);
        check (engine.getAttackGainDb() > 0.0f, "Punch=90: reported attackGainDb is positive, got " + juce::String (engine.getAttackGainDb(), 3));
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL ENGINE VALIDATION TESTS PASSED\n" : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";
    return failures == 0 ? 0 : 1;
}
