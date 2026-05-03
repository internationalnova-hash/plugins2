#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaHarmonyAudioProcessor : public juce::AudioProcessor
{
public:
    NovaHarmonyAudioProcessor();
    ~NovaHarmonyAudioProcessor() override;

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

    std::atomic<float> outputPeakLevel { 0.0f };
    std::atomic<bool> outputIsHot { false };

private:
    static constexpr int maxVoices = 32;
    static constexpr int maxDelaySamples = 16384;

    struct VoiceState
    {
        float phaseA { 0.0f };
        float phaseB { 0.0f };
        float gainJitter { 0.0f };
    };

    struct ToneState
    {
        float hpCoreL { 0.0f };
        float hpCoreR { 0.0f };
        float hpSupportL { 0.0f };
        float hpSupportR { 0.0f };
        float hpTextureL { 0.0f };
        float hpTextureR { 0.0f };
    };

    using Filter = juce::dsp::IIR::Filter<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, juce::dsp::IIR::Coefficients<float>>;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateKeyDetection (float monoSample) noexcept;
    float getDetectedKeyNote() const noexcept;
    int getActiveVoiceCount (int selectedVoices, int styleIndex, bool lowLatencyOn) const noexcept;

    StereoFilter toneLowMidCut;
    StereoFilter tonePresence;
    StereoFilter toneHighShelf;
    StereoFilter mudCut;

    juce::AudioBuffer<float> delayBuffer;
    int delayWritePosition { 0 };

    std::array<VoiceState, maxVoices> voiceStates;
    ToneState toneState;

    juce::AudioBuffer<float> dryBuffer;

    double currentSampleRate { 44100.0 };
    int64_t sampleCounter { 0 };
    float inputEnvelope { 0.0f };
    int zeroCrossings { 0 };
    int keyWindowSamples { 0 };
    float previousDetectorSample { 0.0f };
    float detectedKeyNote { 0.0f };
    float peakHold { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaHarmonyAudioProcessor)
};
