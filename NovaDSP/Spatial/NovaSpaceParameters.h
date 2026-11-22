#pragma once

// Plain POD parameter struct — no JUCE types, no APVTS, no UI includes.
// All fields carry the raw values read from APVTS once per block.
struct NovaSpaceParameters
{
    // Main controls (0–10 range unless noted)
    float space    = 1.8f;    // 0–10
    float air      = 3.2f;    // 0–10
    float depth    = 2.6f;    // 0–10
    float mix      = 16.0f;   // 0–100 (percent)
    float width    = 3.8f;    // 0–10

    // Mode: 0=Studio, 1=Arena, 2=Dream, 3=Vintage
    int   mode     = 0;

    // Advanced controls
    float preDelayMs = 22.0f;  // 0–120 ms
    float decay      = 2.2f;   // 0.5–6.5 s
    float damping    = 45.0f;  // 0–100 (percent)
    float early      = 35.0f;  // 0–100 (earlyReflections percent)
};
