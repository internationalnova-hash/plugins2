#pragma once

#include "../Voice/NovaBassVoiceParams.h"
#include "../Color/NovaColorEngine.h"
#include "../Macro/NovaMacroMap.h"
#include <juce_core/juce_core.h>

namespace nova::bass
{

enum class NovaBassSourceType { Synth, Sample, Hybrid };

struct NovaBassStereoParams
{
    float widthAmount = 0.5f;
    float crossoverHz = 150.0f;
};

struct NovaBassPresetMeta
{
    juce::String name;
    juce::String author = "Nova Audio";
    juce::String category;
    juce::StringArray tags;
    juce::String description;
    juce::String artworkRef;
    bool favorite = false;
};

// Plain, data-driven preset format (matching this catalog's established
// "do not hardcode factory presets in C++" convention, same shape as
// Nova Keys' NovaPreset). One NovaBassPreset fully describes a bass
// patch: the deep voice/color/stereo parameters Nova Bass Lab authors,
// plus the macro table that determines what the customer's six simple
// knobs actually do for this specific sound. NovaBassPresetManager
// (de)serializes this to/from *.novabasspreset JSON files; nothing here
// does file I/O or DSP itself.
struct NovaBassPreset
{
    NovaBassPresetMeta meta;

    NovaBassSourceType sourceType = NovaBassSourceType::Synth;
    juce::String sampleRef; // vault-relative, resolved via NovaBassVault - used when sourceType != Synth

    NovaBassVoiceParams voiceParams;
    NovaColorMode colorMode = NovaColorMode::Warm;
    float colorDrive = 0.3f;
    NovaBassStereoParams stereoParams;

    NovaMacroTable macros;

    float outputVolumeDb = 0.f;
};

} // namespace nova::bass
