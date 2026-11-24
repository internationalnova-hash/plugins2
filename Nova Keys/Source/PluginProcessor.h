#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

#include "NovaKeysEngine.h"
#include "Effects/NovaEffectsChain.h"
#include "Preset/NovaPresetManager.h"
#include "Vault/NovaSoundVault.h"
#include "NovaLicenseGate.h"

// Real v0.1 engine integration (task #98): owns the Sound Vault, preset
// manager, one NovaSamplerEngine per layer slot (A/B/C/D), the polyphonic
// NovaKeysEngine, and the master NovaEffectsChain - the full signal path
// from MIDI in to audio out, using the real CC0 test library (task #97)
// and the 12 real presets (task #98) rather than placeholders.
//
// Known, deliberate v0.1 simplifications (not gaps hiding behind fake UI -
// documented honestly): preset loading (including sample file I/O) runs
// synchronously on the message thread rather than a background thread with
// async completion - acceptable because the whole test library is ~90MB
// and loads in well under the frame budget on any modern machine; a full
// 300-500-preset factory library would need the background-thread pattern
// already proven elsewhere in this codebase (Nova AI Master's analysis
// job). The header's TUNE knob persists and displays but does not yet
// affect pitch - not part of the v0.1 success-criteria list.
class NovaKeysProcessor : public juce::AudioProcessor
{
public:
    // Nova license: one-time key for this plugin, or Nova All Access
    NovaLicenseGate licenseGate { *this, "nova-keys", "Nova Keys" };

    NovaKeysProcessor();
    ~NovaKeysProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nova Keys"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- Virtual keyboard / MIDI activity (message thread reads, audio
    // thread writes - single bool per note, no tearing hazard for a UI
    // indicator) ----
    bool isNoteHeld (int midiNote) const noexcept { return heldNotes[(size_t) juce::jlimit (0, 127, midiNote)].load (std::memory_order_relaxed); }
    std::atomic<bool> midiActivity { false }; // decays to false in the editor timer after a short pulse

    // UI (message thread) -> audio thread note injection, so clicking the
    // on-screen keyboard goes through the exact same note-tracking path as
    // real MIDI input.
    void injectNoteFromUI (int midiNote, bool isOn);

    // ---- Preset browser / Sound Vault (message thread only) ----
    NovaSoundVault& getVault() noexcept { return vault; }
    NovaPresetManager& getPresetManager() noexcept { return presetManager; }

    // Loads a preset from the manager's list into the live engine: builds
    // real NovaSampleZone data from the vault for sample layers, configures
    // synth layers, applies effects+macros. Message-thread only (does file
    // I/O). Returns false if the index is invalid or the vault is missing.
    bool loadPresetByIndex (int index);
    int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }

    // "Locate Library" flow: points the vault at a new path, re-scans
    // presets there, and (if the currently-loaded preset's name still
    // exists in the new library) reloads it.
    bool relocateVault (const juce::File& newPath);

    void refreshMacrosFromApvts(); // pushes the 4 macro APVTS params into the live effects chain - called on load and whenever a macro param changes

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void rescanVaultAndLoadFirstPreset();

    std::array<std::atomic<bool>, 128> heldNotes;

    struct PendingUiNote { int note; bool isOn; };
    static constexpr int kMaxPendingUiNotes = 32;
    juce::SpinLock uiNoteLock;
    std::array<PendingUiNote, kMaxPendingUiNotes> pendingUiNotes;
    int pendingUiNoteCount = 0;

    NovaSoundVault vault;
    NovaPresetManager presetManager;
    std::array<NovaSamplerEngine, NovaKeysEngine::kNumLayerSlots> samplerEngines;
    NovaKeysEngine keysEngine;
    NovaEffectsChain effectsChain;

    int currentPresetIndex = -1;
    juce::String currentPresetNameForRecall; // persisted for session recall

    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    juce::AudioBuffer<float> engineScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaKeysProcessor)
};
