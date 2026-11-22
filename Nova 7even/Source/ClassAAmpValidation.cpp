// Headless validation of NovaClassAAmp (NOVA_7EVEN_PLAN.md section 5.3/5.4
// / 8.1 step 2).

#include "DSP/NovaClassAAmp.h"
#include <juce_core/juce_core.h>
#include <iostream>
#include <cmath>
#include <vector>

using namespace nova::seven;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    // Not rounded to the nearest integer bin - see EQValidation.cpp for
    // why (single-frequency coherent probe; rounding causes spectral
    // leakage for frequencies not aligned to this window's bin spacing).
    float goertzelMagnitude (const float* samples, int N, double sampleRate, double targetFreqHz)
    {
        const double k = (targetFreqHz * (double) N) / sampleRate;
        const double omega = (2.0 * juce::MathConstants<double>::pi * k) / (double) N;
        const double coeff = 2.0 * std::cos (omega);

        double s0 = 0.0, s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < N; ++i)
        {
            s0 = (double) samples[i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        const double realPart = s1 - s2 * std::cos (omega);
        const double imagPart = s2 * std::sin (omega);
        return (float) (2.0 * std::sqrt (realPart * realPart + imagPart * imagPart) / (double) N);
    }

    std::vector<float> renderSine (NovaCharacterMode mode, float preampGain, double sampleRate, double freqHz, float amplitude, int numSamples)
    {
        NovaClassAAmp amp;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) numSamples, 1 };
        amp.prepare (spec);
        amp.setMode (mode);
        amp.setPreampGain (preampGain);

        juce::AudioBuffer<float> buffer (1, numSamples);
        auto* w = buffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            w[i] = amplitude * std::sin (2.0 * juce::MathConstants<double>::pi * freqHz * (double) i / sampleRate);

        amp.process (buffer);
        return std::vector<float> (buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples);
    }

    float measureThd (const std::vector<float>& out, double sampleRate, double fundamentalHz)
    {
        const float fundamentalMag = goertzelMagnitude (out.data(), (int) out.size(), sampleRate, fundamentalHz);
        if (fundamentalMag < 1.0e-9f) return 0.0f;

        double harmonicEnergy = 0.0;
        for (int h = 2; h <= 6; ++h)
        {
            const float mag = goertzelMagnitude (out.data(), (int) out.size(), sampleRate, fundamentalHz * h);
            harmonicEnergy += (double) mag * (double) mag;
        }
        return (float) (std::sqrt (harmonicEnergy) / (double) fundamentalMag);
    }

    float measureSecondHarmonicRatio (const std::vector<float>& out, double sampleRate, double fundamentalHz)
    {
        const float fundamentalMag = goertzelMagnitude (out.data(), (int) out.size(), sampleRate, fundamentalHz);
        if (fundamentalMag < 1.0e-9f) return 0.0f;
        const float secondMag = goertzelMagnitude (out.data(), (int) out.size(), sampleRate, fundamentalHz * 2.0);
        return secondMag / fundamentalMag;
    }
}

int main()
{
    const double sampleRate = 48000.0;
    const int numSamples = 48000;
    const float matchedAmplitude = 0.5f;
    const float matchedPreampGain = 5.0f;

    std::cout << "\n[ClassAAmp] Clean/Classic/Hot produce measurably distinct THD at matched input\n";
    {
        auto outClean = renderSine (NovaCharacterMode::Clean, matchedPreampGain, sampleRate, 300.0, matchedAmplitude, numSamples);
        auto outClassic = renderSine (NovaCharacterMode::Classic, matchedPreampGain, sampleRate, 300.0, matchedAmplitude, numSamples);
        auto outHot = renderSine (NovaCharacterMode::Hot, matchedPreampGain, sampleRate, 300.0, matchedAmplitude, numSamples);

        const float thdClean = measureThd (outClean, sampleRate, 300.0);
        const float thdClassic = measureThd (outClassic, sampleRate, 300.0);
        const float thdHot = measureThd (outHot, sampleRate, 300.0);

        std::cout << "    THD - Clean: " << (thdClean * 100.0f) << "%, Classic: " << (thdClassic * 100.0f)
                   << "%, Hot: " << (thdHot * 100.0f) << "%\n";

        check (thdClassic > thdClean, "Classic THD exceeds Clean THD at matched input");
        check (thdHot > thdClassic, "Hot THD exceeds Classic THD at matched input");
    }

    std::cout << "\n[ClassAAmp] Clean/Classic/Hot produce measurably distinct 2nd-harmonic (asymmetry) content\n";
    {
        auto outClean = renderSine (NovaCharacterMode::Clean, matchedPreampGain, sampleRate, 300.0, matchedAmplitude, numSamples);
        auto outHot = renderSine (NovaCharacterMode::Hot, matchedPreampGain, sampleRate, 300.0, matchedAmplitude, numSamples);

        const float h2Clean = measureSecondHarmonicRatio (outClean, sampleRate, 300.0);
        const float h2Hot = measureSecondHarmonicRatio (outHot, sampleRate, 300.0);

        std::cout << "    2nd-harmonic ratio - Clean: " << h2Clean << ", Hot: " << h2Hot << "\n";
        check (h2Hot > h2Clean, "Hot mode's higher asymmetry produces measurably more 2nd-harmonic content than Clean");
    }

    std::cout << "\n[ClassAAmp] Sag envelope engages progressively with sustained level, not instantly on transients\n";
    {
        // First attempt (300Hz fundamental, preampGain=8, amplitude=0.7)
        // measured early=39.36% vs late=37.64% THD - no clear ordering.
        // Root cause, confirmed by hand-computing the tanh argument: at
        // that drive the signal is already pinned deep in tanh's
        // saturated region even at k=baseCurve alone (argument >> 1), so
        // the extra ~0.65*sagAmount added to k by a fully-built envelope
        // makes no measurable difference - both windows are equally
        // clipped. Fixed by choosing a drive level that sits in the
        // *transitional* (not fully pinned) region, where k actually
        // matters: preampGain=3/amplitude=0.25 puts the argument around
        // 1.3-1.4, right where tanh is still curving. Also switched the
        // probe tone to 2kHz (from 300Hz) purely so a short "early" window
        // still contains enough full cycles (10 at 5ms) for the Goertzel
        // measurement to be accurate - this is a measurement-resolution
        // fix, not a claim about tonal behavior at 2kHz specifically.
        NovaClassAAmp amp;
        const int totalSamples = (int) (sampleRate * 1.0); // 1 second
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) totalSamples, 1 };
        amp.prepare (spec);
        amp.setMode (NovaCharacterMode::Hot);
        amp.setPreampGain (3.0f);

        juce::AudioBuffer<float> buffer (1, totalSamples);
        auto* w = buffer.getWritePointer (0);
        for (int i = 0; i < totalSamples; ++i)
            w[i] = 0.25f * std::sin (2.0 * juce::MathConstants<double>::pi * 2000.0 * (double) i / sampleRate);

        amp.process (buffer);

        // Early window: first 5ms (10 cycles at 2kHz - enough for an
        // accurate Goertzel reading) - well under the 18ms sag time
        // constant, envelope has barely risen from 0.
        const int earlyWindowSamples = (int) (sampleRate * 0.005);
        const float thdEarly = measureThd (
            std::vector<float> (buffer.getReadPointer (0), buffer.getReadPointer (0) + earlyWindowSamples),
            sampleRate, 2000.0);

        // Late window: a 40ms window starting at 300ms in (>15 time
        // constants - envelope fully settled).
        const int lateWindowStart = (int) (sampleRate * 0.3);
        const int lateWindowSamples = (int) (sampleRate * 0.04);
        const float thdLate = measureThd (
            std::vector<float> (buffer.getReadPointer (0) + lateWindowStart, buffer.getReadPointer (0) + lateWindowStart + lateWindowSamples),
            sampleRate, 2000.0);

        std::cout << "    THD in first 5ms: " << (thdEarly * 100.0f) << "%, THD at 300ms (settled): " << (thdLate * 100.0f) << "%\n";
        check (thdLate > thdEarly, "settled-sag THD exceeds just-started THD, confirming sag builds over time rather than reacting instantly");
    }

    std::cout << "\n[ClassAAmp] Bounded, finite output at max drive (Hot, preampGain=10, extreme amplitude)\n";
    {
        auto out = renderSine (NovaCharacterMode::Hot, 10.0f, sampleRate, 300.0, 3.0f, numSamples);
        bool finite = true, bounded = true;
        for (float v : out)
        {
            if (! std::isfinite (v)) finite = false;
            if (std::abs (v) > 2.0f) bounded = false;
        }
        check (finite, "max-drive extreme-amplitude input produces no NaN/Inf");
        check (bounded, "max-drive extreme-amplitude input stays bounded (|y| <= 2.0)");
    }

    std::cout << "\n[ClassAAmp] Silence in, silence out\n";
    {
        NovaClassAAmp amp;
        juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };
        amp.prepare (spec);

        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();
        amp.process (buffer);

        float maxAbs = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                maxAbs = juce::jmax (maxAbs, std::abs (buffer.getSample (ch, i)));

        check (maxAbs < 1.0e-6f, "silence in produces silence out, max abs = " + juce::String (maxAbs, 8));
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL CLASS-A AMP VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
