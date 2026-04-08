#pragma once
#include <JuceHeader.h>

class NovaLevelAudioProcessor : public juce::AudioProcessor {
public:
    NovaLevelAudioProcessor();
    ~NovaLevelAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
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
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> outputPeakLevel { 0.0f };
    std::atomic<float> gainReductionLevel { 0.0f };
    std::atomic<bool> outputIsHot { false };

    // Preset logic
    void applyPreset(const juce::String& presetName);

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, juce::dsp::IIR::Coefficients<float>>;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    StereoFilter toneLowShelf;
    StereoFilter toneHighShelf;
    StereoFilter topPolishFilter;
    juce::dsp::Gain<float> outputTrim;

    juce::AudioBuffer<float> dryBuffer;
};
