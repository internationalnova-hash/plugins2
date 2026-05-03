#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaDelayAudioProcessor : public juce::AudioProcessor
{
public:
    NovaDelayAudioProcessor();
    ~NovaDelayAudioProcessor() override;

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

    std::atomic<float> inputPeakLevelL { 0.0f };
    std::atomic<float> inputPeakLevelR { 0.0f };
    std::atomic<float> outputPeakLevelL { 0.0f };
    std::atomic<float> outputPeakLevelR { 0.0f };
    std::atomic<float> inputPeakLevel { 0.0f };
    std::atomic<float> outputPeakLevel { 0.0f };
    std::atomic<bool> outputIsHot { false };

private:
    struct DelayChannelState
    {
        float toneLp { 0.0f };
        float hpLp { 0.0f };
        float lpOut { 0.0f };
        float antiHarshLp { 0.0f };
        float antiHarshHpLp { 0.0f };
        float lofiHold { 0.0f };
        int lofiCounter { 0 };
        float timingSmear { 0.0f };
        float clockHold { 0.0f };
        int clockCounter { 0 };
    };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    float getSyncDelayMs (int syncIndex, double bpm) const noexcept;
    float readDelaySample (int channel, float delaySamples) const noexcept;
    float processDelayPathSample (int channel, float sample, float hpHz, float lpHz, float toneHz,
                                  float feedbackNorm, bool lofiOn, float lofiAmount,
                                  float modeDarkness, float modeGrit) noexcept;
    float applySaturation (float x, float amount) const noexcept;
    double getHostBpm() const noexcept;

    juce::AudioBuffer<float> delayBuffer;
    int delayBufferSize { 0 };
    int writePosition { 0 };

    std::array<DelayChannelState, 2> delayStates {};

    juce::SmoothedValue<float> delayTimeMsSmoothed;
    juce::SmoothedValue<float> feedbackSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> toneSmoothed;
    juce::SmoothedValue<float> wowFlutterSmoothed;
    juce::SmoothedValue<float> saturationSmoothed;
    juce::SmoothedValue<float> hpSmoothed;
    juce::SmoothedValue<float> lpSmoothed;
    juce::SmoothedValue<float> duckingSmoothed;
    juce::SmoothedValue<float> freezeMixSmoothed;
    juce::SmoothedValue<float> modeBlendSmoothed;

    float wowPhase { 0.0f };
    float flutterPhase { 0.0f };
    float duckEnvelope { 0.0f };
    float inputPeakHoldL { 0.0f };
    float inputPeakHoldR { 0.0f };
    float outputPeakHoldL { 0.0f };
    float outputPeakHoldR { 0.0f };
    float inputPeakHold { 0.0f };
    float outputPeakHold { 0.0f };
    juce::Random randomGenerator;

    double currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaDelayAudioProcessor)
};
