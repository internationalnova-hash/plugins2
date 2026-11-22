#pragma once

// Plain POD parameter struct — no JUCE types, no APVTS, no UI includes.
// All fields carry the raw values read from APVTS once per block.
struct NovaMotionParameters
{
    // ---- Active DSP parameters ----
    float cutoff    = 20000.f;  // Hz, 20–20000; engine clamps to 0.4 * sampleRate
    float resonance = 0.707f;   // 0.1–20; engine clamps to 4.0 max
    float feedback  = 0.0f;     // 0–0.95
    float delayMix  = 0.0f;     // 0–1
    float reverbMix = 0.0f;     // 0–1
    float size      = 0.6f;     // 0–1 room size
    float inputDb   = 0.0f;     // -24 to +12 dB
    float outputDb  = 0.0f;     // -24 to +12 dB
    float mix       = 1.0f;     // 0–1 master wet/dry

    // ---- APVTS params not yet connected to DSP (future use) ----
    float motion    = 0.5f;     // 0–1 macro
    float drive     = 0.0f;     // 0–1
    float lfoRate   = 0.25f;    // 0.01–8 Hz
    float lfoDepth  = 0.75f;    // 0–1
    float decay     = 2.85f;    // 0.1–10 s
};
