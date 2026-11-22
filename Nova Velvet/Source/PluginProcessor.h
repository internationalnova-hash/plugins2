#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

#include "NovaVelvetEngine.h"
#include "Presets/NovaVelvetPresetManager.h"

// NovaVelvetEngine drives all DSP (validated standalone via the headless
// console-app harnesses in this Source/ directory - see the plan file for
// the full 9-target validation suite). The engine is stereo-only by design
// (Stereo Link needs a channel pair); a mono host bus is adapted here the
// same way this catalog's other stereo-only engines do (Glow/Royal670) -
// duplicate into a scratch stereo buffer, average back down on the way out.
//
// No message-thread reconfigure Timer here: the locked UI mockup has no
// oversampling-quality control, so oversampling stages are fixed at
// NovaVelvetCalibration::kOversamplingStagesRealtime (2x) for V1 - nothing
// changes the engine's internal buffer sizing at runtime.
class NovaVelvetAudioProcessor : public juce::AudioProcessor
{
public:
    NovaVelvetAudioProcessor();
    ~NovaVelvetAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

    void reset() override { engine.reset(); }

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
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

    NovaVelvetPresetManager presetManager;
    void loadPreset (int index) { presetManager.applyToApvts (index, apvts); }
    void loadNextPreset() { presetManager.selectNext (apvts); }
    void loadPreviousPreset() { presetManager.selectPrevious (apvts); }
    int getCurrentPresetIndex() const noexcept { return presetManager.getCurrentIndex(); }
    int saveCurrentAsPreset (const juce::String& name) { return presetManager.saveCurrentAsUserPreset (name, apvts); }

    // Real (not decorative) telemetry, written once per block on the audio
    // thread, safe to read from the message thread - same atomic-snapshot
    // pattern used throughout this catalog.
    float getGrDisplayDb() const noexcept { return telemetryGrDb.load(); }
    float getOutputDisplayVu() const noexcept { return telemetryOutputVu.load(); }
    float getInputDisplayVu() const noexcept { return telemetryInputVu.load(); }
    float getGainReductionDb() const noexcept { return telemetryGainReductionDb.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    nova::velvet::NovaVelvetEngine engine;
    juce::AudioBuffer<float> monoScratchBuffer;

    double currentSampleRate = 44100.0;

    std::atomic<float> telemetryGrDb { 0.0f };
    std::atomic<float> telemetryOutputVu { 0.0f };
    std::atomic<float> telemetryInputVu { 0.0f };
    std::atomic<float> telemetryGainReductionDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVelvetAudioProcessor)
};
