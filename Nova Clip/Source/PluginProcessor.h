#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaClipAudioProcessor : public juce::AudioProcessor
{
public:
    NovaClipAudioProcessor();
    ~NovaClipAudioProcessor() override;

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

    std::atomic<float> inputPeakL { 0.0f };
    std::atomic<float> inputPeakR { 0.0f };
    std::atomic<float> outputPeakL { 0.0f };
    std::atomic<float> outputPeakR { 0.0f };
    std::atomic<float> clipReductionDb { 0.0f };
    std::atomic<float> heatAmount { 0.0f };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    float processClippingSample (float sample,
                                 int channel,
                                 float driveDb,
                                 float shapeNorm,
                                 float toneNorm,
                                 float punchNorm,
                                 int mode,
                                 bool safeMode,
                                 bool linkLR) noexcept;

    void resetDynamicsState() noexcept;

    juce::AudioBuffer<float> dryBuffer;
    std::array<float, 2> transientFastEnv { 0.0f, 0.0f };
    std::array<float, 2> transientSlowEnv { 0.0f, 0.0f };
    std::array<float, 2> toneLowpassState { 0.0f, 0.0f };
    std::array<float, 2> postSmoothState { 0.0f, 0.0f };
    std::array<float, 2> safeHfLowpassState { 0.0f, 0.0f };

    juce::dsp::Oversampling<float> oversampler2x { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };
    juce::dsp::Oversampling<float> oversampler4x { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };

    double currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaClipAudioProcessor)
};
