/*
  Nova Clean — Functional DSP diagnostic.
  Directly instantiates the real processor and drives it with synthetic
  noise/click signals to verify the noise-reduction and click-repair DSP
  actually reduces energy/clicks by a measurable amount - not just that the
  code compiles and loads. This targets the reported bug: several UI-visible
  controls (Strength, Shape, Interpolation, Vocal Protect, Transient Guard)
  were declared but never read by the DSP, and the noise-reduction depth was
  capped too low to be clearly audible.
*/

#include <JuceHeader.h>
#include <iostream>
#include <cmath>
#include <random>

#include "PluginProcessor.h"

namespace
{
    void setParam (NovaCleanV2AudioProcessor& proc, const char* id, float value)
    {
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    // Fills a buffer with broadband white noise at the given RMS amplitude.
    void fillNoise (juce::AudioBuffer<float>& buffer, float rmsAmplitude, std::mt19937& rng)
    {
        std::normal_distribution<float> dist (0.0f, rmsAmplitude);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int s = 0; s < buffer.getNumSamples(); ++s)
                data[s] = dist (rng);
        }
    }

    void addSine (juce::AudioBuffer<float>& buffer, float amplitude, float freqHz, double sampleRate, int startSampleOffset)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int s = 0; s < buffer.getNumSamples(); ++s)
            {
                const float t = (float) (startSampleOffset + s) / (float) sampleRate;
                data[s] += amplitude * std::sin (2.0f * juce::MathConstants<float>::pi * freqHz * t);
            }
        }
    }

    double rms (const juce::AudioBuffer<float>& buffer)
    {
        double sum = 0.0;
        juce::int64 count = 0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);
            for (int s = 0; s < buffer.getNumSamples(); ++s)
            {
                sum += (double) data[s] * (double) data[s];
                ++count;
            }
        }
        return std::sqrt (sum / (double) juce::jmax ((juce::int64) 1, count));
    }

    bool allFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);
            for (int s = 0; s < buffer.getNumSamples(); ++s)
                if (! std::isfinite (data[s]))
                    return false;
        }
        return true;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::cout << "=== NOVA CLEAN FUNCTIONAL DSP DIAGNOSTIC ===\n\n";

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr int numBlocksForNoiseFloorLearn = 60; // ~640ms, well past the 30-frame min-stats window
    constexpr int numBlocksMeasure = 40;

    int passed = 0, failed = 0;
    auto check = [&] (const std::string& label, bool ok)
    {
        std::cout << (ok ? "  [PASS] " : "  [FAIL] ") << label << "\n";
        if (ok) ++passed; else ++failed;
    };

    std::mt19937 rng (12345);

    // ── Test 1: noise reduction actually reduces broadband noise energy ──
    std::cout << "--- Test 1: noise-reduction depth (Clean=100, Preserve=0 vs Preserve=100) ---\n";
    {
        auto measureReduction = [&] (float cleanVal, float preserveVal) -> std::pair<double, double>
        {
            NovaCleanV2AudioProcessor proc;
            proc.prepareToPlay (sampleRate, blockSize);
            setParam (proc, "clean", cleanVal);
            setParam (proc, "preserve", preserveVal);
            setParam (proc, "mix", 100.0f);
            setParam (proc, "sensitivity", 0.0f); // minimize click-repair interference with this NR-focused measurement

            std::mt19937 localRng (777);
            juce::MidiBuffer midi;

            // Let the noise-floor tracker learn the (constant) noise level.
            for (int b = 0; b < numBlocksForNoiseFloorLearn; ++b)
            {
                juce::AudioBuffer<float> buf (2, blockSize);
                fillNoise (buf, 0.05f, localRng);
                proc.processBlock (buf, midi);
            }

            // Now measure input vs output RMS over a further stretch of the same noise.
            double inSum = 0.0, outSum = 0.0;
            juce::int64 n = 0;
            for (int b = 0; b < numBlocksMeasure; ++b)
            {
                juce::AudioBuffer<float> buf (2, blockSize);
                fillNoise (buf, 0.05f, localRng);
                const double inRms = rms (buf);
                proc.processBlock (buf, midi);
                const double outRms = rms (buf);
                inSum += inRms; outSum += outRms;
                ++n;
            }
            return { inSum / (double) n, outSum / (double) n };
        };

        auto [inLo, outLo] = measureReduction (100.0f, 0.0f);     // max depth, no protection
        auto [inHi, outHi] = measureReduction (100.0f, 100.0f);   // max depth, full protection

        const double reductionLoDb = 20.0 * std::log10 (juce::jmax (1.0e-9, outLo / juce::jmax (1.0e-9, inLo)));
        const double reductionHiDb = 20.0 * std::log10 (juce::jmax (1.0e-9, outHi / juce::jmax (1.0e-9, inHi)));

        std::cout << "  [INFO] Clean=100/Preserve=0   -> " << reductionLoDb << " dB (in=" << inLo << " out=" << outLo << ")\n";
        std::cout << "  [INFO] Clean=100/Preserve=100 -> " << reductionHiDb << " dB (in=" << inHi << " out=" << outHi << ")\n";

        check ("Clean=100/Preserve=0 reduces noise by at least 15dB", reductionLoDb <= -15.0);
        check ("Preserve=100 reduces noise less aggressively than Preserve=0", reductionHiDb > reductionLoDb + 3.0);
    }

    // ── Test 2: click repair actually reduces an injected click's energy ──
    std::cout << "\n--- Test 2: click repair reduces an injected click (Strength=100 vs Strength=0) ---\n";
    {
        auto measureClickRepair = [&] (float strengthVal) -> double
        {
            NovaCleanV2AudioProcessor proc;
            proc.prepareToPlay (sampleRate, blockSize);
            setParam (proc, "clean", 0.0f); // isolate click-repair from NR
            setParam (proc, "sensitivity", 100.0f);
            setParam (proc, "strength", strengthVal);
            setParam (proc, "mix", 100.0f);

            juce::MidiBuffer midi;

            // Prime the pipeline with quiet steady tone so surrounding-average
            // stats are established before the click hits.
            for (int b = 0; b < 4; ++b)
            {
                juce::AudioBuffer<float> buf (2, blockSize);
                buf.clear();
                addSine (buf, 0.05f, 220.0f, sampleRate, b * blockSize);
                proc.processBlock (buf, midi);
            }

            // Inject a sharp click (large single-sample discontinuity) into an
            // otherwise quiet block.
            juce::AudioBuffer<float> clickBlock (2, blockSize);
            clickBlock.clear();
            addSine (clickBlock, 0.05f, 220.0f, sampleRate, 4 * blockSize);
            const int clickPos = blockSize / 2;
            for (int ch = 0; ch < 2; ++ch)
                clickBlock.setSample (ch, clickPos, 0.9f);

            const double clickInputAbs = std::abs (clickBlock.getSample (0, clickPos));
            proc.processBlock (clickBlock, midi);

            // The repair is lookahead-delayed, so scan the whole block (plus a
            // couple of follow-up blocks) for the post-repair peak that used
            // to be the click.
            double peakAfter = 0.0;
            for (int s = 0; s < clickBlock.getNumSamples(); ++s)
                peakAfter = juce::jmax (peakAfter, (double) std::abs (clickBlock.getSample (0, s)));

            for (int b = 0; b < 3; ++b)
            {
                juce::AudioBuffer<float> buf (2, blockSize);
                buf.clear();
                addSine (buf, 0.05f, 220.0f, sampleRate, (5 + b) * blockSize);
                proc.processBlock (buf, midi);
                for (int s = 0; s < buf.getNumSamples(); ++s)
                    peakAfter = juce::jmax (peakAfter, (double) std::abs (buf.getSample (0, s)));
            }

            juce::ignoreUnused (clickInputAbs);
            return peakAfter;
        };

        const double peakFullStrength = measureClickRepair (100.0f);
        const double peakZeroStrength = measureClickRepair (0.0f);

        std::cout << "  [INFO] post-repair peak, Strength=100: " << peakFullStrength << "\n";
        std::cout << "  [INFO] post-repair peak, Strength=0:   " << peakZeroStrength << "\n";

        check ("Strength=100 removes the click (peak well below the 0.9 click amplitude)", peakFullStrength < 0.3);
        check ("Strength=0 leaves the click mostly intact (Strength now has real effect)", peakZeroStrength > 0.7);
    }

    // ── Test 3: output stays finite across mode/param combinations ───────
    std::cout << "\n--- Test 3: finite, bounded output across modes and extreme params ---\n";
    {
        const char* modeNames[] = { "Vocal", "Digital", "Crackle" };
        for (int modeIdx = 0; modeIdx < 3; ++modeIdx)
        {
            NovaCleanV2AudioProcessor proc;
            proc.prepareToPlay (sampleRate, blockSize);
            setParam (proc, "mode", (float) modeIdx);
            setParam (proc, "clean", 100.0f);
            setParam (proc, "preserve", 0.0f);
            setParam (proc, "sensitivity", 100.0f);
            setParam (proc, "strength", 100.0f);
            setParam (proc, "shape", 100.0f);
            setParam (proc, "interpolation", 1.0f); // Smart
            setParam (proc, "vocalProtect", 0.0f);
            setParam (proc, "transientGuard", 0.0f);
            setParam (proc, "mix", 100.0f);

            std::mt19937 localRng (99);
            juce::MidiBuffer midi;
            bool finite = true;
            for (int b = 0; b < 20; ++b)
            {
                juce::AudioBuffer<float> buf (2, blockSize);
                fillNoise (buf, 0.2f, localRng);
                if (b % 5 == 0)
                    buf.setSample (0, blockSize / 2, 1.5f); // occasional loud click
                proc.processBlock (buf, midi);
                finite = finite && allFinite (buf);
            }
            check (std::string ("mode=") + modeNames[modeIdx] + " stays finite under extreme params", finite);
        }
    }

    std::cout << "\n=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}
