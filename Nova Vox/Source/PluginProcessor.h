#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "NovaConsoleDSP.h"
#include "dsp/NovaCurveDSP.h"
#include "NovaLevelDSP.h"
#include "NovaMotionDSP.h"
#include "NovaSpaceDSP.h"

#include "ui/Visualizer.h"

class NovaVoxAudioProcessor : public juce::AudioProcessor
{
public:
    NovaVoxAudioProcessor();
    ~NovaVoxAudioProcessor() override;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
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

    // ── Meters (read by UI timer) ──────────────────────────────────────────
    float getInputPeak()           const noexcept { return inputPeak.load (std::memory_order_relaxed); }
    float getOutputPeak()          const noexcept { return outputPeak.load (std::memory_order_relaxed); }
    float getLevelGainReduction()  const noexcept { return levelGainReduction.load (std::memory_order_relaxed); }
    float getConsoleGainReduction() const noexcept { return consoleGainReduction.load (std::memory_order_relaxed); }

    // ── Visualizer data (written audio thread, read UI thread) ─────────────
    VisualizerData vizData;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── DSP engines ────────────────────────────────────────────────────────
    NovaConsoleDSP  consoleEngine;
    NovaCurveDSP    curveEngine;
    NovaLevelDSP    levelEngine;
    NovaMotionDSP   motionEngine;
    NovaSpaceDSP    spaceEngine;

    // ── Meters ─────────────────────────────────────────────────────────────
    std::atomic<bool>  prepared            { false };
    std::atomic<float> inputPeak           { 0.0f };
    std::atomic<float> outputPeak          { 0.0f };
    std::atomic<float> levelGainReduction  { 0.0f };
    std::atomic<float> consoleGainReduction{ 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVoxAudioProcessor)
};
