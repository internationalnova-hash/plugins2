// Headless validation for NovaTubeStage / NovaTransformerStage: bypass
// transparency at 0% drive, DC-blocking after the nonlinear waveshaper,
// and monotonic behavior of the GR-driven drive coupling used by the
// engine (heavier gain reduction -> more tube drive -> more harmonic
// content, not a static independent saturation amount).

#include "DSP/NovaTubeStage.h"
#include "DSP/NovaTransformerStage.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& label)
    {
        if (condition)
            std::printf ("PASS: %s\n", label.toRawUTF8());
        else
        {
            std::printf ("FAIL: %s\n", label.toRawUTF8());
            ++failures;
        }
    }

    using namespace nova::royal670;

    juce::dsp::ProcessSpec makeSpec (double sr, int blockSize)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sr;
        spec.maximumBlockSize = (juce::uint32) blockSize;
        spec.numChannels = 2;
        return spec;
    }

    void fillTestSignal (juce::AudioBuffer<float>& buf, double sr, int startSampleIndex)
    {
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const float x = 0.6f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * (startSampleIndex + i) / sr);
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
        }
    }

    float measureMaxAbsDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        float maxDiff = 0.0f;
        for (int ch = 0; ch < a.getNumChannels(); ++ch)
            for (int i = 0; i < a.getNumSamples(); ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
        return maxDiff;
    }
}

int main()
{
    constexpr double sr = 48000.0;
    constexpr int blockSize = 512;

    // ---- NovaTubeStage: bypass-transparent at 0 drive ----
    {
        NovaTubeStage stage;
        stage.prepare (makeSpec (sr, blockSize));
        stage.setDrive (0.0f);

        juce::AudioBuffer<float> in (2, blockSize), out (2, blockSize);
        fillTestSignal (in, sr, 0);
        out.makeCopyOf (in);
        stage.process (out);

        check (measureMaxAbsDiff (in, out) < 1.0e-6f, "NovaTubeStage: bit-exact bypass at 0% drive");
    }

    // ---- NovaTubeStage: DC-blocked after an asymmetric-clipping drive ----
    {
        NovaTubeStage stage;
        stage.prepare (makeSpec (sr, blockSize));
        stage.setDrive (1.0f);
        stage.setEvenHarmonicBias (1.0f); // maximum asymmetry - the case most likely to leak DC

        // Measure DC over one long combined tail (many full 300Hz periods)
        // rather than per-512-sample-block averages - 512 samples is not
        // an integer number of periods at 300Hz/48kHz, so a per-block
        // average carries a spurious residual from the partial trailing
        // cycle even for a properly DC-free signal.
        juce::AudioBuffer<float> buf (2, blockSize);
        std::vector<float> tail;
        for (int block = 0; block < 50; ++block) // let the DC blocker settle
        {
            fillTestSignal (buf, sr, block * blockSize);
            stage.process (buf);
            if (block > 20)
                for (int i = 0; i < blockSize; ++i)
                    tail.push_back (buf.getSample (0, i));
        }
        double sum = 0.0;
        for (float v : tail) sum += v;
        const float settledDc = (float) (sum / (double) (tail.empty() ? 1 : tail.size()));
        check (std::abs (settledDc) < 0.01f, "NovaTubeStage: settled DC offset stays near zero even at max asymmetry");
    }

    // ---- NovaTransformerStage: bypass-transparent at 0 drive ----
    {
        NovaTransformerStage stage;
        stage.prepare (makeSpec (sr, blockSize));
        stage.setDriveLevel (0.0f);

        juce::AudioBuffer<float> in (2, blockSize), out (2, blockSize);
        fillTestSignal (in, sr, 0);
        out.makeCopyOf (in);
        stage.process (out);

        check (measureMaxAbsDiff (in, out) < 1.0e-6f, "NovaTransformerStage: bit-exact bypass at 0% drive");
    }

    // ---- NovaTransformerStage: DC-blocked at high LF drive ----
    {
        NovaTransformerStage stage;
        stage.prepare (makeSpec (sr, blockSize));
        stage.setDriveLevel (1.0f);
        stage.setLfDrive (1.0f);

        juce::AudioBuffer<float> buf (2, blockSize);
        std::vector<float> tail;
        for (int block = 0; block < 50; ++block)
        {
            fillTestSignal (buf, sr, block * blockSize);
            stage.process (buf);
            if (block > 20)
                for (int i = 0; i < blockSize; ++i)
                    tail.push_back (buf.getSample (0, i));
        }
        double sum = 0.0;
        for (float v : tail) sum += v;
        const float settledDc = (float) (sum / (double) (tail.empty() ? 1 : tail.size()));
        check (std::abs (settledDc) < 0.01f, "NovaTransformerStage: settled DC offset stays near zero at max LF drive");
    }

    // ---- GR-coupling monotonicity: the exact formula the engine uses to
    //      derive output-tube drive from gain reduction should produce a
    //      measurably louder harmonic content (RMS of the difference from
    //      a dry pass) as the simulated GR value increases. ----
    {
        auto measureHarmonicEnergy = [&] (float simulatedGrDb) -> float
        {
            NovaTubeStage stage;
            stage.prepare (makeSpec (sr, blockSize));
            const float tubeGrCoupling = 0.8f; // NovaWeightMacro::fromWeight(1.0) upper bound
            const float grCoupledDrive = 0.35f * (1.0f + tubeGrCoupling * juce::jlimit (0.0f, 1.0f, simulatedGrDb / 12.0f));
            stage.setDrive (grCoupledDrive);

            juce::AudioBuffer<float> dry (2, blockSize), wet (2, blockSize);
            fillTestSignal (dry, sr, 0);
            wet.makeCopyOf (dry);
            stage.process (wet);

            double sumSq = 0.0;
            for (int i = 0; i < blockSize; ++i)
            {
                const float diff = wet.getSample (0, i) - dry.getSample (0, i);
                sumSq += diff * diff;
            }
            return (float) std::sqrt (sumSq / blockSize);
        };

        const float energyAt0Gr = measureHarmonicEnergy (0.0f);
        const float energyAt6Gr = measureHarmonicEnergy (6.0f);
        const float energyAt12Gr = measureHarmonicEnergy (12.0f);

        check (energyAt6Gr > energyAt0Gr && energyAt12Gr > energyAt6Gr,
               "GR-coupling: harmonic content increases monotonically as gain reduction deepens");
        std::printf ("Harmonic diff-RMS at GR=0/6/12dB: %f %f %f\n", energyAt0Gr, energyAt6Gr, energyAt12Gr);
    }

    std::printf ("\n%d check(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
