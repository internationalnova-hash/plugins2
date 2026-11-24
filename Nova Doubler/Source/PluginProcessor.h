#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "NovaDoublerEngine.h"
#include <atomic>
#include "NovaLicenseGate.h"

// Real doubling DSP wired in (task #107): NovaDoublerEngine drives all
// four subsystems (ensemble, analyzer, MIX crossfade, bypass ramp),
// already validated standalone via the headless console-app harnesses in
// this Source/ directory (ModulatorValidation, ShiftLineValidation,
// VoiceValidation, EnsembleValidation, StereoAnalyzerValidation,
// IntegrationValidation).
class NovaDoublerProcessor : public juce::AudioProcessor
{
public:
    // Nova license: one-time key for this plugin, or Nova All Access
    NovaLicenseGate licenseGate { *this, "nova-doubler", "Nova Doubler" };

    NovaDoublerProcessor();
    ~NovaDoublerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nova Doubler"; }
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

    // Written once per block on the audio thread (processBlock), from the
    // engine's real post-mix analyzer output. Read by the editor's 30Hz
    // timer to drive the live stereo visualization - never faked/simulated.
    std::atomic<float> telemetryLeftRms { 0.0f };
    std::atomic<float> telemetryRightRms { 0.0f };
    std::atomic<float> telemetryMidRms { 0.0f };
    std::atomic<float> telemetrySideRms { 0.0f };
    std::atomic<float> telemetryCorrelation { 1.0f };
    std::atomic<float> telemetrySpread { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    double currentSampleRate = 44100.0;
    NovaDoublerEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaDoublerProcessor)
};
