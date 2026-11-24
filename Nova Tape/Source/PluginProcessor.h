#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "NovaTapeEngine.h"
#include "ABSnapshot.h"
#include "NovaTapePresetManager.h"
#include <atomic>
#include "NovaLicenseGate.h"

class NovaTapeAudioProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    // Nova license: one-time key for this plugin, or Nova All Access
    NovaLicenseGate licenseGate { *this, "nova-tape", "Nova Tape" };

    NovaTapeAudioProcessor();
    ~NovaTapeAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nova Tape"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    ABSnapshot abSnapshot;
    NovaTapePresetManager presetManager;

    int saveCurrentAsPreset (const juce::String& name) { return presetManager.saveCurrentAsUserPreset (name, apvts); }

    // Real (not decorative) telemetry for the UI - written once per block
    // on the audio thread, read by the editor's timer.
    std::atomic<float> inputLevelDebug { 0.0f };
    std::atomic<float> outputLevelDebug { 0.0f };
    std::atomic<float> reelSpeedDebug { 0.0f }; // 0=idle, 1=nominal transport speed, drives reel rotation rate

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override;

    nova::tape::NovaTapeEngine engine;

    double currentSampleRate { 44100.0 };
    int lastAppliedOversamplingStages { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaTapeAudioProcessor)
};
