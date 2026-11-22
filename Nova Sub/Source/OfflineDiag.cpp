/*
  Nova Sub — Offline functional diagnostic.
  Verifies the real compiled saturation DSP directly against processBlock(),
  without going through a host or an audio device.
*/

#include <JuceHeader.h>
#include <iostream>
#include <cmath>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

namespace
{
    void setParam (NovaSubAudioProcessor& proc, const char* id, float value)
    {
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    void fillSine (juce::AudioBuffer<float>& buffer, float amplitude, float freqHz, double sampleRate)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int s = 0; s < buffer.getNumSamples(); ++s)
                data[s] = amplitude * std::sin (2.0f * juce::MathConstants<float>::pi * freqHz * (float) s / (float) sampleRate);
        }
    }

    std::pair<float, bool> peakAndFinite (const juce::AudioBuffer<float>& buf) noexcept
    {
        float peak = 0.0f;
        bool finite = true;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const auto* d = buf.getReadPointer (ch);
            for (int s = 0; s < buf.getNumSamples(); ++s)
            {
                if (! std::isfinite (d[s])) finite = false;
                peak = juce::jmax (peak, std::abs (d[s]));
            }
        }
        return { peak, finite };
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::cout << "=== NOVA SUB OFFLINE FUNCTIONAL DIAGNOSTIC ===\n\n";

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    NovaSubAudioProcessor proc;
    proc.prepareToPlay (sampleRate, blockSize);

    int passed = 0, failed = 0;
    auto check = [&] (const std::string& label, bool ok)
    {
        std::cout << (ok ? "  [PASS] " : "  [FAIL] ") << label << "\n";
        if (ok) ++passed; else ++failed;
    };

    // ── Test 1: silence in -> silence out ─────────────────────────────────
    std::cout << "--- Test 1: silence in -> silence out ---\n";
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        juce::MidiBuffer midi;
        for (int b = 0; b < 4; ++b)
            proc.processBlock (buffer, midi);

        auto [peak, finite] = peakAndFinite (buffer);
        check ("output finite", finite);
        check ("output silent (peak < 1e-5)", peak < 1.0e-5f);
    }

    // ── Test 2: all 4 modes, extreme Color/Filter/Mix, stay finite+bounded ─
    std::cout << "\n--- Test 2: all modes at parameter extremes ---\n";
    {
        const float extremes[] = { 0.0f, 10.0f };
        const char* modeNames[] = { "Neon", "Heat", "Nova", "Galaxy" };

        for (int modeIdx = 0; modeIdx < 4; ++modeIdx)
        {
            setParam (proc, ParameterIDs::circuitMode, (float) modeIdx);

            for (float colorVal : extremes)
            {
                for (float filterVal : extremes)
                {
                    setParam (proc, ParameterIDs::color, colorVal);
                    setParam (proc, ParameterIDs::filterKnob, filterVal);
                    setParam (proc, ParameterIDs::mix, 100.0f);

                    juce::AudioBuffer<float> buffer (2, blockSize);
                    // Loud input on purpose (amplitude 1.5, i.e. above 0dBFS) to
                    // stress-test the saturation + safety ceiling under worst case.
                    fillSine (buffer, 1.5f, 220.0f, sampleRate);
                    juce::MidiBuffer midi;

                    bool allFinite = true;
                    float maxPeak = 0.0f;
                    for (int b = 0; b < 8; ++b)
                    {
                        proc.processBlock (buffer, midi);
                        auto [peak, finite] = peakAndFinite (buffer);
                        allFinite = allFinite && finite;
                        maxPeak = juce::jmax (maxPeak, peak);
                        fillSine (buffer, 1.5f, 220.0f, sampleRate); // refill (processBlock overwrites in place)
                    }

                    std::string label = std::string (modeNames[modeIdx]) + " color=" + std::to_string ((int) colorVal)
                                       + " filter=" + std::to_string ((int) filterVal)
                                       + " -> finite, peak<=1.0 (peak=" + std::to_string (maxPeak) + ")";
                    check (label, allFinite && maxPeak <= 1.0f + 1.0e-4f);
                }
            }
        }
    }

    // ── Test 3: mix=0% stays close to the dry signal ──────────────────────
    std::cout << "\n--- Test 3: mix=0% reproduces the dry signal (safety ceiling is near-identity at moderate levels) ---\n";
    {
        setParam (proc, ParameterIDs::circuitMode, 0.0f);
        setParam (proc, ParameterIDs::color, 5.0f);
        setParam (proc, ParameterIDs::filterKnob, 5.0f);
        setParam (proc, ParameterIDs::mix, 0.0f);

        juce::AudioBuffer<float> dry (2, blockSize);
        fillSine (dry, 0.3f, 220.0f, sampleRate);
        juce::AudioBuffer<float> wet (dry);

        juce::MidiBuffer midi;
        // Run a few blocks to let the filters settle to steady state.
        for (int b = 0; b < 4; ++b)
        {
            wet = dry;
            proc.processBlock (wet, midi);
        }

        float maxDiff = 0.0f;
        for (int ch = 0; ch < wet.getNumChannels(); ++ch)
        {
            const auto* w = wet.getReadPointer (ch);
            const auto* d = dry.getReadPointer (ch);
            for (int s = 0; s < wet.getNumSamples(); ++s)
                maxDiff = juce::jmax (maxDiff, std::abs (w[s] - d[s]));
        }

        std::cout << "  [INFO] max sample difference at mix=0%: " << maxDiff << "\n";
        check ("mix=0% output closely matches dry signal (diff < 0.01)", maxDiff < 0.01f);
    }

    // ── Test 4: safety ceiling holds even at maximum drive ────────────────
    std::cout << "\n--- Test 4: safety ceiling under maximum drive ---\n";
    {
        setParam (proc, ParameterIDs::circuitMode, 3.0f); // Galaxy - most extreme
        setParam (proc, ParameterIDs::color, 10.0f);
        setParam (proc, ParameterIDs::filterKnob, 5.0f);
        setParam (proc, ParameterIDs::mix, 100.0f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        fillSine (buffer, 4.0f, 55.0f, sampleRate); // absurdly loud on purpose
        juce::MidiBuffer midi;

        float maxPeak = 0.0f;
        bool allFinite = true;
        for (int b = 0; b < 16; ++b)
        {
            proc.processBlock (buffer, midi);
            auto [peak, finite] = peakAndFinite (buffer);
            maxPeak = juce::jmax (maxPeak, peak);
            allFinite = allFinite && finite;
            fillSine (buffer, 4.0f, 55.0f, sampleRate);
        }

        std::cout << "  [INFO] max peak under extreme drive: " << maxPeak << "\n";
        check ("output finite under extreme drive", allFinite);
        check ("safety ceiling holds (peak <= 0.99)", maxPeak <= 0.99f);
    }

    std::cout << "\n=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}
