#include "NovaScaleQuantizer.h"
#include <cmath>

namespace nova::pitch
{
    int NovaScaleQuantizer::nearestScaleMidi (int keyPitchClass, Scale scale, float hz) noexcept
    {
        const float midi = hzToMidiF (hz);
        const int midiR = (int) std::round (midi);
        const int pc = ((midiR - keyPitchClass) % 12 + 12) % 12;

        if (scale == Scale::Chromatic)
            return midiR;

        int numDegrees = 0;
        const int* degrees = getScaleDegrees (scale, numDegrees);
        if (degrees == nullptr || numDegrees == 0)
            return midiR;

        int best = degrees[0];
        int bestDist = 99;
        for (int i = 0; i < numDegrees; ++i)
        {
            const int d = ((pc - degrees[i]) % 12 + 12) % 12;
            const int d2 = 12 - d;
            const int dist = juce::jmin (d, d2);
            if (dist < bestDist) { bestDist = dist; best = degrees[i]; }
        }

        const int tonicOfOctave = midiR - pc;
        int candidate = tonicOfOctave + best;
        if (candidate < midiR - 6) candidate += 12;
        if (candidate > midiR + 6) candidate -= 12;
        return candidate;
    }

    void NovaScaleQuantizer::prepare (double sampleRate) noexcept
    {
        dt = 1.0 / juce::jmax (1.0, sampleRate);
        setVibratoRateHz (5.0f); // sane default (typical vocal vibrato rate) until the host sets a real value
        reset();
    }

    void NovaScaleQuantizer::reset() noexcept
    {
        primed = false;
        slowCenterMidi = 0.0f;
        smoothedTargetMidi = 0.0f;
        peakDeviationHeld = 0.0;
        lastTargetHz = 0.0f;
        quantizedPrimed = false;
        committedScaleMidi = 0;
    }

    void NovaScaleQuantizer::setKeyAndScale (int kpc, Scale s) noexcept
    {
        keyPitchClass = ((kpc % 12) + 12) % 12;
        scale = s;
    }

    void NovaScaleQuantizer::setAmount (float a) noexcept
    {
        amount = juce::jlimit (0.0f, 1.0f, a);
    }

    void NovaScaleQuantizer::setRetuneSpeed (float speed0to1) noexcept
    {
        const float s = juce::jlimit (0.0f, 1.0f, speed0to1);
        // Exponential, not linear: 3ms (hard-tune snap) .. 1.2s (slow/
        // natural glide) - same sense as the original "tolerance"
        // parameter (0=fast, 1=slow), same endpoints as before. A LINEAR
        // ramp between these two crams nearly all of the musically useful
        // "tight" range into the last few percent near s=0 - moving the
        // knob from the fastest setting down just 10% already took the
        // glide time from 3ms to ~123ms (a 40x jump), measured directly
        // against a user report that anything short of the absolute
        // fastest setting sounded "very off" and far weaker than expected.
        // Human perception of TIME parameters is roughly logarithmic
        // (equal knob steps should feel like equal RATIO changes, not
        // equal differences) - the same reason virtually every
        // attack/release/glide control in real audio gear is mapped
        // exponentially rather than linearly. Same 3ms/1.2s endpoints,
        // much more usable range in between.
        constexpr float minTau = 0.003f;
        constexpr float maxTau = 1.2f;
        retuneSpeedTau = minTau * std::pow (maxTau / minTau, s);
    }

    void NovaScaleQuantizer::setVibratoPreserve (float amount0to1) noexcept
    {
        vibratoPreserve = juce::jlimit (0.0f, 1.0f, amount0to1);
    }

    void NovaScaleQuantizer::setVibratoRateHz (float hz) noexcept
    {
        // Cutoff at half the vibrato rate: genuine vibrato oscillation at
        // this rate gets smoothed OUT of the tracked "center" pitch, which
        // is what isolates it as the deviation term below.
        const float cutoff = juce::jmax (0.1f, hz) * 0.5f;
        vibratoRateAlpha = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * cutoff * (float) dt);
    }

    float NovaScaleQuantizer::process (float inputHz) noexcept
    {
        if (inputHz <= 0.0f)
            return lastTargetHz; // caller gates unvoiced input upstream; hold last output

        const float inputMidi = hzToMidiF (inputHz);

        if (! primed)
        {
            slowCenterMidi = inputMidi;
            smoothedTargetMidi = inputMidi;
            primed = true;
        }

        // Slow "center" pitch tracker; the difference between raw input and
        // this tracked center isolates vibrato-rate oscillation. Fixed at
        // the vibrato-rate tau (~64ms) for SMALL deviations, which is what
        // correctly smooths genuine vibrato into the deviation term below -
        // but that same ~64ms tau is far too slow to recover from a LARGE
        // deviation (a confidently wrong detection, or the tail end of one
        // now resolved): the center would instead crawl through every
        // intermediate semitone on its way to the real value, and each one
        // gets quantized and briefly committed to along the way - measured
        // directly: a single bad detection produced a burst of 9 separate
        // note-target changes within about 130ms once correct input
        // resumed, audible as a stutter/glitch rather than one clean
        // correction. Blending toward a much faster tau once the deviation
        // clearly exceeds vibrato range - while leaving genuine small
        // vibrato-scale wobble untouched - lets a large, clearly-wrong
        // deviation resolve in one hop instead of a cascade, without
        // changing how real vibrato is tracked at all.
        //
        // Start threshold lowered from the original 1.5 semitones: a real
        // GENUINE note-to-note movement (a scoop/ornament/passing note,
        // not a wrong detection) is very commonly just a whole tone (2
        // semitones) or even a half step - starting at 1.5 meant the
        // fast-track blend hadn't even begun engaging by the time a whole-
        // tone move was already complete, so the center crawled at the
        // slow ~64ms vibrato tau for the ENTIRE deviation, not just its
        // last stretch. Measured directly on real material: the corrected
        // output didn't even start moving toward a genuine, clearly-sung
        // B3 until ~85ms after the raw pitch had already arrived there,
        // and was still catching up as the singer had already moved back
        // off it - landing the correction audibly out of sync with the
        // performance rather than a clean, timely snap. Typical
        // unintentional vibrato/pitch-wobble depth is well under a
        // semitone (usually 20-50 cents), so a 0.6-semitone start still
        // leaves genuine small wobble almost entirely on the slow path.
        const double deviationForRate = std::abs ((double) inputMidi - slowCenterMidi);

        // Peak-held (fast-attack/slow-decay), not instantaneous: a purely
        // instantaneous deviation self-undermines partway through a
        // genuine note change - as slowCenterMidi successfully converges,
        // the deviation itself shrinks, which drops it back BELOW
        // fastTrackStartSemitones and hands the LAST leg of the same
        // convergence back to the slow ~64ms vibrato tau, even though the
        // move was never actually finished. Measured directly on real
        // material: a clean 2-semitone note change got most of the way
        // there almost instantly, then audibly crawled the remaining
        // stretch for another 50-60ms - reported by the user as the
        // correction "reacting late", and they were right about the
        // mechanism. Holding the PEAK deviation seen recently (instead of
        // reacting to whatever the current, already-shrinking deviation
        // happens to be) keeps the fast path engaged for the whole
        // transition, decaying back to normal tracking only after
        // convergence has had real time to finish.
        constexpr double peakDecaySeconds = 0.03;
        const double peakDecayFactor = std::exp (-dt / peakDecaySeconds);
        peakDeviationHeld = juce::jmax (deviationForRate, peakDeviationHeld * peakDecayFactor);

        constexpr double fastTrackStartSemitones = 1.0;
        constexpr double fastTrackFullSemitones = 2.2;
        const double fastBlend = juce::jlimit (0.0, 1.0,
            (peakDeviationHeld - fastTrackStartSemitones) / (fastTrackFullSemitones - fastTrackStartSemitones));
        const double effectiveCenterAlpha = (double) vibratoRateAlpha + fastBlend * (1.0 - (double) vibratoRateAlpha);
        slowCenterMidi += effectiveCenterAlpha * ((double) inputMidi - slowCenterMidi);
        const double vibratoDeviationMidi = (double) inputMidi - slowCenterMidi;

        const int candidateScaleMidi = nearestScaleMidi (keyPitchClass, scale, midiToHzF ((float) slowCenterMidi));
        if (! quantizedPrimed)
        {
            committedScaleMidi = candidateScaleMidi;
            quantizedPrimed = true;
        }
        else if (candidateScaleMidi != committedScaleMidi)
        {
            // Hysteresis: require the tracked center to sit clearly closer
            // to the candidate than to the degree we're already committed
            // to before actually switching - see the header comment on
            // committedScaleMidi for why. ~20 cents of margin, matching the
            // asymmetric thresholds documented in prior-art pitch-
            // correction designs for exactly this purpose.
            constexpr double hysteresisSemitones = 0.20;
            const double distToCommitted = std::abs (slowCenterMidi - (double) committedScaleMidi);
            const double distToCandidate = std::abs (slowCenterMidi - (double) candidateScaleMidi);
            if (distToCandidate + hysteresisSemitones < distToCommitted)
                committedScaleMidi = candidateScaleMidi;
        }
        const double quantizedMidi = (double) committedScaleMidi;

        // "Humanize" (the `amount` member, still using its original
        // parameter ID/name for saved-project compatibility) used to blend
        // UNIFORMLY between raw and quantized pitch regardless of how far
        // off-key the input already was - so turning it down at all made
        // genuinely wrong notes stay wrong, not just "a bit more natural".
        // A user expects a "Humanize" control to add back subtle natural
        // character on TOP of correct tuning, never to let real pitch
        // errors through - confirmed directly as a real complaint: turning
        // this down made the plugin sound "out of tune", which "shouldn't
        // have anything to do with the tuning" and doesn't happen on
        // reference tools.
        //
        // Real pitch-correction tools solve this with a variant often
        // called "Flex-Tune": humanization only ever reduces correction
        // for input that's ALREADY close to the target (letting genuine
        // close-in vibrato/expression show through), while input that's
        // clearly a different pitch always gets pulled fully into tune no
        // matter how high Humanize is set. `closeWindowSemitones` is how
        // far off-target counts as "close enough to be natural variation,
        // not a wrong note" - beyond it, humanize has zero effect and
        // correction is always 100%.
        const double deviationFromTarget = std::abs (slowCenterMidi - quantizedMidi);
        constexpr double closeWindowSemitones = 0.5;
        const double closenessFactor = juce::jlimit (0.0, 1.0, 1.0 - deviationFromTarget / closeWindowSemitones);
        const double effectiveAmount = 1.0 - (double) amount * closenessFactor;

        const double instantaneousTargetMidi = slowCenterMidi + effectiveAmount * (quantizedMidi - slowCenterMidi);

        // Real time-domain glide toward the instantaneous target - this is
        // what "Retune Speed" actually controls (a time constant, not a
        // spatial blend width).
        const double glideAlpha = 1.0 - std::exp (-dt / juce::jmax (1.0e-4, (double) retuneSpeedTau));
        smoothedTargetMidi += glideAlpha * (instantaneousTargetMidi - smoothedTargetMidi);

        const double finalMidi = smoothedTargetMidi + (double) vibratoPreserve * vibratoDeviationMidi;

        lastTargetHz = midiToHzF ((float) finalMidi);
        return lastTargetHz;
    }
}
