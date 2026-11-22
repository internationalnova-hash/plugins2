// Headless validation of NovaScaleQuantizer: nearestScaleMidi() regression-
// matches the retired quantizeToScale() logic, Amount=0 is a steady-state
// passthrough, RetuneSpeed reshapes glide time monotonically, and octave-
// invariance holds.

#include "Correction/NovaScaleQuantizer.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <iostream>
#include <random>

using nova::pitch::NovaScaleQuantizer;
using nova::pitch::Scale;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    float hzToMidiF (float hz) { return 69.0f + 12.0f * std::log2f (hz / 440.0f); }

    // Reference oracle: the retired NovaPitchAudioProcessor::quantizeToScale()
    // logic, reproduced here verbatim (not called from the new code) so the
    // new nearestScaleMidi() can be checked against it directly.
    int oracleQuantizeToScale (float hz, int key, int scale)
    {
        const float midi  = hzToMidiF (hz);
        const int   midiR = (int) std::round (midi);
        const int   pc    = ((midiR - key) % 12 + 12) % 12;

        auto nearest = [&] (const int* degrees, int count) -> int
        {
            int best = degrees[0], bestDist = 99;
            for (int i = 0; i < count; ++i)
            {
                int d  = ((pc - degrees[i]) % 12 + 12) % 12;
                int d2 = 12 - d;
                int dist = std::min (d, d2);
                if (dist < bestDist) { bestDist = dist; best = degrees[i]; }
            }
            int tonicOfOctave = midiR - pc;
            int candidate = tonicOfOctave + best;
            if (candidate < midiR - 6) candidate += 12;
            if (candidate > midiR + 6) candidate -= 12;
            return candidate;
        };

        switch (scale)
        {
            case 0: return midiR; // Chromatic
            case 1: { int deg[] = {0,2,4,5,7,9,11}; return nearest (deg, 7); } // Major
            case 2: { int deg[] = {0,2,3,5,7,8,10}; return nearest (deg, 7); } // Minor
            case 3: { int deg[] = {0,2,4,7,9};       return nearest (deg, 5); } // Pentatonic
            case 4: { int deg[] = {0,3,5,6,7,10};    return nearest (deg, 6); } // Blues
            default: return midiR;
        }
    }
}

int main()
{
    std::cout << "\n[ScaleQuantizer] nearestScaleMidi regression-matches the retired quantizeToScale()\n";
    {
        std::mt19937 rng (777);
        std::uniform_real_distribution<float> hzDist (60.0f, 1000.0f);
        std::uniform_int_distribution<int> keyDist (0, 11);

        int mismatches = 0;
        const int trials = 2000;
        for (int scale = 0; scale <= 4; ++scale)
        {
            for (int t = 0; t < trials; ++t)
            {
                const float hz = hzDist (rng);
                const int key = keyDist (rng);
                const int expected = oracleQuantizeToScale (hz, key, scale);
                const int actual = NovaScaleQuantizer::nearestScaleMidi (key, (Scale) scale, hz);
                if (expected != actual) ++mismatches;
            }
        }
        check (mismatches == 0, juce::String (trials * 5) + " random (hz,key,scale) trials, mismatches=" + juce::String (mismatches));
    }

    std::cout << "\n[ScaleQuantizer] Octave-invariance: doubling hz shifts the nearest scale note by exactly one octave\n";
    {
        int failuresLocal = 0;
        for (int scale = 0; scale <= 4; ++scale)
        {
            for (float hz : { 110.0f, 165.0f, 247.0f, 330.0f })
            {
                const int m1 = NovaScaleQuantizer::nearestScaleMidi (0, (Scale) scale, hz);
                const int m2 = NovaScaleQuantizer::nearestScaleMidi (0, (Scale) scale, hz * 2.0f);
                if (m2 - m1 != 12) ++failuresLocal;
            }
        }
        check (failuresLocal == 0, "octave-doubled hz always lands exactly 12 semitones above, failures=" + juce::String (failuresLocal));
    }

    std::cout << "\n[ScaleQuantizer] Humanize never lets a clearly off-key note stay uncorrected, at any setting\n";
    {
        // Flex-Tune-style behavior: Humanize=0 means "no humanization -
        // always fully correct", and even Humanize=100 (max) must still
        // fully correct input that's clearly a different pitch, not just
        // close natural variation. This replaces the old "Amount=0 is a
        // passthrough" contract, which was the actual bug being fixed here
        // - turning that control down used to let genuinely wrong notes
        // through uncorrected.
        const double sr = 44100.0;
        const float inputHz = 240.0f; // not on the C-major scale, and closer to B3 than to A3 (avoids the exact-tie case at 233Hz)
        for (float humanize : { 0.0f, 1.0f })
        {
            NovaScaleQuantizer q;
            q.prepare (sr);
            q.setKeyAndScale (0, Scale::Major);
            q.setAmount (humanize);
            q.setRetuneSpeed (0.0f); // fastest settle, so steady state arrives quickly
            q.setVibratoPreserve (0.0f);

            float out = 0.0f;
            for (int i = 0; i < (int) (sr * 0.2); ++i) // 200ms - many time constants at the fastest glide
                out = q.process (inputHz);

            const float cents = 1200.0f * std::log2f (out / 246.94f); // nearest C-major degree to 240Hz is B3 (246.94Hz)
            check (std::abs (cents) < 5.0f, "Humanize=" + juce::String (humanize, 1) + ": clearly off-key input still gets fully corrected, got "
                                                 + juce::String (out, 2) + "Hz (" + juce::String (cents, 2) + "c from B3)");
        }
    }

    std::cout << "\n[ScaleQuantizer] Humanize=100 preserves near-target closeness that Humanize=0 snaps away\n";
    {
        const double sr = 44100.0;
        // Just 15 cents flat of A4 (440Hz) - well inside the "close"
        // window, the kind of natural micro-pitch a Humanize control
        // should be able to preserve.
        const float inputHz = 440.0f * std::pow (2.0f, -15.0f / 1200.0f);

        auto steadyState = [&] (float humanize) -> float
        {
            NovaScaleQuantizer q;
            q.prepare (sr);
            q.setKeyAndScale (0, Scale::Major); // A is in C major
            q.setAmount (humanize);
            q.setRetuneSpeed (0.0f);
            q.setVibratoPreserve (0.0f);
            float out = 0.0f;
            for (int i = 0; i < (int) (sr * 0.2); ++i)
                out = q.process (inputHz);
            return out;
        };

        const float outNoHumanize = steadyState (0.0f);
        const float outMaxHumanize = steadyState (1.0f);
        // Measure distance from the TARGET note (440Hz) - fully corrected
        // means this is near zero; preserving natural closeness means this
        // stays measurably nonzero (closer to the original 15c offset).
        const float centsFromTargetNoHumanize = 1200.0f * std::log2f (outNoHumanize / 440.0f);
        const float centsFromTargetMaxHumanize = 1200.0f * std::log2f (outMaxHumanize / 440.0f);

        check (std::abs (centsFromTargetNoHumanize) < 1.0f, "Humanize=0: near-target input still snaps fully to the note, got "
                                                                  + juce::String (centsFromTargetNoHumanize, 2) + "c from target");
        check (std::abs (centsFromTargetMaxHumanize) > std::abs (centsFromTargetNoHumanize) + 5.0f,
               "Humanize=100: near-target input keeps measurably more of its natural offset (" + juce::String (centsFromTargetMaxHumanize, 2)
                   + "c from target) than Humanize=0 (" + juce::String (centsFromTargetNoHumanize, 2) + "c)");
    }

    std::cout << "\n[ScaleQuantizer] RetuneSpeed reshapes glide time monotonically\n";
    {
        const double sr = 44100.0;
        auto measureConvergenceSamples = [&] (float speed0to1) -> int
        {
            NovaScaleQuantizer q;
            q.prepare (sr);
            q.setKeyAndScale (0, Scale::Chromatic); // input hz itself is exactly on-scale, so target is fixed
            q.setAmount (1.0f);
            q.setRetuneSpeed (speed0to1);
            q.setVibratoPreserve (0.0f);

            const float startHz = 220.0f; // A3
            const float targetHz = 246.94f; // B3, a full tone away - clear, sizeable glide distance
            // Prime the quantizer's internal state near startHz first.
            for (int i = 0; i < 200; ++i) q.process (startHz);

            const int maxSamples = (int) (sr * 10.0); // covers ~6 time constants even at the slowest setting (tau up to ~1.2s)
            for (int i = 0; i < maxSamples; ++i)
            {
                const float out = q.process (targetHz);
                const float cents = 1200.0f * std::log2f (out / targetHz);
                if (std::abs (cents) < 5.0f) // "arrived" threshold
                    return i;
            }
            return maxSamples;
        };

        const int fastSamples = measureConvergenceSamples (0.0f);
        const int midSamples  = measureConvergenceSamples (0.5f);
        const int slowSamples = measureConvergenceSamples (1.0f);

        check (fastSamples < midSamples && midSamples < slowSamples,
               "convergence time increases monotonically with RetuneSpeed: fast=" + juce::String (fastSamples)
                   + " mid=" + juce::String (midSamples) + " slow=" + juce::String (slowSamples) + " samples");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL SCALE QUANTIZER VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
