#pragma once

#include "Voicing/NovaVoiceEnsemble.h"
#include "Analysis/NovaStereoFieldAnalyzer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// Top-level facade tying the doubling engine together: mono/stereo input
// handling, the voice ensemble (wet signal), an equal-power dry/wet MIX
// crossfade, a click-free bypass ramp, and the stereo-field analyzer run
// on the real final output. This is the only class PluginProcessor talks
// to for audio.
class NovaDoublerEngine
{
public:
    NovaDoublerEngine() = default;

    void prepare (double sampleRate) noexcept
    {
        ensemble.prepare (sampleRate, kMaxDelayMs);
        analyzer.prepare (sampleRate);

        // 20ms: fast enough that turning a knob feels immediate, slow
        // enough that no parameter step (including the 2<->4 voice
        // switch handled inside the ensemble) produces an audible zipper.
        widthSmoothed.reset (sampleRate, 0.02);
        tightnessSmoothed.reset (sampleRate, 0.02);
        mixSmoothed.reset (sampleRate, 0.02);
        // Bypass gets a longer ramp - it's an on/off toggle rather than a
        // continuously-automated knob, so a slightly longer crossfade
        // reads as intentional rather than abrupt.
        bypassSmoothed.reset (sampleRate, 0.03);

        widthSmoothed.setCurrentAndTargetValue (0.5f);
        tightnessSmoothed.setCurrentAndTargetValue (0.5f);
        mixSmoothed.setCurrentAndTargetValue (1.0f);
        bypassSmoothed.setCurrentAndTargetValue (1.0f); // 1 = active, 0 = bypassed

        reset();
    }

    void reset() noexcept
    {
        ensemble.reset();
        analyzer.reset();
    }

    // Percent values (0..100) as they arrive from the APVTS parameters;
    // converted to 0..1 internally. Call once per block from processBlock.
    void setParams (float widthPercent, float tightnessPercent, float mixPercent,
                     bool fourVoiceMode, bool bypassed) noexcept
    {
        widthSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, widthPercent / 100.0f));
        tightnessSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, tightnessPercent / 100.0f));
        mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, mixPercent / 100.0f));
        bypassSmoothed.setTargetValue (bypassed ? 0.0f : 1.0f);
        voicesFourMode = fourVoiceMode;
    }

    // inputChannelData/numInputChannels: the plugin's real input bus (1 for
    // mono-in, 2 for stereo-in). leftOut/rightOut: always written (2-channel
    // out). All buffers must be at least numSamples long; in-place aliasing
    // with the input is fine since inputs are read before outputs are written
    // per sample.
    void processBlock (const float* const* inputChannelData, int numInputChannels,
                        float* leftOut, float* rightOut, int numSamples) noexcept
    {
        const bool stereoIn = numInputChannels >= 2;
        const float* inL = inputChannelData[0];
        const float* inR = stereoIn ? inputChannelData[1] : inputChannelData[0];

        for (int i = 0; i < numSamples; ++i)
        {
            const float width = widthSmoothed.getNextValue();
            const float tightness = tightnessSmoothed.getNextValue();
            const float mix = mixSmoothed.getNextValue();
            const float bypassMix = bypassSmoothed.getNextValue();

            ensemble.setParams (width, tightness, voicesFourMode);

            const float dryL = inL[i];
            const float dryR = inR[i];
            const float monoSource = stereoIn ? 0.5f * (dryL + dryR) : dryL;

            float wetL = 0.0f, wetR = 0.0f;
            ensemble.processSample (monoSource, wetL, wetR);

            // Equal-power MIX crossfade (spec: avoid loudness jumps across
            // the dry<->wet blend, unlike a linear crossfade).
            const float mixAngle = mix * juce::MathConstants<float>::halfPi;
            const float dryGain = std::cos (mixAngle);
            const float wetGain = std::sin (mixAngle);
            const float mixedL = dryL * dryGain + wetL * wetGain;
            const float mixedR = dryR * dryGain + wetR * wetGain;

            // Bypass ramp: linear crossfade between the fully dry signal and
            // the MIX-blended signal (not the wet ensemble alone), so
            // toggling bypass never jumps the MIX setting itself.
            const float finalL = dryL * (1.0f - bypassMix) + mixedL * bypassMix;
            const float finalR = dryR * (1.0f - bypassMix) + mixedR * bypassMix;

            analyzer.processSample (finalL, finalR);

            leftOut[i] = finalL;
            rightOut[i] = finalR;
        }
    }

    const NovaStereoFieldAnalyzer& getAnalyzer() const noexcept { return analyzer; }

private:
    static constexpr float kMaxDelayMs = 40.0f; // margin above the spec's 5-35ms range

    NovaVoiceEnsemble ensemble;
    NovaStereoFieldAnalyzer analyzer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoothed, tightnessSmoothed, mixSmoothed, bypassSmoothed;
    bool voicesFourMode = false;
};
