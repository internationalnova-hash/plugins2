#pragma once

#include "NovaPunchEnvelopeFollower.h"
#include "NovaPunchCrossover.h"

// The heart of Nova Punch: turns a raw signal into a normalized, hysteresis-
// gated onset decision plus a continuous "how transient is this RIGHT NOW"
// probability. No reusable equivalent exists in the catalog - the closest
// prior art (Nova Apex's inline transientWeight(), and
// NovaAIMasterCore/Analysis/NovaDynamicsAnalyzer's offline fast/slow ratio
// detector) confirms the core TECHNIQUE (fast-vs-slow envelope ratio as the
// primary onset signal) but neither is a real-time, per-sample, band-aware
// class - this one is built from scratch on that same proven foundation,
// extended per spec with slope, crest, and a crossover-derived flux proxy.
//
// Combines four independent cues into one probability, each catching a
// different failure mode of using any single one alone:
//   ratio (fast/slow envelope)  - the primary "something just got louder
//                                 than the recent local level" signal
//   slope (fast envelope delta) - catches a fast RISE even before the
//                                 fast envelope's absolute level clears
//                                 the ratio threshold (helps catch soft-
//                                 but-sudden events, e.g. a vocal
//                                 consonant, that a pure level-ratio
//                                 read would under-weight)
//   crest (short peak / short rms) - a broadband spike sitting well above
//                                 the local RMS floor looks transient
//                                 even if the ratio/slope cues are
//                                 ambiguous (helps on dense/loud material
//                                 where the "local level" is already high)
//   flux (per-band energy rise) - a genuinely NEW event usually shows a
//                                 rise concentrated in specific bands
//                                 (a kick's low thump, a hi-hat's high
//                                 click), not a uniform broadband nudge -
//                                 this is also what SENSITIVITY's
//                                 threshold and FOCUS's band weighting
//                                 both key off of
//
// SENSITIVITY sets the trigger threshold relative to the combined
// probability, not an absolute level - spec section 7 is explicit that
// sensitivity must not depend on absolute input level, which this
// satisfies structurally (every cue above is already normalized against
// the signal's own recent energy, not a fixed dB number).
//
// Hysteresis: onsetTriggered only fires on the RISING edge of probability
// crossing the threshold, and a refractory period after each trigger
// blocks immediate retriggering on the same event's own internal ripple -
// spec's explicit "do not repeatedly retrigger on the same event".
namespace nova::punch
{
    class NovaPunchTransientDetector
    {
    public:
        NovaPunchTransientDetector() = default;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        // 0..100, matches the SENSITIVITY parameter directly.
        void setSensitivity (float sensitivity0to100) noexcept;

        struct Result
        {
            float transientProbability = 0.0f; // 0..1, continuous
            float transientStrength = 0.0f;    // 0..1, set only on the sample onsetTriggered is true
            bool  onsetTriggered = false;       // true for exactly one sample per detected event
            float lowActivity = 0.0f, midActivity = 0.0f, highActivity = 0.0f; // 0..1-ish, for telemetry + Focus weighting
        };

        Result processSample (float broadbandInput, const NovaPunchCrossover::Bands& bands) noexcept;

        float getFastEnvelope() const noexcept { return follower.getFast(); }
        float getMediumEnvelope() const noexcept { return follower.getMedium(); }
        float getSlowEnvelope() const noexcept { return follower.getSlow(); }

    private:
        double sr = 44100.0;
        NovaPunchEnvelopeFollower follower;

        float sensitivityNorm = 0.65f;

        float prevFastEnv = 0.0f;

        float lowEnergy = 0.0f, midEnergy = 0.0f, highEnergy = 0.0f;
        float prevLowEnergy = 0.0f, prevMidEnergy = 0.0f, prevHighEnergy = 0.0f;
        float bandEnergyAttackAlpha = 1.0f, bandEnergyReleaseAlpha = 1.0f;

        float shortPeak = 0.0f;
        float shortPeakDecayAlpha = 1.0f;
        float shortRmsSq = 0.0f;
        float shortRmsAlpha = 1.0f;

        int refractoryCounter = 0;
        int refractorySamples = 0;

        static constexpr float kNoiseFloorLinear = 0.001f; // -60dBFS - below this, never trigger
    };
}
