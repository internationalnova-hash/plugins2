#include "DSP/NovaTapeSaturator.h"
#include "NovaTapeTypes.h"
#include <juce_dsp/juce_dsp.h>
#include <iostream>
#include <cmath>
#include <vector>

using namespace nova::tape;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    // Runs a pure sine through the saturator (mono, left channel), returns
    // the settled tail (drops the first half to let filter/hysteresis
    // state settle) as a windowed magnitude spectrum.
    std::vector<float> renderSpectrum (NovaTapeSaturator& sat, double sr, float freqHz, int fftOrder)
    {
        const int fftSize = 1 << fftOrder;
        const int totalSamples = fftSize * 4;
        std::vector<float> out (fftSize, 0.0f);

        double phase = 0.0;
        for (int i = 0; i < totalSamples; ++i)
        {
            const float x = 0.5f * std::sin (phase);
            phase += juce::MathConstants<double>::twoPi * freqHz / sr;
            const float y = sat.dcBlockL (sat.processSampleL (x));
            if (i >= totalSamples - fftSize)
                out[(size_t) (i - (totalSamples - fftSize))] = y;
        }

        juce::dsp::FFT fft (fftOrder);
        std::vector<float> windowed (fftSize * 2, 0.0f);
        juce::dsp::WindowingFunction<float> window (fftSize, juce::dsp::WindowingFunction<float>::hann);
        for (int i = 0; i < fftSize; ++i) windowed[i] = out[(size_t) i];
        window.multiplyWithWindowingTable (windowed.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform (windowed.data());
        windowed.resize (fftSize / 2);
        return windowed;
    }

    float magAtBin (const std::vector<float>& spectrum, double sr, int fftSize, float freqHz)
    {
        const int bin = (int) std::round (freqHz * fftSize / sr);
        if (bin < 0 || bin >= (int) spectrum.size()) return 0.0f;
        return spectrum[(size_t) bin];
    }
}

int main()
{
    constexpr double sr = 48000.0;
    constexpr int fftOrder = 13; // 8192
    constexpr int fftSize = 1 << fftOrder;
    constexpr float fundamental = 220.0f;

    std::cout << "\n[Saturator] Finite/bounded output under extreme drive\n";
    {
        NovaTapeSaturator sat;
        sat.prepare (sr);
        auto profile = getTapeTypeProfile (TapeType::Hot);
        profile.driveAmount *= 3.0f; // deliberately extreme
        sat.setProfile (profile);
        sat.setUserBias (1.0f);

        bool allFinite = true;
        float maxAbs = 0.0f;
        double phase = 0.0;
        for (int i = 0; i < 48000; ++i)
        {
            const float x = std::sin (phase) * (1.0f + 2.0f * std::sin (phase * 0.01));
            phase += 0.3;
            const float y = sat.dcBlockL (sat.processSampleL (x));
            if (! std::isfinite (y)) allFinite = false;
            maxAbs = juce::jmax (maxAbs, std::abs (y));
        }
        check (allFinite, "output stays finite under extreme drive + bias");
        check (maxAbs < 10.0f, "output stays reasonably bounded, got max=" + juce::String (maxAbs, 3));
    }

    std::cout << "\n[Saturator] DC blocking removes asymmetric-curve DC offset\n";
    {
        NovaTapeSaturator sat;
        sat.prepare (sr);
        sat.setProfile (getTapeTypeProfile (TapeType::Vintage));
        sat.setUserBias (0.8f);

        double sum = 0.0;
        int settleSamples = 4000;
        int measureSamples = 4000;
        double phase = 0.0;
        for (int i = 0; i < settleSamples; ++i)
        {
            const float x = 0.6f * std::sin (phase);
            phase += juce::MathConstants<double>::twoPi * fundamental / sr;
            sat.dcBlockL (sat.processSampleL (x));
        }
        for (int i = 0; i < measureSamples; ++i)
        {
            const float x = 0.6f * std::sin (phase);
            phase += juce::MathConstants<double>::twoPi * fundamental / sr;
            sum += sat.dcBlockL (sat.processSampleL (x));
        }
        const double dcAvg = sum / measureSamples;
        check (std::abs (dcAvg) < 0.01, "settled DC average near zero, got " + juce::String (dcAvg, 5));
    }

    std::cout << "\n[Saturator] Even-harmonic content is present (real tape signature, unlike a symmetric clipper)\n";
    {
        NovaTapeSaturator sat;
        sat.prepare (sr);
        sat.setProfile (getTapeTypeProfile (TapeType::Vintage));
        sat.setUserBias (0.3f);
        auto spectrum = renderSpectrum (sat, sr, fundamental, fftOrder);
        const float h1 = magAtBin (spectrum, sr, fftSize, fundamental);
        const float h2 = magAtBin (spectrum, sr, fftSize, fundamental * 2.0f);
        const float h3 = magAtBin (spectrum, sr, fftSize, fundamental * 3.0f);
        check (h1 > 0.01f, "fundamental present, got " + juce::String (h1, 5));
        check (h2 > 0.0005f, "2nd harmonic (even) measurably present, got " + juce::String (h2, 5));
        std::cout << "  (info) h1=" << h1 << " h2=" << h2 << " h3=" << h3 << "\n";
    }

    std::cout << "\n[Saturator] TapeType personalities produce measurably different THD at equal drive\n";
    {
        auto thdFor = [&] (TapeType type) -> float
        {
            NovaTapeSaturator sat;
            sat.prepare (sr);
            sat.setProfile (getTapeTypeProfile (type));
            auto spectrum = renderSpectrum (sat, sr, fundamental, fftOrder);
            const float h1 = magAtBin (spectrum, sr, fftSize, fundamental);
            float harmonicSumSq = 0.0f;
            for (int n = 2; n <= 6; ++n)
            {
                const float h = magAtBin (spectrum, sr, fftSize, fundamental * (float) n);
                harmonicSumSq += h * h;
            }
            return h1 > 1.0e-6f ? std::sqrt (harmonicSumSq) / h1 : 0.0f;
        };

        const float thdStudio = thdFor (TapeType::Studio);
        const float thdVintage = thdFor (TapeType::Vintage);
        const float thdHot = thdFor (TapeType::Hot);
        std::cout << "  (info) THD Studio=" << thdStudio << " Vintage=" << thdVintage << " Hot=" << thdHot << "\n";

        check (thdStudio < thdVintage, "Studio has lower THD than Vintage (cleanest personality)");
        check (std::abs (thdHot - thdStudio) > 0.01f, "Hot and Studio are measurably different (not the same curve scaled)");
        check (std::abs (thdVintage - thdHot) > 0.001f, "Vintage and Hot are measurably different from each other");
    }

    std::cout << "\n[Saturator] TAPE-amount-style drive scaling progressively increases harmonic density\n";
    {
        auto h2For = [&] (float driveMultiplier) -> float
        {
            NovaTapeSaturator sat;
            sat.prepare (sr);
            auto profile = getTapeTypeProfile (TapeType::Studio);
            profile.driveAmount *= driveMultiplier;
            sat.setProfile (profile);
            auto spectrum = renderSpectrum (sat, sr, fundamental, fftOrder);
            return magAtBin (spectrum, sr, fftSize, fundamental * 2.0f);
        };

        const float low = h2For (0.3f);
        const float mid = h2For (0.7f);
        const float high = h2For (1.15f);
        std::cout << "  (info) h2 at low/mid/high drive: " << low << " / " << mid << " / " << high << "\n";
        check (high >= mid && mid >= low, "2nd harmonic energy increases monotonically with drive");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL TAPE SATURATOR VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
