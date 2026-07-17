#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Level/NovaLevelDSP.h"

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

    // Meter accessors — delegate to shared DSP engine
    float getOutputPeakLevel()    const noexcept { return levelDSP.getOutputPeak(); }
    float getGainReductionLevel() const noexcept { return levelDSP.getGainReductionDb(); }
    bool  getOutputIsHot()        const noexcept { return levelDSP.isOutputHot(); }

    // Legacy atomics kept for backwards-compat with existing editor code
    std::atomic<float> outputPeakLevel { 0.0f };
    std::atomic<float> gainReductionLevel { 0.0f };
    std::atomic<bool>  outputIsHot { false };

    void applyPreset(const juce::String& presetName);

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Shared DSP engine — no APVTS coupling
    NovaLevelDSP levelDSP;

    juce::AudioBuffer<float> dryBuffer;
};
