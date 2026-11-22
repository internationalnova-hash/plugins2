#pragma once

#include <juce_core/juce_core.h>

// A NovaPunchPreset is a plain aggregate - every field maps 1:1 to a
// locked APVTS parameter (see ParameterIDs.hpp). The factory library
// (NovaPunchFactoryPresets.cpp) is a flat array of these structs; applying
// one is a uniform loop over its fields, not plugin-specific conditional
// DSP.
struct NovaPunchPreset
{
    juce::String id;
    juce::String displayName;
    juce::String category;

    float punchPercent = 50.0f;
    float bodyPercent = 50.0f;
    int   focusIndex = 0; // 0=Full,1=Low,2=Mid,3=High
    float sensitivityPercent = 65.0f;
    float mixPercent = 100.0f;
    bool  clipSafe = true;
    bool  bypass = false;
};
