#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "NovaBendTunedEngine.h" // shared NovaTuneCore tracker/shifter
#include "Presets/NovaBendPresetManager.h"
#include "NovaLicenseGate.h"

class NovaBendAudioProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    // Nova license: one-time key for this plugin, or Nova All Access
    NovaLicenseGate licenseGate { *this, "nova-bend", "Nova Bend" };

    NovaBendAudioProcessor();
    ~NovaBendAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    // See Nova Pitch's own hard-won lesson (this engine shares the same
    // phase-accumulator/WSOLA-history state shape): without this override,
    // shifter state silently accumulates across an entire session
    // regardless of host transport position, producing different output
    // for byte-identical content depending on unrelated prior playback.
    void reset() override { engine.reset(); }

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

    // Factory preset browser. loadPreset() applies a preset's values
    // through the same setValueNotifyingHost() path automation uses - a
    // preset is just APVTS values, not a separate engine. Public (like
    // apvts above) so PluginEditor's native-function bindings can query
    // presetManager directly for listPresets.
    NovaBendPresetManager presetManager;
    void loadPreset (int index) { presetManager.applyToApvts (index, apvts); }
    void loadNextPreset() { presetManager.selectNext (apvts); }
    void loadPreviousPreset() { presetManager.selectPrevious (apvts); }
    int getCurrentPresetIndex() const noexcept { return presetManager.getCurrentIndex(); }
    int saveCurrentAsPreset (const juce::String& name) { return presetManager.saveCurrentAsUserPreset (name, apvts); }

    float getDetectedHz() const noexcept { return detectedHz.load(); }
    float getEffectivePitchSemitonesDebug() const noexcept { return effectivePitchSemitonesDebug.load(); }

    // Real (not decorative) level history for the mockup's waveform visual -
    // one RMS value per processed block, from the ACTUAL output audio.
    // Ring buffer; index 0 is oldest. Matches Nova Pitch's own proven
    // pattern - spec explicitly forbids fake/decorative telemetry.
    static constexpr int kLevelHistorySize = 96;
    float getLevelHistoryAt (int indexFromOldest) const noexcept;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override;

    NovaBendTunedEngine engine;

    double currentSampleRate { 44100.0 };
    bool   lastAppliedHighQuality { true };

    std::atomic<float> detectedHz { 0.0f };
    std::atomic<float> effectivePitchSemitonesDebug { 0.0f };

    std::array<std::atomic<float>, kLevelHistorySize> levelHistory {};
    std::atomic<int> levelHistoryWrite { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaBendAudioProcessor)
};
