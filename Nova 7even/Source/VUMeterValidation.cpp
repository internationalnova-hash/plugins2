// Headless validation of NovaVUBallistics (NOVA_7EVEN_PLAN.md section 5.7
// / 8.1 step 5).

#include "DSP/NovaVUBallistics.h"
#include <juce_core/juce_core.h>
#include <iostream>
#include <cmath>

using namespace nova::seven;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    // Drives a constant-amplitude DC step through the meter one sample at
    // a time and returns the display level at each millisecond boundary
    // requested.
    float displayLevelAtTime (NovaVUBallistics& meter, double sampleRate, float stepAmplitude, double atSeconds)
    {
        const int numSamples = (int) (atSeconds * sampleRate);
        juce::AudioBuffer<float> buffer (1, 1);
        buffer.setSample (0, 0, stepAmplitude);
        for (int i = 0; i < numSamples; ++i)
            meter.process (buffer);
        return meter.getDisplayLevel();
    }
}

int main()
{
    const double sampleRate = 48000.0;

    std::cout << "\n[VUBallistics] Step response reaches ~99% of final value at 300ms (ANSI C16.5)\n";
    {
        NovaVUBallistics meter;
        meter.prepare (sampleRate);

        const float stepAmplitude = 0.5f;
        const float level = displayLevelAtTime (meter, sampleRate, stepAmplitude, 0.3);
        const float fraction = level / stepAmplitude;

        std::cout << "    display level at 300ms: " << level << " (" << (fraction * 100.0f) << "% of step amplitude)\n";
        check (std::abs (fraction - 0.99f) < 0.099f, "reaches 99% +/-10% of step amplitude at t=300ms, got " + juce::String (fraction * 100.0f, 2) + "%");
    }

    std::cout << "\n[VUBallistics] Symmetric attack and release time constants\n";
    {
        NovaVUBallistics meter;
        meter.prepare (sampleRate);

        // Attack: step from 0 to 0.5, measure time to reach 63.2% (one
        // time constant) of the step.
        const float stepAmplitude = 0.5f;
        juce::AudioBuffer<float> buffer (1, 1);
        int attackSamples = 0;
        const float attackTarget = stepAmplitude * (1.0f - std::exp (-1.0f));
        while (meter.getDisplayLevel() < attackTarget && attackSamples < (int) sampleRate * 2)
        {
            buffer.setSample (0, 0, stepAmplitude);
            meter.process (buffer);
            ++attackSamples;
        }
        const double attackTimeMs = 1000.0 * (double) attackSamples / sampleRate;

        // Release: from the now-settled level, step back down to 0,
        // measure time to fall to 36.8% (1 - 63.2%) of the original level
        // - the release-side equivalent one-time-constant point.
        const float startLevel = meter.getDisplayLevel();
        const float releaseTarget = startLevel * std::exp (-1.0f);
        int releaseSamples = 0;
        while (meter.getDisplayLevel() > releaseTarget && releaseSamples < (int) sampleRate * 2)
        {
            buffer.setSample (0, 0, 0.0f);
            meter.process (buffer);
            ++releaseSamples;
        }
        const double releaseTimeMs = 1000.0 * (double) releaseSamples / sampleRate;

        std::cout << "    attack time constant: " << attackTimeMs << "ms, release time constant: " << releaseTimeMs << "ms\n";
        check (std::abs (attackTimeMs - releaseTimeMs) < (attackTimeMs * 0.05), "attack and release time constants match within 5%, attack=" + juce::String (attackTimeMs, 2) + "ms release=" + juce::String (releaseTimeMs, 2) + "ms");
    }

    std::cout << "\n[VUBallistics] 0VU reading corresponds to a calibrated -18dBFS input\n";
    {
        NovaVUBallistics meter;
        meter.prepare (sampleRate);

        // -18dBFS sine, RMS amplitude = 10^(-18/20) = 0.12589..., but we
        // feed a settled DC-equivalent (peak amplitude of a sine averages
        // out under this rectified-abs ballistics to (2/pi)*peak - so
        // drive with the peak amplitude that gives -18dBFS RMS and settle
        // long enough for full step response, then compare the *meter's
        // own steady-state analog of -18dBFS* using a true, actual
        // -18dBFS full-scale DC level directly, which is the cleanest,
        // least ambiguous input for calibration purposes.
        const float minus18DbfsAmplitude = juce::Decibels::decibelsToGain (-18.0f);

        juce::AudioBuffer<float> buffer (1, 1);
        for (int i = 0; i < (int) (sampleRate * 2.0); ++i) // 2s - fully settled
        {
            buffer.setSample (0, 0, minus18DbfsAmplitude);
            meter.process (buffer);
        }

        const float vuReading = meter.getDisplayLevelVu();
        std::cout << "    VU reading for a settled -18dBFS input: " << vuReading << " VU (expect ~0.0)\n";
        check (std::abs (vuReading) < 0.1f, "settled -18dBFS input reads within 0.1VU of 0VU, got " + juce::String (vuReading, 4) + "VU");
    }

    std::cout << "\n[VUBallistics] Silence in, zero out\n";
    {
        NovaVUBallistics meter;
        meter.prepare (sampleRate);

        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();
        for (int i = 0; i < 100; ++i)
            meter.process (buffer);

        check (meter.getDisplayLevel() < 1.0e-6f, "silence in produces zero display level, got " + juce::String (meter.getDisplayLevel(), 8));
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL VU BALLISTICS VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
