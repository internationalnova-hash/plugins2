#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// Independent reference pitch tracker (offline YIN, FFT difference function,
// median-smoothed) plus the scoring that compares a tuner's output against
// its input. Deliberately shares no code with any Nova pitch engine, so the
// bench can't inherit the same blind spots as the thing it is grading.

struct PitchFrame
{
    float midi = 0.f;      // 0 when unvoiced
    float aperiodicity = 1.f;
    float rmsDb = -120.f;
    bool  voiced = false;
};

struct PitchTrack
{
    double hopSeconds = 0.0;
    std::vector<PitchFrame> frames;
};

PitchTrack trackPitch (const float* mono, int numSamples, double sampleRate);

struct TuneScore
{
    // Pitch accuracy (settled frames, i.e. not just after a note change)
    float inTunePct       = 0.f; // |error| < 10 cents
    float medianErrCents  = 0.f;
    float warbleCents     = 0.f; // median per-held-note std dev of the output
    float wrongNotePct    = 0.f; // landed on a different note than the nearest scale note
    float octaveErrPct    = 0.f; // ...of which an octave (or more) away
    float snapMs          = 0.f; // median time to land within 15 cents after a note change

    // Sound quality
    float voicedLossPct   = 0.f; // input clearly sung, output no longer clearly pitched
    float roughness       = 0.f; // median output-minus-input aperiodicity (higher = rougher/metallic)
    float clicksPerMin    = 0.f;
    float levelDb         = 0.f; // output RMS minus input RMS

    // Timing
    float reportedLatencyMs = 0.f;
    float measuredLatencyMs = 0.f;

    int   scoredFrames = 0;
    int   noteChanges  = 0;
};

// keyPitchClass 0 = C. scaleMask: 12-bit mask of allowed pitch classes
// relative to the key (0xFFF = chromatic).
TuneScore scoreTuning (const juce::AudioBuffer<float>& input, const juce::AudioBuffer<float>& output,
                       double sampleRate, int keyPitchClass, int scaleMask,
                       int reportedLatencySamples, bool alignByMeasuredLatency, int knownLatencySamples = -1,
                       float expectShiftSemis = 0.f, int expectFixedMidi = -1);
// expectShiftSemis != 0: the output should be the input moved by that many
// semitones (pitch-shift plugins). expectFixedMidi >= 0: the output should sit
// on that one note (Flatten / monotone). Otherwise: nearest note of the scale.

// Envelope cross-correlation; returns output delay relative to input in samples.
int measureLatencySamples (const juce::AudioBuffer<float>& input, const juce::AudioBuffer<float>& output, double sampleRate);

int scaleMaskFromName (const juce::String& name); // chromatic, major, minor
