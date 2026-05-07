#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaAuraAudioProcessor : public juce::AudioProcessor
{
public:
    NovaAuraAudioProcessor();
    ~NovaAuraAudioProcessor() override;

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
    std::atomic<float> auraIntensity { 0.0f };
    std::atomic<float> harshnessAmount { 0.0f };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void resetState() noexcept;

    std::array<float, 2> presenceLpLo { 0.0f, 0.0f };
    std::array<float, 2> presenceLpHi { 0.0f, 0.0f };
    std::array<float, 2> airLp { 0.0f, 0.0f };
    std::array<float, 2> harshEnv { 0.0f, 0.0f };
    std::array<float, 2> transientFast { 0.0f, 0.0f };
    std::array<float, 2> transientSlow { 0.0f, 0.0f };
    std::array<float, 2> outputSmooth { 0.0f, 0.0f };

    juce::dsp::Oversampling<float> oversampler2x { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };
    juce::dsp::Oversampling<float> oversampler4x { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };

    double currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaAuraAudioProcessor)
};
