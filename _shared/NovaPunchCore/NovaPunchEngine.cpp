#include "NovaPunchEngine.h"
#include <cmath>

namespace nova::punch
{
    void NovaPunchEngine::prepare (double sampleRate, int maxBlockSize) noexcept
    {
        juce::ignoreUnused (maxBlockSize);

        crossoverMono.prepare (sampleRate);
        crossoverL.prepare (sampleRate);
        crossoverR.prepare (sampleRate);
        detector.prepare (sampleRate);
        segmenter.prepare (sampleRate);
        gainShaper.prepare (sampleRate);
        clipSafeL.prepare (sampleRate);
        clipSafeR.prepare (sampleRate);

        mixSmoothed.reset (sampleRate, 0.02);
        bypassSmoothed.reset (sampleRate, 0.03);
        mixSmoothed.setCurrentAndTargetValue (1.0f);
        bypassSmoothed.setCurrentAndTargetValue (1.0f);

        reset();
    }

    void NovaPunchEngine::reset() noexcept
    {
        crossoverMono.reset();
        crossoverL.reset();
        crossoverR.reset();
        detector.reset();
        segmenter.reset();
        gainShaper.reset();
        clipSafeL.reset();
        clipSafeR.reset();
    }

    void NovaPunchEngine::setParams (const Params& p) noexcept
    {
        params = p;
        detector.setSensitivity (p.sensitivityPercent);
        clipSafeL.setEnabled (p.clipSafe);
        clipSafeR.setEnabled (p.clipSafe);
        mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, p.mixPercent / 100.0f));
        bypassSmoothed.setTargetValue (p.bypassed ? 0.0f : 1.0f);
    }

    void NovaPunchEngine::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
    {
        float blockInPeakL = 0.0f, blockInPeakR = 0.0f, blockOutPeakL = 0.0f, blockOutPeakR = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float mono = 0.5f * (inL[i] + inR[i]);
            const auto monoBands = crossoverMono.processSample (mono);
            const auto detectorResult = detector.processSample (mono, monoBands);
            const auto segResult = segmenter.processSample (detectorResult.onsetTriggered, detectorResult.transientStrength,
                                                              detector.getMediumEnvelope());
            const auto gainResult = gainShaper.processSample (params.punchPercent, params.bodyPercent,
                                                                segResult.attackBlend, segResult.bodyBlend,
                                                                segResult.eventStrength, segResult.region);

            const float finalGain = gainResult.punchGainLinear * gainResult.bodyGainLinear;

            const auto bandsL = crossoverL.processSample (inL[i]);
            const auto bandsR = crossoverR.processSample (inR[i]);

            const auto applyFocus = [&] (const NovaPunchCrossover::Bands& b) -> float
            {
                switch (params.focus)
                {
                    case Focus::Low:  return b.low * finalGain + b.mid + b.high;
                    case Focus::Mid:  return b.low + b.mid * finalGain + b.high;
                    case Focus::High: return b.low + b.mid + b.high * finalGain;
                    case Focus::Full:
                    default:          return (b.low + b.mid + b.high) * finalGain;
                }
            };

            float wetL = applyFocus (bandsL);
            float wetR = applyFocus (bandsR);

            wetL = clipSafeL.processSample (wetL);
            wetR = clipSafeR.processSample (wetR);

            const float mix = mixSmoothed.getNextValue();
            const float mixedL = inL[i] * (1.0f - mix) + wetL * mix;
            const float mixedR = inR[i] * (1.0f - mix) + wetR * mix;

            const float bypassAmt = bypassSmoothed.getNextValue();
            outL[i] = inL[i] * (1.0f - bypassAmt) + mixedL * bypassAmt;
            outR[i] = inR[i] * (1.0f - bypassAmt) + mixedR * bypassAmt;

            blockInPeakL = juce::jmax (blockInPeakL, std::abs (inL[i]));
            blockInPeakR = juce::jmax (blockInPeakR, std::abs (inR[i]));
            blockOutPeakL = juce::jmax (blockOutPeakL, std::abs (outL[i]));
            blockOutPeakR = juce::jmax (blockOutPeakR, std::abs (outR[i]));

            lastTransientProbability = detectorResult.transientProbability;
            if (detectorResult.onsetTriggered)
                lastTransientStrength = detectorResult.transientStrength;
            lastAttackGainDb = gainResult.attackGainDb;
            lastDecayGainDb = gainResult.decayGainDb;
            lastSustainGainDb = gainResult.sustainGainDb;
            lastPunchActivity = juce::jlimit (0.0f, 1.0f, std::abs (gainToDb (gainResult.punchGainLinear)) / NovaPunchGainShaper::kMaxPunchDb);
            lastBodyActivity = juce::jlimit (0.0f, 1.0f, std::abs (gainToDb (gainResult.bodyGainLinear)) / NovaPunchGainShaper::kMaxBodyDb);
            lastLowActivity = detectorResult.lowActivity;
            lastMidActivity = detectorResult.midActivity;
            lastHighActivity = detectorResult.highActivity;
            lastRegion = segResult.region;
        }

        lastInputPeakL = blockInPeakL;
        lastInputPeakR = blockInPeakR;
        lastOutputPeakL = blockOutPeakL;
        lastOutputPeakR = blockOutPeakR;
    }
}
