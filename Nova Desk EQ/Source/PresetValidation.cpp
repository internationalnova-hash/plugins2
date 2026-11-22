// Headless validation of the factory preset system, driving the REAL
// NovaDeskEQAudioProcessor directly. Covers everything from build spec
// section 35's "Final Acceptance Test" that's objectively (not
// subjectively/audibly) verifiable: every preset applies its exact
// values, produces finite/bounded audio with no missing parameters, has
// a unique id/name, next/prev navigation wraps correctly, and switching
// presets mid-stream doesn't click. Section 31's actual listening-based
// musicality/gain-consistency audit is NOT something this harness can
// perform - see NovaDeskFactoryPresets.cpp's comment on that.

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"
#include "Presets/NovaDeskFactoryPresets.h"
#include <juce_core/juce_core.h>
#include <iostream>
#include <cmath>
#include <set>

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    float getParam (juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        return apvts.getRawParameterValue (id)->load();
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
}

int main()
{
    const double sampleRate = 48000.0;
    const int blockSize = 512;
    const int chunkSize = 512;

    const auto& presets = getNovaDeskFactoryPresets();

    std::cout << "\n[Presets] Library shape: 26 presets, unique ids, unique names, all 7 categories present\n";
    {
        check (presets.size() == 26, "factory library has exactly 26 presets, got " + juce::String ((int) presets.size()));

        std::set<juce::String> ids, names;
        std::set<juce::String> categories;
        for (const auto& p : presets)
        {
            ids.insert (p.id);
            names.insert (p.displayName);
            categories.insert (p.category);
        }
        check (ids.size() == presets.size(), "every preset id is unique");
        check (names.size() == presets.size(), "every preset display name is unique");
        check (categories.size() == 7, "all 7 spec'd categories are present, got " + juce::String ((int) categories.size()));
        check (presets[0].id == "default_channel_eq", "preset 0 is Default Channel EQ (the plugin's startup state)");
    }

    std::cout << "\n[Presets] Every preset applies its exact values through the real APVTS\n";
    {
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        bool allExact = true;
        for (int i = 0; i < (int) presets.size() && allExact; ++i)
        {
            const auto& preset = presets[(size_t) i];
            proc.loadPreset (i);

            auto approxEq = [] (float a, float b, float tol) { return std::abs (a - b) < tol; };

            if (! approxEq (getParam (proc.apvts, ParameterIDs::egMode), (float) preset.egMode, 0.01f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::input), preset.inputDb, 0.01f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::hpfFreqIndex), (float) preset.hpfFreqIndex, 0.01f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::lowFreq), preset.lowFreqHz, 0.5f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::lowGain), preset.lowGainDb, 0.01f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::lowType), (float) preset.lowType, 0.01f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::lowMidFreq), preset.lowMidFreqHz, 0.5f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::lowMidGain), preset.lowMidGainDb, 0.01f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::lowMidQ), preset.lowMidQ, 0.01f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::highMidFreq), preset.highMidFreqHz, 0.5f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::highMidGain), preset.highMidGainDb, 0.01f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::highMidQ), preset.highMidQ, 0.01f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::highFreq), preset.highFreqHz, 0.5f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::highGain), preset.highGainDb, 0.01f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::highType), (float) preset.highType, 0.01f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::lpfFreqIndex), (float) preset.lpfFreqIndex, 0.01f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::drive), preset.driveAmount, 0.01f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::mix), preset.mixPercent, 0.1f)) allExact = false;
            if (! approxEq (getParam (proc.apvts, ParameterIDs::output), preset.outputDb, 0.01f)) allExact = false;

            if (! allExact)
                std::cout << "    mismatch on preset " << i << " (" << preset.id << ")\n";
        }
        check (allExact, "all 26 presets' values match exactly after loadPreset() through the real APVTS");
    }

    std::cout << "\n[Presets] Every preset produces finite, bounded audio on a broadband test signal\n";
    {
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        bool allFinite = true;
        for (int i = 0; i < (int) presets.size(); ++i)
        {
            proc.loadPreset (i);

            juce::AudioBuffer<float> buffer (2, 9600);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* w = buffer.getWritePointer (ch);
                for (int s = 0; s < 9600; ++s)
                {
                    // A crude multi-tone "broadband-ish" probe (40Hz, 300Hz,
                    // 2kHz, 8kHz summed) - not trying to be a mix reference,
                    // just enough spectral content to exercise every band.
                    const double t = (double) s / sampleRate;
                    w[s] = 0.15f * (float) (std::sin (2.0 * juce::MathConstants<double>::pi * 40.0 * t)
                                           + std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * t)
                                           + std::sin (2.0 * juce::MathConstants<double>::pi * 2000.0 * t)
                                           + std::sin (2.0 * juce::MathConstants<double>::pi * 8000.0 * t));
                }
            }
            processInChunks (proc, buffer, chunkSize);

            if (! isFiniteAndBounded (buffer, 10.0f))
            {
                allFinite = false;
                std::cout << "    preset " << i << " (" << presets[(size_t) i].id << ") produced non-finite or unbounded output\n";
            }
        }
        check (allFinite, "every preset produces finite, bounded output on a multi-tone probe signal");
    }

    std::cout << "\n[Presets] Next/Previous navigation wraps correctly around the full library\n";
    {
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        proc.loadPreset (0);
        proc.loadPreviousPreset(); // should wrap to the LAST preset
        check (proc.getCurrentPresetIndex() == (int) presets.size() - 1,
               "Previous from index 0 wraps to the last preset, got " + juce::String (proc.getCurrentPresetIndex()));

        proc.loadPreset ((int) presets.size() - 1);
        proc.loadNextPreset(); // should wrap to the FIRST preset
        check (proc.getCurrentPresetIndex() == 0,
               "Next from the last preset wraps to index 0, got " + juce::String (proc.getCurrentPresetIndex()));

        proc.loadPreset (5);
        proc.loadNextPreset();
        check (proc.getCurrentPresetIndex() == 6, "Next from index 5 advances to 6, got " + juce::String (proc.getCurrentPresetIndex()));
    }

    std::cout << "\n[Click-Safety] Switching presets mid-stream produces no hard discontinuity\n";
    {
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        const double toneFreq = 200.0;
        const int localBlockSize = 256;
        const int numBlocks = 60;
        std::vector<float> fullOutput;
        fullOutput.reserve ((size_t) (localBlockSize * numBlocks));

        int sampleCounter = 0;
        for (int b = 0; b < numBlocks; ++b)
        {
            // Cycle through a handful of very differently-shaped presets.
            if (b == 10) proc.loadPreset (7);  // Kick Punch (E, heavy low-mid cut, high drive)
            if (b == 25) proc.loadPreset (23); // Console Drive (all EQ flat, high drive)
            if (b == 40) proc.loadPreset (18); // Guitar Bite (LPF engaged)

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
        for (size_t i = 1; i < fullOutput.size(); ++i)
            maxJump = juce::jmax (maxJump, std::abs (fullOutput[i] - fullOutput[i - 1]));

        std::cout << "    max inter-sample jump across 3 preset switches: " << maxJump << "\n";
        check (maxJump < 0.1f, "no hard discontinuity across mid-stream preset switching, got " + juce::String (maxJump, 4));
    }

    std::cout << "\n[Presets] Selected preset index round-trips through getStateInformation/setStateInformation\n";
    {
        NovaDeskEQAudioProcessor procA;
        procA.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        procA.prepareToPlay (sampleRate, blockSize);
        procA.loadPreset (12);

        juce::MemoryBlock state;
        procA.getStateInformation (state);

        NovaDeskEQAudioProcessor procB;
        procB.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        procB.prepareToPlay (sampleRate, blockSize);
        procB.setStateInformation (state.getData(), (int) state.getSize());

        check (procB.getCurrentPresetIndex() == 12, "selected preset index (12) recalls correctly after a state round trip, got " + juce::String (procB.getCurrentPresetIndex()));

        // Values should ALSO match preset 12's own values (since procA
        // never tweaked anything after loading it).
        check (std::abs (getParam (procB.apvts, ParameterIDs::drive) - presets[12].driveAmount) < 0.01f,
               "recalled Drive value matches the selected preset's own value");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL PRESET VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
