#pragma once

// Plain parameter struct for NovaLevelDSP.
// No APVTS, no ValueTree, no UI dependencies.
//
// Field names and value ranges match the original Nova Level APVTS parameters
// exactly so the translation layer in each plugin processor is trivial.
struct NovaLevelParameters
{
    float compression = 0.0f;   // raw APVTS value 0.0 – 10.0 (not pre-divided)
    float outputDb    = 0.0f;   // -12.0 to +12.0 dB
    int   mode        = 0;      // 0 = SMOOTH, 1 = PUNCH, 2 = LIMIT
    float magic       = 0.0f;   // 0.0 = off, > 0.5 = on (matches bool param as float)
};
