// Headless validation of NovaBendPitchDetector: pitch accuracy across the
// vocal range, confidence-gate hold-through-silence/noise, and analysis
// window sizing across sample rates / Live-Studio quality.

#include "Analysis/NovaBendPitchDetector.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

using nova::bend::NovaBendPitchDetector;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    void feedTone (NovaBendPitchDetector& det, double sr, float freqHz, double seconds, int blockSize)
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

    void feedSilence (NovaBendPitchDetector& det, double seconds, double sr, int blockSize)
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
        struct Case { float freq, toleranceCents; };
        for (Case c : { Case{82.0f, 25.0f}, Case{110.0f, 15.0f}, Case{220.0f, 15.0f}, Case{440.0f, 10.0f}, Case{880.0f, 10.0f} })
        {
            NovaBendPitchDetector det;
            det.prepare (sr, false);
            det.setConfidenceGate (0.5f);
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
        NovaBendPitchDetector det;
        det.prepare (sr, false);
        det.setConfidenceGate (0.5f);
        feedTone (det, sr, 220.0f, 1.0, 256);
        check (det.isVoiced(), "voiced after a clean tone");

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
        NovaBendPitchDetector det;
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
        check (! det.isVoiced(), "white noise stays unvoiced (confidence gate not fooled) - matters for Flatten's unvoiced carve-out (spec 7/23)");
    }

    std::cout << "\n[PitchDetector] Analysis window sizing across sample rates / quality\n";
    {
        NovaBendPitchDetector d1;
        d1.prepare (44100.0, false);
        check (d1.getAnalysisWindowSize() == 2048, "44.1kHz Studio: window=2048, got " + juce::String (d1.getAnalysisWindowSize()));

        NovaBendPitchDetector d2;
        d2.prepare (96000.0, false);
        check (d2.getAnalysisWindowSize() == 4096, "96kHz Studio: window=4096, got " + juce::String (d2.getAnalysisWindowSize()));

        NovaBendPitchDetector d3;
        d3.prepare (96000.0, true);
        check (d3.getAnalysisWindowSize() == 2048, "96kHz Live: window=2048, got " + juce::String (d3.getAnalysisWindowSize()));
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL PITCH DETECTOR VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
