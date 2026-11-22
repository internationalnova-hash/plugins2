// Headless validation of the FULL signal chain, driving the real
// NovaSevenAudioProcessor class directly (not a reimplementation) -
// NOVA_7EVEN_PLAN.md section 8.1 step 6 / section 63's comprehensive
// suite (silence, DC, sine sweep, impulse, white/pink noise, transient
// burst) run through the complete chain, not just per-subsystem.

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

    void processInChunks (NovaSevenAudioProcessor& proc, juce::AudioBuffer<float>& buffer, int chunkSize)
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
}

int main()
{
    const double sampleRate = 48000.0;
    const int blockSize = 512;
    const int chunkSize = 512;

    std::cout << "\n[DSPHarness] Silence in, silence out (default settings)\n";
    {
        NovaSevenAudioProcessor proc;
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

    std::cout << "\n[DSPHarness] DC input stays finite and bounded, Hot mode, high drive\n";
    {
        NovaSevenAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        setChoice (proc.apvts, ParameterIDs::characterMode, 2); // Hot
        setFloat (proc.apvts, ParameterIDs::preampGain, 9.0f);
        setFloat (proc.apvts, ParameterIDs::inputTrim, 20.0f);

        juce::AudioBuffer<float> buffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i) w[i] = 0.9f;
        }
        processInChunks (proc, buffer, chunkSize);

        check (isFiniteAndBounded (buffer, 5.0f), "DC input through Hot/high-drive chain stays finite and bounded");
    }

    std::cout << "\n[DSPHarness] Sine sweep stays finite and bounded across the audible range\n";
    {
        NovaSevenAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        setChoice (proc.apvts, ParameterIDs::characterMode, 2);
        setFloat (proc.apvts, ParameterIDs::preampGain, 6.0f);
        setChoice (proc.apvts, ParameterIDs::hpfFreq, 3);
        setFloat (proc.apvts, ParameterIDs::lowGain, 10.0f);
        setFloat (proc.apvts, ParameterIDs::midGain, -8.0f);
        setFloat (proc.apvts, ParameterIDs::highShelfGain, 6.0f);

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
        NovaSevenAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        setChoice (proc.apvts, ParameterIDs::characterMode, 1);
        setFloat (proc.apvts, ParameterIDs::preampGain, 5.0f);

        juce::AudioBuffer<float> buffer (2, 4800);
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        buffer.setSample (1, 0, 1.0f);
        processInChunks (proc, buffer, chunkSize);

        check (isFiniteAndBounded (buffer, 5.0f), "impulse response stays finite and bounded through the full chain");
    }

    std::cout << "\n[DSPHarness] White/pink noise and a transient burst stay finite and bounded\n";
    {
        NovaSevenAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        setChoice (proc.apvts, ParameterIDs::characterMode, 2);
        setFloat (proc.apvts, ParameterIDs::preampGain, 8.0f);

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
                // Simple Paul Kellett pink noise approximation.
                pinkState[0] = 0.99886f * pinkState[0] + white * 0.0555179f;
                pinkState[1] = 0.99332f * pinkState[1] + white * 0.0750759f;
                pinkState[2] = 0.96900f * pinkState[2] + white * 0.1538520f;
                const float pink = pinkState[0] + pinkState[1] + pinkState[2] + white * 0.1848f;
                w[i] = 0.3f * pink;
            }
        }
        processInChunks (proc, noiseBuffer, chunkSize);
        check (isFiniteAndBounded (noiseBuffer, 5.0f), "pink noise stays finite and bounded through the full chain");

        // Transient burst: silence, then a short loud spike, then silence.
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

    std::cout << "\n[DSPHarness] Mix=0% is exactly dry (post Input Trim, pre color/EQ)\n";
    {
        NovaSevenAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        setFloat (proc.apvts, ParameterIDs::mix, 0.0f);
        setFloat (proc.apvts, ParameterIDs::outputTrim, 0.0f);
        setFloat (proc.apvts, ParameterIDs::inputTrim, 0.0f); // 0dB, so dry == raw input
        setChoice (proc.apvts, ParameterIDs::characterMode, 2);
        setFloat (proc.apvts, ParameterIDs::preampGain, 9.0f);
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

        float maxDiff = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 4800; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (buffer.getSample (ch, i) - reference[(size_t) i]));

        check (maxDiff < 1.0e-4f, "Mix=0% output matches the raw input exactly (0dB Input Trim, no color/EQ audible), max diff = " + juce::String (maxDiff, 6));
    }

    std::cout << "\n[DSPHarness] Bypass is a true hard passthrough, ignoring every other parameter\n";
    {
        NovaSevenAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        setBool (proc.apvts, ParameterIDs::bypass, true);
        setFloat (proc.apvts, ParameterIDs::inputTrim, 20.0f); // would NOT be transparent if applied
        setFloat (proc.apvts, ParameterIDs::outputTrim, 12.0f);
        setBool (proc.apvts, ParameterIDs::phaseInvert, true);
        setChoice (proc.apvts, ParameterIDs::characterMode, 2);
        setFloat (proc.apvts, ParameterIDs::preampGain, 10.0f);

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

        float maxDiff = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 4800; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (buffer.getSample (ch, i) - reference[(size_t) i]));

        check (maxDiff < 1.0e-6f, "bypassed output matches raw input exactly regardless of every other parameter, max diff = " + juce::String (maxDiff, 8));
    }

    std::cout << "\n[DSPHarness] Phase invert flips polarity exactly, nothing else changes (Mix=0%)\n";
    {
        NovaSevenAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        setFloat (proc.apvts, ParameterIDs::mix, 0.0f);
        setFloat (proc.apvts, ParameterIDs::inputTrim, 0.0f);
        setFloat (proc.apvts, ParameterIDs::outputTrim, 0.0f);
        setBool (proc.apvts, ParameterIDs::phaseInvert, true);

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

        float maxDiff = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 4800; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (buffer.getSample (ch, i) - (-reference[(size_t) i])));

        check (maxDiff < 1.0e-4f, "phase-inverted, Mix=0%, 0dB trims output matches -input exactly, max diff = " + juce::String (maxDiff, 6));
    }

    std::cout << "\n[DSPHarness] Latency is reported and consistent with the color stage's oversampling\n";
    {
        NovaSevenAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        const int latency = proc.getLatencySamples();
        std::cout << "    reported latency: " << latency << " samples\n";
        check (latency > 0, "processor reports positive latency (from the oversampling wrapper)");
        check (latency < 200, "reported latency stays sane (< 200 samples)");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL DSP HARNESS (FULL-CHAIN) TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
