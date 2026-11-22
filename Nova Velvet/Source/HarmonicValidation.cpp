// Headless harmonic/THD validation of NovaVelvetTubeStage and
// NovaVelvetTransformerStage - spec's own "THD strictly monotonic with
// drive, low at nominal, no sudden digital clipping/aliasing" requirement,
// and this catalog's own already-documented regression check (an earlier
// tube-stage design with drive-dependent makeup gain made heavier drive
// measure as LESS distortion - this test exists specifically to catch that
// class of bug again).

#include "DSP/NovaVelvetTubeStage.h"
#include "DSP/NovaVelvetTransformerStage.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <iostream>
#include <vector>

using namespace nova::velvet;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    // Runs a pure sine through the stage, FFTs the (settled) output, and
    // returns {fundamentalMag, sumOfHarmonics2to5PlusMag}.
    struct HarmonicResult { double fundamental; double harmonicsSum; double thdPercent; };

    template <typename ProcessFn>
    HarmonicResult measureHarmonics (ProcessFn processBuffer, double sr, double freq, float amplitude)
    {
        constexpr int fftOrder = 14; // 16384-point FFT
        constexpr int fftSize = 1 << fftOrder;
        juce::dsp::FFT fft (fftOrder);

        // Settle the stage's internal state first (a few hundred ms of the
        // same sine, discarded) so we measure steady-state harmonics, not
        // the stage's own attack transient.
        juce::AudioBuffer<float> settleBuffer (1, (int) (sr * 0.2));
        {
            auto* d = settleBuffer.getWritePointer (0);
            for (int i = 0; i < settleBuffer.getNumSamples(); ++i)
                d[i] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr);
        }
        processBuffer (settleBuffer);

        juce::AudioBuffer<float> analysisBuffer (1, fftSize);
        {
            auto* d = analysisBuffer.getWritePointer (0);
            for (int i = 0; i < fftSize; ++i)
                d[i] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr);
        }
        processBuffer (analysisBuffer);

        std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
        const auto* src = analysisBuffer.getReadPointer (0);
        // Apply a Hann window to reduce spectral leakage before the FFT.
        for (int i = 0; i < fftSize; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (2.0 * juce::MathConstants<double>::pi * i / (fftSize - 1));
            fftData[(size_t) i] = src[i] * w;
        }
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const double binHz = sr / fftSize;
        const auto binFor = [binHz] (double hz) { return (int) std::round (hz / binHz); };
        const auto magAt = [&] (double hz) -> double
        {
            const int bin = binFor (hz);
            double peak = 0.0;
            for (int b = juce::jmax (0, bin - 2); b <= juce::jmin (fftSize / 2, bin + 2); ++b)
                peak = juce::jmax (peak, (double) fftData[(size_t) b]);
            return peak;
        };

        const double fundamental = magAt (freq);
        double harmonicsSum = 0.0;
        for (int h = 2; h <= 8; ++h)
        {
            const double hz = freq * h;
            if (hz < sr * 0.45) // stay well clear of Nyquist
                harmonicsSum += magAt (hz) * magAt (hz);
        }
        harmonicsSum = std::sqrt (harmonicsSum);

        const double thd = fundamental > 1.0e-9 ? (harmonicsSum / fundamental) * 100.0 : 0.0;
        return { fundamental, harmonicsSum, thd };
    }
}

int main()
{
    const double sr = 44100.0;
    const double testFreq = 1000.0;

    std::cout << "\n[Harmonic] NovaVelvetTubeStage: THD monotonic with drive, low at nominal\n";
    {
        std::vector<float> drives = { 0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 1.0f };
        std::vector<double> thds;

        for (float drive : drives)
        {
            NovaVelvetTubeStage stage;
            juce::dsp::ProcessSpec spec { sr, 512, 1 };
            stage.prepare (spec);
            stage.setDrive (drive);

            auto result = measureHarmonics ([&stage] (juce::AudioBuffer<float>& buf) { stage.process (buf); }, sr, testFreq, 0.5f);
            thds.push_back (result.thdPercent);
            std::cout << "  drive=" << drive << " -> THD=" << result.thdPercent << "%\n";
        }

        bool monotonic = true;
        for (size_t i = 1; i < thds.size(); ++i)
            if (thds[i] < thds[i - 1] - 0.01) monotonic = false; // small float tolerance
        check (monotonic, "THD is monotonic non-decreasing with drive (catches the drive-dependent-makeup-gain regression)");
        check (thds.front() < 0.5, "THD at drive=0 (bypass) is near-zero");

        bool allFinite = true;
        for (double t : thds) if (! std::isfinite (t)) allFinite = false;
        check (allFinite, "THD measurements stay finite across the whole drive range");
    }

    std::cout << "\n[Harmonic] NovaVelvetTransformerStage: THD monotonic with drive\n";
    {
        std::vector<float> drives = { 0.0f, 0.12f, 0.20f, 0.5f, 1.0f };
        std::vector<double> thds;

        for (float drive : drives)
        {
            NovaVelvetTransformerStage stage;
            stage.configure (150.0f, 9000.0f);
            juce::dsp::ProcessSpec spec { sr, 512, 1 };
            stage.prepare (spec);
            stage.setDriveLevel (drive);

            // Use a low-ish test frequency (200Hz) so the LF-band saturation
            // path (below the 150Hz split) is actually exercised.
            auto result = measureHarmonics ([&stage] (juce::AudioBuffer<float>& buf) { stage.process (buf); }, sr, 200.0, 0.5f);
            thds.push_back (result.thdPercent);
            std::cout << "  drive=" << drive << " -> THD=" << result.thdPercent << "%\n";
        }

        bool monotonic = true;
        for (size_t i = 1; i < thds.size(); ++i)
            if (thds[i] < thds[i - 1] - 0.01) monotonic = false;
        check (monotonic, "THD is monotonic non-decreasing with drive");
        check (thds.front() < 0.5, "THD at drive=0 (bypass) is near-zero");
    }

    std::cout << "\n[Harmonic] Restrained-at-nominal sanity check (spec: input transformer should be far more\n"
                  "           restrained than this catalog's own Nova Heat saturator at its default)\n";
    {
        NovaVelvetTransformerStage inputStage;
        inputStage.configure (100.0f, 9000.0f);
        juce::dsp::ProcessSpec spec { sr, 512, 1 };
        inputStage.prepare (spec);
        inputStage.setDriveLevel (0.12f); // kInputTransformerBaselineDrive

        auto result = measureHarmonics ([&inputStage] (juce::AudioBuffer<float>& buf) { inputStage.process (buf); }, sr, 200.0, 0.5f);
        std::cout << "  input transformer at baseline drive (0.12): THD=" << result.thdPercent << "%\n";
        check (result.thdPercent < 5.0, "input transformer stays well under 5% THD at its restrained baseline drive");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL HARMONIC VALIDATION TESTS PASSED\n" : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";
    return failures == 0 ? 0 : 1;
}
