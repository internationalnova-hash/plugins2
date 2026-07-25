#pragma once

#include <atomic>
#include <array>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>

#include "EnvironmentProfiles.hpp"

// Plain-value snapshot of the current parameter state, filled by the real
// juce::AudioProcessor wrapper from APVTS each block, or set directly by the
// validation console app - the engine itself has no APVTS/editor dependency
// so it can be linked and exercised headlessly.
struct NovaTranslateParams
{
    int environmentIndex = EnvironmentLibrary::NovaMainRoom;
    float environmentAmount = 100.0f; // 0..100
    int positionIndex = 0;            // 0..3, see ParameterIDs::positionNames
    float roomAmount = 50.0f;         // 0..100
    float characterAmount = 60.0f;    // 0..100
    float noiseAmount = 0.0f;         // 0..100
    bool noiseEnabled = false;
    bool levelMatchEnabled = true;
    float outputTrimDb = 0.0f;        // -12..12
    bool monoEnabled = false;
    bool leftOnly = false;
    bool rightOnly = false;
    bool midOnly = false;
    bool sideOnly = false;
    bool dimEnabled = false;
    bool phaseInvert = false;
    bool bypass = false;
};

// One fully self-contained processing chain for a single environment
// (filters + convolution + compressor state). Two of these are kept live at
// once so environment switches can crossfade between them with zero clicks.
class EnvironmentChain
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    // Heavy work (filter coefficient calc + synthetic IR generation +
    // convolution load) - must only ever be called off the audio thread.
    void loadEnvironment (int environmentIndex, double sampleRate);

    void process (juce::dsp::AudioBlock<float>& block, const NovaTranslateParams& p,
                   double sampleRate, juce::AudioBuffer<float>& scratchBuffer);

    int getLoadedEnvironmentIndex() const noexcept { return loadedEnvironmentIndex; }
    bool isReady() const noexcept { return ready; }

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, juce::dsp::IIR::Coefficients<float>>;

    StereoFilter lowCutFilter, highCutFilter, lowShelfFilter, midPeakFilter, highShelfFilter;
    juce::dsp::Convolution convolution;

    float compressorEnvelope = 0.0f;
    float compressorGainReductionDb = 0.0f;
    float sideLowState = 0.0f;
    float positionBiasSmoothed = 0.0f; // ramps toward the target stereo-position bias, ~40ms time
                                        // constant, so Position changes (and environment switches,
                                        // which can also shift the baseline bias) never click/jump

    int loadedEnvironmentIndex = -1;
    bool ready = false;
    juce::dsp::ProcessSpec preparedSpec {};
};

// The real-time-safe DSP core. Owns two EnvironmentChains (for click-free
// crossfading), the noise generator, auto level-match, and utility routing.
// AsyncUpdater is used so environment-change work (IR synthesis, filter
// coefficient calculation) happens on the message thread, never inside
// processBlock - processBlock only ever compares an integer and posts a flag.
class NovaTranslateEngine : private juce::AsyncUpdater
{
public:
    NovaTranslateEngine();
    ~NovaTranslateEngine() override;

    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    // Real-time-safe: no allocation, no locking, no blocking calls.
    void process (juce::AudioBuffer<float>& buffer, const NovaTranslateParams& params);

    // For tests / headless use: performs the environment load synchronously
    // on the calling thread instead of going through triggerAsyncUpdate().
    // Safe to call as long as it isn't from inside process().
    void loadEnvironmentSynchronously (int environmentIndex);

    int getLatencySamples() const noexcept { return latencySamples; }

    // Telemetry (safe to read from any thread).
    std::atomic<float> inputPeakLevel { 0.0f };
    std::atomic<float> inputRmsLevel { 0.0f };
    std::atomic<float> outputPeakLevel { 0.0f };
    std::atomic<float> outputRmsLevel { 0.0f };
    std::atomic<float> stereoCorrelation { 0.0f };
    std::atomic<float> levelMatchCompensationDb { 0.0f };

private:
    void handleAsyncUpdate() override;

    std::array<EnvironmentChain, 2> chains;
    int activeChain = 0; // fully-active chain (crossfadeProgress == 0 means 100% this one)
    int fadeTargetChain = 1;
    float crossfadeProgress = 0.0f; // 0 = fully activeChain, 1 = fully fadeTargetChain
    bool crossfading = false;

    std::atomic<int> requestedEnvironmentIndex { EnvironmentLibrary::NovaMainRoom };
    int lastSeenRequestedIndex = -1;
    std::atomic<int> pendingLoadTarget { -1 };
    std::atomic<bool> pendingLoadReady { false };

    double currentSampleRate = 44100.0;
    int latencySamples = 0;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> chainAScratch, chainBScratch, scratchA, scratchB;

    // Noise layer (per-environment ambience bed).
    juce::Random noiseRandom { 424242 };
    float noiseLpStateL = 0.0f, noiseLpStateR = 0.0f;

    // Auto level match.
    float levelMatchGainDb = 0.0f;
    float dryLoudnessEnvelope = 0.0f, wetLoudnessEnvelope = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaTranslateEngine)
};
