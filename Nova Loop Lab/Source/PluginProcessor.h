#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LoopLabCore.h"

struct FlipStyle
{
    const char* name;
    int mode;          // 0 transients, 1 grid, 2 musical
    float sensitivity; // 0..100
    int grid;          // 0 = 1/4, 1 = 1/8, 2 = 1/16
    int bars;          // 0 = 1, 1 = 2, 2 = 4
    float variation, swing, stutter;
    enum Fx { none, lofi, reverseAlternate, slowed } fx;
};

/** Factory flip styles, also exposed to hosts as programs. */
const std::vector<FlipStyle>& flipStyles();

/** Parameter IDs (bypass excluded). */
const juce::StringArray& loopLabParamIds();

class NovaLoopLabAudioProcessor : public juce::AudioProcessor,
                                  private juce::Timer
{
public:
    NovaLoopLabAudioProcessor();
    ~NovaLoopLabAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioParameterBool* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;
    ll::LoopLabCore core;
    std::atomic<int> currentProgram { 0 };

    // ── UI <-> audio ──
    std::atomic<int> auditionChop { -1 };   // UI pad click (consumed by the audio thread)
    std::atomic<bool> auditionLoop { false }; // play the loop while the host is stopped
    std::atomic<float> playhead { -1.0f };  // 0..1 through the loop, -1 when silent
    std::atomic<int> playingPattern { 0 };
    std::atomic<float> hostBpm { 0.0f };
    std::atomic<bool> hostPlaying { false };
    std::atomic<float> outL { 0 }, outR { 0 };
    static float take (std::atomic<float>& a) { return a.exchange (0.0f); }

    // Applies a flip style's per-chop effects (lo-fi, reverse, slowed).
    void applyStyleFx (int styleIndex);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override; // params / tempo -> core spec
    void syncSpecFromParams (bool force);

    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<int> pendingStyleFx { -1 };
    double currentSampleRate = 48000.0;

    // Last values pushed to the core, so the timer only rebuilds on change.
    struct Pushed { int mode = -1, grid = -1, bars = -1; float sens = -1, variation = -1, stutter = -1, swing = -1; double bpm = 0, sr = 0; } pushed;

    // Audio-thread state.
    struct Voice { int chop = -1; int pos = 0; float gain = 0; int fade = 0; };
    std::array<Voice, 8> voices {};
    int curPattern = 0, prevPattern = 0, patternXfade = 0;
    const ll::Model* lastModel = nullptr;
    const ll::Model* xfadeModel = nullptr;
    int modelXfade = 0;
    double freePos = 0.0;
    float loopGain = 0.0f;
    int midiPattern = -1, gateNotes = 0;
    juce::LinearSmoothedValue<float> outGain;

    void startVoice (int chop, float velocity);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaLoopLabAudioProcessor)
};
