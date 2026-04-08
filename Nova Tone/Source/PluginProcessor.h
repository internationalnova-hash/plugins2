#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaToneAudioProcessor : public juce::AudioProcessor
{
public:
    NovaToneAudioProcessor();
    ~NovaToneAudioProcessor() override;

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
    using Filter = juce::dsp::IIR::Filter<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, juce::dsp::IIR::Coefficients<float>>;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    StereoFilter lowBoostFilter;
    StereoFilter lowAttenuationFilter;
    StereoFilter highBoostFilter;
    StereoFilter highAttenuationFilter;
    juce::dsp::Gain<float> outputTrim;

    double currentSampleRate { 44100.0 };
    float peakHold { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaToneAudioProcessor)
};
