#pragma once

// Plain POD result of analyzing a stereo mix. No JUCE UI types, no APVTS.
// Produced by NovaAIMasterEngine::analyze(), consumed by the decision engine.
// Field set matches Nova AI Master spec section 7.
struct NovaAudioProfile
{
    // ---- Loudness ----
    float integratedLufs   = -70.f;  // ITU-R BS.1770 integrated loudness (gated)
    float shortTermLufs    = -70.f;  // 3s window, most recent/max
    float loudnessRangeLu  = 0.f;    // LRA, high-percentile - low-percentile short-term loudness
    float truePeakDb       = -100.f; // oversampled true peak, dBTP
    float samplePeakDb     = -100.f;
    float headroomDb       = 0.f;    // 0dBFS - truePeakDb

    // ---- Dynamics ----
    float crestFactorDb      = 0.f;  // windowed peak/RMS
    float macroDynamicRange  = 0.f;  // loud-section vs quiet-section spread
    float microDynamics      = 0.f;  // short-window RMS variance, normalized 0-1
    float transientDensity   = 0.f;  // onsets per second, normalized 0-1
    float transientStrength  = 0.f;  // average transient prominence, normalized 0-1
    float compressionAmount  = 0.f;  // 0 = highly dynamic, 1 = overly compressed (derived)

    // ---- Spectral balance (normalized energy 0-1 per band, relative to total) ----
    float subEnergy        = 0.f;   // ~20-60 Hz
    float bassEnergy       = 0.f;   // ~60-250 Hz
    float lowMidEnergy     = 0.f;   // ~250-500 Hz
    float midEnergy        = 0.f;   // ~500-2000 Hz
    float upperMidEnergy   = 0.f;   // ~2000-4000 Hz
    float presenceEnergy   = 0.f;   // ~4000-8000 Hz
    float highEnergy       = 0.f;   // ~8000-20000 Hz ("air")
    float spectralTilt     = 0.f;   // negative = dark, positive = bright
    float harshness        = 0.f;   // 0-1, persistent narrow-band energy in 2-6kHz
    float resonanceScore   = 0.f;   // 0-1, general narrow-peak prominence across spectrum

    // ---- Stereo ----
    float stereoWidth        = 0.f; // 0 = mono, 1 = wide
    float stereoCorrelation  = 1.f; // -1..1, Pearson
    float lowFrequencyWidth  = 0.f; // width specifically below ~150Hz (should usually be low)

    // ---- Meta ----
    float headroomAvailableDb = 0.f;
    bool  isMono               = false;
    bool  isSilent              = false;
    double sampleRate           = 44100.0;
    double lengthSeconds        = 0.0;
};
