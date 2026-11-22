#pragma once

#include <juce_core/juce_core.h>

namespace nova::glue
{

// GLUE is Nova Glue's signature control - a coordinator, not a second
// threshold/ratio/saturation knob. Shaped like NovaGlowCore's
// NovaGlowMacro::fromGlow() fan-out pattern, but with one constraint that
// is load-bearing for this entire plugin's reason to exist (per the user's
// own explicit framing): NONE of these sub-targets may raise the
// steady-state/average gain-reduction depth for a given Threshold/Ratio/
// input. Threshold and Ratio stay the ONLY controls of how much
// compression happens; Glue only ever reshapes onset/trajectory/character.
// See NovaGlueEngineValidation's GLUE-invariance test, which asserts
// average GR across a full-mix-style signal changes by less than 0.5dB
// between Glue=0 and Glue=100 at matching Threshold/Ratio.
//
// - kneeWideningDb: softens the knee's onset shape (same eventual
//   over-knee slope, just a gentler transition) - redistributes WHERE
//   compression starts, never HOW MUCH on average.
// - autoMemoryDepthNudge: nudges AUTO release's cellCharge blend weight -
//   affects release TRAJECTORY shape, never the fixed release times, and
//   never touches Attack (which stays exactly the LOCKED stepped value).
// - analogDensityNudge: a small amount added to the console/VCA stages'
//   drive, capped far below Drive's own range - "a small amount of analog
//   density" per spec, never a second saturation knob.
// - detectorIntegrationNudge: a small widening of the detector's fixed
//   energy-integration window - smooths micro-fluctuations in the detected
//   level slightly more, never changes the threshold/ratio relationship.
struct NovaGlueMacro
{
    float kneeWideningDb = 0.0f;
    float autoMemoryDepthNudge = 0.0f;
    float analogDensityNudge01 = 0.0f;
    float detectorIntegrationNudge01 = 0.0f;

    static NovaGlueMacro fromGlue (float glue01) noexcept
    {
        const float g = juce::jlimit (0.0f, 1.0f, glue01);
        NovaGlueMacro m;
        m.kneeWideningDb = g * 3.0f;              // up to +3dB wider knee at Glue=100
        m.autoMemoryDepthNudge = g;                 // full range - this is Auto's own signature contribution
        m.analogDensityNudge01 = g * 0.12f;         // capped far below Drive's own 0-1 range
        m.detectorIntegrationNudge01 = g;           // maps to NovaGlueDetector's own small nudge cap
        return m;
    }
};

} // namespace nova::glue
