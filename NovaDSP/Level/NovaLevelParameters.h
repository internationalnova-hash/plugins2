#pragma once

// Plain parameter struct for NovaLevelDSP.
// No APVTS, no ValueTree, no UI dependencies.
// Each plugin translates its own APVTS values into this struct.
struct NovaLevelParameters
{
    // 0.0 = no compression, 1.0 = maximum compression
    float compressionAmount = 0.4f;

    // 0 = Smooth, 1 = Punch, 2 = Limit (maps to mode presets)
    int mode = 0;

    // Output trim in dB (-12 to +12)
    float outputDb = 0.0f;

    // Harmonic saturation on output (tanh-based)
    bool magic = false;

    // Parallel compression: 0.0 = dry only, 1.0 = wet only
    float mix = 1.0f;
};
