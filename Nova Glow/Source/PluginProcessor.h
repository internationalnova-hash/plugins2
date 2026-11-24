#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include "NovaGlowEngine.h"
#include "Preset/NovaGlowPresetManager.h"
#include <atomic>
#include "NovaLicenseGate.h"

// NovaGlowEngine drives all DSP (validated standalone via the headless
// console-app harnesses in this Source/ directory: DetectorValidation,
// EngineValidation). The engine is stereo-only by design (Stereo Link
// needs a channel pair); a mono host bus is adapted here the same way
// Royal 670 does - duplicate into a scratch stereo buffer, average back
// down on the way out - rather than teaching the Core engine about mono.
class NovaGlowAudioProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    // Nova license: one-time key for this plugin, or Nova All Access
    NovaLicenseGate licenseGate { *this, "nova-glow", "Nova Glow" };

    NovaGlowAudioProcessor();
    ~NovaGlowAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nova Glow"; }
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

    NovaGlowPresetManager& getPresetManager() noexcept { return presetManager; }

    // Written once per block on the audio thread, read by the editor's
    // 30Hz timer to drive the VU needle (whichever of these meterMode
    // currently selects) and the Power jewel.
    std::atomic<float> telemetryGrDb { 0.0f };
    std::atomic<float> telemetryOutputVu { 0.0f };
    std::atomic<float> telemetryInputVu { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Called from prepareToPlay() and from timerCallback() (message thread
    // only, via reconfigureOversamplingIfNeeded()) - never from inside
    // processBlock(), since reallocating the engine's Oversampling objects
    // is not RT-safe. This is the Nova 7even pattern
    // (Source/PluginProcessor.cpp's reconfigureOversamplingIfNeeded()),
    // deliberately NOT Royal 670's in-processBlock() reconfigure (which
    // reallocates on the audio thread).
    void reprepareEngine();
    void reconfigureOversamplingIfNeeded();
    void timerCallback() override { reconfigureOversamplingIfNeeded(); }

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int lastPreparedOversamplingStages = 1;

    nova::glow::NovaGlowEngine engine;
    juce::AudioBuffer<float> monoScratchBuffer;
    NovaGlowPresetManager presetManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaGlowAudioProcessor)
};
