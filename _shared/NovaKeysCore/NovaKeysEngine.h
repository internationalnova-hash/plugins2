#pragma once

#include "Layer/NovaInstrumentLayer.h"
#include <array>
#include <vector>
#include <limits>

// Top-level polyphony/voice-pool engine (spec section 8): a fixed-capacity
// pool of voices, each owning all 4 layers (A/B/C/D) so one triggered note
// plays through the whole preset. Graceful stealing prefers an already-
// released (quietest) voice over cutting something still being held, and
// falls back to oldest only when every voice is genuinely still held.
//
// Realtime-safety: the voice pool and all per-voice scratch buffers are
// sized once in prepare() (message thread). noteOn/noteOff/renderNextBlock
// never allocate - they only index into the pre-sized fixed array.
//
// Layer *templates* (configureLayerAsSample/AsSynth) are message-thread
// setup calls for now (spec's preset system, task #95, will decide how
// templates get swapped live during playback - not solved here).
class NovaKeysEngine
{
public:
    NovaKeysEngine() = default;

    static constexpr int kNumLayerSlots = 4;
    static constexpr int kMaxPossibleVoices = 128;

    void prepare (double sampleRate, int maxBlockSize, int numVoices);

    void configureLayerAsSample (int layerIndex, NovaSamplerEngine* samplerEngine, const NovaLayerParams& p);
    void configureLayerAsSynth (int layerIndex, const NovaSynthVoiceParams& synthParams, const NovaLayerParams& p);
    void setLayerUnconfigured (int layerIndex);

    // Realtime-safe: audio-thread callable.
    void noteOn (int midiNote, int velocity) noexcept;
    void noteOff (int midiNote) noexcept;
    void allNotesOff() noexcept;

    // Immediately silences every voice (bypassing release envelopes) and
    // discards every layer's zone/synth playback state. NOT for normal use
    // (it's an audible hard cut, not graceful) - this exists specifically
    // so a caller can safely swap out a layer's underlying sample-zone data
    // (e.g. switching presets) without leaving any voice holding a raw
    // pointer into zone data that's about to be replaced. Callers must
    // ensure this completes (and no concurrent renderNextBlock is in
    // flight) before mutating anything a configured NovaSamplerEngine
    // points at.
    void hardResetAllVoices() noexcept;

    void renderNextBlock (float* outL, float* outR, int numSamples) noexcept;

    int getMaxVoices() const noexcept { return activeVoiceCount; }
    int getNumActiveVoices() const noexcept;

private:
    struct Voice
    {
        std::array<NovaInstrumentLayer, kNumLayerSlots> layers;
        int currentNote = -1;
        bool held = false;
        juce::int64 startOrder = 0;
        float lastPeak = 0.f;
    };

    struct LayerTemplate
    {
        bool configured = false;
        NovaLayerSourceType type = NovaLayerSourceType::Sample;
        NovaSamplerEngine* samplerEngine = nullptr;
        NovaSynthVoiceParams synthParams;
        NovaLayerParams layerParams;
    };

    bool isVoiceSounding (const Voice& v) const noexcept;
    int findFreeOrStealVoice() noexcept;

    std::array<Voice, kMaxPossibleVoices> voices;
    std::array<LayerTemplate, kNumLayerSlots> layerTemplates;

    int activeVoiceCount = 64;
    juce::int64 nextOrder = 0;

    std::vector<float> mixScratchL, mixScratchR;

    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaKeysEngine)
};
