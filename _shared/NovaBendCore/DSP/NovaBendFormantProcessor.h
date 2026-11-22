#pragma once

#include <juce_core/juce_core.h>
#include <vector>

// Stage 4 of the pitch-correction chain: formant-preserving pre/post
// filtering, so a pitch shift doesn't drag the singer's vocal-tract
// resonances (formants) up/down with it (the classic chipmunk/underwater
// artifact). Independently implemented via LPC (Linear Predictive Coding)
// analysis-synthesis - a different, standard, widely-taught technique from
// the reference architecture's own adaptive-lattice approach, not a port
// of it: periodically (every analysis hop) run Levinson-Durbin recursion
// on the raw dry input to get an all-pole spectral-envelope model, use its
// inverse (whitening) filter as the PRE stage (removes the envelope,
// producing the excitation/residual the pitch shifter actually operates
// on), and its synthesis (all-pole) filter as the POST stage (re-applies
// the envelope to the shifted residual).
//
// setFormantAmount scales both filters' coefficients directly (0 = zero
// coefficients = both filters degrade to exact identity/passthrough, 1 =
// full-strength LPC filtering) - this is what makes amount=0 a bit-exact
// bypass regardless of warp.
//
// setFormantWarp shifts where the RE-APPLIED envelope's resonances land:
// the synthesis-side coefficients are analyzed from a resampled copy of
// the same frame (read at a warp-scaled stride) rather than the frame
// itself, so warp only affects reconstruction, never the residual
// extraction - at warp=0 the resample factor is 1.0, so the analysis and
// synthesis coefficients are identical and processPost(processPre(x)) is a
// clean analysis/resynthesis round trip.
//
// Real-time-safe: analysis frame/coefficient buffers sized once in
// prepare(); the once-per-hop Levinson-Durbin/resample work is cheap
// (order ~12, frame ~1024 samples, run every 256 samples) and allocation-
// free (all scratch buffers pre-sized).
namespace nova::bend
{
    class NovaBendFormantProcessor
    {
    public:
        NovaBendFormantProcessor() = default;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        void setFormantAmount (float amount0to1) noexcept;
        // Nova Bend's FORMANT hero control is a full +/-12 semitone range
        // (see .cpp for why this differs from the class's Nova Pitch
        // ancestor's narrower +/-4 semitone "creative nudge" mapping).
        void setFormantWarpSemitones (float semitones) noexcept;

        // Call BEFORE the pitch shifter, on the raw dry input.
        float processPre (float input) noexcept;
        // Call AFTER the pitch shifter, on the shifted signal.
        float processPost (float shiftedInput) noexcept;

        // Diagnostic only: counts how many samples have hit the stability
        // guard's hard reset in processPost() since prepare()/reset(). Each
        // occurrence is an audible discontinuity (the guard snaps straight
        // to a raw passthrough sample and wipes lattice state) - a
        // synthetic-tone validation pass tells you the guard never lets
        // output go non-finite, but says nothing about how OFTEN it fires
        // on real material, which is what actually determines whether it's
        // audible as clicking. See NovaBendFormantValidation.cpp's
        // "guard trigger rate" diagnostic.
        int getGuardTriggerCount() const noexcept { return guardTriggerCount; }

    private:
        void pushAnalysisSample (float x) noexcept;
        void updateCoefficients() noexcept;
        float sampleChronological (float pos) const noexcept;
        static void autocorrelateFrame (const float* frame, float* outAc) noexcept;
        static void levinsonDurbin (const float* autocorr, float* outCoeffs, float* outReflection) noexcept;

        double sr = 44100.0;

        static constexpr int kOrder = 12;
        static constexpr int kFrameSize = 1024;
        static constexpr int kHopSize = 256;

        std::vector<float> analysisFrame;   // rolling raw-input history, kFrameSize long
        int frameWrite = 0;
        int hopCounter = 0;

        std::vector<float> analysisWindowLPC; // Hamming window, kFrameSize long, computed once
        std::vector<float> orderedFrame;      // scratch: windowed unwarped frame, kFrameSize long
        std::vector<float> resampleScratch;   // scratch for warp-resampled frame, kFrameSize long

        float analysisCoeffs[kOrder] {};  // unwarped direct-form coefficients - used by processPre (FIR, unconditionally stable regardless of coefficient magnitude)
        float synthesisReflection[kOrder] {}; // warp-resampled reflection (lattice) coefficients - used by processPost

        float preHistory[kOrder] {};  // past raw-input samples, for the analysis/inverse filter
        float latticeState[kOrder] {}; // per-stage lattice memory for the all-pole synthesis filter (see processPost)

        float amount = 1.0f;
        float warpFactor = 1.0f; // >1 = brighter/higher formants, <1 = darker/lower

        int guardTriggerCount = 0;
    };
}
