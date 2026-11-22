/*
  Nova Voice — Offline DSP Diagnostic
  Feeds synthetic signals through the real PluginProcessor::processBlock()
  offline and reports peak/RMS level per block plus NaN/Inf detection, so
  the Morph-knob "audio disappears" report can be verified against the real
  compiled DSP rather than a hand-derived model.
*/

#include <JuceHeader.h>
#include <cmath>
#include <iostream>

#include "PluginProcessor.h"

namespace
{
    bool bufferIsFinite (const juce::AudioBuffer<float>& buf)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const auto* d = buf.getReadPointer (ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                if (! std::isfinite (d[i]))
                    return false;
        }
        return true;
    }

    float bufferPeak (const juce::AudioBuffer<float>& buf)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            peak = juce::jmax (peak, buf.getMagnitude (ch, 0, buf.getNumSamples()));
        return peak;
    }

    float bufferRms (const juce::AudioBuffer<float>& buf)
    {
        float sumSq = 0.0f;
        int count = 0;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const auto* d = buf.getReadPointer (ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                sumSq += d[i] * d[i];
                ++count;
            }
        }
        return count > 0 ? std::sqrt (sumSq / static_cast<float> (count)) : 0.0f;
    }

    // Synthesized "vowel-like" broadband signal: fundamental + harmonics,
    // similar spectral density to a sustained vocal note, to stress the
    // per-band harshness detector across many bands simultaneously (a pure
    // sine only excites one band). Only reaches ~12th harmonic (~2.6kHz for
    // a 220Hz fundamental) though, so bands above that see zero excitation -
    // use fillWhiteNoise for a true full-spectrum (30Hz-18kHz) stress test.
    void fillVowelLike (juce::AudioBuffer<float>& buf, double sampleRate, double& phase, float levelRms)
    {
        const float fundamental = 220.0f; // A3, typical vocal fundamental
        const int numHarmonics = 12;
        const int n = buf.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            float sample = 0.0f;
            for (int h = 1; h <= numHarmonics; ++h)
                sample += std::sin (static_cast<float> (phase) * h) / static_cast<float> (h);
            sample *= levelRms;
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                buf.setSample (ch, i, sample);
            phase += 2.0 * juce::MathConstants<double>::pi * fundamental / sampleRate;
            if (phase > 2.0 * juce::MathConstants<double>::pi)
                phase -= 2.0 * juce::MathConstants<double>::pi;
        }
    }

    // White noise: flat energy across the full 30Hz-18kHz band layout, so
    // every one of the 24 reduction bands sees real excitation at once -
    // the actual worst case the cascade-overlap bug depends on.
    void fillWhiteNoise (juce::AudioBuffer<float>& buf, juce::Random& random, float levelRms)
    {
        const int n = buf.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            const float sample = (random.nextFloat() * 2.0f - 1.0f) * levelRms;
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                buf.setSample (ch, i, sample);
        }
    }

    void setParams (juce::AudioProcessorValueTreeState& apvts, float morph, float texture, float form,
                     float air, float blend, int modeIndex)
    {
        apvts.getParameter ("morph")->setValueNotifyingHost (
            apvts.getParameter ("morph")->convertTo0to1 (morph));
        apvts.getParameter ("texture")->setValueNotifyingHost (
            apvts.getParameter ("texture")->convertTo0to1 (texture));
        apvts.getParameter ("form")->setValueNotifyingHost (
            apvts.getParameter ("form")->convertTo0to1 (form));
        apvts.getParameter ("air")->setValueNotifyingHost (
            apvts.getParameter ("air")->convertTo0to1 (air));
        apvts.getParameter ("blend")->setValueNotifyingHost (
            apvts.getParameter ("blend")->convertTo0to1 (blend));
        apvts.getParameter ("voice_mode")->setValueNotifyingHost (
            apvts.getParameter ("voice_mode")->convertTo0to1 (static_cast<float> (modeIndex)));
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::ScopedNoDenormals noDenormals;

    std::cout << "\n=== NOVA VOICE OFFLINE DIAGNOSTIC ===\n";

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    NovaVoiceAudioProcessor proc;
    proc.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    double phase = 0.0;

    // Hybrid mode (index 2), default Texture/Form/Air/Blend, sweep Morph 0..10,
    // hold each value for 2 seconds (~188 blocks @ 512/48000), moderate-hot
    // vowel-like signal (RMS ~0.2, comparable to a present vocal take).
    const float testLevel = 0.2f;
    const int modeIndex = 2;

    for (float morph : { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f })
    {
        setParams (proc.apvts, morph, 4.0f, 5.5f, 5.0f, 55.0f, modeIndex);

        float minPeak = 1.0f, maxPeak = 0.0f, lastRms = 0.0f;
        bool anyNonFinite = false;
        constexpr int numBlocks = 188; // ~2s
        for (int b = 0; b < numBlocks; ++b)
        {
            fillVowelLike (buffer, sampleRate, phase, testLevel);
            proc.processBlock (buffer, midi);
            if (! bufferIsFinite (buffer)) anyNonFinite = true;
            const float peak = bufferPeak (buffer);
            minPeak = juce::jmin (minPeak, peak);
            maxPeak = juce::jmax (maxPeak, peak);
            if (b == numBlocks - 1) lastRms = bufferRms (buffer);
        }

        std::cout << "Morph=" << morph
                   << "  peak(min/max)=" << minPeak << "/" << maxPeak
                   << "  finalBlockRMS=" << lastRms
                   << "  finite=" << (anyNonFinite ? "NO <-- NaN/Inf DETECTED" : "yes")
                   << "\n";
    }

    // Full-spectrum white-noise stress test - every band gets real
    // excitation simultaneously, the actual worst case for the cascade bug.
    std::cout << "\n--- White noise (full-spectrum) ---\n";
    juce::Random random (12345);
    const float noiseAmp = 0.4f;
    for (float morph : { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f })
    {
        setParams (proc.apvts, morph, 4.0f, 5.5f, 5.0f, 55.0f, modeIndex);

        float minPeak = 1.0f, maxPeak = 0.0f, lastRms = 0.0f;
        bool anyNonFinite = false;
        constexpr int numBlocks = 188;
        for (int b = 0; b < numBlocks; ++b)
        {
            fillWhiteNoise (buffer, random, noiseAmp);
            proc.processBlock (buffer, midi);
            if (! bufferIsFinite (buffer)) anyNonFinite = true;
            const float peak = bufferPeak (buffer);
            minPeak = juce::jmin (minPeak, peak);
            maxPeak = juce::jmax (maxPeak, peak);
            if (b == numBlocks - 1) lastRms = bufferRms (buffer);
        }

        std::cout << "Morph=" << morph
                   << "  peak(min/max)=" << minPeak << "/" << maxPeak
                   << "  finalBlockRMS=" << lastRms
                   << "  finite=" << (anyNonFinite ? "NO <-- NaN/Inf DETECTED" : "yes")
                   << "\n";
    }

    // Push harder: loud white noise (near clipping, like a hot mix bus) in
    // every Voice Mode, to find where this actually breaks down.
    std::cout << "\n--- Loud white noise (amp=0.85), all Voice Modes, Morph=10 ---\n";
    const char* modeNames[] = { "Clean", "Digital", "Hybrid", "Extreme" };
    for (int mi = 0; mi < 4; ++mi)
    {
        setParams (proc.apvts, 10.0f, 4.0f, 5.5f, 5.0f, 55.0f, mi);
        float minPeak = 1.0f, maxPeak = 0.0f, lastRms = 0.0f;
        bool anyNonFinite = false;
        for (int b = 0; b < 188; ++b)
        {
            fillWhiteNoise (buffer, random, 0.85f);
            proc.processBlock (buffer, midi);
            if (! bufferIsFinite (buffer)) anyNonFinite = true;
            const float peak = bufferPeak (buffer);
            minPeak = juce::jmin (minPeak, peak);
            maxPeak = juce::jmax (maxPeak, peak);
            if (b == 187) lastRms = bufferRms (buffer);
        }
        std::cout << "Mode=" << modeNames[mi]
                   << "  peak(min/max)=" << minPeak << "/" << maxPeak
                   << "  finalBlockRMS=" << lastRms
                   << "  finite=" << (anyNonFinite ? "NO <-- NaN/Inf DETECTED" : "yes")
                   << "\n";
    }

    // Also try Blend=100% (full wet) -- if wet path is heavily cut AND no
    // dry signal is mixed back in, that alone could read as "no audio" even
    // without extreme cascade behavior.
    std::cout << "\n--- Loud white noise, Extreme mode, Morph=10, Blend=100% ---\n";
    setParams (proc.apvts, 10.0f, 4.0f, 5.5f, 5.0f, 100.0f, 3);
    {
        float minPeak = 1.0f, maxPeak = 0.0f, lastRms = 0.0f;
        bool anyNonFinite = false;
        for (int b = 0; b < 188; ++b)
        {
            fillWhiteNoise (buffer, random, 0.85f);
            proc.processBlock (buffer, midi);
            if (! bufferIsFinite (buffer)) anyNonFinite = true;
            const float peak = bufferPeak (buffer);
            minPeak = juce::jmin (minPeak, peak);
            maxPeak = juce::jmax (maxPeak, peak);
            if (b == 187) lastRms = bufferRms (buffer);
        }
        std::cout << "  peak(min/max)=" << minPeak << "/" << maxPeak
                   << "  finalBlockRMS=" << lastRms
                   << "  finite=" << (anyNonFinite ? "NO <-- NaN/Inf DETECTED" : "yes")
                   << "\n";
    }

    // Recovery check: hold at Morph=10 for a while, then drop back to 0 and
    // see whether output level recovers (tests for a stuck/non-recovering
    // state, as opposed to a bounded steady-state cut).
    std::cout << "\n--- Recovery check: Morph 10 -> 0 ---\n";
    setParams (proc.apvts, 10.0f, 4.0f, 5.5f, 5.0f, 55.0f, modeIndex);
    for (int b = 0; b < 188; ++b)
    {
        fillVowelLike (buffer, sampleRate, phase, testLevel);
        proc.processBlock (buffer, midi);
    }
    std::cout << "At Morph=10, settled RMS: " << bufferRms (buffer) << "\n";

    setParams (proc.apvts, 0.0f, 4.0f, 5.5f, 5.0f, 55.0f, modeIndex);
    for (int b = 0; b < 188; ++b)
    {
        fillVowelLike (buffer, sampleRate, phase, testLevel);
        proc.processBlock (buffer, midi);
    }
    std::cout << "After dropping to Morph=0, settled RMS: " << bufferRms (buffer) << "\n";

    return 0;
}
