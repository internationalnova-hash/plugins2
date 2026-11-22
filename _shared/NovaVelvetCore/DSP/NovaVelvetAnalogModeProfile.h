#pragma once

#include <juce_core/juce_core.h>

namespace nova::velvet
{

// Clean/Tube/Vintage coloration-scale presets. Deliberately NARROWER than
// this catalog's usual analog-mode-profile shape (compare
// NovaAnalogModeProfile in NovaGlowCore, which also carries
// gainCellMemoryScale/gainCellAttackSpeedScale - i.e. its Analog Mode
// subtly touches compression timing too): Nova Velvet's spec explicitly
// requires "same core compression architecture across all three modes,
// do NOT alter Ratio/Threshold between modes", so this struct structurally
// cannot carry a detector/timing/optical-cell field at all - only
// coloration-amount scales exist here, enforcing the requirement by
// construction rather than by convention/discipline.
//
// tube() is the reference/default baseline described throughout the spec
// (all scales = 1.0, i.e. NovaVelvetCalibration's own baseline constants
// apply unmodified). Block-rate-smoothed on mode change via lerp(), same
// idiom as NovaAnalogModeProfile.
struct NovaVelvetAnalogModeProfile
{
    float transformerDriveScale = 1.0f;
    float tubeEvenHarmonicBiasScale = 1.0f;
    float tubeDriveScale = 1.0f;
    float bandwidthSofteningHzScale = 1.0f; // <1 lowers the HF-soften cutoff = more softening

    static NovaVelvetAnalogModeProfile clean() noexcept
    {
        NovaVelvetAnalogModeProfile p;
        p.transformerDriveScale = 0.30f;
        p.tubeEvenHarmonicBiasScale = 0.40f;
        p.tubeDriveScale = 0.50f;
        p.bandwidthSofteningHzScale = 1.3f;
        return p;
    }

    static NovaVelvetAnalogModeProfile tube() noexcept
    {
        return NovaVelvetAnalogModeProfile {}; // all defaults - the reference baseline
    }

    static NovaVelvetAnalogModeProfile vintage() noexcept
    {
        NovaVelvetAnalogModeProfile p;
        p.transformerDriveScale = 1.5f;
        p.tubeEvenHarmonicBiasScale = 1.4f;
        p.tubeDriveScale = 1.3f;
        p.bandwidthSofteningHzScale = 0.75f;
        return p;
    }

    static NovaVelvetAnalogModeProfile forModeIndex (int index) noexcept
    {
        switch (index)
        {
            case 0: return clean();
            case 2: return vintage();
            default: return tube();
        }
    }

    static NovaVelvetAnalogModeProfile lerp (const NovaVelvetAnalogModeProfile& a, const NovaVelvetAnalogModeProfile& b, float t) noexcept
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        NovaVelvetAnalogModeProfile r;
        r.transformerDriveScale = juce::jmap (t, a.transformerDriveScale, b.transformerDriveScale);
        r.tubeEvenHarmonicBiasScale = juce::jmap (t, a.tubeEvenHarmonicBiasScale, b.tubeEvenHarmonicBiasScale);
        r.tubeDriveScale = juce::jmap (t, a.tubeDriveScale, b.tubeDriveScale);
        r.bandwidthSofteningHzScale = juce::jmap (t, a.bandwidthSofteningHzScale, b.bandwidthSofteningHzScale);
        return r;
    }
};

} // namespace nova::velvet
