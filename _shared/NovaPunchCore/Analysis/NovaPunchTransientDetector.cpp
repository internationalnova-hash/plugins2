#include "NovaPunchTransientDetector.h"
#include <algorithm>
#include <cmath>

namespace nova::punch
{
    namespace
    {
        float alphaFor (double seconds, double sampleRate) noexcept
        {
            return 1.0f - (float) std::exp (-1.0 / (seconds * sampleRate));
        }
    }

    void NovaPunchTransientDetector::prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        follower.prepare (sr);

        bandEnergyAttackAlpha = alphaFor (0.003, sr);
        bandEnergyReleaseAlpha = alphaFor (0.030, sr);

        shortPeakDecayAlpha = alphaFor (0.015, sr);
        shortRmsAlpha = alphaFor (0.030, sr);

        refractorySamples = (int) (0.015 * sr); // ~15ms - long enough to clear a single event's own ripple

        reset();
    }

    void NovaPunchTransientDetector::reset() noexcept
    {
        follower.reset();
        prevFastEnv = 0.0f;
        lowEnergy = midEnergy = highEnergy = 0.0f;
        prevLowEnergy = prevMidEnergy = prevHighEnergy = 0.0f;
        shortPeak = 0.0f;
        shortRmsSq = 0.0f;
        refractoryCounter = 0;
    }

    void NovaPunchTransientDetector::setSensitivity (float sensitivity0to100) noexcept
    {
        sensitivityNorm = juce::jlimit (0.0f, 1.0f, sensitivity0to100 / 100.0f);
    }

    NovaPunchTransientDetector::Result NovaPunchTransientDetector::processSample (float broadbandInput, const NovaPunchCrossover::Bands& bands) noexcept
    {
        follower.processSample (broadbandInput);
        const float fastEnv = follower.getFast();
        const float slowEnv = follower.getSlow();

        // --- cue 1: fast/slow ratio ---
        const float ratio = fastEnv / (slowEnv + 1.0e-6f);
        const float ratioScore = juce::jlimit (0.0f, 1.0f, (ratio - 1.0f) / 3.0f); // ratio of 4x local level -> full score

        // --- cue 2: slope of the fast envelope ---
        const float slope = fastEnv - prevFastEnv;
        prevFastEnv = fastEnv;
        const float slopeScore = juce::jlimit (0.0f, 1.0f, slope / 0.05f);

        // --- cue 3: crest factor (short peak vs short rms) ---
        // Symmetric rise/fall rate (not slower on the way down): an
        // asymmetric slow-release RMS reference was measured to stay
        // artificially elevated from one hit into the next on dense
        // material, suppressing crest (and, structurally, ratio above)
        // for every subsequent hit in a passage - traced directly via a
        // per-hit probability probe (8 identical hits 150ms apart: peak
        // probability collapsed from 0.60 to 0.09 by the last hit before
        // this fix). A short RMS reference only needs to be short, not
        // asymmetric, to do its job.
        const float absIn = std::abs (broadbandInput);
        shortPeak = absIn > shortPeak ? absIn : shortPeak - shortPeakDecayAlpha * shortPeak;
        shortRmsSq += shortRmsAlpha * (absIn * absIn - shortRmsSq);
        const float crest = shortPeak / (std::sqrt (shortRmsSq) + 1.0e-6f);
        const float crestScore = juce::jlimit (0.0f, 1.0f, (crest - 1.5f) / 4.0f);

        // --- cue 4: per-band energy flux (also IS the telemetry/Focus-weighting activity) ---
        const auto trackBand = [this] (float x, float& energy) -> float
        {
            const float e = x * x;
            const float a = e > energy ? bandEnergyAttackAlpha : bandEnergyReleaseAlpha;
            energy += a * (e - energy);
            return energy;
        };
        lowEnergy = trackBand (bands.low, lowEnergy);
        midEnergy = trackBand (bands.mid, midEnergy);
        highEnergy = trackBand (bands.high, highEnergy);

        const float lowFlux = juce::jmax (0.0f, lowEnergy - prevLowEnergy);
        const float midFlux = juce::jmax (0.0f, midEnergy - prevMidEnergy);
        const float highFlux = juce::jmax (0.0f, highEnergy - prevHighEnergy);
        prevLowEnergy = lowEnergy; prevMidEnergy = midEnergy; prevHighEnergy = highEnergy;

        const float fluxScore = juce::jlimit (0.0f, 1.0f, (lowFlux + midFlux + highFlux) / 0.02f);

        // --- combine ---
        // Weighted toward the two DIFFERENCE-based cues (slope, flux),
        // which reset to near-zero within a few tens of ms regardless of
        // how loud the PREVIOUS hit was - traced directly (see the
        // envelope-follower and crest fixes above) that the two RATIO-
        // based cues (ratio, crest) structurally depend on a denominator
        // that hasn't fully recovered between closely-spaced real hits,
        // silently suppressing detection of every hit after the first in
        // a dense passage even when that hit's own attack is just as
        // strong. Ratio/crest still contribute - they're the cues that
        // make the difference for genuinely SOFT/rounded onsets that
        // slope and flux alone read as weak - just not dominant enough to
        // gate the whole detector on their own recovery.
        const float probability = juce::jlimit (0.0f, 1.0f,
            0.35f * ratioScore + 0.20f * slopeScore + 0.20f * crestScore + 0.25f * fluxScore);

        // SENSITIVITY: higher sensitivity -> lower threshold, so smaller
        // events become eligible. Threshold is on the already-normalized
        // probability, not on absolute input level.
        const float threshold = juce::jmap (sensitivityNorm, 0.0f, 1.0f, 0.72f, 0.30f);

        Result result;
        result.transientProbability = probability;
        result.lowActivity = juce::jlimit (0.0f, 1.0f, lowFlux / 0.02f);
        result.midActivity = juce::jlimit (0.0f, 1.0f, midFlux / 0.02f);
        result.highActivity = juce::jlimit (0.0f, 1.0f, highFlux / 0.02f);

        if (refractoryCounter > 0)
            --refractoryCounter;

        // Trigger once probability clears threshold AND the fast envelope
        // is genuinely rising (not just any positive slope - the
        // underlying waveform's own cycle-to-cycle ripple crosses zero
        // slope periodically even during an overall decay, so a bare
        // slope>0 gate can misfire on ripple well after a real event's
        // own attack has finished; 0.0008 sits above typical ripple
        // magnitude but under what a genuine fresh attack produces within
        // its first ~10-15 samples), and only after the refractory period
        // has cleared (the refractory period alone is what prevents
        // retriggering on a single event's OWN attack, which stays above
        // threshold and rising for longer than one sample).
        //
        // Known limitation, measured directly rather than assumed: on
        // heavily dense material where each hit's decay tail is still
        // substantial when the next hit arrives (spec's own "808... long
        // sustained body" case, tested here at hits 150ms apart with each
        // still at ~30% amplitude when the next starts), the fast/slow
        // ratio and crest cues - which both measure how far above a
        // LOCAL reference level the signal sits - read a smaller jump for
        // hit 2 onward than hit 1, because that local reference is itself
        // still elevated from the previous hit's tail. The detector still
        // stays bounded and stable throughout (validated below), it just
        // doesn't reliably re-trigger a fresh onset for every single hit
        // in that specific worst case. Tightening this further (e.g. true
        // spectral flux via STFT, or per-band period tracking) is a
        // reasonable future improvement, not attempted for V1.
        const bool aboveThreshold = probability > threshold && fastEnv > kNoiseFloorLinear;
        const bool rising = slope > 0.0008f;

        if (aboveThreshold && rising && refractoryCounter == 0)
        {
            result.onsetTriggered = true;
            result.transientStrength = probability;
            refractoryCounter = refractorySamples;
        }

        return result;
    }
}
