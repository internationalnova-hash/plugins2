#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

class NovaMotionFXProcessor : public juce::AudioProcessor
{
public:
    NovaMotionFXProcessor();
    ~NovaMotionFXProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nova Motion FX"; }
    bool  acceptsMidi()  const override { return false; }
    bool  producesMidi() const override { return false; }
    bool  isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int  getNumPrograms()  override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> peakL { 0.f }, peakR { 0.f };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // DSP modules
    juce::dsp::StateVariableTPTFilter<float> filterL, filterR;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL, delayLineR;
    juce::dsp::Reverb reverbL, reverbR;

    double currentSR { 44100.0 };
    int    maxDelaySamples { 0 };

    // Smoothed parameter values
    juce::SmoothedValue<float> smCutoff, smResonance, smDrive;
    juce::SmoothedValue<float> smFeedback, smDelayMix;
    juce::SmoothedValue<float> smSize, smDecay, smReverbMix;
    juce::SmoothedValue<float> smInput, smOutput, smMix;

    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMotionFXProcessor)
};
