#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaMasterAudioProcessor : public juce::AudioProcessor
{
public:
    NovaMasterAudioProcessor();
    ~NovaMasterAudioProcessor() override;

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
    std::atomic<float> outputRmsLevel { 0.0f };
    std::atomic<float> limiterReductionLevel { 0.0f };
    std::atomic<bool> outputIsHot { false };

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, juce::dsp::IIR::Coefficients<float>>;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    StereoFilter tiltLowShelf;
    StereoFilter tiltHighShelf;
    StereoFilter weightShelf;
    StereoFilter lowTightFilter;
    StereoFilter airShelf;
    StereoFilter airSmoothFilter;
    juce::dsp::Gain<float> outputTrim;
    juce::AudioBuffer<float> dryBuffer;

    double currentSampleRate { 44100.0 };
    float compressorEnvelope { 0.0f };
    float compressorGainReductionDb { 0.0f };
    float limiterGainReductionDb { 0.0f };
    float outputPeakHold { 0.0f };
    float outputRmsHold { 0.0f };
    float sideLowState { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMasterAudioProcessor)
};
