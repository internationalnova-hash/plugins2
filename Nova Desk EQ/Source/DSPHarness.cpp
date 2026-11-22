// Headless validation of the FULL signal chain, driving the real
// NovaDeskEQAudioProcessor class directly (not a reimplementation) -
// Input -> Drive -> [HPF -> Low -> Low-Mid -> High-Mid -> High -> LPF] ->
// Mix -> Output (build spec section 4), run through the complete chain
// with silence/DC/sine-sweep/impulse/noise/transient probes, matching the
// catalog's proven DSPHarness convention (see Nova 7even's DSPHarness.cpp).

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"
#include <juce_core/juce_core.h>
#include <iostream>
#include <cmath>
#include <random>

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    void setChoice (juce::AudioProcessorValueTreeState& apvts, const char* id, int index)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
    }

    void setFloat (juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    void setBool (juce::AudioProcessorValueTreeState& apvts, const char* id, bool value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (value ? 1.0f : 0.0f);
    }

    void processInChunks (NovaDeskEQAudioProcessor& proc, juce::AudioBuffer<float>& buffer, int chunkSize)
    {
        juce::MidiBuffer midi;
        const int total = buffer.getNumSamples();
        for (int start = 0; start < total; start += chunkSize)
        {
            const int n = juce::jmin (chunkSize, total - start);
            juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, n);
            proc.processBlock (chunk, midi);
        }
    }

    bool isFiniteAndBounded (const juce::AudioBuffer<float>& buffer, float bound)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* s = buffer.getReadPointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (! std::isfinite (s[i])) return false;
                if (std::abs (s[i]) > bound) return false;
            }
        }
        return true;
    }

    float goertzelMagnitude (const float* samples, int N, double sampleRate, double targetFreqHz)
    {
        const double k = (targetFreqHz * (double) N) / sampleRate;
        const double omega = (2.0 * juce::MathConstants<double>::pi * k) / (double) N;
        const double coeff = 2.0 * std::cos (omega);
        double s0 = 0.0, s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < N; ++i) { s0 = (double) samples[i] + coeff * s1 - s2; s2 = s1; s1 = s0; }
        const double realPart = s1 - s2 * std::cos (omega);
        const double imagPart = s2 * std::sin (omega);
        return (float) (2.0 * std::sqrt (realPart * realPart + imagPart * imagPart) / (double) N);
    }
}

int main()
{
    const double sampleRate = 48000.0;
    const int blockSize = 512;
    const int chunkSize = 512;

    std::cout << "\n[DSPHarness] Silence in, silence out (default settings)\n";
    {
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, 48000);
        buffer.clear();
        processInChunks (proc, buffer, chunkSize);

        float maxAbs = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                maxAbs = juce::jmax (maxAbs, std::abs (buffer.getSample (ch, i)));

        check (maxAbs < 1.0e-4f, "silence in produces near-silence out through the full chain, max abs = " + juce::String (maxAbs, 6));
    }

    std::cout << "\n[DSPHarness] DC input stays finite and bounded, max drive + all bands boosted\n";
    {
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        setFloat (proc.apvts, ParameterIDs::drive, 9.0f);
        setFloat (proc.apvts, ParameterIDs::lowGain, 15.0f);
        setFloat (proc.apvts, ParameterIDs::lowMidGain, -15.0f);
        setFloat (proc.apvts, ParameterIDs::highMidGain, 15.0f);
        setFloat (proc.apvts, ParameterIDs::highGain, -15.0f);
        setFloat (proc.apvts, ParameterIDs::input, 20.0f);

        juce::AudioBuffer<float> buffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i) w[i] = 0.9f;
        }
        processInChunks (proc, buffer, chunkSize);

        check (isFiniteAndBounded (buffer, 10.0f), "DC input through max-drive/all-bands-boosted chain stays finite and bounded");
    }

    std::cout << "\n[DSPHarness] Sine sweep stays finite and bounded across the audible range\n";
    {
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        setFloat (proc.apvts, ParameterIDs::drive, 6.0f);
        setChoice (proc.apvts, ParameterIDs::hpfFreqIndex, 3);
        setChoice (proc.apvts, ParameterIDs::lpfFreqIndex, 3);
        setFloat (proc.apvts, ParameterIDs::lowGain, 10.0f);
        setFloat (proc.apvts, ParameterIDs::highMidGain, -8.0f);
        setFloat (proc.apvts, ParameterIDs::highGain, 6.0f);

        bool allOk = true;
        const float testFreqs[] = { 20.0f, 60.0f, 110.0f, 300.0f, 700.0f, 1000.0f, 3200.0f, 8000.0f, 12000.0f, 18000.0f };
        for (float freq : testFreqs)
        {
            juce::AudioBuffer<float> buffer (2, 4800);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* w = buffer.getWritePointer (ch);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    w[i] = 0.6f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / sampleRate);
            }
            processInChunks (proc, buffer, chunkSize);
            if (! isFiniteAndBounded (buffer, 5.0f)) allOk = false;
        }
        check (allOk, "sine sweep across 20Hz-18kHz stays finite and bounded through the full chain");
    }

    std::cout << "\n[DSPHarness] Impulse response stays finite\n";
    {
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        setFloat (proc.apvts, ParameterIDs::drive, 5.0f);
        setFloat (proc.apvts, ParameterIDs::lowMidGain, 8.0f);

        juce::AudioBuffer<float> buffer (2, 4800);
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        buffer.setSample (1, 0, 1.0f);
        processInChunks (proc, buffer, chunkSize);

        check (isFiniteAndBounded (buffer, 5.0f), "impulse response stays finite and bounded through the full chain");
    }

    std::cout << "\n[DSPHarness] White/pink noise and a transient burst stay finite and bounded\n";
    {
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        setFloat (proc.apvts, ParameterIDs::drive, 8.0f);
        setFloat (proc.apvts, ParameterIDs::highGain, 10.0f);

        std::mt19937 rng (12345);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);

        juce::AudioBuffer<float> noiseBuffer (2, 48000);
        float pinkState[7] = {};
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* w = noiseBuffer.getWritePointer (ch);
            for (int i = 0; i < noiseBuffer.getNumSamples(); ++i)
            {
                const float white = dist (rng);
                pinkState[0] = 0.99886f * pinkState[0] + white * 0.0555179f;
                pinkState[1] = 0.99332f * pinkState[1] + white * 0.0750759f;
                pinkState[2] = 0.96900f * pinkState[2] + white * 0.1538520f;
                const float pink = pinkState[0] + pinkState[1] + pinkState[2] + white * 0.1848f;
                w[i] = 0.3f * pink;
            }
        }
        processInChunks (proc, noiseBuffer, chunkSize);
        check (isFiniteAndBounded (noiseBuffer, 5.0f), "pink noise stays finite and bounded through the full chain");

        juce::AudioBuffer<float> burstBuffer (2, 4800);
        burstBuffer.clear();
        for (int i = 1000; i < 1050; ++i)
        {
            burstBuffer.setSample (0, i, 0.95f);
            burstBuffer.setSample (1, i, -0.95f);
        }
        processInChunks (proc, burstBuffer, chunkSize);
        check (isFiniteAndBounded (burstBuffer, 5.0f), "transient burst stays finite and bounded through the full chain");
    }

    std::cout << "\n[DSPHarness] Mix=0% settles to dry (post Input gain, pre Drive/EQ)\n";
    {
        // Mix is now a click-free ~20ms smoothed crossfade (gap-closure
        // item 1/2), not an instant switch - the parameter changes are
        // all applied on the FIRST processBlock call after prepareToPlay,
        // so the first ~20ms (~960 samples at 48kHz) of this buffer is a
        // genuine, correct transition from the prepare-time default
        // (Mix=100%) down to 0%, not dry yet. Comparing only the settled
        // region after that window is the correct check now.
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        setFloat (proc.apvts, ParameterIDs::mix, 0.0f);
        setFloat (proc.apvts, ParameterIDs::input, 0.0f);
        setFloat (proc.apvts, ParameterIDs::output, 0.0f);
        setFloat (proc.apvts, ParameterIDs::drive, 9.0f);
        setFloat (proc.apvts, ParameterIDs::lowGain, 12.0f);

        juce::AudioBuffer<float> buffer (2, 4800);
        std::vector<float> reference (4800);
        for (int i = 0; i < 4800; ++i)
        {
            const float v = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * (double) i / sampleRate);
            reference[(size_t) i] = v;
            buffer.setSample (0, i, v);
            buffer.setSample (1, i, v);
        }
        processInChunks (proc, buffer, chunkSize);

        // Dry content at Mix=0% still crosses BOTH the input and output
        // DC-blocker boundaries (gap-closure item 4's "DC blocking at
        // INPUT and OUTPUT boundaries" - deliberately two boundaries, not
        // one, so DC introduced anywhere - upstream gear, or by the
        // Drive/EQ processing itself when Mix isn't 0% - gets caught).
        // Two cascaded ~5Hz one-pole highpasses introduce a small but
        // real phase shift even well into the passband (300Hz here);
        // measured ~0.022 max instantaneous diff on a 0.5-amplitude
        // tone, consistent with that expected phase shift, not a defect.
        const int settleSamples = 1100;
        float maxDiff = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = settleSamples; i < 4800; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (buffer.getSample (ch, i) - reference[(size_t) i]));

        check (maxDiff < 0.03f, "Mix=0% settled output matches the raw input within I/O DC-blocker phase-shift tolerance, max diff = " + juce::String (maxDiff, 6));
    }

    std::cout << "\n[DSPHarness] Bypass settles to a true passthrough, ignoring every other parameter\n";
    {
        // Same reasoning as the Mix test above: bypass is now a click-
        // free ~20ms smoothed crossfade against the true raw input, not
        // an instant hard branch - the settled region (after the ramp)
        // is what must match the raw input exactly.
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        setBool (proc.apvts, ParameterIDs::bypass, true);
        setFloat (proc.apvts, ParameterIDs::input, 20.0f); // would NOT be transparent if applied
        setFloat (proc.apvts, ParameterIDs::output, 12.0f);
        setFloat (proc.apvts, ParameterIDs::drive, 10.0f);
        setFloat (proc.apvts, ParameterIDs::lowGain, 15.0f);

        juce::AudioBuffer<float> buffer (2, 4800);
        std::vector<float> reference (4800);
        for (int i = 0; i < 4800; ++i)
        {
            const float v = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * (double) i / sampleRate);
            reference[(size_t) i] = v;
            buffer.setSample (0, i, v);
            buffer.setSample (1, i, v);
        }
        processInChunks (proc, buffer, chunkSize);

        const int settleSamples = 1100;
        float maxDiff = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = settleSamples; i < 4800; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (buffer.getSample (ch, i) - reference[(size_t) i]));

        check (maxDiff < 1.0e-6f, "bypassed output matches raw input exactly regardless of every other parameter, max diff = " + juce::String (maxDiff, 8));
    }

    std::cout << "\n[DSPHarness] Latency is constant (the Drive oversampler now always runs, see gap-closure notes)\n";
    {
        // NovaDeskDrive's oversampler runs unconditionally regardless of
        // drive amount - an FIR filter that goes idle and is later fed
        // live audio outputs near-silence for its first `latency` samples
        // (its delay line starts zero-filled), which is an audible click
        // on re-engage. Keeping it always warm means latency is now a
        // fixed property of the oversampling factor, not something that
        // changes (and needs re-reporting to the host) as Drive moves.
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        const int latencyAtDefault = proc.getLatencySamples();
        std::cout << "    reported latency at Drive=0 (default): " << latencyAtDefault << " samples\n";
        check (latencyAtDefault > 0, "latency is already reported at Drive=0, since the oversampler is already running");
        check (latencyAtDefault < 200, "reported latency stays sane (< 200 samples)");

        setFloat (proc.apvts, ParameterIDs::drive, 5.0f);
        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();
        processInChunks (proc, buffer, chunkSize);

        const int latencyEngaged = proc.getLatencySamples();
        std::cout << "    reported latency once Drive engages: " << latencyEngaged << " samples\n";
        check (latencyEngaged == latencyAtDefault, "latency does not change when Drive engages, got " + juce::String (latencyEngaged) + " vs default " + juce::String (latencyAtDefault));
    }

    std::cout << "\n[DSPHarness] E and G produce a measurably different full-chain output at matched settings\n";
    {
        auto renderWithMode = [&] (int egModeIndex)
        {
            NovaDeskEQAudioProcessor proc;
            proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
            proc.prepareToPlay (sampleRate, blockSize);
            setFloat (proc.apvts, ParameterIDs::mix, 100.0f);
            setChoice (proc.apvts, ParameterIDs::egMode, egModeIndex);
            setFloat (proc.apvts, ParameterIDs::highMidFreq, 2000.0f);
            setFloat (proc.apvts, ParameterIDs::highMidGain, 12.0f);
            setFloat (proc.apvts, ParameterIDs::highMidQ, 1.0f);

            juce::AudioBuffer<float> buffer (2, 48000);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* w = buffer.getWritePointer (ch);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    w[i] = 0.4f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 4000.0 * (double) i / sampleRate);
            }
            processInChunks (proc, buffer, chunkSize);
            return goertzelMagnitude (buffer.getReadPointer (0), buffer.getNumSamples(), sampleRate, 4000.0);
        };

        const auto magE = renderWithMode (0);
        const auto magG = renderWithMode (1);
        std::cout << "    one octave above High-Mid center (4kHz) - E: " << magE << ", G: " << magG << "\n";
        check (std::abs (magE - magG) > 1.0e-3f, "E and G leave a real, measurable difference at the octave-above probe frequency through the full chain");
    }

    std::cout << "\n[Click-Safety] Rapid bypass toggling produces no hard discontinuity\n";
    {
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        setFloat (proc.apvts, ParameterIDs::drive, 6.0f);
        setFloat (proc.apvts, ParameterIDs::lowMidGain, 8.0f);

        const double toneFreq = 200.0;
        const int localBlockSize = 256;
        const int numBlocks = 60;
        std::vector<float> fullOutput;
        fullOutput.reserve ((size_t) (localBlockSize * numBlocks));

        int sampleCounter = 0;
        bool bypassState = false;
        for (int b = 0; b < numBlocks; ++b)
        {
            if (b == 10 || b == 25 || b == 40)
            {
                bypassState = ! bypassState;
                setBool (proc.apvts, ParameterIDs::bypass, bypassState);
            }

            juce::AudioBuffer<float> buffer (2, localBlockSize);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* w = buffer.getWritePointer (ch);
                for (int i = 0; i < localBlockSize; ++i)
                    w[i] = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * toneFreq * (double) (sampleCounter + i) / sampleRate);
            }
            sampleCounter += localBlockSize;

            juce::MidiBuffer midi;
            proc.processBlock (buffer, midi);
            for (int i = 0; i < localBlockSize; ++i)
                fullOutput.push_back (buffer.getSample (0, i));
        }

        float maxJump = 0.0f;
        int maxJumpIdx = -1;
        for (size_t i = 1; i < fullOutput.size(); ++i)
        {
            const float j = std::abs (fullOutput[i] - fullOutput[i - 1]);
            if (j > maxJump) { maxJump = j; maxJumpIdx = (int) i; }
        }

        // A probe run (logging the jump's sample index) found the max
        // jump landing ~968 samples after a toggle - well outside the
        // ~960-sample (20ms) bypass ramp window, not at a toggle boundary
        // - confirming it's legitimate Drive-distorted waveform content
        // (Drive=6 is engaged here), not a residual click, matching the
        // same threshold finding as DriveValidation.cpp's own click test.
        std::cout << "    max inter-sample jump across 3 rapid bypass toggles: " << maxJump
                   << " at sample " << maxJumpIdx << " (toggles at samples 2560, 6400, 10240)\n";
        check (maxJump < 0.08f, "no hard discontinuity across rapid bypass toggling, got " + juce::String (maxJump, 4));
    }

    std::cout << "\n[DSPHarness] DC offset is measurably removed at the input/output boundaries\n";
    {
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                w[i] = 0.3f; // pure DC offset, no AC content at all
        }
        processInChunks (proc, buffer, chunkSize);

        // Settled region (past the DC blocker's own settling time).
        double sum = 0.0;
        int count = 0;
        for (int i = 24000; i < 48000; ++i)
        {
            sum += buffer.getSample (0, i);
            ++count;
        }
        const double meanLevel = sum / count;
        std::cout << "    settled mean output level for a 0.3 DC input: " << meanLevel << "\n";
        check (std::abs (meanLevel) < 0.01, "pure DC input is reduced to near-zero by the I/O DC blockers, got " + juce::String (meanLevel, 5));
    }

    std::cout << "\n[DSPHarness] 808/sub-bass fundamental is not audibly thinned by the I/O DC blockers\n";
    {
        // The DC blockers use a ~5Hz corner specifically so real sub
        // material (808s bottom out around 30-40Hz, sub-bass extends to
        // ~20Hz) is untouched - verify a 40Hz tone's amplitude survives
        // essentially intact through Input+Output DC blocking alone
        // (Drive/EQ neutral, Mix=100%).
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        // The processor now starts on the "Default Channel EQ" factory
        // preset (Drive=2, G mode - see the constructor), not raw-neutral
        // scaffold defaults, so this test must explicitly flatten
        // Drive/EQ itself rather than relying on construction-time
        // defaults to isolate the DC blockers' effect alone.
        setFloat (proc.apvts, ParameterIDs::drive, 0.0f);
        setFloat (proc.apvts, ParameterIDs::lowGain, 0.0f);
        setFloat (proc.apvts, ParameterIDs::lowMidGain, 0.0f);
        setFloat (proc.apvts, ParameterIDs::highMidGain, 0.0f);
        setFloat (proc.apvts, ParameterIDs::highGain, 0.0f);

        const double subFreq = 40.0;
        const int numSamples = 48000;
        juce::AudioBuffer<float> buffer (2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                w[i] = 0.6f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * subFreq * (double) i / sampleRate);
        }
        processInChunks (proc, buffer, chunkSize);

        const float outputMag = goertzelMagnitude (buffer.getReadPointer (0) + 24000, numSamples - 24000, sampleRate, subFreq);
        const float gainDb = juce::Decibels::gainToDecibels (outputMag / 0.6f);
        std::cout << "    40Hz gain through Input+Output DC blocking: " << gainDb << " dB\n";
        check (gainDb > -0.5f, "sub-bass fundamental (40Hz) loses less than 0.5dB through the I/O DC blockers, got " + juce::String (gainDb, 3) + "dB");
    }

    std::cout << "\n[DSPHarness] Session recall: full state round-trips through getStateInformation/setStateInformation\n";
    {
        NovaDeskEQAudioProcessor procA;
        procA.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        procA.prepareToPlay (sampleRate, blockSize);

        setFloat (procA.apvts, ParameterIDs::input, 6.5f);
        setFloat (procA.apvts, ParameterIDs::output, -3.5f);
        setChoice (procA.apvts, ParameterIDs::hpfFreqIndex, 3);
        setBool (procA.apvts, ParameterIDs::hpfIn, true);
        setFloat (procA.apvts, ParameterIDs::lowFreq, 80.0f);
        setFloat (procA.apvts, ParameterIDs::lowGain, 4.5f);
        setChoice (procA.apvts, ParameterIDs::lowType, 0);
        setFloat (procA.apvts, ParameterIDs::highMidQ, 2.1f);
        setChoice (procA.apvts, ParameterIDs::egMode, 1);
        setFloat (procA.apvts, ParameterIDs::drive, 4.2f);
        setFloat (procA.apvts, ParameterIDs::mix, 65.0f);
        setBool (procA.apvts, ParameterIDs::bypass, false);

        juce::MemoryBlock state;
        procA.getStateInformation (state);

        NovaDeskEQAudioProcessor procB;
        procB.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        procB.prepareToPlay (sampleRate, blockSize);
        procB.setStateInformation (state.getData(), (int) state.getSize());

        auto get = [] (juce::AudioProcessorValueTreeState& apvts, const char* id)
        {
            return apvts.getRawParameterValue (id)->load();
        };

        bool allMatch = true;
        allMatch &= std::abs (get (procA.apvts, ParameterIDs::input) - get (procB.apvts, ParameterIDs::input)) < 1.0e-4f;
        allMatch &= std::abs (get (procA.apvts, ParameterIDs::output) - get (procB.apvts, ParameterIDs::output)) < 1.0e-4f;
        allMatch &= get (procA.apvts, ParameterIDs::hpfFreqIndex) == get (procB.apvts, ParameterIDs::hpfFreqIndex);
        allMatch &= get (procA.apvts, ParameterIDs::hpfIn) == get (procB.apvts, ParameterIDs::hpfIn);
        allMatch &= std::abs (get (procA.apvts, ParameterIDs::lowFreq) - get (procB.apvts, ParameterIDs::lowFreq)) < 1.0e-2f;
        allMatch &= std::abs (get (procA.apvts, ParameterIDs::lowGain) - get (procB.apvts, ParameterIDs::lowGain)) < 1.0e-4f;
        allMatch &= get (procA.apvts, ParameterIDs::lowType) == get (procB.apvts, ParameterIDs::lowType);
        allMatch &= std::abs (get (procA.apvts, ParameterIDs::highMidQ) - get (procB.apvts, ParameterIDs::highMidQ)) < 1.0e-3f;
        allMatch &= get (procA.apvts, ParameterIDs::egMode) == get (procB.apvts, ParameterIDs::egMode);
        allMatch &= std::abs (get (procA.apvts, ParameterIDs::drive) - get (procB.apvts, ParameterIDs::drive)) < 1.0e-3f;
        allMatch &= std::abs (get (procA.apvts, ParameterIDs::mix) - get (procB.apvts, ParameterIDs::mix)) < 1.0e-2f;
        allMatch &= get (procA.apvts, ParameterIDs::bypass) == get (procB.apvts, ParameterIDs::bypass);

        check (allMatch, "every checked parameter recalls exactly after a getStateInformation/setStateInformation round trip");
    }

    std::cout << "\n[DSPHarness] Oversampling mode changes safely and latency updates to match\n";
    {
        // reconfigureOversamplingIfNeeded() is normally driven by a
        // message-thread timer (never from processBlock() - reallocating
        // the oversampler is not real-time-safe), but is public
        // specifically so this test can trigger it deterministically.
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        const int latencyAt2x = proc.getLatencySamples();
        std::cout << "    latency at default (2x): " << latencyAt2x << " samples\n";

        setChoice (proc.apvts, ParameterIDs::oversamplingMode, 2); // 4x
        proc.reconfigureOversamplingIfNeeded();
        const int latencyAt4x = proc.getLatencySamples();
        std::cout << "    latency after switching to 4x: " << latencyAt4x << " samples\n";
        check (latencyAt4x > latencyAt2x, "4x oversampling reports higher latency than 2x, got " + juce::String (latencyAt4x) + " vs " + juce::String (latencyAt2x));

        setChoice (proc.apvts, ParameterIDs::oversamplingMode, 0); // Off
        proc.reconfigureOversamplingIfNeeded();
        const int latencyOff = proc.getLatencySamples();
        std::cout << "    latency after switching to Off: " << latencyOff << " samples\n";
        check (latencyOff == 0, "Off (1x) oversampling reports zero latency, got " + juce::String (latencyOff));

        // Processing still works correctly after two live reconfigurations.
        setFloat (proc.apvts, ParameterIDs::drive, 5.0f);
        juce::AudioBuffer<float> buffer (2, 4800);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            for (int i = 0; i < 4800; ++i)
                w[i] = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * (double) i / sampleRate);
        }
        processInChunks (proc, buffer, chunkSize);
        check (isFiniteAndBounded (buffer, 5.0f), "processing stays finite and bounded after switching oversampling mode twice");
    }

    std::cout << "\n[DSPHarness] processBlock(AudioBuffer<double>&) processes correctly at native double precision\n";
    {
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        check (proc.supportsDoublePrecisionProcessing(), "processor declares double-precision support");

        setFloat (proc.apvts, ParameterIDs::drive, 6.0f);
        setFloat (proc.apvts, ParameterIDs::lowGain, 8.0f);
        setChoice (proc.apvts, ParameterIDs::egMode, 1);

        const int numSamples = 4800;
        juce::AudioBuffer<double> buffer (2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                w[i] = 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * (double) i / sampleRate);
        }

        juce::MidiBuffer midi;
        for (int start = 0; start < numSamples; start += chunkSize)
        {
            const int n = juce::jmin (chunkSize, numSamples - start);
            juce::AudioBuffer<double> chunk (buffer.getArrayOfWritePointers(), 2, start, n);
            proc.processBlock (chunk, midi);
        }

        bool finite = true;
        double maxAbs = 0.0;
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto* r = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                if (! std::isfinite (r[i])) finite = false;
                maxAbs = juce::jmax (maxAbs, std::abs (r[i]));
            }
        }
        std::cout << "    max abs output through double-precision processBlock: " << maxAbs << "\n";
        check (finite, "double-precision processBlock produces finite output with Drive+EQ engaged");
        check (maxAbs > 0.05 && maxAbs < 5.0, "double-precision processBlock produces sane, non-degenerate output level");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL DSP HARNESS (FULL-CHAIN) TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
