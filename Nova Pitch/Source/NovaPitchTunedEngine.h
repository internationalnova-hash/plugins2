#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <functional>
#include <vector>

#include "NovaVocalTuner.h"                       // _shared/NovaTuneCore - same tuner as Nova AI Vocal
#include "NovaPitchTypes.h"                       // nova::pitch::Scale
#include "DSP/NovaFormantCorrector.h"             // kept only for the Formant Warp effect
#include "DSP/NovaPitchSynchronousShifter.h"      // CaptureDebugInfo type (debug API kept for compatibility)

// Nova Pitch's engine, rebuilt on the shared NovaVocalTuner (the tuner that
// scored ahead of Neptune Lite on Nova Tune Bench). Same interface and the
// same Params fields as the retired nova::pitch::NovaPitchEngine, so the
// processor, presets, A/B snapshots and saved sessions are unchanged.
//
// Chain: tuner -> formant warp (only when Formant Warp != 0) -> dry/wet mix
// against a latency-matched dry copy -> output gain. Bypass crossfades to
// that same latency-matched dry, so reported latency never changes.
class NovaPitchTunedEngine
{
public:
    struct Params
    {
        int   keyPitchClass = 0;
        nova::pitch::Scale scale = nova::pitch::Scale::Chromatic;
        float retuneSpeedPercent = 50.0f;         // "tolerance": 0 = fast/hard, 100 = slow/natural
        float amountPercent = 0.0f;               // "Humanize": 0 = always fully correct
        float confidenceThresholdPercent = 70.0f;
        float vibratoPreservePercent = 0.0f;
        float vibratoRateHz = 5.0f;
        float formantAmountPercent = 100.0f;      // scales the Formant Warp effect
        float formantWarpPercent = 0.0f;          // -100..100
        float rangePercent = 100.0f;
        float mixPercent = 100.0f;
        float outputGainDb = 0.0f;
        float transientsPercent = 30.0f;
        bool  bypassed = false;
    };

    void prepare (double sampleRate, int maxBlockSize, bool lowLatencyMode);
    void reset() noexcept;

    // Message thread only (allocates). The processor suspends processing around it.
    void reconfigureIfNeeded (double sampleRate, bool desiredLowLatencyMode);

    void setParams (const Params& p) noexcept;
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

    float getDetectedHz() const noexcept;
    float getCorrectedHz() const noexcept;
    // Read-only views for the harmony engine and keyboard (no effect on audio)
    const NovaVocalTuner::Params& getTunerParams() const noexcept { return lastTunerParams; }
    float getInputMidi() const noexcept  { return tuner.getInputMidi(); }
    float getTargetMidi() const noexcept { return tuner.getTargetMidi(); }
    float getConfidence() const noexcept { return tuner.getInputMidi() > 0.f ? 1.f : 0.f; }
    int   getLatencySamples() const noexcept { return tuner.getLatencySamples(); }
    float getShifterRatioDebug() const noexcept { return tuner.getCurrentRatio(); }
    bool  isVoicedDebug() const noexcept { return tuner.getInputMidi() > 0.f; }

    // The new tuner has no grain-capture stage; kept so existing callers compile.
    void setGrainCaptureDebugCallback (std::function<void (const nova::pitch::NovaPitchSynchronousShifter::CaptureDebugInfo&)>) noexcept {}

private:
    void allocate (double sampleRate, int maxBlockSize, bool lowLatencyMode);

    NovaVocalTuner tuner;
    NovaVocalTuner::Params lastTunerParams;
    nova::pitch::NovaFormantCorrector warpL, warpR;

    double sr = 44100.0;
    int maxBlock = 512;
    bool lowLatency = false;

    std::vector<float> tL, tR;     // working copy (tuner runs in place)
    std::vector<float> dryL, dryR; // latency-matched dry ring
    int dryWrite = 0, drySize = 1;

    float formantAmount = 0.f, formantWarp = 0.f;
    juce::SmoothedValue<float> mixSm { 1.f }, outSm { 1.f }, activeSm { 1.f };
};
