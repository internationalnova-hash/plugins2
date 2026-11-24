#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>

#include "NovaVocalTuner.h"
#include "DSP/NovaFormantCorrector.h"

// Nova Pitch harmony engine. Runs beside the tuning engine and never touches
// it: the processor mixes this engine's output on top of the tuned lead.
//
// Two layers:
//  - Parts (up to 4): real harmony notes. Each part is its own NovaVocalTuner
//    fed the same dry input as the lead tuner, so it lands on the same note
//    with exactly the same latency, then shifted by an in-key interval
//    (Scale mode) or locked onto a held MIDI note (MIDI mode). Formants are
//    preserved with an LPC pre/post corrector so shifted parts don't sound
//    chipmunky.
//  - Voice stack (up to 32): cheap doubles of the parts - short, slowly
//    modulated delays (natural timing + a few cents of drift), spread across
//    the stereo field. Voice v belongs to part (v % activeParts).
class NovaPitchHarmony
{
public:
    static constexpr int kParts = 4;
    static constexpr int kMaxVoices = 32;
    static constexpr int kNumIntervals = 12;

    struct Params
    {
        bool  on = false;
        bool  midiMode = false;
        NovaVocalTuner::Params lead;            // the lead tuner's settings (key, scale, speed...)
        int   keyPc = 0;
        int   stepMask = 0xAB5;                 // scale used to step intervals (relative to keyPc)
        std::array<bool,  kParts> partOn       { { true, true, false, false } };
        std::array<int,   kParts> interval     { { 2, 4, 11, 0 } };
        std::array<float, kParts> partGainDb   { { -4.f, -6.f, -8.f, -6.f } };
        int   voices = 8;
        float width = 1.f;        // 0..2
        float humanize = 0.3f;    // 0..1
        float leadGainDb = 0.f;
        float mix = 0.7f;         // 0..1
        float outGainDb = 0.f;    // the plugin's Output knob
    };

    void prepare (double sampleRate, int maxBlockSize, bool lowLatency, bool tracking = false);
    void reset() noexcept;
    void setParams (const Params& p) noexcept { params = p; }

    // MIDI mode: held notes in the order they were pressed (audio thread).
    void noteOn (int note) noexcept;
    void noteOff (int note) noexcept;
    void allNotesOff() noexcept { numHeld = 0; }

    // dryL/dryR: the same input the lead tuner got. outL/outR: the tuning
    // engine's output, mixed into in place (outR may equal outL for mono).
    // leadTargetMidi: the note the lead is snapping to (0 = unvoiced).
    void process (const float* dryL, const float* dryR, float* outL, float* outR,
                  int numSamples, float leadTargetMidi) noexcept;

    // True while the engine is doing anything (on, or fading out).
    bool isActive() const noexcept { return active; }

    // UI
    float getPartMidi (int p) const noexcept { return partMidiMeter[(size_t) p].load (std::memory_order_relaxed); }
    float getVoiceLevel (int v) const noexcept { return voiceMeter[(size_t) v].load (std::memory_order_relaxed); }
    int   getActiveVoiceCount() const noexcept { return activeVoiceMeter.load (std::memory_order_relaxed); }

    // Semitone shift for an interval choice from a note (exposed for tests).
    static int intervalShift (int note, int intervalIndex, int keyPc, int stepMask) noexcept;

private:
    struct Part
    {
        NovaVocalTuner tuner;
        nova::pitch::NovaFormantCorrector formant;      // whitens the live input
        nova::pitch::NovaFormantCorrector formantPost;  // re-colours, measured on input delayed like the shifter
        std::vector<float> ring;              // shifted part, for the voice stack
        juce::SmoothedValue<float> gate { 0.f };
        float gain = 0.f;
        int shift = 0;
        bool running = false;
    };

    void resetPart (struct Part& p) noexcept;

    struct Voice
    {
        float phase = 0.f, rate = 0.3f, baseMs = 0.f, depthMs = 0.f, jitter = 1.f;
        float pan = 0.f, panL = 0.7071f, panR = 0.7071f;
        double sumSq = 0.0;
    };

    double sr = 48000.0;
    int maxBlock = 512;
    std::array<Part, kParts> parts;
    std::array<Voice, kMaxVoices> voices;
    std::vector<float> mono, work, delayed, inDelay;
    int inDelayMask = 0, inDelayWrite = 0;
    int ringSize = 1, ringMask = 0, ringWrite = 0;

    Params params;
    bool active = false;
    juce::SmoothedValue<float> onSm { 0.f }, leadSm { 1.f }, mixSm { 0.f };

    std::array<int, 16> held {};
    int numHeld = 0;
    float lastLeadNote = 0.f;
    int unvoicedSamples = 1 << 24;

    std::array<std::atomic<float>, kParts> partMidiMeter {};
    std::array<std::atomic<float>, kMaxVoices> voiceMeter {};
    std::atomic<int> activeVoiceMeter { 0 };
};
