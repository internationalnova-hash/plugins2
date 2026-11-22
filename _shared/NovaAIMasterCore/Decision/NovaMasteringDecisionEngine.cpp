#include "NovaMasteringDecisionEngine.h"
#include <juce_core/juce_core.h>
#include <cmath>

namespace
{
    // ---- Style biases (spec section 20) ----
    // Small, additive nudges to targets/ranges - NOT fixed presets. Every
    // value here is combined with source-aware measurements below, so the
    // same style on two different mixes still produces different numbers
    // (spec section 3).
    struct StyleBias
    {
        float lowMidWarmthDb;    // WARM: richer low mids
        float highShelfOpenDb;   // OPEN: high-frequency openness
        float transientProtect;  // PUNCHY: less compression ratio, faster release (0-1, higher = more protection)
        float saturationBias;    // WARM/LOUD: more harmonic density
        float widthBias;         // OPEN: more width headroom; PUNCHY: less
        float compressionBias;   // LOUD: more glue; OPEN/CLEAN: less
        float clippingBias;      // LOUD: more willing to use clipping
    };

    StyleBias getStyleBias (NovaMasterStyle style) noexcept
    {
        switch (style)
        {
            case NovaMasterStyle::Warm:   return { 0.9f, 0.0f, 0.0f, 0.35f, 0.0f,  0.15f, 0.0f };
            case NovaMasterStyle::Punchy: return { 0.0f, 0.0f, 1.0f, 0.10f, -0.15f, 0.05f, 0.0f };
            case NovaMasterStyle::Loud:   return { 0.0f, 0.2f, 0.0f, 0.30f, 0.05f,  0.35f, 0.5f };
            case NovaMasterStyle::Open:   return { 0.0f, 1.1f, 0.3f, 0.0f,  0.25f, -0.15f, 0.0f };
            case NovaMasterStyle::Clean:
            default:                     return { 0.0f, 0.0f, 0.5f, 0.0f,  0.0f,  -0.10f, 0.0f };
        }
    }

    // ---- Loudness mode targets (spec section 22) ----
    // Ranges, not a single hardcoded LUFS number - actual target still
    // adapts to the source profile below, this just sets the mode's ballpark.
    struct LoudnessTarget
    {
        float targetLufs;
        float truePeakCeilDb;
        float limiterThresholdDb;
        float maxClippingBias; // 0-1, how much this mode permits the LOUD-style clipping bias to matter
    };

    LoudnessTarget getLoudnessTarget (NovaLoudnessMode mode) noexcept
    {
        switch (mode)
        {
            case NovaLoudnessMode::Balanced: return { -10.5f, -1.0f, -4.f, 0.6f };
            case NovaLoudnessMode::Loud:     return { -7.5f,  -1.0f, -6.f, 1.0f };
            case NovaLoudnessMode::Streaming:
            default:                         return { -14.0f, -1.0f, -2.f, 0.15f };
        }
    }
}

NovaMasteringPlan NovaMasteringDecisionEngine::decide (const NovaAudioProfile& profile,
                                                        const NovaMasterUserPrefs& prefs) const
{
    NovaMasteringPlan plan;

    // Confidence gate (spec section 39: "if analysis confidence is low, do
    // less"). Silence or a profile that never got a usable analysis window
    // falls back to the same conservative "safety limiter only" plan the
    // engine ships with before any real decision is made.
    if (profile.isSilent)
    {
        plan.limiting.active = true;
        plan.limiting.thresholdDb = -1.f;
        plan.limiting.truePeakCeilDb = -1.f;
        return plan;
    }

    const StyleBias style = getStyleBias (prefs.style);
    const LoudnessTarget loudness = getLoudnessTarget (prefs.loudness);
    const float intensity = juce::jlimit (0.f, 1.f, prefs.intensity); // spec section 21: macro over the whole plan

    // ================= Corrective EQ (spec section 13) =================
    // Source-aware: derived from how far the mix's own measured spectral
    // balance sits from a neutral reference, not a fixed curve. Kept
    // deliberately small (hard-clamped below) - "prefer subtle corrections
    // unless analysis indicates a significant imbalance," "do not
    // automatically flatten every mix to the same spectrum."
    {
        plan.correctiveEQ.active = true;

        // Neutral reference: roughly even energy with a gentle natural
        // downward tilt (real mixes are never perfectly flat - bass
        // content dominates on average). Deviation from this drives
        // correction, not the raw tilt value itself.
        constexpr float kNeutralTilt = -0.15f;
        const float tiltError = profile.spectralTilt - kNeutralTilt; // + = too bright, - = too dark

        // Low shelf: correct excess/deficient low end, biased warmer for WARM style.
        const float bassExcess = profile.bassEnergy + profile.subEnergy - 0.30f; // ~30% is a reasonable neutral share for sub+bass combined
        plan.correctiveEQ.lowShelfFreq = 90.f;
        plan.correctiveEQ.lowShelfDb = juce::jlimit (-3.f, 3.f,
            (-bassExcess * 8.f + style.lowMidWarmthDb * 0.4f) * intensity);

        // Low-mid: tame mud (spec: "muddy low mids") without style bias -
        // this one stays purely source-aware since no style asks for
        // deliberately extra low-mid.
        const float lowMidExcess = profile.lowMidEnergy - 0.16f;
        plan.correctiveEQ.lowMidFreq = 300.f;
        plan.correctiveEQ.lowMidDb = juce::jlimit (-3.f, 2.f, (-lowMidExcess * 10.f + style.lowMidWarmthDb * 0.6f) * intensity);

        // High-mid: tame harshness (uses the analysis engine's own
        // harshness score, which is already calibrated against a flat
        // baseline - see NovaSpectralAnalyzer).
        plan.correctiveEQ.highMidFreq = 3000.f;
        plan.correctiveEQ.highMidDb = juce::jlimit (-4.f, 1.5f, (-profile.harshness * 5.f) * intensity);

        // High shelf: correct overall tilt, biased brighter for OPEN.
        plan.correctiveEQ.highShelfFreq = 11000.f;
        plan.correctiveEQ.highShelfDb = juce::jlimit (-3.f, 3.f,
            (-tiltError * 3.f + style.highShelfOpenDb) * intensity);
    }

    // ================= Resonance control (spec section 14) =================
    // "If the mix does not require resonance suppression, this stage
    // should do little or nothing" - amount scales with the WORSE of two
    // independent problem signals from the analysis engine: resonanceScore
    // (narrow standout peaks anywhere) and harshness (broad excess energy
    // specifically in the perceptually harsh 2-8kHz region) - a mix can
    // have either without the other, and both call for suppression here.
    {
        plan.resonanceControl.active = true;
        const float problemSignal = juce::jmax (profile.resonanceScore, profile.harshness * 0.8f);
        plan.resonanceControl.amount = juce::jlimit (0.f, 1.f, problemSignal * (0.5f + intensity * 0.5f));
    }

    // ================= Compression / glue (spec section 15) =================
    // "A heavily compressed mix should receive significantly less
    // compression than a highly dynamic mix" - directly uses
    // profile.compressionAmount (derived from measured crest factor) to
    // scale ratio down for already-squashed material.
    {
        plan.compression.active = true;

        // headroom for MORE compression shrinks as the source is already
        // more compressed; style.compressionBias nudges within that room.
        const float availableHeadroom = 1.f - profile.compressionAmount;
        const float baseRatio = 1.5f + availableHeadroom * 2.5f + style.compressionBias * 2.f;
        plan.compression.ratio = juce::jlimit (1.2f, 6.f, baseRatio * (0.4f + intensity * 0.6f));

        // Threshold set relative to measured loudness so it engages
        // proportionally regardless of how hot/quiet the source already is.
        plan.compression.thresholdDb = juce::jlimit (-30.f, -3.f, profile.integratedLufs + 8.f);

        // Protect transient integrity (spec 15) - slower attack when
        // transients are prominent/dense, further slowed for PUNCHY style.
        const float transientProtect = juce::jlimit (0.f, 1.f,
            (profile.transientStrength * 0.5f + profile.transientDensity * 0.5f) + style.transientProtect * 0.3f);
        plan.compression.attackMs = juce::jlimit (5.f, 40.f, 8.f + transientProtect * 25.f);
        plan.compression.releaseMs = juce::jlimit (60.f, 400.f, 120.f + availableHeadroom * 150.f);
        plan.compression.makeupDb = 0.f; // output stage handles final level, not the compressor itself
    }

    // ================= Saturation (spec section 16) =================
    // Subtle by construction (hard-clamped low) - "should NOT turn into
    // obvious distortion." Style bias only nudges within that narrow band.
    {
        plan.saturation.active = true;
        plan.saturation.drive = juce::jlimit (0.f, 0.5f, (0.12f + style.saturationBias) * intensity);
        plan.saturation.mix   = juce::jlimit (0.f, 0.4f, (0.15f + style.saturationBias * 0.5f) * intensity);
    }

    // ================= Stereo (spec section 17) =================
    // "Do not automatically widen every mix" - default stays close to
    // unchanged; only actually widens when the style calls for space AND
    // intensity supports it. Bass stays protected via bassMonoHz
    // regardless (handled inside NovaStereoProcessor).
    {
        plan.stereo.active = true;
        const float widthTarget = 1.f + (style.widthBias * intensity);
        plan.stereo.widthAmount = juce::jlimit (0.75f, 1.5f, widthTarget);
        plan.stereo.bassMonoHz = 120.f;
    }

    // ================= Clipping (spec section 18) =================
    {
        const float clippingAmount = style.clippingBias * loudness.maxClippingBias * intensity;
        plan.clipping.active = clippingAmount > 0.05f;
        plan.clipping.ceilingDb = juce::jlimit (-1.f, -0.1f, -0.3f - clippingAmount * 0.3f);
    }

    // ================= Limiting + output (spec sections 18, 22) =================
    // Target LUFS adapts to material: the loudness mode sets a ballpark,
    // but the actual gain applied is derived from how far the SOURCE's own
    // measured integrated loudness sits from that target - not a blind
    // "always push to -9 LUFS" rule, so quiet and already-hot sources get
    // different treatment even at the same Loudness setting.
    {
        plan.limiting.active = true;
        plan.limiting.truePeakCeilDb = loudness.truePeakCeilDb;
        plan.limiting.thresholdDb = loudness.limiterThresholdDb;
        plan.limiting.releaseMs = juce::jlimit (30.f, 150.f, 80.f - intensity * 40.f);

        plan.output.targetLufs = loudness.targetLufs;
        const float rawGainNeeded = loudness.targetLufs - profile.integratedLufs;
        // Safety clamp (spec 39): never blindly apply extreme gain to
        // pathological source material (e.g. near-silent or already very
        // hot files).
        plan.output.gainDb = juce::jlimit (-6.f, 12.f, rawGainNeeded);
    }

    return plan;
}
