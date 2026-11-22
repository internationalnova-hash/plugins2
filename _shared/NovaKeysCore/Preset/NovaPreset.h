#pragma once

#include "../Layer/NovaInstrumentLayer.h"
#include "../Effects/NovaEffectsChain.h"
#include <array>

// Plain, data-driven preset format (spec section 15: "Do not hardcode
// factory presets in C++. Use external preset files or another
// maintainable structured format"). One NovaPreset fully describes a
// four-layer patch - NovaPresetManager (de)serializes this to/from JSON
// .novapreset files; nothing in here does any file I/O or DSP itself.
struct NovaPresetLayer
{
    bool used = false; // whether this A/B/C/D slot is populated at all
    NovaLayerSourceType sourceType = NovaLayerSourceType::Sample;

    // For sample layers: a reference into the Sound Vault (task #96
    // resolves this into actual NovaSampleZone data) - NOT a raw absolute
    // path, so presets stay portable across machines/vault locations.
    juce::String sampleRef;

    NovaSynthVoiceParams synthParams; // used only when sourceType == Synth
    NovaLayerParams layerParams;      // shared control surface either way

    juce::String displayName; // e.g. "Grand Piano", "Warm Pad" - shown in the layer strip UI
};

struct NovaPresetMeta
{
    juce::String name;
    juce::String author = "Nova Audio";
    juce::String category;
    juce::StringArray tags;
    juce::String description;

    // Shared category/expansion-level artwork reference (spec section 11:
    // presets can reference individual, shared-category, or expansion
    // artwork - v0.1 does not require unique art per preset).
    juce::String artworkRef;

    bool favorite = false;
};

struct NovaPreset
{
    NovaPresetMeta meta;
    std::array<NovaPresetLayer, 4> layers;
    NovaEffectsParams effects;
    NovaMacroValues macros;
    float outputVolumeDb = 0.f;
};
