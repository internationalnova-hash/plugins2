#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "DSP/NovaOversampledColorStage.h"
#include "DSP/NovaSevenEQ.h"
#include "DSP/NovaVUBallistics.h"
#include "ABSnapshot.h"
#include "NovaLicenseGate.h"

// Full v0.1 signal chain (NOVA_7EVEN_PLAN.md section 5.1, locked):
//   In -> Input Trim -> [Transformer + Class-A, oversampled together]
//      -> HPF -> Low EQ -> Mid EQ -> 12k High Shelf
//      -> Mix (dry/wet) -> Out Trim -> Phase Invert -> Bypass -> Out
// Note: section 5.1's diagram places Out Trim AFTER Mix; section 5.8's
// prose says "pre-mix-blend-with-dry" - those two parts of the plan
// contradict each other. Followed the diagram (Out Trim is a final
// master-level control over the whole blended signal, matching how an
// "Output Trim" knob normally behaves) - see PluginProcessor.cpp comment
// at the mix/trim ordering for the full reasoning.
class NovaSevenAudioProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    // Nova license: one-time key for this plugin, or Nova All Access
    NovaLicenseGate licenseGate { *this, "nova-7even", "Nova 7even" };

    NovaSevenAudioProcessor();
    ~NovaSevenAudioProcessor() override;

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

    // VU meter telemetry (never an APVTS param - not automatable, plan
    // section 5.10 / 3.6 convention). Value is in VU units (0 = 0VU =
    // -18dBFS, plan section 5.7).
    std::atomic<float> vuDisplayLevel { 0.0f };

    ABSnapshot abSnapshot;

    // Rebuilds colorStage's oversampler if oversampling_mode has changed
    // since it was last applied. Runs on the MESSAGE thread only (via the
    // 250ms timer below, or called directly by tests) - NEVER from
    // processBlock(), since NovaOversampledColorStage::prepare() heap-
    // allocates a new juce::dsp::Oversampling, which would be a real-time-
    // safety violation there. Public so tests can trigger a
    // reconfiguration deterministically without needing a running message
    // loop (mirrors Nova Desk EQ's proven pattern).
    void reconfigureOversamplingIfNeeded();

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override { reconfigureOversamplingIfNeeded(); }

    nova::seven::NovaOversampledColorStage colorStage;
    nova::seven::NovaSevenEQ eq;
    nova::seven::NovaVUBallistics vuBallistics;

    juce::AudioBuffer<float> dryBuffer;

    double currentSampleRate { 44100.0 };
    int lastAppliedOversamplingStages { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaSevenAudioProcessor)
};
