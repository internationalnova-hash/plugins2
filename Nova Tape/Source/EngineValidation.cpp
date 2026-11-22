#include "NovaTapeEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <iostream>
#include <cmath>

using namespace nova::tape;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    void fillSine (juce::AudioBuffer<float>& buf, double sr, float freqHz, float amp)
    {
        double phase = 0.0;
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const float x = amp * (float) std::sin (phase);
            phase += juce::MathConstants<double>::twoPi * freqHz / sr;
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                buf.setSample (ch, i, x);
        }
    }

    bool allFinite (const juce::AudioBuffer<float>& buf)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* d = buf.getReadPointer (ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                if (! std::isfinite (d[i])) return false;
        }
        return true;
    }
}

int main()
{
    constexpr int blockSize = 512;

    std::cout << "\n[Engine] Finite, bounded output on real program-like material across sample rates\n";
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        NovaTapeEngine engine;
        engine.prepare (sr, blockSize);

        NovaTapeEngine::Params p;
        p.tapeAmountPercent = 90.0f;
        p.tapeType = TapeType::Hot;
        p.ips = IpsSpeed::Speed7_5;
        p.wowFlutter0to10 = 8.0f;
        p.noise0to10 = 5.0f;
        engine.setParams (p);

        juce::AudioBuffer<float> buf (2, blockSize);
        bool finite = true;
        for (int block = 0; block < 20 && finite; ++block)
        {
            fillSine (buf, sr, 220.0f, 0.8f);
            engine.process (buf);
            finite = allFinite (buf);
        }
        check (finite, "sr=" + juce::String (sr) + "Hz: finite output across 20 blocks at aggressive settings");
    }

    std::cout << "\n[Engine] Silence in produces silence (or near-silence) out - no NaN/denormal blowup\n";
    {
        NovaTapeEngine engine;
        engine.prepare (48000.0, blockSize);
        NovaTapeEngine::Params p;
        p.noise0to10 = 0.0f; // silence should stay silent with noise off
        engine.setParams (p);

        juce::AudioBuffer<float> buf (2, blockSize);
        buf.clear();
        for (int block = 0; block < 50; ++block)
            engine.process (buf);

        check (allFinite (buf), "silence input stays finite throughout");
        check (buf.getMagnitude (0, blockSize) < 0.001f, "settled output on silence (noise=0) stays near zero, got "
               + juce::String (buf.getMagnitude (0, blockSize), 6));
    }

    std::cout << "\n[Engine] MIX crossfades correctly between dry and processed\n";
    {
        NovaTapeEngine engine;
        engine.prepare (48000.0, blockSize);
        NovaTapeEngine::Params p;
        p.tapeAmountPercent = 100.0f;
        p.mixPercent = 0.0f; // fully dry
        p.wowFlutter0to10 = 0.0f;
        p.noise0to10 = 0.0f;
        engine.setParams (p);

        juce::AudioBuffer<float> dryRef (2, blockSize);
        fillSine (dryRef, 48000.0, 220.0f, 0.5f);
        juce::AudioBuffer<float> buf (2, blockSize);
        // Feed a FRESH copy of the dry signal each call (not the previous
        // call's already-mutated output) so only the final iteration's
        // settled behavior is measured, not an accumulation of earlier
        // wet-mixed passes made before the smoother had settled.
        for (int i = 0; i < 5; ++i) { buf.makeCopyOf (dryRef); engine.process (buf); }

        float maxDiff = 0.0f;
        for (int i = 0; i < blockSize; ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (buf.getSample (0, i) - dryRef.getSample (0, i)));
        check (maxDiff < 0.05f, "mix=0 stays close to dry (allowing for input/output gain being neutral), maxDiff=" + juce::String (maxDiff, 4));
    }

    std::cout << "\n[Engine] Bypass crossfades to the untouched input\n";
    {
        NovaTapeEngine engine;
        engine.prepare (48000.0, blockSize);
        NovaTapeEngine::Params p;
        p.bypassed = true;
        p.tapeAmountPercent = 100.0f;
        p.inputDb = 12.0f; // would be very audible if NOT bypassed
        engine.setParams (p);

        juce::AudioBuffer<float> dryRef (2, blockSize);
        fillSine (dryRef, 48000.0, 220.0f, 0.5f);
        juce::AudioBuffer<float> buf (2, blockSize);
        for (int i = 0; i < 10; ++i) { buf.makeCopyOf (dryRef); engine.process (buf); } // let bypass ramp fully settle

        float maxDiff = 0.0f;
        for (int i = 0; i < blockSize; ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (buf.getSample (0, i) - dryRef.getSample (0, i)));
        check (maxDiff < 0.01f, "bypassed output matches dry input closely, maxDiff=" + juce::String (maxDiff, 4));
    }

    std::cout << "\n[Engine] reset() is safe mid-stream (no crash, output stays finite after)\n";
    {
        NovaTapeEngine engine;
        engine.prepare (48000.0, blockSize);
        NovaTapeEngine::Params p;
        p.tapeAmountPercent = 80.0f;
        p.wowFlutter0to10 = 5.0f;
        engine.setParams (p);

        juce::AudioBuffer<float> buf (2, blockSize);
        fillSine (buf, 48000.0, 220.0f, 0.6f);
        engine.process (buf);
        engine.reset();
        fillSine (buf, 48000.0, 220.0f, 0.6f);
        engine.process (buf);
        check (allFinite (buf), "output stays finite after a mid-stream reset()");
    }

    std::cout << "\n[Engine] Oversampling mode switch is safe (no crash, finite output after reconfigure)\n";
    {
        NovaTapeEngine engine;
        engine.prepare (48000.0, blockSize);
        NovaTapeEngine::Params p;
        p.tapeAmountPercent = 70.0f;
        engine.setParams (p);

        juce::AudioBuffer<float> buf (2, blockSize);
        fillSine (buf, 48000.0, 220.0f, 0.5f);
        engine.process (buf);

        engine.reconfigureOversamplingIfNeeded (48000.0, blockSize, 0); // Off
        fillSine (buf, 48000.0, 220.0f, 0.5f);
        engine.process (buf);
        check (allFinite (buf), "finite output after switching oversampling Off");

        engine.reconfigureOversamplingIfNeeded (48000.0, blockSize, 2); // 4x
        fillSine (buf, 48000.0, 220.0f, 0.5f);
        engine.process (buf);
        check (allFinite (buf), "finite output after switching oversampling to 4x");
    }

    std::cout << "\n[Engine] getLatencySamples() reports a sane, non-negative value\n";
    {
        NovaTapeEngine engine;
        engine.prepare (48000.0, blockSize);
        engine.reconfigureOversamplingIfNeeded (48000.0, blockSize, 1);
        const int latency = engine.getLatencySamples();
        check (latency >= 0 && latency < 4096, "latency=" + juce::String (latency) + " samples is in a sane range");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL ENGINE VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
