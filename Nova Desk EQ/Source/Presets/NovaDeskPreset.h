#pragma once

#include <juce_core/juce_core.h>

// Factory preset system (build spec: "Factory presets must be DATA, not
// hardcoded conditional DSP"). A NovaDeskPreset is a plain aggregate -
// every field maps 1:1 to a locked APVTS parameter (see ParameterIDs.hpp),
// no plugin-specific behavior lives here. The factory library itself
// (NovaDeskFactoryPresets.cpp) is a flat array of these structs; applying
// one is a uniform loop over its fields, not a chain of if/else.
struct NovaDeskPreset
{
    juce::String id;          // stable, lowercase-with-underscores identifier
    juce::String displayName; // shown in the preset browser
    juce::String category;    // "Utility", "Vocals", "Drums", "Bass & 808", "Instruments", "Bus", "Color"

    int egMode = 0;      // 0 = E, 1 = G

    float inputDb = 0.0f;

    int hpfFreqIndex = 0; // 0=Off,1=20,2=50,3=100,4=200,5=400
    bool hpfIn = true;

    float lowFreqHz = 100.0f;
    float lowGainDb = 0.0f;
    int lowType = 1; // 0=Bell, 1=Shelf

    float lowMidFreqHz = 400.0f;
    float lowMidGainDb = 0.0f;
    float lowMidQ = 0.8f;

    float highMidFreqHz = 2000.0f;
    float highMidGainDb = 0.0f;
    float highMidQ = 0.8f;

    float highFreqHz = 8000.0f;
    float highGainDb = 0.0f;
    int highType = 1; // 0=Bell, 1=Shelf

    int lpfFreqIndex = 0; // 0=Off,1=5k,2=8k,3=12k,4=16k,5=20k
    bool lpfIn = true;

    float driveAmount = 0.0f;
    float mixPercent = 100.0f;
    float outputDb = 0.0f;
};
