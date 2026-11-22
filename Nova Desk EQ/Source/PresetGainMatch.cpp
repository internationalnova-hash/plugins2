// Offline gain-matching pass for factory presets whose Output the spec
// leaves as "Gain-match" rather than an exact number (build spec sections
// 31-33). Drives the REAL NovaDeskEQAudioProcessor for each preset,
// measures broadband RMS on pink noise against a neutral reference
// (Drive/EQ flat, Output 0dB - the spec's own "match bypass loudness"
// framing), and prints the Output trim (rounded to 0.5dB, clamped to
// +-6dB) that brings each preset back to roughly the same perceived
// level. This is a defensible starting point per section 33's own
// "listening plus appropriate level measurements" - it does not replace
// the real listening pass section 31 asks for, and is printed as a report
// for manual transcription into NovaDeskFactoryPresets.cpp, not applied
// automatically.

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"
#include "Presets/NovaDeskFactoryPresets.h"
#include <juce_core/juce_core.h>
#include <iostream>
#include <random>
#include <cmath>

namespace
{
    void setFloat (juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    juce::AudioBuffer<float> makePinkNoise (double sampleRate, int numSamples, int seed)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);
        std::mt19937 rng ((unsigned) seed);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);

        for (int ch = 0; ch < 2; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            float pinkState[7] = {};
            for (int i = 0; i < numSamples; ++i)
            {
                const float white = dist (rng);
                pinkState[0] = 0.99886f * pinkState[0] + white * 0.0555179f;
                pinkState[1] = 0.99332f * pinkState[1] + white * 0.0750759f;
                pinkState[2] = 0.96900f * pinkState[2] + white * 0.1538520f;
                const float pink = pinkState[0] + pinkState[1] + pinkState[2] + white * 0.1848f;
                w[i] = 0.25f * pink;
            }
        }
        juce::ignoreUnused (sampleRate);
        return buffer;
    }

    double measureRms (const juce::AudioBuffer<float>& buffer, int settleSamples)
    {
        double sumSq = 0.0;
        int count = 0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* r = buffer.getReadPointer (ch);
            for (int i = settleSamples; i < buffer.getNumSamples(); ++i)
            {
                sumSq += (double) r[i] * (double) r[i];
                ++count;
            }
        }
        return std::sqrt (sumSq / (double) count);
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

    double measurePresetRms (const double sampleRate, const int blockSize, const int chunkSize,
                              const NovaDeskPreset* preset, int noiseSeed)
    {
        NovaDeskEQAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        if (preset != nullptr)
        {
            for (int i = 0; i < proc.presetManager.getNumPresets(); ++i)
            {
                if (proc.presetManager.getPreset (i).id == preset->id)
                {
                    proc.loadPreset (i);
                    break;
                }
            }
            // Force Output to 0dB for measurement - we're SOLVING for it.
            setFloat (proc.apvts, ParameterIDs::output, 0.0f);
        }
        else
        {
            // Neutral reference: flat EQ, no drive, unity trims.
            setFloat (proc.apvts, ParameterIDs::input, 0.0f);
            setFloat (proc.apvts, ParameterIDs::output, 0.0f);
            setFloat (proc.apvts, ParameterIDs::drive, 0.0f);
            setFloat (proc.apvts, ParameterIDs::lowGain, 0.0f);
            setFloat (proc.apvts, ParameterIDs::lowMidGain, 0.0f);
            setFloat (proc.apvts, ParameterIDs::highMidGain, 0.0f);
            setFloat (proc.apvts, ParameterIDs::highGain, 0.0f);
            setFloat (proc.apvts, ParameterIDs::mix, 100.0f);
        }

        auto buffer = makePinkNoise (sampleRate, (int) (sampleRate * 2.0), noiseSeed);
        processInChunks (proc, buffer, chunkSize);

        return measureRms (buffer, (int) (sampleRate * 0.3)); // skip 300ms settle
    }
}

int main()
{
    const double sampleRate = 48000.0;
    const int blockSize = 512;
    const int chunkSize = 512;

    const double neutralRms = measurePresetRms (sampleRate, blockSize, chunkSize, nullptr, 1);
    std::cout << "Neutral reference RMS: " << neutralRms << "\n\n";

    std::cout << juce::String ("id").paddedRight (' ', 22)
               << juce::String ("presetDb").paddedRight (' ', 12)
               << juce::String ("suggestedOutputDb").paddedRight (' ', 20) << "\n";

    for (const auto& preset : getNovaDeskFactoryPresets())
    {
        const double presetRms = measurePresetRms (sampleRate, blockSize, chunkSize, &preset, 1);
        const double presetDb = 20.0 * std::log10 (presetRms / neutralRms);
        double suggested = -presetDb;
        suggested = juce::jlimit (-6.0, 6.0, suggested);
        suggested = std::round (suggested * 2.0) / 2.0; // nearest 0.5dB

        std::cout << juce::String (preset.id).paddedRight (' ', 22)
                   << juce::String (presetDb, 2).paddedRight (' ', 12)
                   << juce::String (suggested, 1).paddedRight (' ', 20) << "\n";
    }

    return 0;
}
