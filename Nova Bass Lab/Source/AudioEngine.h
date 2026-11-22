#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <array>

#include "NovaBassEngine.h"
#include "Vault/NovaBassVault.h"
#include "Preset/NovaBassPresetManager.h"
#include "Generate/NovaBassGenerator.h"
#include "Mutate/NovaBassMutator.h"
#include "Lifecycle/NovaBassCandidateIndex.h"
#include "Render/NovaBassRenderer.h"

// Owns Nova Bass Lab's live audition path: real-time MIDI/on-screen-key
// input driving the SAME NovaBassEngine the customer plugin uses (no
// second DSP implementation). Unlike Nova AI Master Standalone's preview
// (a fixed offline-rendered buffer played through AudioTransportSource),
// this is genuinely live - closer to how PluginProcessor::processBlock
// drives its engine than to Standalone's offline-preview pattern.
//
// Threading: `engineLock` is held for the audio callback's entire per-
// block work (matches NovaKeysProcessor's `getEngineLock()` precedent -
// a released voice can hold state a live parameter/preset change is about
// to replace) and briefly by every message-thread setter below. Contention
// is negligible at typical block durations.
//
// Lab always drives the engine with sourceType::Synth - the deep pages
// this app exposes (Synth/Color/Stereo) only cover the synth voice path;
// a sample-zone editor is out of v0.1-B's scope.
class NovaBassLabAudioEngine : public juce::AudioIODeviceCallback,
                                public juce::MidiInputCallback
{
public:
    NovaBassLabAudioEngine();
    ~NovaBassLabAudioEngine() override;

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                            float* const* outputChannelData, int numOutputChannels,
                                            int numSamples, const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // MidiInputCallback
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

    // On-screen keyboard (message thread) -> audio thread, via a small
    // pending-note queue (same pattern as Nova Keys' injectNoteFromUI).
    void injectNoteFromUI (int midiNote, bool isOn);

    // ---- Deep parameter editing (message thread) ----
    void setVoiceParams (const nova::bass::NovaBassVoiceParams& params);
    nova::bass::NovaBassVoiceParams getVoiceParams() const;

    void setColorMode (nova::bass::NovaColorMode mode);
    void setColorDrive (float drive01);
    nova::bass::NovaColorMode getColorMode() const;
    float getColorDrive() const;

    void setStereoParams (const nova::bass::NovaBassStereoParams& params);
    nova::bass::NovaBassStereoParams getStereoParams() const;

    // Replaces the entire current sound (used by Generate/Mutate, and when
    // loading a candidate back from the lifecycle index) - always leaves
    // macros EMPTY, since Lab's deep pages edit raw params directly and a
    // populated macro table would otherwise get resolved on top of them at
    // the engine's default 0.5 knob positions. Only the Macro Mapping
    // Editor writes into macros, immediately before "Save as Preset".
    void loadSound (const nova::bass::NovaBassPreset& sound);
    nova::bass::NovaBassPreset getCurrentSound() const;

    void setMacroTable (const nova::bass::NovaMacroTable& table);

    nova::bass::NovaBassVault& getVault() noexcept { return vault; }
    nova::bass::NovaBassPresetManager& getPresetManager() noexcept { return presetManager; }
    nova::basslab::NovaBassCandidateIndex& getCandidateIndex() noexcept { return candidateIndex; }

    juce::String getCurrentCandidateId() const noexcept { return currentCandidateId; }

    // ---- Generate / Mutate (message thread) ----
    // Loads the result live and registers it in the candidate index.
    // Returns the new candidate id.
    juce::String generateNew (const juce::String& profileId);
    juce::String mutateCurrent (nova::basslab::NovaMutateIntensity intensity);

    // ---- Lifecycle ----
    bool setCurrentLifecycleStatus (nova::basslab::NovaCandidateStatus status);

    // ---- Render ----
    // Renders the CURRENT sound (headless, via renderEngine - never
    // touches liveEngine) to a WAV in the vault's NovaGenerated samples
    // folder, appends a provenance manifest entry, and records the
    // asset path against the current candidate. Returns the written
    // file's vault-relative name on success, empty string on failure.
    juce::String renderCurrentToVault (const nova::basslab::NovaBassRenderOptions& options);

    // ---- Save as Nova Bass preset ----
    // Builds a NovaBassPreset from the current sound + the given meta
    // (name/category/tags/description) and the current macro table
    // (as set via setMacroTable), then saves it via the SAME
    // NovaBassPresetManager class the customer plugin reads - zero
    // reprogramming to load, by construction.
    bool saveCurrentAsPreset (const nova::bass::NovaBassPresetMeta& meta);

private:
    juce::AudioDeviceManager deviceManager;
    nova::bass::NovaBassEngine liveEngine;
    nova::bass::NovaBassEngine renderEngine;
    juce::SpinLock engineLock;

    nova::bass::NovaBassPreset currentSound;

    nova::bass::NovaBassVault vault;
    nova::bass::NovaBassPresetManager presetManager;
    nova::basslab::NovaBassCandidateIndex candidateIndex;
    juce::String currentCandidateId;
    juce::Random rng;

    struct PendingUiNote { int note; bool isOn; };
    static constexpr int kMaxPendingUiNotes = 32;
    juce::SpinLock uiNoteLock;
    std::array<PendingUiNote, kMaxPendingUiNotes> pendingUiNotes;
    int pendingUiNoteCount = 0;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaBassLabAudioEngine)
};
