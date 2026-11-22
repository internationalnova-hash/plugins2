// Headless validation of NovaPitchSynchronousShifter: unity-ratio
// transparency, known-shift cents accuracy, and the key regression test -
// a simulated-vibrato instantaneous input pitch produces a measurably
// SMALLER output cents deviation than the input's own modulation, proving
// the old "locked reference passes vibrato straight through" bug class
// can't recur in this pitch-synchronous-OLA design (ratio is recomputed
// fresh every call, with no frozen reference anywhere).

#include "DSP/NovaPitchSynchronousShifter.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <iostream>
#include <vector>

using nova::pitch::NovaPitchSynchronousShifter;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    // Sub-sample-precision frequency estimate via interpolated rising
    // zero-crossings (same technique proven in Nova Doubler's
    // ShiftLineValidation.cpp).
    float measureFrequencyByZeroCrossing (const std::vector<float>& x, double sampleRate)
    {
        std::vector<double> crossingTimes;
        for (size_t i = 1; i < x.size(); ++i)
        {
            if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            {
                const double frac = (double) (-x[i - 1]) / (double) (x[i] - x[i - 1]);
                crossingTimes.push_back (((double) (i - 1) + frac) / sampleRate);
            }
        }
        if (crossingTimes.size() < 2) return 0.0f;
        const double totalSpan = crossingTimes.back() - crossingTimes.front();
        const int numPeriods = (int) crossingTimes.size() - 1;
        return (float) ((double) numPeriods / totalSpan);
    }
}

int main()
{
    const double sr = 44100.0;

    std::cout << "\n[Shifter] Unity-ratio transparency (input pitch == output pitch)\n";
    {
        NovaPitchSynchronousShifter shifter;
        shifter.prepare (sr);

        const float testFreq = 220.0f;
        const int n = (int) (sr * 1.0);
        std::vector<float> output ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const float in = std::sin (2.0 * juce::MathConstants<double>::pi * testFreq * (double) i / sr);
            output[(size_t) i] = shifter.processSample (in, testFreq, testFreq);
        }

        const int skip = shifter.getLatencySamples() + 2000; // past the fixed algorithmic delay and settling
        std::vector<float> tail (output.begin() + skip, output.end());
        const float measured = measureFrequencyByZeroCrossing (tail, sr);
        const float cents = 1200.0f * std::log2 (measured / testFreq);
        check (std::abs (cents) < 8.0f,
               "unity ratio: expected=" + juce::String (testFreq, 2) + "Hz, measured=" + juce::String (measured, 2)
                   + "Hz (" + juce::String (cents, 2) + "c)");
    }

    std::cout << "\n[Shifter] Known-shift cents accuracy\n";
    {
        for (float shiftSemitones : { -7.0f, -2.0f, 2.0f, 7.0f })
        {
            NovaPitchSynchronousShifter shifter;
            shifter.prepare (sr);

            const float inFreq = 220.0f;
            const float outFreq = inFreq * std::pow (2.0f, shiftSemitones / 12.0f);

            const int n = (int) (sr * 1.0);
            std::vector<float> output ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const float in = std::sin (2.0 * juce::MathConstants<double>::pi * inFreq * (double) i / sr);
                output[(size_t) i] = shifter.processSample (in, inFreq, outFreq);
            }

            const int skip = shifter.getLatencySamples() + 2000;
            std::vector<float> tail (output.begin() + skip, output.end());
            const float measured = measureFrequencyByZeroCrossing (tail, sr);
            const float cents = 1200.0f * std::log2 (measured / outFreq);
            check (std::abs (cents) < 15.0f,
                   juce::String (shiftSemitones, 0) + " semitones: expected=" + juce::String (outFreq, 2)
                       + "Hz, measured=" + juce::String (measured, 2) + "Hz (" + juce::String (cents, 2) + "c)");
        }
    }

    std::cout << "\n[Shifter] Vibrato regression: output tracks a modulated input pitch with reduced deviation\n";
    {
        // A held target, fed a rapidly-wobbling instantaneous INPUT pitch
        // (5Hz LFO, +-40 cents) - the old dual-head-delay-line shifter's
        // documented bug was that a constant/locked ratio structurally
        // cannot suppress this (multiplying every instantaneous frequency
        // by the same constant factor preserves the relative/cents
        // variation exactly). This shifter recomputes its ratio fresh
        // every single call from whatever input/output pitch it's given,
        // so feeding it the actual wobbling estimate each call should
        // measurably suppress the wobble, not pass it straight through.
        NovaPitchSynchronousShifter shifter;
        shifter.prepare (sr);

        const float centerFreq = 220.0f;
        const float targetFreq = 220.0f; // held constant - shifter should pull the wobble toward this
        const float lfoHz = 5.0f;
        const float lfoDepthCents = 40.0f;

        const int n = (int) (sr * 2.0);
        std::vector<float> output ((size_t) n);
        double phase = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float cents = lfoDepthCents * std::sin (2.0 * juce::MathConstants<double>::pi * lfoHz * (double) i / sr);
            const float instantHz = centerFreq * std::pow (2.0f, cents / 1200.0f);

            const float in = (float) std::sin (phase);
            phase += 2.0 * juce::MathConstants<double>::pi * (double) instantHz / sr;
            if (phase > 2.0 * juce::MathConstants<double>::pi) phase -= 2.0 * juce::MathConstants<double>::pi;

            output[(size_t) i] = shifter.processSample (in, instantHz, targetFreq);
        }

        // Sliding-window frequency remeasurement over the output, past the
        // fixed algorithmic delay and initial settling.
        const int skip = shifter.getLatencySamples() + 2000;
        const int windowSamples = (int) (sr / lfoHz); // one LFO cycle per window
        std::vector<float> outputCents;
        for (int start = skip; start + windowSamples < n; start += windowSamples / 4)
        {
            std::vector<float> window (output.begin() + start, output.begin() + start + windowSamples);
            const float freq = measureFrequencyByZeroCrossing (window, sr);
            if (freq > 1.0f)
                outputCents.push_back (1200.0f * std::log2 (freq / targetFreq));
        }

        check (! outputCents.empty(), "collected sliding-window frequency measurements");

        float maxAbsOutputCents = 0.0f;
        for (float c : outputCents) maxAbsOutputCents = juce::jmax (maxAbsOutputCents, std::abs (c));

        check (maxAbsOutputCents < lfoDepthCents * 0.85f,
               "output cents deviation (" + juce::String (maxAbsOutputCents, 2)
                   + "c) is measurably smaller than the input's own +-" + juce::String (lfoDepthCents, 0) + "c modulation");
    }

    std::cout << "\n[Shifter] Large one-directional glide: held target should stay locked, not track a real note-to-note portamento\n";
    {
        // Real-world regression case: a singer sliding from A3 (220Hz) up
        // toward B3 (247Hz) over ~200ms (measured directly on real vocal
        // material via the plugin's own telemetry - the quantizer commits
        // to a flat 220Hz target for nearly the whole glide, exactly as
        // hard-tune should, and the shifter's ratio math looks correct on
        // paper - but independent re-measurement of the actual rendered
        // output audio showed the perceived pitch drifting upward along
        // with the glide instead of staying pinned at the target). The
        // existing vibrato test above only covers a small (+-40c),
        // OSCILLATING input wobble; this covers a much larger (~200c),
        // ONE-DIRECTIONAL glide, which is a meaningfully different case
        // for an OLA-based shifter (repeated re-capture of a genuinely
        // drifting grain, vs. oscillation around one center).
        NovaPitchSynchronousShifter shifter;
        shifter.prepare (sr);

        const float startFreq = 220.0f;
        const float endFreq = 246.94f; // A3 -> B3, ~207 cents
        const float targetFreq = 220.0f; // held flat throughout, like the real quantizer output
        const double glideSeconds = 0.2;

        const int totalSeconds = 1;
        const int n = (int) (sr * totalSeconds);
        const int glideSamples = (int) (sr * glideSeconds);
        const int glideStart = (int) (sr * 0.3); // let it settle at startFreq first

        std::vector<float> output ((size_t) n);
        double phase = 0.0;
        for (int i = 0; i < n; ++i)
        {
            float instantHz = startFreq;
            if (i >= glideStart && i < glideStart + glideSamples)
            {
                const float frac = (float) (i - glideStart) / (float) glideSamples;
                instantHz = startFreq + frac * (endFreq - startFreq);
            }
            else if (i >= glideStart + glideSamples)
            {
                instantHz = endFreq;
            }

            const float in = (float) std::sin (phase);
            phase += 2.0 * juce::MathConstants<double>::pi * (double) instantHz / sr;
            if (phase > 2.0 * juce::MathConstants<double>::pi) phase -= 2.0 * juce::MathConstants<double>::pi;

            output[(size_t) i] = shifter.processSample (in, instantHz, targetFreq);
        }

        // Measure the output's actual frequency in a short window centered
        // partway through the glide (where a leaky shifter would show the
        // most deviation from the flat target) and again just after the
        // glide ends (where it should have fully caught back up).
        const int latency = shifter.getLatencySamples();
        auto measureAt = [&] (int centerSample, int windowSamples) -> float
        {
            const int start = juce::jlimit (0, n - windowSamples, centerSample + latency - windowSamples / 2);
            std::vector<float> window (output.begin() + start, output.begin() + start + windowSamples);
            return measureFrequencyByZeroCrossing (window, sr);
        };

        const int windowSamples = (int) (sr * 0.02); // 20ms
        const float midGlideFreq = measureAt (glideStart + glideSamples / 2, windowSamples);
        const float afterGlideFreq = measureAt (glideStart + glideSamples + (int) (sr * 0.05), windowSamples);

        const float midGlideCents = 1200.0f * std::log2 (midGlideFreq / targetFreq);
        const float afterGlideCents = 1200.0f * std::log2 (afterGlideFreq / targetFreq);

        std::cout << "  (info)  input glide reaches " + juce::String (1200.0f * std::log2 (endFreq / startFreq), 0)
                         + "c above start; target held flat at " + juce::String (targetFreq, 1) + "Hz throughout\n";
        check (std::abs (midGlideCents) < 40.0f,
               "mid-glide: output=" + juce::String (midGlideFreq, 2) + "Hz (" + juce::String (midGlideCents, 2)
                   + "c from flat target) - should stay close to target, not follow the glide");
        check (std::abs (afterGlideCents) < 15.0f,
               "after glide settles: output=" + juce::String (afterGlideFreq, 2) + "Hz (" + juce::String (afterGlideCents, 2)
                   + "c from flat target)");
    }

    std::cout << "\n[Shifter] Stepped (hop-quantized) glide: real usage updates inputPitchHz once per ~512-sample hop, not per-sample\n";
    {
        // The previous glide test fed a smoothly per-sample-interpolated
        // inputPitchHz - kind to the phase accumulator, but NOT how the
        // real engine actually calls this class: NovaPitchEngine reads
        // detector.getCurrentHz() once per sample, but that value itself
        // only actually CHANGES once per detector hop (~512 samples,
        // ~10.7ms at 48kHz) - so the real inputPitchHz argument is a
        // STAIRCASE of sudden jumps, held constant within each hop, not a
        // continuous glide. A sudden per-hop jump in the phase
        // accumulator's own rate is a meaningfully different stress case
        // than smooth interpolation and wasn't covered by the test above.
        NovaPitchSynchronousShifter shifter;
        shifter.prepare (sr);

        const float startFreq = 220.0f;
        const float endFreq = 246.94f;
        const float targetFreq = 220.0f;
        const int hopSamples = 512;
        const double glideSeconds = 0.2;

        const int totalSeconds = 1;
        const int n = (int) (sr * totalSeconds);
        const int glideSamples = (int) (sr * glideSeconds);
        const int glideStart = (int) (sr * 0.3);
        const int numHopsInGlide = juce::jmax (1, glideSamples / hopSamples);

        std::vector<float> output ((size_t) n);
        double phase = 0.0;
        for (int i = 0; i < n; ++i)
        {
            float instantHz = startFreq;
            if (i >= glideStart && i < glideStart + glideSamples)
            {
                const int hopIndex = (i - glideStart) / hopSamples;
                const float frac = (float) hopIndex / (float) numHopsInGlide;
                instantHz = startFreq + frac * (endFreq - startFreq); // held constant for the whole hop
            }
            else if (i >= glideStart + glideSamples)
            {
                instantHz = endFreq;
            }

            const float in = (float) std::sin (phase);
            phase += 2.0 * juce::MathConstants<double>::pi * (double) instantHz / sr;
            if (phase > 2.0 * juce::MathConstants<double>::pi) phase -= 2.0 * juce::MathConstants<double>::pi;

            output[(size_t) i] = shifter.processSample (in, instantHz, targetFreq);
        }

        const int latency = shifter.getLatencySamples();
        auto measureAt = [&] (int centerSample, int windowSamples) -> float
        {
            const int start = juce::jlimit (0, n - windowSamples, centerSample + latency - windowSamples / 2);
            std::vector<float> window (output.begin() + start, output.begin() + start + windowSamples);
            return measureFrequencyByZeroCrossing (window, sr);
        };

        const int windowSamples = (int) (sr * 0.02);
        const float midGlideFreq = measureAt (glideStart + glideSamples / 2, windowSamples);
        const float afterGlideFreq = measureAt (glideStart + glideSamples + (int) (sr * 0.05), windowSamples);

        const float midGlideCents = 1200.0f * std::log2 (midGlideFreq / targetFreq);
        const float afterGlideCents = 1200.0f * std::log2 (afterGlideFreq / targetFreq);

        std::cout << "  (info)  " + juce::String (numHopsInGlide) + " discrete hop-steps across the glide, "
                         + juce::String (1200.0f * std::log2 (endFreq / startFreq) / (float) numHopsInGlide, 0) + "c per step\n";
        check (std::abs (midGlideCents) < 40.0f,
               "mid-glide (stepped input): output=" + juce::String (midGlideFreq, 2) + "Hz (" + juce::String (midGlideCents, 2)
                   + "c from flat target)");
        check (std::abs (afterGlideCents) < 15.0f,
               "after glide settles (stepped input): output=" + juce::String (afterGlideFreq, 2) + "Hz (" + juce::String (afterGlideCents, 2)
                   + "c from flat target)");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL SHIFTER VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
