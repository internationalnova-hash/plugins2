#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterIDs.hpp"
#include "NovaRideEngine.h"
#include "NovaRidePresetManager.h"
#include "NovaLicenseGate.h"

class NovaRideAudioProcessor : public juce::AudioProcessor
{
public:
    // Nova license: one-time key for this plugin, or Nova All Access
    NovaLicenseGate licenseGate { *this, "nova-ride", "Nova Ride" };

    NovaRideAudioProcessor();
    ~NovaRideAudioProcessor() override;

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

    // Live telemetry for the UI (30Hz poll from the editor's Timer - see
    // PluginEditor.cpp). Written on the audio thread at the end of each
    // processBlock, read on the message thread; std::atomic makes that safe.
    std::atomic<float> inputLevelDb { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };
    std::atomic<float> rideCorrectionDb { 0.0f };
    std::atomic<bool>  gateOpen { false };
    std::atomic<bool>  sidechainConnected { false };
    std::atomic<float> sidechainPresence { 0.0f };

    NovaRidePresetManager& getPresetManager() noexcept { return presetManager; }
    nova::ride::NovaRideEngine& getEngine() noexcept { return engine; }

    // Save-mode FileChooser target for the Write automation export.
    bool exportRideAutomation (const juce::File& destination) const;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    nova::ride::NovaRideEngine::Params paramsFromApvts() const;

    nova::ride::NovaRideEngine engine;
    NovaRidePresetManager presetManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaRideAudioProcessor)
};
