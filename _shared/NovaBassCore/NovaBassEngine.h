#pragma once

#include "Voice/NovaBassVoiceEngine.h"
#include "Sampler/NovaBassSamplerEngine.h"
#include "Sampler/NovaBassSamplePlayback.h"
#include "Color/NovaColorEngine.h"
#include "Stereo/NovaBassStereoProcessor.h"
#include "Macro/NovaMacroEngine.h"
#include "Preset/NovaBassPreset.h"
#include <array>
#include <map>

namespace nova::bass
{

// Top-level facade composing every NovaBassCore subsystem into the fixed
// signal flow: MIDI -> Voice -> PitchEnv -> AmpEnv -> Filter -> Color ->
// Stereo -> Out. Shared by the customer plugin (Nova Bass) and, in
// Milestone B, Nova Bass Lab - neither owns a second, divergent DSP
// engine; Lab just drives this same class with the macro abstraction
// bypassed (talking to the raw params directly).
//
// A preset's sourceType selects which voice path(s) actually produce
// audio: Synth drives the mono/legato/glide NovaBassVoiceEngine only,
// Sample drives the polyphonic NovaBassSamplerEngine/NovaBassSamplePlayback
// pool only, Hybrid runs both simultaneously and sums them (no special-
// casing needed - both paths are already purely additive).
class NovaBassEngine
{
public:
    static constexpr int kMaxPolyVoices = 8;

    void prepare (double sampleRate, int maximumBlockSize);
    void reset() noexcept;

    // Loads a preset: sets the baseline (Lab-authored) deep params, sample
    // zones (if applicable), color mode, and stereo crossover. Does NOT
    // touch the current macro knob positions - those are the customer's
    // live UI state, independent of which preset is selected.
    void loadPreset (const NovaBassPreset& preset, const NovaBassSampleZone* sampleZones, int numSampleZones);

    // macroName: "SHAPE" | "DECAY" | "DRIVE" | "WIDTH" | "GLIDE" | "BASS".
    // Recomputes the live resolved params immediately (message-thread or
    // audio-thread safe - no allocation, just arithmetic + struct copies).
    void setMacroValue (const juce::String& macroName, float value01) noexcept;
    float getMacroValue (const juce::String& macroName) const noexcept;

    void setBypassed (bool shouldBypass) noexcept;

    // CLEAN/WARM/GRITTY is a direct customer-facing selector (spec section
    // 65), not a macro-mapped value - it defaults to the loaded preset's
    // authored colorMode but the customer can override it live without
    // that being a preset reload. The override is what actually drives
    // NovaColorEngine from this point on, until the next loadPreset().
    void setColorMode (NovaColorMode mode) noexcept;
    NovaColorMode getColorMode() const noexcept { return liveColorMode; }

    void noteOn (int midiNote, float velocity) noexcept;
    void noteOff (int midiNote) noexcept;

    // Renders numSamples starting at buffer offset 0 into a stereo buffer
    // (buffer must have >= 2 channels). Realtime-safe.
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    bool isAnyVoiceActive() const noexcept;

private:
    void recomputeResolvedTargets() noexcept;
    void applyResolvedTargetsToVoice() noexcept;

    double sampleRate = 44100.0;

    NovaBassPreset currentPreset;
    NovaColorMode liveColorMode = NovaColorMode::Warm;
    NovaBassMacroTargets baselineTargets;
    NovaBassMacroTargets resolvedTargets;
    std::map<juce::String, float> macroKnobValues01;

    NovaBassVoiceEngine monoVoice;

    NovaBassSamplerEngine samplerEngine;
    struct PolyVoiceSlot { NovaBassSamplePlayback playback; bool active = false; };
    std::array<PolyVoiceSlot, kMaxPolyVoices> polyVoices;

    NovaColorEngine colorEngine;
    NovaBassStereoProcessor stereoProcessor;

    // Nova Bass is a pure instrument (no external audio input to preserve
    // a "dry" path from) - BYPASS therefore means a click-free ramp of the
    // engine's own output to silence, not a dry/wet crossfade. Voices keep
    // running underneath so un-bypassing resumes instantly without a
    // fresh attack.
    juce::SmoothedValue<float> bypassGain { 1.0f };
    bool bypassed = false;
};

} // namespace nova::bass
