#include "NovaPunchCrossover.h"

namespace nova::punch
{
    void NovaPunchCrossover::prepare (double sampleRate) noexcept
    {
        sr = sampleRate;

        // Butterworth Q (0.7071) cascaded twice at the SAME cutoff is the
        // standard Linkwitz-Riley recipe: LP4+HP4 sum to unity gain with
        // no ripple at the crossover, unlike two independently-designed
        // filters.
        auto lowCoeffs1 = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, kLowMidHz, 0.7071f);
        auto highCoeffs1 = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, kLowMidHz, 0.7071f);
        lowStageA.coefficients = lowCoeffs1;
        lowStageB.coefficients = lowCoeffs1;
        highPass1StageA.coefficients = highCoeffs1;
        highPass1StageB.coefficients = highCoeffs1;

        auto lowCoeffs2 = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, kMidHighHz, 0.7071f);
        auto highCoeffs2 = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, kMidHighHz, 0.7071f);
        midStageA.coefficients = lowCoeffs2;
        midStageB.coefficients = lowCoeffs2;
        highStageA.coefficients = highCoeffs2;
        highStageB.coefficients = highCoeffs2;

        reset();
    }

    void NovaPunchCrossover::reset() noexcept
    {
        lowStageA.reset(); lowStageB.reset();
        highPass1StageA.reset(); highPass1StageB.reset();
        midStageA.reset(); midStageB.reset();
        highStageA.reset(); highStageB.reset();
    }

    NovaPunchCrossover::Bands NovaPunchCrossover::processSample (float input) noexcept
    {
        const float low = lowStageB.processSample (lowStageA.processSample (input));
        const float highPass1 = highPass1StageB.processSample (highPass1StageA.processSample (input));

        const float mid = midStageB.processSample (midStageA.processSample (highPass1));
        const float high = highStageB.processSample (highStageA.processSample (highPass1));

        return { low, mid, high };
    }
}
