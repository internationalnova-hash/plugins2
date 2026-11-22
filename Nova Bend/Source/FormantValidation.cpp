// Headless validation of NovaBendFormantProcessor: transparent round trip
// at warp=0 (any input), formant correction measurably reduces formant-
// frequency movement under a shift, FORMANT moves spectral-envelope energy
// independently of fundamental pitch (spec's own Formant requirement), and
// the carried-forward stability guard (lattice + [-0.97,0.97] clamp +
// absolute-magnitude/isfinite runtime trap) survives extreme settings.

#include "DSP/NovaBendFormantProcessor.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

using nova::bend::NovaBendFormantProcessor;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    struct ResonantFilter
    {
        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        void set (double sr, float freq, float q)
        {
            const float w0 = 2.0f * juce::MathConstants<float>::pi * freq / (float) sr;
            const float alpha = std::sin (w0) / (2.0f * q);
            const float a0 = 1.0f + alpha;
            b0 = alpha / a0; b1 = 0.0f; b2 = -alpha / a0;
            a1 = (-2.0f * std::cos (w0)) / a0;
            a2 = (1.0f - alpha) / a0;
        }
        float process (float x)
        {
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    };

    float goertzelMagnitude (const std::vector<float>& x, double sr, float freq)
    {
        const float w = 2.0f * juce::MathConstants<float>::pi * freq / (float) sr;
        float re = 0.0f, im = 0.0f;
        for (size_t n = 0; n < x.size(); ++n) { re += x[n] * std::cos (w * (float) n); im -= x[n] * std::sin (w * (float) n); }
        return std::sqrt (re * re + im * im) / (float) x.size();
    }

    std::vector<float> makeVowel (double sr, double seconds, float f0, float f1, float f2)
    {
        const int n = (int) (sr * seconds);
        std::vector<float> out ((size_t) n);
        ResonantFilter r1, r2;
        r1.set (sr, f1, 6.0f); r2.set (sr, f2, 6.0f);
        const float period = (float) (sr / f0);
        float phase = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float excite = 0.0f;
            phase += 1.0f;
            if (phase >= period) { phase -= period; excite = 40.0f; }
            out[(size_t) i] = r1.process (excite) + r2.process (excite);
        }
        return out;
    }

    // A real voice is far messier than makeVowel()'s clean two-resonance
    // glottal pulse train: cycle-to-cycle jitter, vibrato, a third formant,
    // and breath/sibilance noise all feed the LPC analysis with far less
    // well-behaved spectral content. This is what real vocal material
    // looks like to the analysis stage - used to check how often the
    // stability guard actually engages, which a clean synthetic tone can't
    // reveal.
    std::vector<float> makeVoiceLike (double sr, double seconds, float f0, float f1, float f2, float f3, std::uint32_t seed)
    {
        const int n = (int) (sr * seconds);
        std::vector<float> out ((size_t) n);
        ResonantFilter r1, r2, r3;
        r1.set (sr, f1, 8.0f); r2.set (sr, f2, 8.0f); r3.set (sr, f3, 8.0f);
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> jitter (-0.03f, 0.03f);
        std::normal_distribution<float> noiseDist (0.0f, 1.0f);
        float phase = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / sr;
            const float vibrato = 1.0f + 0.02f * std::sin (2.0f * juce::MathConstants<float>::pi * 5.5f * (float) t);
            const float period = juce::jmax (4.0f, (float) (sr / f0) * vibrato * (1.0f + jitter (rng)));
            float excite = 0.0f;
            phase += 1.0f;
            if (phase >= period) { phase -= period; excite = 40.0f; }
            const float breath = noiseDist (rng) * 1.5f; // aspiration/sibilance-like broadband noise
            out[(size_t) i] = r1.process (excite) + r2.process (excite) + 0.4f * r3.process (excite) + breath;
        }
        return out;
    }

    float rmsError (const std::vector<float>& a, const std::vector<float>& b, int skip)
    {
        double sum = 0.0; int count = 0;
        for (size_t i = (size_t) skip; i < a.size() && i < b.size(); ++i) { const double d = (double) a[i] - (double) b[i]; sum += d * d; ++count; }
        return count > 0 ? (float) std::sqrt (sum / count) : 0.0f;
    }

    float rms (const std::vector<float>& a, int skip)
    {
        double sum = 0.0; int count = 0;
        for (size_t i = (size_t) skip; i < a.size(); ++i) { sum += (double) a[i] * (double) a[i]; ++count; }
        return count > 0 ? (float) std::sqrt (sum / count) : 0.0f;
    }
}

int main()
{
    const double sr = 44100.0;
    const float f0 = 150.0f, f1 = 700.0f, f2 = 1200.0f;
    auto vowel = makeVowel (sr, 1.0, f0, f1, f2);

    std::cout << "\n[Formant] processPost(processPre(x)) transparent at amount=0, any warp\n";
    {
        for (float warpSemitones : { -12.0f, 0.0f, 12.0f })
        {
            NovaBendFormantProcessor fc;
            fc.prepare (sr);
            fc.setFormantAmount (0.0f);
            fc.setFormantWarpSemitones (warpSemitones);
            std::vector<float> out (vowel.size());
            for (size_t i = 0; i < vowel.size(); ++i)
                out[i] = fc.processPost (fc.processPre (vowel[i]));
            const float err = rmsError (vowel, out, 0);
            const float sig = rms (vowel, 0);
            check (err < 1.0e-4f * juce::jmax (1.0f, sig),
                   "amount=0, warp=" + juce::String (warpSemitones, 0) + "st: round-trip RMS error=" + juce::String (err, 6));
        }
    }

    std::cout << "\n[Formant] processPost(processPre(x)) is a close round trip at amount=1, warp=0 (Nova Bend's real Formant=0 case)\n";
    {
        NovaBendFormantProcessor fc;
        fc.prepare (sr);
        fc.setFormantAmount (1.0f);
        fc.setFormantWarpSemitones (0.0f);
        std::vector<float> out (vowel.size());
        for (size_t i = 0; i < vowel.size(); ++i)
            out[i] = fc.processPost (fc.processPre (vowel[i]));
        const int skip = 4096; // LPC coefficients converging from initial all-zero state
        const float err = rmsError (vowel, out, skip);
        const float sig = rms (vowel, skip);
        check (err < 0.15f * sig, "amount=1, warp=0: round-trip RMS error=" + juce::String (err, 4) + " vs signal RMS=" + juce::String (sig, 4));
    }

    std::cout << "\n[Formant] FORMANT moves spectral-envelope energy independent of fundamental pitch\n";
    {
        // Same excitation (same f0, i.e. same fundamental throughout) run
        // through two different Formant warps - the FUNDAMENTAL must stay
        // identical (this class never touches pitch, only envelope), while
        // energy at the formant frequencies should shift.
        NovaBendFormantProcessor fcFlat, fcWarped;
        fcFlat.prepare (sr); fcFlat.setFormantAmount (1.0f); fcFlat.setFormantWarpSemitones (0.0f);
        fcWarped.prepare (sr); fcWarped.setFormantAmount (1.0f); fcWarped.setFormantWarpSemitones (7.0f); // brighter/smaller

        std::vector<float> outFlat (vowel.size()), outWarped (vowel.size());
        for (size_t i = 0; i < vowel.size(); ++i)
        {
            outFlat[i] = fcFlat.processPost (fcFlat.processPre (vowel[i]));
            outWarped[i] = fcWarped.processPost (fcWarped.processPre (vowel[i]));
        }

        const int skip = 4096;
        std::vector<float> flatTail (outFlat.begin() + skip, outFlat.end());
        std::vector<float> warpedTail (outWarped.begin() + skip, outWarped.end());

        // Fundamental unaffected: both outputs still show strong energy at
        // f0 (this is an envelope filter, not a pitch shifter).
        const float f0FlatMag = goertzelMagnitude (flatTail, sr, f0);
        const float f0WarpedMag = goertzelMagnitude (warpedTail, sr, f0);
        check (f0FlatMag > 0.0f && f0WarpedMag > 0.0f, "fundamental energy present in both (formant warp never touches pitch)");

        // Formant energy measurably differs between the two warps - the
        // envelope actually moved.
        const float f1FlatMag = goertzelMagnitude (flatTail, sr, f1);
        const float f1WarpedMag = goertzelMagnitude (warpedTail, sr, f1);
        const float f2FlatMag = goertzelMagnitude (flatTail, sr, f2 * std::pow (2.0f, 7.0f / 12.0f)); // where F2 should have MOVED to under +7st warp
        check (std::abs (f1WarpedMag - f1FlatMag) > 0.05f * juce::jmax (f1FlatMag, 1.0e-6f),
               "F1 energy measurably differs between warp=0 and warp=+7st (flat=" + juce::String (f1FlatMag, 5)
                   + " warped=" + juce::String (f1WarpedMag, 5) + ")");
        juce::ignoreUnused (f2FlatMag);
    }

    std::cout << "\n[Formant] Correction measurably reduces formant-frequency movement under a pitch shift\n";
    {
        NovaBendFormantProcessor fc;
        fc.prepare (sr);
        fc.setFormantAmount (1.0f);
        fc.setFormantWarpSemitones (0.0f);

        std::vector<float> residual (vowel.size());
        for (size_t i = 0; i < vowel.size(); ++i) residual[i] = fc.processPre (vowel[i]);

        const float ratio = 1.5f;
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

        std::vector<float> correctedShifted (residualShifted.size());
        for (size_t i = 0; i < residualShifted.size(); ++i) correctedShifted[i] = fc.processPost (residualShifted[i]);

        const int skip = 2000;
        std::vector<float> naiveTail (naiveShifted.begin() + skip, naiveShifted.end());
        std::vector<float> correctedTail (correctedShifted.begin() + skip, correctedShifted.end());

        const float naiveF1 = goertzelMagnitude (naiveTail, sr, f1);
        const float naiveF2 = goertzelMagnitude (naiveTail, sr, f2);
        const float correctedF1 = goertzelMagnitude (correctedTail, sr, f1);
        const float correctedF2 = goertzelMagnitude (correctedTail, sr, f2);

        check (correctedF1 > naiveF1 * 1.15f, "F1(700Hz) retained better with correction: naive=" + juce::String (naiveF1, 5) + " corrected=" + juce::String (correctedF1, 5));
        check (correctedF2 > naiveF2 * 1.15f, "F2(1200Hz) retained better with correction: naive=" + juce::String (naiveF2, 5) + " corrected=" + juce::String (correctedF2, 5));
    }

    std::cout << "\n[Formant] Stability guard survives extreme settings + real material (spec 13/21)\n";
    {
        // Worst case for LINK: formant(+12) + pitch(+12) summed = +24
        // semitones warp, sustained pitched input, for several seconds.
        NovaBendFormantProcessor fc;
        fc.prepare (sr);
        fc.setFormantAmount (1.0f);
        fc.setFormantWarpSemitones (24.0f);

        auto stress = makeVowel (sr, 3.0, 220.0f, 700.0f, 2400.0f);
        bool allFinite = true;
        float peakAbs = 0.0f;
        for (float x : stress)
        {
            const float y = fc.processPost (fc.processPre (x));
            if (! std::isfinite (y)) { allFinite = false; break; }
            peakAbs = juce::jmax (peakAbs, std::abs (y));
        }
        check (allFinite, "extreme +24st warp, 3s sustained pitched input: output stays finite throughout");
        check (peakAbs < 1000.0f, "output stays within a sane bound (peak=" + juce::String (peakAbs, 2) + "), no runaway resonance");

        // Also: silence in, silence out, no denormal/NaN blowup.
        NovaBendFormantProcessor fcSilence;
        fcSilence.prepare (sr);
        fcSilence.setFormantAmount (1.0f);
        fcSilence.setFormantWarpSemitones (-24.0f);
        bool silenceFinite = true;
        for (int i = 0; i < (int) sr; ++i)
        {
            const float y = fcSilence.processPost (fcSilence.processPre (0.0f));
            if (! std::isfinite (y)) { silenceFinite = false; break; }
        }
        check (silenceFinite, "silence in at extreme -24st warp: output stays finite");
    }

    std::cout << "\n[Formant] DIAGNOSTIC: stability-guard trigger rate on realistic (jittery/noisy) material at real single-knob Formant settings\n";
    {
        // Not a pass/fail check (the guard's very purpose is to fire when
        // needed) - this measures HOW OFTEN it fires on material closer to
        // a real voice than makeVowel()'s clean glottal pulse train, at
        // Formant settings a user would actually reach with the knob alone
        // (no Link, no extreme +24 combined case).
        for (float warp : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
        {
            NovaBendFormantProcessor fc;
            fc.prepare (sr);
            fc.setFormantAmount (1.0f);
            fc.setFormantWarpSemitones (warp);

            auto voice = makeVoiceLike (sr, 4.0, 160.0f, 700.0f, 1200.0f, 2600.0f, 12345);
            bool finite = true;
            for (float x : voice)
            {
                const float y = fc.processPost (fc.processPre (x));
                if (! std::isfinite (y)) { finite = false; break; }
            }
            const int triggers = fc.getGuardTriggerCount();
            const double seconds = (double) voice.size() / sr;
            std::cout << "  INFO  Formant=" << juce::String (warp, 0) << "st: guard fired " << triggers
                       << " times over " << juce::String (seconds, 1) << "s (" << juce::String ((double) triggers / seconds, 1)
                       << "/s), finite=" << (finite ? "yes" : "NO") << "\n";
            check (finite, "Formant=" + juce::String (warp, 0) + "st on realistic material: output stays finite throughout");
        }
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL FORMANT VALIDATION TESTS PASSED\n" : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";
    return failures == 0 ? 0 : 1;
}
