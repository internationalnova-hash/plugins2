#pragma once

#include "NovaMasteringPlan.h"
#include "../Analysis/NovaAudioProfile.h"

// Style options (spec section 19-20).
enum class NovaMasterStyle { Clean, Warm, Punchy, Loud, Open };

// Loudness targets (spec section 19, 22).
enum class NovaLoudnessMode { Streaming, Balanced, Loud };

struct NovaMasterUserPrefs
{
    NovaMasterStyle   style     = NovaMasterStyle::Clean;
    float             intensity = 0.5f; // 0 = soft, 1 = aggressive
    NovaLoudnessMode  loudness  = NovaLoudnessMode::Streaming;
};

// Maps AudioProfile + UserPrefs -> a conservative, safety-clamped
// MasteringPlan. No reference-track blending yet (post-v0.1, spec Phase 6).
//
// STATUS: skeleton only (scaffolding pass). Real Style/Intensity/Loudness
// mapping logic and safety-range clamps land in task #84.
class NovaMasteringDecisionEngine
{
public:
    NovaMasteringDecisionEngine() = default;

    NovaMasteringPlan decide (const NovaAudioProfile& profile,
                              const NovaMasterUserPrefs& prefs) const;
};
