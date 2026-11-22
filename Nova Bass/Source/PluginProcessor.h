#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>

#include "ParameterIDs.hpp"
#include "NovaBassEngine.h"
#include "Vault/NovaBassVault.h"
#include "Preset/NovaBassPresetManager.h"

// Owns the full v0.1-A signal path: Sound Vault -> preset manager ->
// NovaBassEngine (which itself owns every DSP subsystem) -> audio out.
// The processor's own job is thin by design - all real DSP composition
// lives in NovaBassEngine so Nova Bass Lab (Milestone B) can drive the
// identical engine without a second copy of this wiring.
//
// Deliberate v0.1 simplification, documented honestly: preset loading
// (including any sample file I/O for Sample/Hybrid-sourced presets) runs
// synchronously on the message thread, matching the precedent set by
// NovaKeysProcessor for the same reason (a single-preset load is small and
// fast enough not to need the background-thread pattern that a large
// factory library would eventually want).
class NovaBassProcessor : public juce::AudioProcessor
{
public:
    NovaBassProcessor();
    ~NovaBassProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nova Bass"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; } // covers a long-decay 808's release tail

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- Preset browser / Sound Vault (message thread only) ----
    nova::bass::NovaBassVault& getVault() noexcept { return vault; }
    nova::bass::NovaBassPresetManager& getPresetManager() noexcept { return presetManager; }

    bool loadPresetByIndex (int index);
    int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }

    bool relocateVault (const juce::File& newPath);

    // ---- MIDI activity / telemetry for the UI (audio thread writes,
    // message thread reads via the editor's timer - single scalars, no
    // tearing hazard for a UI indicator) ----
    std::atomic<bool> midiActivity { false };
    std::atomic<bool> anyVoiceActive { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void rescanVaultAndLoadFirstPreset();
    void pushMacroAndColorParamsToEngine();

    nova::bass::NovaBassVault vault;
    nova::bass::NovaBassPresetManager presetManager;
    nova::bass::NovaBassEngine engine;
    juce::AudioFormatManager formatManager;

    int currentPresetIndex = -1;
    juce::String currentPresetNameForRecall;

    double sampleRate = 44100.0;
    int maxBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaBassProcessor)
};
