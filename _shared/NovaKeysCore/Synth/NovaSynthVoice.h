#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

// V1 synth layer voice (spec section 7): oscillator with basic waveforms,
// unison/detune, octave shift, filter, amp ADSR, and a pitch/filter LFO -
// a complete, self-sufficient voice (pads/basses/leads/plucks/textures on
// its own), independent of the later per-layer filter/ADSR that
// NovaInstrumentLayer (task #92) applies uniformly on top for both sample
// and synth layers alike.
enum class NovaOscWaveform { Sine, Saw, Square, Triangle, Noise };

struct NovaSynthVoiceParams
{
    NovaOscWaveform waveform = NovaOscWaveform::Saw;

    int unisonVoices = 1;            // 1-8 (spec section 7)
    float unisonDetuneCents = 12.f;
    int octaveShift = 0;             // -3..+3
    float tuneCents = 0.f;           // fine-tune offset, e.g. applied by an owning layer

    float filterCutoffHz = 8000.f;
    float filterResonance = 0.7f;    // Q
    bool filterHighPass = false;     // false = low-pass, true = high-pass (spec section 9 minimum)

    float ampAttackMs = 5.f, ampDecayMs = 200.f, ampSustain = 0.8f, ampReleaseMs = 300.f;

    float lfoRateHz = 5.f;
    float lfoToPitchCents = 0.f;     // vibrato depth
    float lfoToFilterOctaves = 0.f;  // filter cutoff modulation depth, in octaves
};

class NovaSynthVoice
{
public:
    NovaSynthVoice() = default;

    void prepare (double outputSampleRate) noexcept;
    void setParams (const NovaSynthVoiceParams& p) noexcept { params = p; filter.setResonance (juce::jmax (0.1f, p.filterResonance)); }

    void noteOn (int midiNote, float velocity01) noexcept;
    void noteOff() noexcept;
    void reset() noexcept;

    bool isActive() const noexcept { return ampEnvelope.isActive(); }

    // Renders numSamples, ADDING into outL/outR (mono voice core, no stereo
    // spread in v0.1).
    void renderAdding (float* outL, float* outR, int numSamples) noexcept;

private:
    struct UnisonOsc { double phase = 0.0; };
    static constexpr int kMaxUnison = 8;
    std::array<UnisonOsc, kMaxUnison> oscillators;

    static float oscillatorSample (double phase01, NovaOscWaveform wf, juce::Random& rng) noexcept;
    static float unisonSpreadCents (int i, int n, float totalDetuneCents) noexcept;

    NovaSynthVoiceParams params;
    int currentNote = 60;
    float currentVelocity = 1.f;
    double sampleRate = 44100.0;

    juce::ADSR ampEnvelope;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::Random noiseRng;

    double lfoPhase = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaSynthVoice)
};
