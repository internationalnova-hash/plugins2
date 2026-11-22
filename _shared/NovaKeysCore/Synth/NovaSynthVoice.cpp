#include "NovaSynthVoice.h"
#include <cmath>

void NovaSynthVoice::prepare (double outputSampleRate) noexcept
{
    sampleRate = outputSampleRate;
    ampEnvelope.setSampleRate (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, 1u, 1u }; // filter runs per-sample, mono core
    filter.prepare (spec);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    reset();
}

void NovaSynthVoice::reset() noexcept
{
    for (auto& osc : oscillators) osc.phase = 0.0;
    lfoPhase = 0.0;
    filter.reset();
    ampEnvelope.reset();
}

void NovaSynthVoice::noteOn (int midiNote, float velocity01) noexcept
{
    currentNote = midiNote;
    currentVelocity = juce::jlimit (0.f, 1.f, velocity01);

    juce::ADSR::Parameters adsrParams;
    adsrParams.attack  = juce::jmax (0.0001f, params.ampAttackMs / 1000.f);
    adsrParams.decay   = juce::jmax (0.0001f, params.ampDecayMs / 1000.f);
    adsrParams.sustain = juce::jlimit (0.f, 1.f, params.ampSustain);
    adsrParams.release = juce::jmax (0.0001f, params.ampReleaseMs / 1000.f);
    ampEnvelope.setParameters (adsrParams);
    ampEnvelope.noteOn();
}

void NovaSynthVoice::noteOff() noexcept
{
    ampEnvelope.noteOff();
}

float NovaSynthVoice::unisonSpreadCents (int i, int n, float totalDetuneCents) noexcept
{
    if (n <= 1) return 0.f;
    return juce::jmap ((float) i, 0.f, (float) (n - 1), -totalDetuneCents * 0.5f, totalDetuneCents * 0.5f);
}

float NovaSynthVoice::oscillatorSample (double phase01, NovaOscWaveform wf, juce::Random& rng) noexcept
{
    switch (wf)
    {
        case NovaOscWaveform::Sine:     return (float) std::sin (2.0 * juce::MathConstants<double>::pi * phase01);
        case NovaOscWaveform::Saw:      return (float) (2.0 * phase01 - 1.0);
        case NovaOscWaveform::Square:   return phase01 < 0.5 ? 1.f : -1.f;
        case NovaOscWaveform::Triangle: return (float) (4.0 * std::abs (phase01 - 0.5) - 1.0);
        case NovaOscWaveform::Noise:    return rng.nextFloat() * 2.f - 1.f;
    }
    return 0.f;
}

void NovaSynthVoice::renderAdding (float* outL, float* outR, int numSamples) noexcept
{
    if (! ampEnvelope.isActive()) return;

    const int numUnison = juce::jlimit (1, kMaxUnison, params.unisonVoices);
    const double baseFreq = 440.0 * std::pow (2.0, ((double) currentNote + (double) params.octaveShift * 12.0 - 69.0) / 12.0);
    const double lfoIncrement = (double) params.lfoRateHz / sampleRate;

    filter.setType (params.filterHighPass ? juce::dsp::StateVariableTPTFilterType::highpass
                                           : juce::dsp::StateVariableTPTFilterType::lowpass);

    for (int i = 0; i < numSamples; ++i)
    {
        const float lfoValue = (float) std::sin (2.0 * juce::MathConstants<double>::pi * lfoPhase);
        lfoPhase += lfoIncrement;
        if (lfoPhase >= 1.0) lfoPhase -= 1.0;

        const float vibratoCents = lfoValue * params.lfoToPitchCents;

        float mixed = 0.f;
        for (int u = 0; u < numUnison; ++u)
        {
            const float cents = unisonSpreadCents (u, numUnison, params.unisonDetuneCents) + vibratoCents + params.tuneCents;
            const double freq = baseFreq * std::pow (2.0, (double) cents / 1200.0);
            const double increment = freq / sampleRate;

            mixed += oscillatorSample (oscillators[(size_t) u].phase, params.waveform, noiseRng);

            oscillators[(size_t) u].phase += increment;
            if (oscillators[(size_t) u].phase >= 1.0) oscillators[(size_t) u].phase -= 1.0;
        }
        mixed /= (float) numUnison;
        mixed *= currentVelocity;

        const float envValue = ampEnvelope.getNextSample();
        mixed *= envValue;

        const float cutoffOctaveMod = lfoValue * params.lfoToFilterOctaves;
        const float modulatedCutoff = juce::jlimit (20.f, 20000.f,
            params.filterCutoffHz * std::pow (2.f, cutoffOctaveMod));
        filter.setCutoffFrequency (modulatedCutoff);

        const float filtered = filter.processSample (0, mixed);

        outL[i] += filtered;
        outR[i] += filtered;
    }
}
