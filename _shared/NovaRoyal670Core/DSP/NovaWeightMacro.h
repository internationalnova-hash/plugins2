#pragma once

#include <juce_core/juce_core.h>

namespace nova::royal670
{

// The Weight macro (0-100 in the UI, 0-1 here) is a coordinator, not an
// EQ: it fans a single knob out to three sub-parameters that together
// build perceived "analog weight" through transformer LF magnetic-
// saturation depth, tube even-harmonic density, and the strength of the
// gain-reduction-to-tube-drive coupling - never a bass shelf.
struct NovaWeightMacro
{
    float transformerLfDrive = 0.0f; // extra low-band saturation depth, feeds NovaTransformerStage::setLfDrive
    float tubeEvenHarmonicBias = 0.0f; // feeds NovaTubeStage::setEvenHarmonicBias (output stage)
    float tubeGrCoupling = 0.0f; // strength of gainReductionDb -> output tube drive coupling

    static NovaWeightMacro fromWeight (float weight01) noexcept
    {
        const float w = juce::jlimit (0.0f, 1.0f, weight01);
        NovaWeightMacro m;
        m.transformerLfDrive = w;
        m.tubeEvenHarmonicBias = w * 0.6f; // kept tasteful - never fully even-only
        m.tubeGrCoupling = w * 0.8f;
        return m;
    }
};

} // namespace nova::royal670
