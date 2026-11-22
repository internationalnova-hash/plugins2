#pragma once

#include <juce_core/juce_core.h>

// A NovaVelvetPreset is a plain aggregate - every field maps 1:1 to a
// locked APVTS parameter (see ParameterIDs.hpp). The factory library
// (NovaVelvetFactoryPresets.cpp) is a flat array of these structs; applying
// one is a uniform loop over its fields, not plugin-specific conditional
// DSP.
struct NovaVelvetPreset
{
    juce::String id;
    juce::String displayName;
    juce::String category;

    float thresholdDbu = -20.0f;
    float ratio = 4.0f;
    float attackMs = 10.0f;
    float releaseMs = 300.0f;
    float gainDb = 6.0f;
    int   timeSelectIndex = 1; // 0=Fixed,1=Fix-Man,2=Manual
    float scHpfHz = 20.0f;
    float mixPercent = 100.0f;
    float outputDb = 0.0f;
    bool  stereoLink = true;
    int   analogModeIndex = 1; // 0=Clean,1=Tube,2=Vintage
    float tubeDrivePercent = 25.0f;
    bool  bypass = false;
};
