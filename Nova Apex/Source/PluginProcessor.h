#pragma once

#include <array>
#include <atomic>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaApexAudioProcessor : public juce::AudioProcessor
{
public:
    NovaApexAudioProcessor();
    ~NovaApexAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Thread-safe metering atomics
    std::atomic<float> inputLevelL  { 0.0f };
    std::atomic<float> inputLevelR  { 0.0f };
    std::atomic<float> outputLevelL { 0.0f };
    std::atomic<float> outputLevelR { 0.0f };
    std::atomic<float> gainReductionDb { 0.0f };

    float getLUFS() const noexcept     { return lufsEstimate.load(); }
    float getTruePeak() const noexcept { return truePeakEstimate.load(); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Per-sample limiter
    float processSampleLimiter (float sample, int channel,
                                float ceilingLinear, float inputGainLinear,
                                float attackCoeff, float releaseCoeff,
                                int style, bool link) noexcept;

    void resetState() noexcept;

    double currentSampleRate { 44100.0 };
    int    currentBlockSize  { 512 };

    // Lookahead circular buffer (per channel)
    static constexpr int kMaxLookaheadSamples = 8192;
    std::array<std::vector<float>, 2> lookaheadBuf;
    std::array<int, 2>                lookaheadWritePos { 0, 0 };
    int lookaheadDelaySamples { 0 };

    // Gain envelope (per channel)
    std::array<float, 2> gainEnv { 1.0f, 1.0f };

    // LUFS / true-peak accumulators
    std::atomic<float> lufsEstimate   { -70.0f };
    std::atomic<float> truePeakEstimate { 0.0f };

    float rmsAccum { 0.0f };
    int   rmsCount { 0 };
    float tpHold   { 0.0f };

    // Peak hold for meters (decay)
    float inputPeakHoldL  { 0.0f }, inputPeakHoldR  { 0.0f };
    float outputPeakHoldL { 0.0f }, outputPeakHoldR { 0.0f };

    // Dither state
    std::array<float, 2> ditherState { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaApexAudioProcessor)
};
