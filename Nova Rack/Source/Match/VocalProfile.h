#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace nr::match
{

/** What Nova Match measures about a vocal. Everything is level-independent,
    so a quiet reference and a loud take compare fairly. */
struct VocalProfile
{
    static constexpr int kBands = 24;                 // 1/3-octave bands, 80 Hz .. 16 kHz
    static float bandCentreHz (int band) noexcept;

    bool  valid = false;
    float activeSeconds = 0.f;

    std::array<float, kBands> spectrumDb {};          // tone curve, normalised to 0 dB average (250 Hz-4 kHz)
    float rangeDb = 0.f;          // loudness range: p90 - p10 of 100 ms levels while singing
    float crestDb = 0.f;          // peak-to-RMS of 100 ms windows (compression)
    float densityDb = 0.f;        // peak-to-RMS of 10 ms windows (saturation / limiting)
    float sibilanceDb = 0.f;      // how far the loudest S's stick out above normal (5-10 kHz vs 300 Hz-3 kHz)
    float tuningCents = 0.f;      // median distance from the nearest semitone (tuning tightness)
    float widthDb = -60.f;        // side vs mid energy (-60 = mono)
    float earlyTailDb = -60.f;    // level 50-150 ms after phrase ends (reverb fills this, a delay doesn't)
    float tailDb = -60.f;         // level 120-300 ms after phrase ends, vs the phrase (reverb amount)
    float tailSlope = 0.f;        // dB per second over 120-450 ms after phrase ends (reverb length)
    float echoDb = 0.f;           // strongest repeat 120-900 ms after phrase ends (delay amount)
    float echoMs = 0.f;           // where that repeat is (delay time)
    int   phraseEnds = 0;
};

/** Measures a vocal (any channel count; uses up to the first 40 s). */
VocalProfile analyseVocal (const juce::AudioBuffer<float>& audio, double sampleRate);

/** Nova Match's stages, in chain order. */
enum Stage { Tuning, Tone, Dynamics, Air, Warmth, Width, Delay, Space, kNumStages };
const char* stageName (int stage) noexcept;       // "Tone (EQ)", ...

/** 0..100 per stage: how close `test` is to `ref`. */
std::array<float, kNumStages> stageScores (const VocalProfile& ref, const VocalProfile& test) noexcept;
/** Weighted overall match, 0..100. */
float overallScore (const std::array<float, kNumStages>& scores) noexcept;
/** Distance used by the search for one stage (0 = identical). */
float stageDistance (int stage, const VocalProfile& ref, const VocalProfile& test) noexcept;

} // namespace nr::match
