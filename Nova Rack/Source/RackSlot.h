#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace nr
{

//==============================================================================
/** Delays a signal by a fixed number of samples (up to its capacity).
    Each rack slot runs its dry signal through one of these, set to the hosted
    plugin's latency, so the mix and bypass stay time-aligned with the wet path. */
class DryDelay
{
public:
    static constexpr int kMaxDelay = 1 << 16; // ~1.4 s at 48 kHz: well past any Nova plugin's lookahead

    void prepare (int numChannels, int maxBlock);
    void setDelay (int samples) noexcept { delay.store (juce::jlimit (0, kMaxDelay, samples)); }
    int getDelay() const noexcept { return delay.load(); }
    void reset() noexcept;

    /** out[c][i] = in[c][i - delay]. `in` and `out` may not alias. */
    void process (const float* const* in, float* const* out, int numChannels, int numSamples) noexcept;

private:
    juce::AudioBuffer<float> ring;
    int size = 0, writePos = 0;
    std::atomic<int> delay { 0 };
};

//==============================================================================
/** One rack unit: a hosted Nova plugin plus its latency-compensated mix/bypass stage.
    Created, moved and destroyed on the message thread only. */
struct Slot
{
    Slot() = default;
    ~Slot();

    juce::String pluginName;                       // e.g. "Nova Tape": the portable key saved in presets
    juce::PluginDescription description;
    std::unique_ptr<juce::AudioPluginInstance> instance;

    juce::Array<int> face;                         // hosted parameter indices shown as knobs on the module
    juce::String presetName;                       // last module preset loaded or saved ("" = none)

    // Kept when the plugin isn't installed here, so a session/preset survives a round trip.
    bool missing = false;
    juce::MemoryBlock savedState;
    juce::StringPairArray pendingParams;           // factory values waiting for the plugin to appear

    /** Called with the audio stopped. */
    void prepare (double sampleRate, int maxBlock, int numChannels);
    void release();

    /** In-place. `wetAmount` = 0 (bypassed) … 1 (fully wet). */
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, float wetAmount,
                  juce::AudioPlayHead* playHead, bool nonRealtime) noexcept;

    int getLatency() const noexcept { return dry.getDelay(); }

    std::atomic<float> outPeak { 0.0f };          // this unit's output level, for its faceplate VU
    void refreshLatency() noexcept;                // message thread: follow the plugin's reported latency

    juce::MemoryBlock getState() const;
    void setState (const juce::MemoryBlock&);

    /** First eight automatable, non-bypass parameters. */
    void chooseDefaultFace();

    // The full-size plugin window. Declared after `instance` so it is destroyed first.
    std::unique_ptr<juce::Component> window;

private:
    DryDelay dry;
    juce::AudioBuffer<float> work, dryBuf;
    juce::LinearSmoothedValue<float> wet { 1.0f };
    bool prepared = false;
};

/** Blend `wet` (in place) with `dry` using a ramped wet gain. Exposed for tests. */
void mixWetDry (juce::AudioBuffer<float>& wetBuf, const juce::AudioBuffer<float>& dryBuf,
                int numChannels, int numSamples, juce::LinearSmoothedValue<float>& wetGain) noexcept;

/** Applies "Parameter name" -> "value text" pairs (names matched case-insensitively; "=0.42"
    sets a normalised value). Returns the keys that matched no parameter. */
juce::StringArray applyParamTexts (juce::AudioPluginInstance&, const juce::StringPairArray&);

} // namespace nr
