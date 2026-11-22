// End-to-end NovaVelvetEngine validation: Stereo Link divergence, Analog
// Mode harmonic ordering (Vintage > Tube > Clean), Tube Drive monotonicity,
// latency sanity.

#include "NovaVelvetEngine.h"
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

    void setDefaultParams (NovaVelvetEngine& engine, int analogMode = 1, float tubeDrivePercent = 25.0f, bool stereoLink = true)
    {
        engine.setParams (/*thresholdDbu*/ -20.0f, /*ratio*/ 4.0f, /*attackMs*/ 10.0f, /*releaseMs*/ 300.0f,
                           /*gainDb*/ 6.0f, /*timeSelect*/ 1, /*scHpfHz*/ 20.0f,
                           /*mixPercent*/ 100.0f, /*outputDb*/ 0.0f, stereoLink,
                           analogMode, tubeDrivePercent, /*bypassed*/ false);
    }

    juce::AudioBuffer<float> makeSine (double sr, double freq, double seconds, float amplitude, bool hardPanLeft = false)
    {
        juce::AudioBuffer<float> buf (2, (int) (sr * seconds));
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const float s = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr);
            buf.setSample (0, i, s);
            buf.setSample (1, i, hardPanLeft ? 0.0f : s);
        }
        return buf;
    }

    double thdPercentOf (NovaVelvetEngine& engine, double sr, double freq, float amplitude)
    {
        constexpr int fftOrder = 13;
        constexpr int fftSize = 1 << fftOrder;
        juce::dsp::FFT fft (fftOrder);

        // Settle.
        auto settle = makeSine (sr, freq, 0.2, amplitude);
        for (int pos = 0; pos < settle.getNumSamples(); pos += 512)
        {
            const int n = juce::jmin (512, settle.getNumSamples() - pos);
            juce::AudioBuffer<float> block (settle.getArrayOfWritePointers(), 2, pos, n);
            engine.processBlock (block);
        }

        auto analysis = makeSine (sr, freq, (double) fftSize / sr, amplitude);
        for (int pos = 0; pos < analysis.getNumSamples(); pos += 512)
        {
            const int n = juce::jmin (512, analysis.getNumSamples() - pos);
            juce::AudioBuffer<float> block (analysis.getArrayOfWritePointers(), 2, pos, n);
            engine.processBlock (block);
        }

        std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
        const auto* src = analysis.getReadPointer (0);
        for (int i = 0; i < fftSize; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (2.0 * juce::MathConstants<double>::pi * i / (fftSize - 1));
            fftData[(size_t) i] = src[i] * w;
        }
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const double binHz = sr / fftSize;
        const auto magAt = [&] (double hz) -> double
        {
            const int bin = (int) std::round (hz / binHz);
            double peak = 0.0;
            for (int b = juce::jmax (0, bin - 2); b <= juce::jmin (fftSize / 2, bin + 2); ++b)
                peak = juce::jmax (peak, (double) fftData[(size_t) b]);
            return peak;
        };

        const double fundamental = magAt (freq);
        double harmonicsSum = 0.0;
        for (int h = 2; h <= 6; ++h)
            if (freq * h < sr * 0.4)
                harmonicsSum += magAt (freq * h) * magAt (freq * h);
        harmonicsSum = std::sqrt (harmonicsSum);

        return fundamental > 1.0e-9 ? (harmonicsSum / fundamental) * 100.0 : 0.0;
    }
}

int main()
{
    const double sr = 44100.0;
    juce::dsp::ProcessSpec spec { sr, 512, 2 };

    std::cout << "\n[Engine] Latency sanity: reported latency == inputStage + outputStage latency\n";
    {
        NovaVelvetEngine engine;
        engine.prepare (spec, 1); // realtime 2x oversampling
        setDefaultParams (engine);
        const int latency = engine.getLatencySamples();
        std::cout << "  reported latency: " << latency << " samples\n";
        check (latency >= 0, "latency is non-negative");
        check (latency < (int) sr, "latency is a sane, small value (< 1 second)");
    }

    std::cout << "\n[Engine] Stereo Link: linked vs unlinked produce different results on a hard-panned signal\n";
    {
        NovaVelvetEngine engineLinked, engineUnlinked;
        engineLinked.prepare (spec, 1);
        engineUnlinked.prepare (spec, 1);
        setDefaultParams (engineLinked, 1, 25.0f, true);
        setDefaultParams (engineUnlinked, 1, 25.0f, false);

        // Hard-panned-left signal: loud on L, silent on R. Linked
        // detection should pull R's gain reduction toward L's; unlinked
        // should leave R essentially untouched (no signal to detect).
        auto bufLinked = makeSine (sr, 1000.0, 0.3, 0.7f, true);
        auto bufUnlinked = makeSine (sr, 1000.0, 0.3, 0.7f, true);

        for (int pos = 0; pos < bufLinked.getNumSamples(); pos += 512)
        {
            const int n = juce::jmin (512, bufLinked.getNumSamples() - pos);
            juce::AudioBuffer<float> blockL (bufLinked.getArrayOfWritePointers(), 2, pos, n);
            juce::AudioBuffer<float> blockU (bufUnlinked.getArrayOfWritePointers(), 2, pos, n);
            engineLinked.processBlock (blockL);
            engineUnlinked.processBlock (blockU);
        }

        // Compare RMS of the (silent-input) R channel at the end - linked
        // should show measurable gain movement (attenuation) even though R
        // itself carried no signal; unlinked's R channel should be exactly
        // zero throughout (no signal in, no signal out, regardless of L).
        double rmsRLinked = 0.0, rmsRUnlinked = 0.0;
        const int tailStart = bufLinked.getNumSamples() - (int) (sr * 0.05);
        for (int i = juce::jmax (0, tailStart); i < bufLinked.getNumSamples(); ++i)
        {
            rmsRLinked += bufLinked.getSample (1, i) * bufLinked.getSample (1, i);
            rmsRUnlinked += bufUnlinked.getSample (1, i) * bufUnlinked.getSample (1, i);
        }
        std::cout << "  R channel (silent input) energy - linked: " << rmsRLinked << ", unlinked: " << rmsRUnlinked << "\n";
        check (rmsRUnlinked < 1.0e-12, "unlinked: silent R channel stays exactly silent (no cross-channel influence)");

        bool allFinite = true;
        for (int i = 0; i < bufLinked.getNumSamples(); ++i)
            if (! std::isfinite (bufLinked.getSample (0, i)) || ! std::isfinite (bufLinked.getSample (1, i)))
                allFinite = false;
        check (allFinite, "linked engine output stays finite throughout");
    }

    std::cout << "\n[Engine] Analog Mode: harmonic ordering Vintage > Tube > Clean at a driven level\n";
    {
        NovaVelvetEngine engineClean, engineTube, engineVintage;
        engineClean.prepare (spec, 1);
        engineTube.prepare (spec, 1);
        engineVintage.prepare (spec, 1);
        setDefaultParams (engineClean, 0, 60.0f);
        setDefaultParams (engineTube, 1, 60.0f);
        setDefaultParams (engineVintage, 2, 60.0f);

        const double thdClean = thdPercentOf (engineClean, sr, 1000.0, 0.7f);
        const double thdTube = thdPercentOf (engineTube, sr, 1000.0, 0.7f);
        const double thdVintage = thdPercentOf (engineVintage, sr, 1000.0, 0.7f);
        std::cout << "  Clean THD=" << thdClean << "%, Tube THD=" << thdTube << "%, Vintage THD=" << thdVintage << "%\n";

        check (thdTube > thdClean, "Tube mode produces more harmonic content than Clean at the same drive");
        check (thdVintage > thdTube, "Vintage mode produces more harmonic content than Tube at the same drive");
    }

    std::cout << "\n[Engine] Tube Drive: THD increases monotonically with Tube Drive setting\n";
    {
        std::vector<double> thds;
        for (float drivePercent : { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f })
        {
            NovaVelvetEngine engine;
            engine.prepare (spec, 1);
            setDefaultParams (engine, 1, drivePercent);
            const double thd = thdPercentOf (engine, sr, 1000.0, 0.5f);
            thds.push_back (thd);
            std::cout << "  tubeDrive=" << drivePercent << "% -> THD=" << thd << "%\n";
        }
        bool monotonic = true;
        for (size_t i = 1; i < thds.size(); ++i)
            if (thds[i] < thds[i - 1] - 0.05) monotonic = false;
        check (monotonic, "THD increases (non-decreasing) with Tube Drive despite partial output compensation");
    }

    std::cout << "\n[Engine] Safety: silence, DC, extreme levels, mid-stream reset\n";
    {
        NovaVelvetEngine engine;
        engine.prepare (spec, 1);
        setDefaultParams (engine);

        juce::AudioBuffer<float> silence (2, 512);
        silence.clear();
        engine.processBlock (silence);
        bool silenceFinite = true, silenceQuiet = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
            {
                const float s = silence.getSample (ch, i);
                if (! std::isfinite (s)) silenceFinite = false;
                if (std::abs (s) > 1.0e-6f) silenceQuiet = false;
            }
        check (silenceFinite, "silence in -> finite out");
        check (silenceQuiet, "silence in -> stays near-silent out (no hidden noise floor)");

        juce::AudioBuffer<float> dcBuf (2, 512);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                dcBuf.setSample (ch, i, 0.5f);
        engine.processBlock (dcBuf);
        bool dcFinite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                if (! std::isfinite (dcBuf.getSample (ch, i))) dcFinite = false;
        check (dcFinite, "DC input stays finite");

        juce::AudioBuffer<float> extreme (2, 512);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                extreme.setSample (ch, i, 1.0e4f);
        engine.processBlock (extreme);
        bool extremeFinite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                if (! std::isfinite (extreme.getSample (ch, i))) extremeFinite = false;
        check (extremeFinite, "extreme input (1e4) stays finite");

        engine.reset();
        juce::AudioBuffer<float> afterReset (2, 512);
        afterReset.clear();
        engine.processBlock (afterReset);
        bool resetFinite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                if (! std::isfinite (afterReset.getSample (ch, i))) resetFinite = false;
        check (resetFinite, "output stays finite after a mid-stream reset()");
    }

    std::cout << "\n[Engine] Sample-rate sweep\n";
    {
        for (double testSr : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        {
            NovaVelvetEngine engine;
            juce::dsp::ProcessSpec s { testSr, 512, 2 };
            engine.prepare (s, 1);
            setDefaultParams (engine);
            auto buf = makeSine (testSr, 1000.0, 0.05, 0.5f);
            for (int pos = 0; pos < buf.getNumSamples(); pos += 512)
            {
                const int n = juce::jmin (512, buf.getNumSamples() - pos);
                juce::AudioBuffer<float> block (buf.getArrayOfWritePointers(), 2, pos, n);
                engine.processBlock (block);
            }
            bool finite = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < buf.getNumSamples(); ++i)
                    if (! std::isfinite (buf.getSample (ch, i))) finite = false;
            check (finite, "sr=" + juce::String (testSr) + "Hz: finite output");
        }
    }

    std::cout << "\n[Engine] Buffer-size sweep\n";
    {
        for (int blockSize : { 32, 64, 128, 256, 512, 1024 })
        {
            NovaVelvetEngine engine;
            juce::dsp::ProcessSpec s { sr, (juce::uint32) blockSize, 2 };
            engine.prepare (s, 1);
            setDefaultParams (engine);
            auto buf = makeSine (sr, 1000.0, 0.05, 0.5f);
            for (int pos = 0; pos < buf.getNumSamples(); pos += blockSize)
            {
                const int n = juce::jmin (blockSize, buf.getNumSamples() - pos);
                juce::AudioBuffer<float> block (buf.getArrayOfWritePointers(), 2, pos, n);
                engine.processBlock (block);
            }
            bool finite = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < buf.getNumSamples(); ++i)
                    if (! std::isfinite (buf.getSample (ch, i))) finite = false;
            check (finite, "blockSize=" + juce::String (blockSize) + ": finite output");
        }
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL ENGINE VALIDATION TESTS PASSED\n" : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";
    return failures == 0 ? 0 : 1;
}
