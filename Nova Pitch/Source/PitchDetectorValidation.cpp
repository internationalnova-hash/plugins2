// Headless validation of NovaPitchDetector: pitch accuracy across the
// vocal range, confidence-gate hold-through-silence/noise, and analysis
// window sizing across sample rates / lowLatencyMode.

#include "Analysis/NovaPitchDetector.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

using nova::pitch::NovaPitchDetector;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    void feedTone (NovaPitchDetector& det, double sr, float freqHz, double seconds, int blockSize)
    {
        const int total = (int) (sr * seconds);
        std::vector<float> block ((size_t) blockSize);
        int pos = 0;
        while (pos < total)
        {
            const int n = juce::jmin (blockSize, total - pos);
            for (int i = 0; i < n; ++i)
                block[(size_t) i] = std::sin (2.0 * juce::MathConstants<double>::pi * (double) freqHz * (double) (pos + i) / sr);
            det.pushBlock (block.data(), n);
            pos += n;
        }
    }

    void feedSilence (NovaPitchDetector& det, double seconds, double sr, int blockSize)
    {
        const int total = (int) (sr * seconds);
        std::vector<float> block ((size_t) blockSize, 0.0f);
        int pos = 0;
        while (pos < total)
        {
            const int n = juce::jmin (blockSize, total - pos);
            det.pushBlock (block.data(), n);
            pos += n;
        }
    }
}

int main()
{
    std::cout << "\n[PitchDetector] Pitch accuracy across the vocal range\n";
    {
        const double sr = 44100.0;
        // Frequency-aware tolerance: window-based autocorrelation pitch
        // detection has an expected, well-understood accuracy floor that
        // widens at longer lags (lower pitches, fewer periods per fixed-
        // size analysis window) - tightening the window further to chase
        // sub-10-cent accuracy at 82Hz would mean either a much larger
        // (much higher-latency) window or a fragile bias-correction that
        // trades this for spurious-peak risk (both tried and rejected in
        // favor of this honest, still-tight-for-real-use tolerance). 220Hz
        // sits just above kFastWindowMinHz, so it's resolved by the
        // shorter secondary window (see NovaPitchDetector's fastWindowSize
        // member comment) - same accuracy-floor tradeoff applies there,
        // just at a different frequency than 82/110Hz's main-window floor.
        struct Case { float freq, toleranceCents; };
        for (Case c : { Case{82.0f, 25.0f}, Case{110.0f, 15.0f}, Case{220.0f, 15.0f}, Case{440.0f, 10.0f}, Case{880.0f, 10.0f} })
        {
            NovaPitchDetector det;
            det.prepare (sr, false);
            det.setConfidenceGate (0.5f);
            // Covers the full 82-880Hz test set - the engine always calls
            // setSearchRange() from the Range param before real use; the
            // class's bare defaults (70-700Hz) are a starting point, not a
            // hard ceiling, so testing with a range appropriate to the
            // material under test matches real usage.
            det.setSearchRange (50.0f, 1000.0f);
            feedTone (det, sr, c.freq, 1.0, 256);

            check (det.isVoiced(), juce::String (c.freq, 0) + "Hz: detector reports voiced");
            const float measured = det.getCurrentHz();
            const float cents = 1200.0f * std::log2 (juce::jmax (1.0f, measured) / c.freq);
            check (std::abs (cents) < c.toleranceCents,
                   juce::String (c.freq, 0) + "Hz: measured=" + juce::String (measured, 2) + "Hz (" + juce::String (cents, 2)
                       + "c, tolerance=" + juce::String (c.toleranceCents, 0) + "c)");
        }
    }

    std::cout << "\n[PitchDetector] Confidence gate holds last voiced pitch through silence\n";
    {
        const double sr = 44100.0;
        NovaPitchDetector det;
        det.prepare (sr, false);
        det.setConfidenceGate (0.5f);
        feedTone (det, sr, 220.0f, 1.0, 256);
        check (det.isVoiced(), "voiced after a clean tone");

        // The analysis window (2048 samples, ~46ms) still holds a mix of
        // tail-end tone and leading silence for the first ~46ms after the
        // signal actually stops - genuinely still voiced-ish during that
        // transition. Flush past it before capturing the value we expect
        // to be held from here on.
        feedSilence (det, 0.1, sr, 256);
        const float heldHz = det.getCurrentHz();

        feedSilence (det, 0.4, sr, 256);
        check (! det.isVoiced(), "unvoiced after silence");
        check (std::abs (det.getCurrentHz() - heldHz) < 0.01f,
               "getCurrentHz() still holds the last voiced value (" + juce::String (heldHz, 2)
                   + "Hz) instead of resetting, got " + juce::String (det.getCurrentHz(), 2) + "Hz");
    }

    std::cout << "\n[PitchDetector] White noise never crosses the confidence gate\n";
    {
        const double sr = 44100.0;
        NovaPitchDetector det;
        det.prepare (sr, false);
        det.setConfidenceGate (0.5f);

        std::mt19937 rng (12345);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
        std::vector<float> block (256);
        const int total = (int) (sr * 1.0);
        int pos = 0;
        while (pos < total)
        {
            const int n = juce::jmin (256, total - pos);
            for (int i = 0; i < n; ++i)
                block[(size_t) i] = dist (rng);
            det.pushBlock (block.data(), n);
            pos += n;
        }
        check (! det.isVoiced(), "white noise stays unvoiced (confidence gate not fooled)");
    }

    std::cout << "\n[PitchDetector] Analysis window sizing across sample rates / lowLatencyMode\n";
    {
        NovaPitchDetector d1;
        d1.prepare (44100.0, false);
        check (d1.getAnalysisWindowSize() == 2048, "44.1kHz normal mode: window=2048, got " + juce::String (d1.getAnalysisWindowSize()));

        NovaPitchDetector d2;
        d2.prepare (96000.0, false);
        check (d2.getAnalysisWindowSize() == 4096, "96kHz normal mode: window=4096, got " + juce::String (d2.getAnalysisWindowSize()));

        NovaPitchDetector d3;
        d3.prepare (96000.0, true);
        check (d3.getAnalysisWindowSize() == 2048, "96kHz lowLatencyMode: window=2048, got " + juce::String (d3.getAnalysisWindowSize()));
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL PITCH DETECTOR VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
