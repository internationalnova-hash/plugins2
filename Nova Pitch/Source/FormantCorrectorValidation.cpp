// Headless validation of NovaFormantCorrector: processPost(processPre(x))
// is a near-transparent round trip at amount=0 (any warp) and at
// amount>0/warp=0, and nonzero amount measurably reduces formant-frequency
// movement under a pitch shift vs. no correction at all.

#include "DSP/NovaFormantCorrector.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <iostream>
#include <vector>

using nova::pitch::NovaFormantCorrector;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    // Simple 2nd-order resonant bandpass biquad (RBJ constant-peak-gain
    // form) - used to build a synthetic "vowel" (pulse train shaped by two
    // formant-like resonances), not to test any actual DSP under test.
    struct ResonantFilter
    {
        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        void set (double sr, float freq, float q)
        {
            const float w0 = 2.0f * juce::MathConstants<float>::pi * freq / (float) sr;
            const float alpha = std::sin (w0) / (2.0f * q);
            const float a0 = 1.0f + alpha;
            b0 = alpha / a0;
            b1 = 0.0f;
            b2 = -alpha / a0;
            a1 = (-2.0f * std::cos (w0)) / a0;
            a2 = (1.0f - alpha) / a0;
        }

        float process (float x)
        {
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
    };

    // Goertzel-style single-frequency magnitude measurement over a block.
    float goertzelMagnitude (const std::vector<float>& x, double sr, float freq)
    {
        const float w = 2.0f * juce::MathConstants<float>::pi * freq / (float) sr;
        float re = 0.0f, im = 0.0f;
        for (size_t n = 0; n < x.size(); ++n)
        {
            re += x[n] * std::cos (w * (float) n);
            im -= x[n] * std::sin (w * (float) n);
        }
        return std::sqrt (re * re + im * im) / (float) x.size();
    }

    std::vector<float> makeVowel (double sr, double seconds, float f0, float f1, float f2)
    {
        const int n = (int) (sr * seconds);
        std::vector<float> out ((size_t) n);

        ResonantFilter r1, r2;
        r1.set (sr, f1, 6.0f);
        r2.set (sr, f2, 6.0f);

        const float period = (float) (sr / f0);
        float phase = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float excite = 0.0f;
            phase += 1.0f;
            if (phase >= period) { phase -= period; excite = 40.0f; } // simple pulse train, strong excitation
            out[(size_t) i] = r1.process (excite) + r2.process (excite);
        }
        return out;
    }

    float rmsError (const std::vector<float>& a, const std::vector<float>& b, int skip)
    {
        double sum = 0.0;
        int count = 0;
        for (size_t i = (size_t) skip; i < a.size() && i < b.size(); ++i)
        {
            const double d = (double) a[i] - (double) b[i];
            sum += d * d;
            ++count;
        }
        return count > 0 ? (float) std::sqrt (sum / count) : 0.0f;
    }

    float rms (const std::vector<float>& a, int skip)
    {
        double sum = 0.0;
        int count = 0;
        for (size_t i = (size_t) skip; i < a.size(); ++i) { sum += (double) a[i] * (double) a[i]; ++count; }
        return count > 0 ? (float) std::sqrt (sum / count) : 0.0f;
    }
}

int main()
{
    const double sr = 44100.0;
    const float f0 = 150.0f, f1 = 700.0f, f2 = 1200.0f;
    auto vowel = makeVowel (sr, 1.0, f0, f1, f2);

    std::cout << "\n[FormantCorrector] processPost(processPre(x)) is transparent at amount=0 (any warp)\n";
    {
        for (float warp : { -1.0f, 0.0f, 1.0f })
        {
            NovaFormantCorrector fc;
            fc.prepare (sr);
            fc.setFormantAmount (0.0f);
            fc.setFormantWarp (warp);

            std::vector<float> out (vowel.size());
            for (size_t i = 0; i < vowel.size(); ++i)
                out[i] = fc.processPost (fc.processPre (vowel[i]));

            const float err = rmsError (vowel, out, 0);
            const float sig = rms (vowel, 0);
            check (err < 1.0e-4f * juce::jmax (1.0f, sig),
                   "warp=" + juce::String (warp, 1) + ": round-trip RMS error=" + juce::String (err, 6)
                       + " (signal RMS=" + juce::String (sig, 4) + ")");
        }
    }

    std::cout << "\n[FormantCorrector] processPost(processPre(x)) is a close round trip at amount=1, warp=0\n";
    {
        NovaFormantCorrector fc;
        fc.prepare (sr);
        fc.setFormantAmount (1.0f);
        fc.setFormantWarp (0.0f);

        std::vector<float> out (vowel.size());
        for (size_t i = 0; i < vowel.size(); ++i)
            out[i] = fc.processPost (fc.processPre (vowel[i]));

        // Skip the first ~4 hops (~1024 samples) while LPC coefficients are
        // still converging from their initial all-zero state.
        const int skip = 4096;
        const float err = rmsError (vowel, out, skip);
        const float sig = rms (vowel, skip);
        check (err < 0.15f * sig,
               "amount=1 round-trip RMS error=" + juce::String (err, 4) + " vs signal RMS=" + juce::String (sig, 4)
                   + " (error under 15% of signal level once coefficients have converged)");
    }

    std::cout << "\n[FormantCorrector] Formant correction measurably reduces formant-frequency movement under a pitch shift\n";
    {
        // Let coefficients converge on the real vowel signal first.
        NovaFormantCorrector fc;
        fc.prepare (sr);
        fc.setFormantAmount (1.0f);
        fc.setFormantWarp (0.0f);

        std::vector<float> residual (vowel.size());
        for (size_t i = 0; i < vowel.size(); ++i)
            residual[i] = fc.processPre (vowel[i]);

        // Naive shift: resample the raw vowel directly (formants move with pitch).
        const float ratio = 1.5f; // shift up a fifth
        auto resample = [&] (const std::vector<float>& src) -> std::vector<float>
        {
            const int n = (int) ((float) src.size() / ratio);
            std::vector<float> out ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const float pos = (float) i * ratio;
                const int i0 = (int) pos;
                const int i1 = juce::jmin ((int) src.size() - 1, i0 + 1);
                const float t = pos - (float) i0;
                out[(size_t) i] = src[(size_t) i0] + t * (src[(size_t) i1] - src[(size_t) i0]);
            }
            return out;
        };

        auto naiveShifted = resample (vowel);
        auto residualShifted = resample (residual);

        // Corrected shift: re-apply the (unwarped, already-converged)
        // formant envelope to the shifted residual.
        std::vector<float> correctedShifted (residualShifted.size());
        for (size_t i = 0; i < residualShifted.size(); ++i)
            correctedShifted[i] = fc.processPost (residualShifted[i]);

        // Measure energy retained AT the original formant frequencies -
        // the naive shift should have moved that energy away (dropping
        // magnitude at 700/1200Hz), while the corrected version should
        // retain meaningfully more of it there.
        const int skip = 2000; // skip filter/glide settling at the start of the shifted signals
        std::vector<float> naiveTail (naiveShifted.begin() + skip, naiveShifted.end());
        std::vector<float> correctedTail (correctedShifted.begin() + skip, correctedShifted.end());

        const float naiveF1 = goertzelMagnitude (naiveTail, sr, f1);
        const float naiveF2 = goertzelMagnitude (naiveTail, sr, f2);
        const float correctedF1 = goertzelMagnitude (correctedTail, sr, f1);
        const float correctedF2 = goertzelMagnitude (correctedTail, sr, f2);

        // 1.15x: a modest but real, consistent bar - a single order-12
        // all-pole model has to represent two resonances plus their
        // interaction from a harmonically-rich pulse-train excitation, so
        // it won't perfectly isolate either formant, but a genuine
        // improvement in the right direction at BOTH is what actually
        // matters here (measured: ~1.2x at F1, ~1.5x at F2).
        check (correctedF1 > naiveF1 * 1.15f,
               "F1(700Hz) magnitude retained better with correction: naive=" + juce::String (naiveF1, 5)
                   + " corrected=" + juce::String (correctedF1, 5));
        check (correctedF2 > naiveF2 * 1.15f,
               "F2(1200Hz) magnitude retained better with correction: naive=" + juce::String (naiveF2, 5)
                   + " corrected=" + juce::String (correctedF2, 5));
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL FORMANT CORRECTOR VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
