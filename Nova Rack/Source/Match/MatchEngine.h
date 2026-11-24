#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <map>

#include "VocalProfile.h"

namespace nr::match
{

/** One unit of the matched chain. */
struct StageResult
{
    int stage = 0;
    juce::String plugin;                 // "Nova Desk EQ"
    juce::StringPairArray params;        // name -> value for the rack ("=0.42" = normalised)
    juce::StringPairArray display;       // name -> the plugin's own value text, for the UI
    juce::String summary;                // "HPF 110 · -3 dB @ 4k"
};

struct MatchResult
{
    bool ok = false;
    juce::String error;
    VocalProfile reference, raw, matched;
    std::array<float, kNumStages> rawScores {}, matchedScores {};
    float rawOverall = 0.f, matchedOverall = 0.f;
    std::vector<StageResult> chain;      // in rack order; stages that didn't help are left out
    std::array<bool, kNumStages> used {};
    juce::AudioBuffer<float> matchedAudio;   // the raw clip through the chain (for A/B)
    juce::String notes;
    int renders = 0;
    double seconds = 0.0;
};

/** Plugin the stage uses. */
const char* stagePlugin (int stage) noexcept;

/** Rebuilds a reference vocal's sound with Nova plugins: measures both vocals, then for each
    stage renders the user's vocal through the real plugin with candidate settings and keeps
    whatever brings it closest to the reference. */
class MatchEngine
{
public:
    using PluginFactory = std::function<std::unique_ptr<juce::AudioPluginInstance> (const juce::String& pluginName,
                                                                                     double sampleRate, int blockSize)>;
    using Progress = std::function<bool (float progress, const juce::String& status)>; // return false to cancel

    /** Message thread (plugin hosts require it). Returns the plugins that couldn't be loaded. */
    juce::StringArray preparePlugins (const PluginFactory&, double sampleRate);

    /** Blocking; run it on a background thread. `locked` stages keep the user's own sound. */
    MatchResult run (const juce::AudioBuffer<float>& reference, double referenceRate,
                     const juce::AudioBuffer<float>& raw, double rawRate,
                     const std::array<bool, kNumStages>& locked, const Progress& progress);

    /** Message thread: drop the plugin instances. */
    void releasePlugins() { plugins.clear(); }

    static constexpr double kMaxClipSeconds = 20.0;

private:
    juce::AudioBuffer<float> render (const juce::String& plugin, const juce::StringPairArray& params,
                                     const juce::AudioBuffer<float>& in);
    std::map<juce::String, std::unique_ptr<juce::AudioPluginInstance>> plugins;
    double sr = 48000.0;
    int renders = 0;
};

} // namespace nr::match
