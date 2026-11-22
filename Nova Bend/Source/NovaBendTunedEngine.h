#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

#include "NovaVocalTuner.h"                  // _shared/NovaTuneCore - same tracker/shifter as Nova Pitch & Nova AI Vocal
#include "DSP/NovaBendFormantProcessor.h"    // Nova Bend's own +/-12 st formant stage

// Nova Bend's engine, rebuilt on the shared NovaVocalTuner. Same interface
// and Params as the retired nova::bend::NovaBendEngine, so the processor,
// presets and saved sessions are unchanged.
//
// Chain: tuner (fixed Pitch shift, or Flatten to one note) -> formant stage
// on the shifted audio -> neutral/bypass/mix crossfades against a
// latency-matched dry copy.
//
// Formants: the delay-line shifter moves formants by exactly the applied
// shift. The formant stage runs AFTER the shift, analysing the shifted
// audio itself, and moves the envelope by (wanted - applied) semitones -
// so with Formant 0 and Link off the voice keeps its own character at any
// Pitch setting, and nothing can drift out of time between analysis and
// re-synthesis.
class NovaBendTunedEngine
{
public:
    struct Params
    {
        float pitchSemitones = 0.0f;     // -12..+12
        float formantSemitones = 0.0f;   // -12..+12
        bool  flatten = false;
        int   flattenNoteIndex = 0;      // 0 = C .. 11 = B
        int   flattenOctave = 3;
        bool  link = false;              // formants follow the pitch
        float mixPercent = 100.0f;
        bool  bypassed = false;
    };

    void prepare (double sampleRate, int maxBlockSize, bool highQuality);
    void reset() noexcept;
    void reconfigureIfNeeded (double sampleRate, bool desiredHighQuality); // message thread; processor suspends audio

    void setParams (const Params& p) noexcept;
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

    float getDetectedHz() const noexcept;
    float getEffectivePitchSemitonesDebug() const noexcept { return tuner.getCurrentShiftSemis(); }
    int   getLatencySamples() const noexcept { return tuner.getLatencySamples(); }

private:
    void allocate (double sampleRate, int maxBlockSize, bool highQuality);

    NovaVocalTuner tuner;
    nova::bend::NovaBendFormantProcessor formantL, formantR;

    double sr = 44100.0;
    int maxBlock = 512;
    bool highQ = true;

    Params params;
    std::vector<float> tL, tR;
    std::vector<float> dryL, dryR;
    int dryWrite = 0, drySize = 1;

    juce::SmoothedValue<float> mixSm { 1.f }, activeSm { 1.f }, effectSm { 0.f };
};
