#pragma once

#include <juce_core/juce_core.h>

namespace nova::glow
{

// GLOW is the signature Nova control - a coordinator, not a saturation
// knob. Fans one value out to five sub-targets that together build tube
// harmonic density, transformer contribution, optical memory, and gentle
// transient softening. Shaped like NovaWeightMacro
// (_shared/NovaRoyal670Core/DSP/NovaWeightMacro.h)'s fromWeight() pattern,
// but richer (5 sub-targets vs. 3), since Glow's spec explicitly demands
// coordinating all of these together.
//
// Every sub-target is capped well below 1.0 even at glow01=1.0, on
// purpose: Glow must never reach fuzz/distortion, and never becomes a bass
// EQ - it only ever touches evenHarmonicBias/lfDrive/gain-cell memory/
// attack/drive amount, never a filter's own frequency or gain.
struct NovaGlowMacro
{
    float tubeEvenHarmonicBias = 0.0f;    // -> both NovaTubeStage instances' setEvenHarmonicBias()
    float transformerLfDrive = 0.0f;      // -> both NovaTransformerStage instances' setLfDrive()
    float gainCellMemoryDepth = 0.0f;     // -> NovaOpticalGainCell::setMemoryDepth() (more Glow = more audible optical memory)
    float attackSofteningAmount = 0.0f;   // -> NovaOpticalGainCell::setAttackSoftening() (capped - never destroys transients)
    float outputStageDriveBoost = 0.0f;   // -> small multiplicative boost to output tube/transformer drive (perceived density)

    static NovaGlowMacro fromGlow (float glow01) noexcept
    {
        const float g = juce::jlimit (0.0f, 1.0f, glow01);
        NovaGlowMacro m;
        m.tubeEvenHarmonicBias = g * 0.5f;    // stays odd-harmonic-dominant even at 100
        m.transformerLfDrive = g * 0.6f;
        m.gainCellMemoryDepth = g;             // the "optical memory" contribution itself
        m.attackSofteningAmount = g * 0.3f;    // deliberately capped
        m.outputStageDriveBoost = g * 0.35f;   // capped well below fuzz territory
        return m;
    }
};

} // namespace nova::glow
