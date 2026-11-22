#pragma once

#include "../NovaBendTypes.h"
#include <juce_core/juce_core.h>

// FLATTEN's target-convergence stage: glides a detected pitch toward a
// single fixed note (from NOTE+OCTAVE), with no key/scale grid, no
// hysteresis, and no vibrato-preserve - unlike a scale quantizer, there is
// only ever one possible target, so none of that surrounding machinery
// (committed-degree hysteresis, Flex-Tune-style closeness blending) applies.
//
// What IS carried forward from Nova Pitch's proven NovaScaleQuantizer:
//   - a real time-domain one-pole GLIDE (not a spatial blend) toward the
//     target, in double precision - float underflows the per-sample
//     increment at slow-enough time constants and permanently freezes
//     short of the true target (measured on the original class this is
//     adapted from; the fixed tau used here is fast enough that this is a
//     defensive habit more than a currently-active risk, but there's no
//     reason to reintroduce a class of bug that's already been solved).
//   - process() only ever being fed genuinely voiced input - the caller
//     (NovaBendEngine) is responsible for gating on isVoiced(), matching
//     the same pitfall already documented on NovaBendPitchDetector.
//
// Real-time-safe: no allocation, plain double-precision members only.
namespace nova::bend
{
    class NovaBendFlattenTarget
    {
    public:
        NovaBendFlattenTarget() = default;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        // From NOTE (0=C..11=B) + OCTAVE (scientific pitch notation).
        void setTargetHz (float hz) noexcept;

        // Call only while genuinely voiced (caller gates on isVoiced()).
        float process (float inputHz) noexcept;
        float getTargetHz() const noexcept { return (float) smoothedMidi; }

    private:
        double dt = 1.0 / 44100.0;
        float targetHz = 130.81f; // C3, matches the UI/parameter default

        // Fixed fast/hard glide - Flatten is a deliberately robotic effect
        // (spec's own framing), not a natural-feeling correction, so this
        // doesn't need to be a user-facing RetuneSpeed control the way
        // NovaScaleQuantizer's does. ~8ms: fast enough to read as a hard
        // lock, not so instantaneous that it reintroduces audible zipper
        // noise on every hop.
        static constexpr float kGlideTauSeconds = 0.008f;

        bool primed = false;
        double smoothedMidi = 0.0; // double: see class comment
    };
}
