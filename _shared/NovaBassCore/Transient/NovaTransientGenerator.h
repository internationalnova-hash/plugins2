#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

namespace nova::bass
{

// Synthesizes a short artificial attack transient (filtered noise burst)
// layered onto the oscillator's output, so basses stay perceptible on small
// speakers/phones where pure sub content disappears. Nothing like this
// exists elsewhere in the catalog - built net-new from three known-good
// ingredients: a noise source, juce::dsp::StateVariableTPTFilter (the
// catalog's standard filter), and a fast juce::ADSR for the burst envelope.
class NovaTransientGenerator
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec) noexcept
    {
        sampleRate = spec.sampleRate;
        filter.prepare (spec);
        filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);

        juce::ADSR::Parameters params;
        params.attack = 0.0005f;
        params.decay = 0.05f;
        params.sustain = 0.0f;
        params.release = 0.01f;
        envelope.setParameters (params);
        envelope.setSampleRate (spec.sampleRate);
    }

    void reset() noexcept
    {
        filter.reset();
        envelope.reset();
    }

    // amount: 0-1 overall level of the transient layer.
    // toneHz: bandpass center frequency (higher = clickier, lower = knock-ier).
    // decaySeconds: how long the burst rings before settling to silence.
    void setParameters (float amount01, float toneHz, float decaySeconds) noexcept
    {
        amount = juce::jlimit (0.0f, 1.0f, amount01);
        filter.setCutoffFrequency (juce::jlimit (40.0f, 12000.0f, toneHz));
        filter.setResonance (0.5f);

        juce::ADSR::Parameters params;
        params.attack = 0.0005f;
        params.decay = juce::jmax (0.001f, decaySeconds);
        params.sustain = 0.0f;
        params.release = 0.01f;
        envelope.setParameters (params);
    }

    void noteOn() noexcept
    {
        envelope.noteOn();
    }

    void noteOff() noexcept
    {
        envelope.noteOff();
    }

    // Called once per sample; returns the transient layer's contribution
    // (add directly to the voice's summed output). Realtime-safe: the RNG
    // is a lightweight LCG advance, no allocation, no locks.
    float getNextSample() noexcept
    {
        if (amount <= 0.0f || ! envelope.isActive())
            return 0.0f;

        const float noise = nextNoiseSample();
        const float filtered = filter.processSample (0, noise);
        const float env = envelope.getNextSample();
        return filtered * env * amount;
    }

private:
    float nextNoiseSample() noexcept
    {
        // Fast, allocation-free LCG - a real RNG object (std::mt19937) is
        // unnecessary weight for a per-voice noise burst; audible quality
        // requirement here is "broadband noise", not statistical rigor.
        rngState = rngState * 1664525u + 1013904223u;
        return ((float) (rngState >> 8) / (float) (1u << 24)) * 2.0f - 1.0f;
    }

    double sampleRate = 44100.0;
    float amount = 0.0f;

    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::ADSR envelope;
    uint32_t rngState = 0x9E3779B9u;
};

} // namespace nova::bass
