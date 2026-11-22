#pragma once

#include <juce_core/juce_core.h>

namespace nova::bass
{

// Classic 808-style pitch-drop envelope: on note-on, the oscillator's pitch
// starts offset by some amount above (or below) its target note and decays
// back to zero offset over a settable time. This is NOT a pitch shifter -
// it drives the oscillator's frequency directly (see NovaBassVoiceEngine),
// which is the correct mechanism for a synthesized attack transient (a
// delay-line-based shifter like NovaMicroShiftLine is built for a different
// problem: slow, small, ongoing modulation of an already-sounding signal).
//
// Sample-rate independence: the decay coefficient is derived from a time
// constant via exp(-1/(tau*sampleRate)), not a fixed per-sample multiplier,
// so behavior is consistent across 44.1/48/88.2/96kHz (same convention used
// throughout this catalog, e.g. NovaOrganicModulator, Nova One's widener).
class NovaPitchEnvelope
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        updateCoefficient();
    }

    void reset() noexcept
    {
        currentOffsetSemitones = 0.0f;
    }

    // amountSemitones: offset applied at note-on (positive = pitch starts
    // above target and falls; negative = starts below and rises).
    // decayTimeSeconds: time constant of the exponential decay to zero.
    // curveShape: 0 = pure exponential, 1 = more linear-feeling decay
    // (implemented as a power-shaped remap of the exponential progress).
    void setParameters (float amountSemitones, float decayTimeSeconds, float curveShapeIn) noexcept
    {
        startOffsetSemitones = amountSemitones;
        decayTimeSeconds = juce::jmax (0.0005f, decayTimeSeconds);
        curveShape = juce::jlimit (0.0f, 1.0f, curveShapeIn);
        updateCoefficient();
    }

    void noteOn() noexcept
    {
        currentOffsetSemitones = startOffsetSemitones;
        envelopePosition01 = 0.0f;
    }

    // Advances the envelope by one sample and returns the current pitch
    // offset in semitones (0 once fully settled). Realtime-safe.
    float getNextSample() noexcept
    {
        if (std::abs (currentOffsetSemitones) < 0.001f)
        {
            currentOffsetSemitones = 0.0f;
            return 0.0f;
        }

        const float result = currentOffsetSemitones;

        // Exponential decay toward zero.
        currentOffsetSemitones *= decayCoefficient;

        // curveShape blends toward a shape whose *perceived* fall feels more
        // linear by tracking a secondary linear-progress ramp and crossfading
        // the reported value between the raw exponential value and a
        // linearly-interpolated one at the same elapsed-time position.
        if (curveShape > 0.0f)
        {
            envelopePosition01 = juce::jmin (1.0f, envelopePosition01 + linearStep);
            const float linearValue = startOffsetSemitones * (1.0f - envelopePosition01);
            currentOffsetSemitones = juce::jmap (curveShape, currentOffsetSemitones, linearValue);
        }

        return result;
    }

    bool isActive() const noexcept { return std::abs (currentOffsetSemitones) >= 0.001f; }

private:
    void updateCoefficient() noexcept
    {
        if (sampleRate > 0.0)
        {
            decayCoefficient = std::exp (-1.0f / (decayTimeSeconds * (float) sampleRate));
            linearStep = 1.0f / (decayTimeSeconds * (float) sampleRate);
        }
    }

    double sampleRate = 44100.0;
    float startOffsetSemitones = 0.0f;
    float decayTimeSeconds = 0.25f;
    float curveShape = 0.0f;
    float decayCoefficient = 0.999f;
    float linearStep = 0.0001f;

    float currentOffsetSemitones = 0.0f;
    float envelopePosition01 = 0.0f;
};

} // namespace nova::bass
