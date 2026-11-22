#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

#include "Modulation/NovaMotionDSP.h"

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

    // Meter accessors — delegate to shared DSP engine
    float getPeakL() const noexcept { return motionDSP.getPeakL(); }
    float getPeakR() const noexcept { return motionDSP.getPeakR(); }

    // Legacy atomics kept for backwards-compat with existing editor code
    std::atomic<float> peakL { 0.f }, peakR { 0.f };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Shared DSP engine — no APVTS coupling
    NovaMotionDSP motionDSP;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMotionFXProcessor)
};
