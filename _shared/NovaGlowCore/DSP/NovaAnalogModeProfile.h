#pragma once

#include <juce_core/juce_core.h>

namespace nova::glow
{

// Clean/Vintage/Hot are Nova-designed calibration personalities - NOT
// claims of three measured physical LA-2A units. Each mode is a full
// scalar parameter set (never just an EQ preset) that modifies nonlinear/
// model behavior: transformer drive baseline, tube even-harmonic bias
// baseline, noise-floor scale, and the gain cell's memory-depth/attack-
// speed baseline scale. The engine SmoothedValue-ramps between profiles on
// mode change (~30-50ms) for click-free switching - see NovaGlowEngine -
// rather than the dual-parallel-chain approach NovaStereoModeCrossfader
// uses, because these quantities are all continuously blendable scalars
// with a coherent intermediate meaning (unlike Stereo<->M/S encode bases).
struct NovaAnalogModeProfile
{
    float transformerDriveBaseline = 0.5f;
    float tubeEvenHarmonicBiasBaseline = 0.3f;
    float noiseFloorScale = 1.0f;
    float gainCellMemoryScale = 1.0f;   // multiplies the GLOW-driven memory depth
    float gainCellAttackSpeedScale = 1.0f; // >1 = slightly faster/tighter attack behavior

    static NovaAnalogModeProfile clean() noexcept
    {
        NovaAnalogModeProfile p;
        p.transformerDriveBaseline = 0.30f;
        p.tubeEvenHarmonicBiasBaseline = 0.15f;
        p.noiseFloorScale = 0.5f;
        p.gainCellMemoryScale = 0.85f;
        p.gainCellAttackSpeedScale = 1.15f;
        return p;
    }

    static NovaAnalogModeProfile vintage() noexcept
    {
        NovaAnalogModeProfile p;
        p.transformerDriveBaseline = 0.55f;
        p.tubeEvenHarmonicBiasBaseline = 0.35f;
        p.noiseFloorScale = 1.0f;
        p.gainCellMemoryScale = 1.0f;
        p.gainCellAttackSpeedScale = 1.0f;
        return p;
    }

    static NovaAnalogModeProfile hot() noexcept
    {
        NovaAnalogModeProfile p;
        p.transformerDriveBaseline = 0.80f;
        p.tubeEvenHarmonicBiasBaseline = 0.55f;
        p.noiseFloorScale = 1.6f;
        p.gainCellMemoryScale = 1.2f;
        p.gainCellAttackSpeedScale = 0.85f; // slightly softer transients
        return p;
    }

    static NovaAnalogModeProfile forModeIndex (int index) noexcept
    {
        switch (index)
        {
            case 0: return clean();
            case 2: return hot();
            default: return vintage();
        }
    }

    static NovaAnalogModeProfile lerp (const NovaAnalogModeProfile& a, const NovaAnalogModeProfile& b, float t) noexcept
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        NovaAnalogModeProfile r;
        r.transformerDriveBaseline = juce::jmap (t, a.transformerDriveBaseline, b.transformerDriveBaseline);
        r.tubeEvenHarmonicBiasBaseline = juce::jmap (t, a.tubeEvenHarmonicBiasBaseline, b.tubeEvenHarmonicBiasBaseline);
        r.noiseFloorScale = juce::jmap (t, a.noiseFloorScale, b.noiseFloorScale);
        r.gainCellMemoryScale = juce::jmap (t, a.gainCellMemoryScale, b.gainCellMemoryScale);
        r.gainCellAttackSpeedScale = juce::jmap (t, a.gainCellAttackSpeedScale, b.gainCellAttackSpeedScale);
        return r;
    }
};

} // namespace nova::glow
