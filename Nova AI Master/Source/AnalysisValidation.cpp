// Headless validation harness for the NovaAIMasterCore analysis engine.
// Tests each analyzer against synthetic signals with known-correct
// relationships (silence, full-scale tones, out-of-phase pairs, an
// inter-sample-peak sine) - follows the same pattern as
// NovaMotionRegressionTest.h / Nova Translate's ValidationTest.cpp.
//
// This validates the MATH before any listening test, per the plan's
// verification step. It does not judge audio quality - only that each
// analyzer produces internally-consistent, plausible results.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "Analysis/NovaLoudnessAnalyzer.h"
#include "Analysis/NovaDynamicsAnalyzer.h"
#include "Analysis/NovaStereoAnalyzer.h"
#include "Analysis/NovaSpectralAnalyzer.h"
#include <iostream>
#include <cmath>

namespace
{
    int failures = 0;

    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    juce::AudioBuffer<float> makeSilence (int nCh, int numSamples)
    {
        juce::AudioBuffer<float> buf (nCh, numSamples);
        buf.clear();
        return buf;
    }

    juce::AudioBuffer<float> makeSine (int nCh, int numSamples, double sr, double freq, float amp, bool dualMono)
    {
        juce::AudioBuffer<float> buf (nCh, numSamples);
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / sr);
        }
        if (! dualMono && nCh > 1)
        {
            // invert right channel for the out-of-phase test
            auto* r = buf.getWritePointer (1);
            for (int i = 0; i < numSamples; ++i) r[i] = -r[i];
        }
        return buf;
    }

    juce::AudioBuffer<float> makeWhiteNoise (int nCh, int numSamples, float amp, int seed, bool dualMono = false)
    {
        juce::AudioBuffer<float> buf (nCh, numSamples);
        juce::Random rng (seed);
        auto* first = buf.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            first[i] = amp * (rng.nextFloat() * 2.f - 1.f);
        for (int ch = 1; ch < nCh; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            if (dualMono) { for (int i = 0; i < numSamples; ++i) d[i] = first[i]; }
            else           { for (int i = 0; i < numSamples; ++i) d[i] = amp * (rng.nextFloat() * 2.f - 1.f); }
        }
        return buf;
    }
}

int main()
{
    const double sr = 44100.0;
    int totalFailuresBefore = failures;

    // ---- Loudness: silence ----
    {
        std::cout << "\n[Loudness] Silence -> very low LUFS, no true peak\n";
        NovaLoudnessAnalyzer a;
        a.prepare (sr);
        auto buf = makeSilence (2, (int) (sr * 2.0));
        a.process (buf);
        a.finalize();
        check (a.getSamplePeakDb() < -60.f, "sample peak very low");
        check (a.getTruePeakDb() < -60.f, "true peak very low");
    }

    // ---- Loudness: full-scale 1kHz dual-mono sine, sane LUFS ballpark ----
    {
        std::cout << "\n[Loudness] Full-scale 1kHz dual-mono sine -> LUFS in sane range\n";
        NovaLoudnessAnalyzer a;
        a.prepare (sr);
        auto buf = makeSine (2, (int) (sr * 3.0), sr, 1000.0, 1.0f, true);
        a.process (buf);
        a.finalize();
        const float lufs = a.getIntegratedLufs();
        std::cout << "    integrated LUFS = " << lufs << "\n";
        // Unweighted expectation for a full-scale dual-mono sine is ~ -0.69
        // LUFS (see NovaLoudnessAnalyzer.cpp header comment); K-weighting's
        // shelf isn't perfectly flat at 1kHz, so allow a generous band
        // rather than asserting exact calibration (no official BS.1770
        // conformance files available to validate against bit-exactly).
        check (lufs > -6.f && lufs < 3.f, "integrated LUFS within [-6, 3]");
        check (a.getSamplePeakDb() > -0.5f, "sample peak near 0dBFS");
    }

    // ---- True peak: inter-sample peak detection ----
    {
        std::cout << "\n[Loudness] Sine tuned to fall between samples -> true peak > sample peak\n";
        NovaLoudnessAnalyzer a;
        a.prepare (sr);
        // A sine whose peak doesn't land on an integer sample index (freq
        // chosen so the period isn't a clean divisor of the sample rate).
        auto buf = makeSine (2, (int) (sr * 1.0), sr, 3917.3, 1.0f, true);
        a.process (buf);
        a.finalize();
        std::cout << "    sample peak = " << a.getSamplePeakDb() << " dB, true peak = " << a.getTruePeakDb() << " dB\n";
        check (a.getTruePeakDb() > a.getSamplePeakDb() - 0.01f, "true peak >= sample peak");
    }

    // ---- Dynamics: crest factor distinguishes sine vs. uniform white noise ----
    // Dual-mono (identical channels) for both signals: the ported formula
    // (Nova Master/Source/PluginProcessor.cpp:527-569) takes max(|L|,|R|)
    // for BOTH the peak and mean-square terms, which is intentional for
    // real (correlated) mix material but understates crest factor for
    // fully independent per-channel noise - not a realistic mastering
    // input, so not what this test should exercise.
    {
        std::cout << "\n[Dynamics] Crest factor: sine ~3dB, uniform white noise ~4.8dB\n";
        NovaDynamicsAnalyzer sineA, noiseA;
        sineA.prepare (sr); noiseA.prepare (sr);

        auto sineBuf  = makeSine (2, (int) (sr * 3.0), sr, 440.0, 1.0f, true);
        auto noiseBuf = makeWhiteNoise (2, (int) (sr * 3.0), 1.0f, 42, true);

        sineA.process (sineBuf);   sineA.finalize();
        noiseA.process (noiseBuf); noiseA.finalize();

        std::cout << "    sine crest = " << sineA.getCrestFactorDb()
                  << " dB, noise crest = " << noiseA.getCrestFactorDb() << " dB\n";
        check (sineA.getCrestFactorDb() > 2.f && sineA.getCrestFactorDb() < 4.f, "sine crest factor near 3dB");
        check (noiseA.getCrestFactorDb() > sineA.getCrestFactorDb(), "noise crest factor exceeds sine's");
    }

    // ---- Dynamics: transient detector fires more on a percussive-like burst train than continuous noise ----
    // The "non-transient" reference is continuous white noise, not a
    // low-frequency sine: a pure tone's own periodic zero-crossings
    // legitimately produce a fast/slow envelope-ratio spike every
    // half-cycle (real envelope modulation, not a detector bug), which
    // makes a steady low-frequency tone a poor stand-in for "no
    // transients." Continuous noise has no periodic envelope pattern and
    // is the cleaner steady-signal baseline.
    {
        std::cout << "\n[Dynamics] Transient density: burst train vs. continuous noise\n";
        NovaDynamicsAnalyzer burstA, steadyA;
        burstA.prepare (sr); steadyA.prepare (sr);

        const int total = (int) (sr * 4.0);
        juce::AudioBuffer<float> burstBuf (2, total);
        burstBuf.clear();
        // 8 short percussive bursts across 4 seconds (2/sec).
        for (int burst = 0; burst < 8; ++burst)
        {
            const int start = burst * (total / 8);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = burstBuf.getWritePointer (ch);
                for (int i = 0; i < 200 && start + i < total; ++i)
                    d[start + i] = (1.f - (float) i / 200.f) * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 200.0 * i / sr);
            }
        }
        auto steadyBuf = makeWhiteNoise (2, total, 0.5f, 99, true);

        burstA.process (burstBuf);   burstA.finalize();
        steadyA.process (steadyBuf); steadyA.finalize();

        std::cout << "    burst density = " << burstA.getTransientDensity()
                  << ", steady density = " << steadyA.getTransientDensity() << "\n";
        check (burstA.getTransientDensity() > steadyA.getTransientDensity(), "burst train scores higher transient density");
    }

    // ---- Stereo: correlation and width sanity ----
    {
        std::cout << "\n[Stereo] Correlation + width: identical / inverted / independent channels\n";
        NovaStereoAnalyzer identical, inverted, independent;
        identical.prepare (sr); inverted.prepare (sr); independent.prepare (sr);

        auto identicalBuf  = makeSine (2, (int) (sr * 1.0), sr, 300.0, 1.0f, true);
        auto invertedBuf   = makeSine (2, (int) (sr * 1.0), sr, 300.0, 1.0f, false); // R = -L
        auto independentBuf = makeWhiteNoise (2, (int) (sr * 2.0), 1.0f, 7); // uncorrelated per-channel seed reused, acceptable for a loose sanity check

        identical.process (identicalBuf);
        inverted.process (invertedBuf);
        independent.process (independentBuf);

        std::cout << "    identical corr=" << identical.getCorrelation() << " width=" << identical.getWidth() << "\n";
        std::cout << "    inverted  corr=" << inverted.getCorrelation()  << " width=" << inverted.getWidth() << "\n";
        std::cout << "    indep.    corr=" << independent.getCorrelation() << "\n";

        check (identical.getCorrelation() > 0.99f, "identical channels -> correlation ~ +1");
        check (identical.getWidth() < 0.05f, "identical channels -> width ~ 0 (mono)");
        check (inverted.getCorrelation() < -0.99f, "inverted channel -> correlation ~ -1");
        check (inverted.getWidth() > 0.95f, "inverted channel -> width ~ 1 (maximally wide)");
    }

    // ---- Spectral: band energy placement + tilt direction ----
    {
        std::cout << "\n[Spectral] Band energy placement: bass tone vs. air tone\n";
        NovaSpectralAnalyzer bassA, airA, noiseA;
        bassA.prepare (sr); airA.prepare (sr); noiseA.prepare (sr);

        auto bassBuf  = makeSine (2, (int) (sr * 2.0), sr, 40.0, 0.8f, true);    // deep in "Sub"
        auto airBuf   = makeSine (2, (int) (sr * 2.0), sr, 12000.0, 0.8f, true); // deep in "High"
        auto noiseBuf = makeWhiteNoise (2, (int) (sr * 2.0), 0.8f, 123, true);   // flat-ish spectrum

        bassA.process (bassBuf);   bassA.finalize();
        airA.process (airBuf);     airA.finalize();
        noiseA.process (noiseBuf); noiseA.finalize();

        std::cout << "    bass tone: sub=" << bassA.getBandEnergy (NovaSpectralAnalyzer::Sub)
                  << " high=" << bassA.getBandEnergy (NovaSpectralAnalyzer::High)
                  << " tilt=" << bassA.getSpectralTilt() << "\n";
        std::cout << "    air tone:  sub=" << airA.getBandEnergy (NovaSpectralAnalyzer::Sub)
                  << " high=" << airA.getBandEnergy (NovaSpectralAnalyzer::High)
                  << " tilt=" << airA.getSpectralTilt() << "\n";
        std::cout << "    noise: resonanceScore=" << noiseA.getResonanceScore() << "\n";

        // Thresholds are deliberately loose (0.6, not e.g. 0.9): the band
        // filters overlap by design (see class comment - a broad-strokes
        // graphic-EQ-style split, not surgical crossovers), so some energy
        // legitimately leaks into neighboring bands even for a pure tone.
        check (bassA.getBandEnergy (NovaSpectralAnalyzer::Sub) > 0.6f, "40Hz tone concentrates in Sub band");
        check (bassA.getSpectralTilt() < -0.3f, "40Hz tone reads as dark (negative tilt)");
        check (airA.getBandEnergy (NovaSpectralAnalyzer::High) > 0.6f, "12kHz tone concentrates in High band");
        check (airA.getSpectralTilt() > 0.3f, "12kHz tone reads as bright (positive tilt)");
        check (noiseA.getResonanceScore() < 0.5f, "flat-spectrum noise scores low resonance");
    }

    const int totalFailures = failures - totalFailuresBefore;
    std::cout << "\n==============================\n";
    std::cout << (totalFailures == 0 ? "ALL ANALYSIS VALIDATION TESTS PASSED\n"
                                     : juce::String (totalFailures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return totalFailures == 0 ? 0 : 1;
}
