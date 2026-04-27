#pragma once

#include <array>
#include <atomic>
#include <deque>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaCleanV2AudioProcessor : public juce::AudioProcessor
{
public:
    NovaCleanV2AudioProcessor();
    ~NovaCleanV2AudioProcessor() override;

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

    float getInputMeterL() const noexcept { return inputMeterL.load(); }
    float getInputMeterR() const noexcept { return inputMeterR.load(); }
    float getOutputMeterL() const noexcept { return outputMeterL.load(); }
    float getOutputMeterR() const noexcept { return outputMeterR.load(); }
    float getRemovedAmountNorm() const noexcept { return removedAmountNorm.load(); }
    int getClicksRemovedCount() const noexcept { return clicksRemovedCount.load(); }

private:
    enum Mode : int { Vocal = 0, Digital, Crackle };
    enum ClickSize : int { Micro = 0, Short, Medium };
    enum FrequencyFocus : int { High = 0, Mid, Full };
    enum InterpolationMode : int { Basic = 0, Smart };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct ChannelState
    {
        // Sidechain HPF (3 kHz) state for click detection.
        float hpPrevX = 0.0f;
        float hpPrevY = 0.0f;

        // Lookahead queues (raw + filtered) used to detect before playback.
        std::deque<float> rawLookAhead;
        std::deque<float> hpLookAhead;

        // Previous delayed raw samples for boundary-safe spline anchors.
        float prevRaw1 = 0.0f;
        float prevRaw2 = 0.0f;

        // Previous delayed HP sidechain samples for discontinuity metrics.
        float prevHp1 = 0.0f;
        float prevHp2 = 0.0f;

        // Active repair segment state.
        bool  repairActive = false;
        int   repairTotal = 0;
        int   repairIndex = 0;
        float repairP0 = 0.0f;
        float repairP1 = 0.0f;
        float repairP2 = 0.0f;
        float repairP3 = 0.0f;
    };

    std::array<ChannelState, 2> channelStates;
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> removedBuffer;
    std::array<std::vector<float>, 2> listenRemovedDryDelay;
    std::array<int, 2> listenRemovedDryWriteIndex { 0, 0 };
    int listenRemovedDelayBufferSize { 0 };
    int listenRemovedAlignSamples { 0 };
    int lookAheadSamples { 0 };
    float detectHpAlpha { 0.0f };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> listenRemovedSmoothed;

    std::atomic<float> inputMeterL { 0.0f };
    std::atomic<float> inputMeterR { 0.0f };
    std::atomic<float> outputMeterL { 0.0f };
    std::atomic<float> outputMeterR { 0.0f };
    std::atomic<float> removedAmountNorm { 0.0f };
    std::atomic<int> clicksRemovedCount { 0 };

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaCleanV2AudioProcessor)
};
