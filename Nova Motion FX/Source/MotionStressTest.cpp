// Standalone stress test for NovaMotionDSP - reproduces the real-world
// "Motion" macro's instant (unsmoothed - see TD-001) parameter jump from
// off to full, using the exact values the actual applyMotionMacro() JS
// formula computes at max motion, against realistic broadband musical
// content (not a single sine sitting conveniently outside the filtered
// band).
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../../NovaDSP/Modulation/NovaMotionDSP.h"
#include <iostream>
#include <cmath>

int main()
{
    const double sr = 48000.0;
    const int blockSize = 512;

    NovaMotionDSP dsp;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) blockSize, 2 };
    NovaMotionParameters initial;
    dsp.prepare (spec, initial);

    // Real values computed by applyMotionMacro(100) in index.html, mapped
    // through each parameter's actual AudioParameterFloat range (all
    // linear, no skew):
    //   cutoff:     UI 20  -> normalized 0.20 -> 20 + 0.20*19980   = 4016 Hz
    //   resonance:  UI 75  -> normalized 0.75 -> 0.1 + 0.75*19.9   = 15.025 (raw; DSP clamps to 4.0)
    //   feedback:   UI 75  -> normalized 0.75 -> 0   + 0.75*0.95   = 0.7125
    //   delay_mix:  UI 70  -> normalized 0.70 -> 0.70
    //   reverb_mix: UI 80  -> normalized 0.80 -> 0.80
    NovaMotionParameters off;
    off.cutoff = 20000.f; off.resonance = 0.1f; off.feedback = 0.f;
    off.delayMix = 0.f; off.reverbMix = 0.f; off.size = 0.6f; off.mix = 1.f;

    NovaMotionParameters maxMotion;
    maxMotion.cutoff = 4016.f; maxMotion.resonance = 15.025f; maxMotion.feedback = 0.7125f;
    maxMotion.delayMix = 0.70f; maxMotion.reverbMix = 0.80f; maxMotion.size = 0.6f; maxMotion.mix = 1.f;

    auto makeBlock = [&] (int blockIndex) {
        juce::AudioBuffer<float> buf (2, blockSize);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < blockSize; ++i)
            {
                double t = (blockIndex * blockSize + i) / sr;
                // Broadband musical content: fundamental + harmonics spanning
                // bass through presence range, like a real mix would have.
                float s = 0.35f * std::sin (2.0*M_PI*110.0*t)
                        + 0.30f * std::sin (2.0*M_PI*440.0*t)
                        + 0.20f * std::sin (2.0*M_PI*1500.0*t)
                        + 0.15f * std::sin (2.0*M_PI*4000.0*t)
                        + 0.10f * std::sin (2.0*M_PI*8000.0*t);
                d[i] = s;
            }
        }
        return buf;
    };

    int totalBlocks = 0;
    int silentBlocksInARow = 0, worstSilentStreak = 0;
    float minPeakAtMax = 1e9f, maxPeakAtMax = 0.f;

    // Settle at "off" for 20 blocks
    for (int b = 0; b < 20; ++b)
    {
        auto buf = makeBlock (totalBlocks);
        dsp.setParameters (off);
        dsp.process (buf);
        ++totalBlocks;
    }

    // Instant jump to max motion, held for 200 blocks (~2.1s) - matches
    // the real UI, which sends the full set of macro-mapped values in one
    // shot with no interpolation (TD-001).
    for (int b = 0; b < 200; ++b)
    {
        auto buf = makeBlock (totalBlocks);
        dsp.setParameters (maxMotion);
        dsp.process (buf);

        float peak = buf.getMagnitude (0, 0, blockSize);
        bool blockIsSilent = peak < 1e-4f;
        if (blockIsSilent) { ++silentBlocksInARow; worstSilentStreak = std::max (worstSilentStreak, silentBlocksInARow); }
        else silentBlocksInARow = 0;

        if (b >= 5) { minPeakAtMax = std::min (minPeakAtMax, peak); maxPeakAtMax = std::max (maxPeakAtMax, peak); }
        ++totalBlocks;
    }

    std::cout << "Total blocks: " << totalBlocks << "\n";
    std::cout << "Worst consecutive silent-block streak after jump: " << worstSilentStreak
               << " blocks (" << (worstSilentStreak * blockSize / sr * 1000.0) << " ms)\n";
    std::cout << "Peak output level while held at max motion (blocks 5-200): min="
              << minPeakAtMax << " max=" << maxPeakAtMax << "\n";
    std::cout << (worstSilentStreak > 5 ? "BUG REPRODUCED: sustained silence after motion jump\n"
                                        : "No sustained silence detected\n");

    // -----------------------------------------------------------------------
    // Scenario 2: exact real-world repro from user screenshot — Pro Tools,
    // Nova Motion FX, CITRUS SPLASH preset, Motion Macro dragged to 63%.
    // Reproduces the *actual* applyMotionMacro(63) math from index.js
    // (cutoff/resonance/feedback/delay_mix/reverb_mix), gradually ramped
    // over ~50 steps like a real mouse drag (not an instant jump), then
    // held for 10 real seconds to catch slow-building instability (e.g.
    // reverb tail energy accumulation) that a short 2s hold would miss.
    // Tested against both possible "size" values, since the confirmed
    // preset-desync bug means the real DSP-side size could be sitting at
    // either the engine default (0.6) or the preset's displayed value
    // (0.45), depending on what was set before this session.
    {
        std::cout << "\n--- Scenario 2: exact screenshot repro (motion 0 -> 63%, held 10s) ---\n";

        auto runScenario = [&] (float sizeValue, const char* label)
        {
            NovaMotionDSP dsp2;
            dsp2.prepare (spec, NovaMotionParameters{});

            int blocks = 0;
            int worstStreak = 0, streak = 0;
            float minPeak = 1e9f, maxPeak = 0.f;
            bool everSilentAfterSettle = false;

            // Gradual ramp 0 -> 63 over 50 steps, ~1 block per step (mimics
            // per-pointermove drag sends, matching onDragMove in index.js).
            for (int step = 0; step <= 63; ++step)
            {
                float n = step / 100.f;
                NovaMotionParameters p;
                p.cutoff    = juce::jlimit (0.f, 100.f, 80.f - n * 60.f) / 100.f * 19980.f + 20.f;
                p.resonance = juce::jmin (100.f, 20.f + n * 55.f) / 100.f * 19.9f + 0.1f;
                p.feedback  = juce::jmin (100.f, 20.f + n * 55.f) / 100.f * 0.95f;
                p.delayMix  = juce::jmin (100.f, 10.f + n * 60.f) / 100.f;
                p.reverbMix = juce::jmin (100.f, 15.f + n * 65.f) / 100.f;
                p.size      = sizeValue;
                p.inputDb   = 6.0f;
                p.outputDb  = 7.2f;
                p.mix       = 1.0f;

                auto buf = makeBlock (totalBlocks + blocks);
                dsp2.setParameters (p);
                dsp2.process (buf);
                ++blocks;
            }

            // Hold final 63% state for 10 real seconds.
            const int holdBlocks = (int) (10.0 * sr / blockSize);
            NovaMotionParameters held;
            held.cutoff = 8451.56f; held.resonance = 10.9754f; held.feedback = 0.519175f;
            held.delayMix = 0.478f; held.reverbMix = 0.5595f; held.size = sizeValue;
            held.inputDb = 6.0f; held.outputDb = 7.2f; held.mix = 1.0f;

            for (int b = 0; b < holdBlocks; ++b)
            {
                auto buf = makeBlock (totalBlocks + blocks);
                dsp2.setParameters (held);
                dsp2.process (buf);

                float peak = buf.getMagnitude (0, 0, blockSize);
                bool silent = peak < 1e-4f;
                if (silent) { ++streak; worstStreak = std::max (worstStreak, streak); }
                else streak = 0;
                if (b > (int)(sr / blockSize) && silent) everSilentAfterSettle = true; // after 1s settle
                if (b > (int)(sr / blockSize)) { minPeak = std::min (minPeak, peak); maxPeak = std::max (maxPeak, peak); }
                ++blocks;
            }

            std::cout << "  [" << label << "] blocks=" << blocks
                      << " worstSilentStreak=" << worstStreak
                      << " (" << (worstStreak * blockSize / sr * 1000.0) << " ms)"
                      << " minPeak(after 1s)=" << minPeak << " maxPeak=" << maxPeak
                      << (everSilentAfterSettle ? "  BUG: silence after settle" : "  OK")
                      << "\n";
        };

        runScenario (0.6f,  "size=0.6 (engine default, if preset never synced)");
        runScenario (0.45f, "size=0.45 (preset's displayed value, if it did sync)");
    }

    return 0;
}
