#pragma once

#include <juce_core/juce_core.h>

// A NovaBendPreset is a plain aggregate - every field maps 1:1 to a locked
// APVTS parameter (see ParameterIDs.hpp). The factory library
// (NovaBendFactoryPresets.cpp) is a flat array of these structs; applying
// one is a uniform loop over its fields (NovaBendPresetManager::applyToApvts),
// not plugin-specific conditional DSP.
struct NovaBendPreset
{
    juce::String id;          // stable, lowercase-with-underscores identifier
    juce::String displayName; // shown in the preset browser
    juce::String category;    // "Init", "Character", "Extreme", "Formant", "Flatten", "User"

    float pitchSemitones = 0.0f;      // -12..12
    float formantSemitones = 0.0f;    // -12..12
    bool  flatten = false;
    int   flattenNoteIndex = 0;       // 0=C .. 11=B
    int   flattenOctaveIndex = 2;     // choice index; actual octave = index + 1
    bool  link = false;
    float mixPercent = 100.0f;        // 0..100
    int   qualityIndex = 1;           // 0=Live, 1=Studio
    bool  bypass = false;
};
