#include "NovaPitchSynchronousShifter.h"
#include <cmath>

namespace nova::pitch
{
    int NovaPitchSynchronousShifter::nextPowerOfTwo (int v) noexcept
    {
        int p = 1;
        while (p < v) p <<= 1;
        return p;
    }

    int NovaPitchSynchronousShifter::wrapIndex (long long v) const noexcept
    {
        const long long size = (long long) bufSize;
        return (int) (((v % size) + size) % size);
    }

    void NovaPitchSynchronousShifter::prepare (double sampleRate) noexcept
    {
        sr = sampleRate;

        // Sized from the lowest supported pitch (longest period) so a
        // full-length grain plus the output-delay margin always fits
        // without wraparound aliasing - generous headroom (8x) keeps this
        // true even with jitter in the actual detected period. Scaled by
        // kHalfLenMultiplier so the widened (R>2) grain/window still fits
        // at the lowest supported pitch - see the kOverlapFactor comment.
        maxGrainHalf = kHalfLenMultiplier * (int) std::round (sr / kMinPitchHz);
        outputDelaySamples = maxGrainHalf + 64;
        bufSize = nextPowerOfTwo (8 * maxGrainHalf);

        historyBuf.assign ((size_t) bufSize, 0.0f);
        outputBuf.assign ((size_t) bufSize, 0.0f);
        grainBuf.assign ((size_t) (2 * maxGrainHalf + 1), 0.0f);
        prevGrainBuf.assign ((size_t) (2 * maxGrainHalf + 1), 0.0f);

        // ~6ms - short enough to be well under the detector/quantizer's
        // own commit time (so this doesn't meaningfully add to how long a
        // genuine note change takes to be heard), long enough to smooth
        // the injection cadence and per-grain resample ratio across an
        // abrupt target change instead of snapping instantly - see the
        // header comment on smoothedOutHz.
        outHzSmoothAlpha = 1.0f - std::exp (-1.0f / (0.006f * (float) sr));

        // ~2.5ms - deliberately much shorter than the output smoother
        // above; see the header comment on smoothedInHz.
        inHzSmoothAlpha = 1.0f - std::exp (-1.0f / (0.0025f * (float) sr));

        reset();
    }

    void NovaPitchSynchronousShifter::reset() noexcept
    {
        std::fill (historyBuf.begin(), historyBuf.end(), 0.0f);
        std::fill (outputBuf.begin(), outputBuf.end(), 0.0f);
        std::fill (grainBuf.begin(), grainBuf.end(), 0.0f);
        std::fill (prevGrainBuf.begin(), prevGrainBuf.end(), 0.0f);
        prevGrainHalfLen = 0;
        havePrevGrain = false;

        phaseIn = 0.0;
        phaseOut = 0.0;
        n = 0;
        lastRatioDebug = 1.0f;
        smoothedOutHz = 0.0;
        smoothedInHz = 0.0;

        // Sane default until the first real grain is extracted, so an
        // early output-phase wrap (before any input-phase wrap has
        // happened) still uses a bounded, sensible window width.
        captureHalfLen = juce::jlimit (16, maxGrainHalf, (int) std::round (sr / 200.0));
    }

    float NovaPitchSynchronousShifter::processSample (float input, float inputPitchHz, float outputPitchHz) noexcept
    {
        historyBuf[(size_t) wrapIndex (n)] = input;

        const double clampedInHz  = juce::jlimit (kMinPitchHz, kMaxPitchHz, (double) inputPitchHz);
        const double clampedOutHz = juce::jlimit (kMinPitchHz, kMaxPitchHz, (double) outputPitchHz);

        if (smoothedOutHz <= 0.0)
            smoothedOutHz = clampedOutHz; // not primed yet - start from a sane value, not zero
        smoothedOutHz += (double) outHzSmoothAlpha * (clampedOutHz - smoothedOutHz);

        if (smoothedInHz <= 0.0)
            smoothedInHz = clampedInHz;
        smoothedInHz += (double) inHzSmoothAlpha * (clampedInHz - smoothedInHz);

        lastRatioDebug = (float) (smoothedOutHz / clampedInHz);

        phaseIn += smoothedInHz / sr;
        phaseOut += smoothedOutHz / sr;

        if (phaseIn >= 1.0)
        {
            phaseIn -= 1.0;

            const int periodInSamples = (int) std::round (sr / smoothedInHz);
            // newHalfLen is the CAPTURED grain's half-width - widened by
            // kHalfLenMultiplier (see kOverlapFactor) so the injection
            // side below has enough real content to fill its own widened
            // window without hitting the read-position clamp. The WSOLA
            // search just below is deliberately still scoped to ONE
            // PERIOD (periodInSamples), not this wider value - that
            // search's radius/comparison width was already tuned and
            // validated against real material, and widening the CAPTURE
            // must not silently change THAT behavior too.
            const int newHalfLen = juce::jlimit (16, maxGrainHalf, kHalfLenMultiplier * periodInSamples);
            const long long expectedCenter = n - newHalfLen;

            // WSOLA-style alignment search - see the header comment on
            // prevGrainBuf. The computed center above assumes the input
            // waveform's own cycle boundary falls exactly one mathematical
            // period behind "now", but real voice always has some cycle-
            // to-cycle jitter that computed period alone can't see -
            // capturing blindly at the computed position measurably
            // degraded phase continuity between consecutive grains on real
            // vocal material (independently re-measured: an integrated
            // signal chain with a provably flat, stable target still
            // produced far more apparent pitch instability than the target
            // itself - i.e. instability being introduced downstream of the
            // quantizer, in exactly this stage). Searching a small
            // neighborhood for the offset that makes the new grain most
            // waveform-similar (normalized cross-correlation) to the
            // PREVIOUSLY captured grain keeps consecutive grains phase-
            // continuous with each other without needing full glottal-
            // epoch detection - the standard, well-documented WSOLA
            // technique for this exact problem.
            int bestOffset = 0;
            float debugBestScore = 0.0f, debugBaselineScore = 0.0f;
            if (havePrevGrain)
            {
                // Scoped to ONE PERIOD (not the wider newHalfLen) - see
                // the comment on newHalfLen above.
                const int compareHalfLen = juce::jlimit (1, juce::jmin (newHalfLen, prevGrainHalfLen), periodInSamples);
                // +-12.5% of the current period - kept deliberately small.
                // A wider search (+-25% was tried first) chased spurious
                // similarity matches during genuine pitch movement - real
                // consecutive cycles during an attack/vibrato/glide are
                // SUPPOSED to differ from each other, so a wide search can
                // find a "well-matched" position that's actually from the
                // wrong part of the waveform, and the chosen offset could
                // swing unpredictably capture-to-capture, breaking the
                // otherwise-regular one-period grain spacing this scheme
                // depends on - measured directly as new click/discontinuity
                // artifacts on real material that weren't present before.
                const int searchRadius = juce::jmax (1, periodInSamples / 8);
                float bestScore = -1.0e30f;
                float baselineScore = -1.0e30f;
                for (int off = -searchRadius; off <= searchRadius; ++off)
                {
                    float dot = 0.0f, energyA = 0.0f, energyB = 0.0f;
                    for (int k = -compareHalfLen; k <= compareHalfLen; ++k)
                    {
                        const float a = historyBuf[(size_t) wrapIndex (expectedCenter + off + k)];
                        const float b = prevGrainBuf[(size_t) (k + prevGrainHalfLen)];
                        dot += a * b;
                        energyA += a * a;
                        energyB += b * b;
                    }
                    const float denom = std::sqrt (energyA * energyB) + 1.0e-9f;
                    const float score = dot / denom;
                    if (off == 0) baselineScore = score;
                    if (score > bestScore) { bestScore = score; bestOffset = off; }
                }

                // Only move away from the mathematically-expected position
                // when the match there is CLEARLY better, not just
                // marginally different - a small, fixed margin so genuine
                // stationary-cycle jitter still gets corrected, but noise-
                // level score differences (common during transients, where
                // no offset is a particularly good match) don't cause the
                // capture point to wander.
                if (bestScore < baselineScore + 0.02f)
                    bestOffset = 0;

                debugBestScore = bestScore;
                debugBaselineScore = baselineScore;
            }

            const long long center = expectedCenter + bestOffset;

            for (int k = -newHalfLen; k <= newHalfLen; ++k)
                grainBuf[(size_t) (k + newHalfLen)] = historyBuf[(size_t) wrapIndex (center + k)];
            captureHalfLen = newHalfLen;

            if (onGrainCaptured)
            {
                // Independent measurement of the CAPTURED GRAIN's own true
                // pitch (simple time-domain autocorrelation over the grain
                // buffer itself, searching lags within +-50% of the
                // assumed period) - deliberately NOT reusing the engine's
                // own detector, so this can reveal cases where the grain
                // actually captured doesn't match what the phase
                // accumulator assumed it would, independent of whether the
                // detector's own pitch tracking is right or wrong.
                // Symmetric search range around the ASSUMED period (+-33%),
                // not capped above by newHalfLen - a range that only ever
                // extends DOWNWARD from the assumed period (as an earlier
                // version of this measurement did) can never find the true
                // lag when it's actually slightly ABOVE the estimate, and
                // silently biases toward a spurious near-octave match
                // instead (autocorrelation is always strong at exact
                // submultiples of any periodic signal's true period).
                const int searchRange = juce::jmax (4, periodInSamples / 3);
                const int lagLo = juce::jmax (4, periodInSamples - searchRange);
                const int lagHi = juce::jmin (newHalfLen, periodInSamples + searchRange);
                // Fixed comparison span (does NOT shrink as lag grows,
                // sized safe for the largest lag in range) and NORMALIZED
                // correlation (divided by the two windows' own energy) - a
                // raw, unnormalized dot product over a lag-shrinking span
                // is systematically biased toward SMALL lags (fewer, more
                // strongly self-correlated terms), which also silently
                // produces spurious pitch-measurement errors unrelated to
                // the signal's real periodicity.
                const int fixedSpan = juce::jmax (4, newHalfLen - lagHi);
                float bestCorr = -1.0e30f;
                int bestLag = periodInSamples;
                if (lagHi > lagLo)
                {
                    for (int lag = lagLo; lag <= lagHi; ++lag)
                    {
                        float dot = 0.0f, energyA = 0.0f, energyB = 0.0f;
                        for (int k = -fixedSpan; k <= fixedSpan; ++k)
                        {
                            const float a = grainBuf[(size_t) (k + newHalfLen)];
                            const float b = grainBuf[(size_t) juce::jlimit (0, 2 * newHalfLen, k + lag + newHalfLen)];
                            dot += a * b;
                            energyA += a * a;
                            energyB += b * b;
                        }
                        const float denom = std::sqrt (energyA * energyB) + 1.0e-9f;
                        const float score = dot / denom;
                        if (score > bestCorr) { bestCorr = score; bestLag = lag; }
                    }
                }
                CaptureDebugInfo info;
                info.sampleIndex = n;
                info.periodInSamples = periodInSamples;
                info.wsolaOffset = bestOffset;
                info.wsolaBestScore = debugBestScore;
                info.wsolaBaselineScore = debugBaselineScore;
                info.measuredGrainHz = bestLag > 0 ? (float) (sr / bestLag) : 0.0f;
                info.assumedHz = (float) (sr / periodInSamples);
                onGrainCaptured (info);
            }

            for (int k = -newHalfLen; k <= newHalfLen; ++k)
                prevGrainBuf[(size_t) (k + newHalfLen)] = grainBuf[(size_t) (k + newHalfLen)];
            prevGrainHalfLen = newHalfLen;
            havePrevGrain = true;
        }

        if (phaseOut >= 1.0)
        {
            phaseOut -= 1.0;

            // The captured grain (captureHalfLen wide, at the INPUT's own
            // native timebase) must be resampled to the OUTPUT's timebase
            // during injection - reusing it unresampled only works when
            // input and output periods are close. Without this, two
            // overlapping copies of a periodic waveform spaced at the
            // OUTPUT period (which generally isn't a whole multiple of the
            // grain's own internal period) interfere with each other
            // instead of reinforcing - measured directly: a -7 semitone
            // shift produced a wildly unstable output frequency (~160-
            // 185Hz swinging around instead of a stable ~147Hz), not just
            // a small accuracy error.
            //
            // The output-domain window half-width is periodOutSamples
            // (giving hop=windowLength/2, the standard Hann-OLA constant-
            // overlap-add ratio); reading the captureHalfLen-wide grain at
            // step (outputHz/inputHz) per output sample stretches/
            // compresses it to exactly fill that span - algebraically,
            // outHalf * (outputHz/inputHz) == periodInSamples ==
            // captureHalfLen (at the time of capture), so the read range
            // lands within the captured data by construction.
            const int periodOutSamples = (int) std::round (sr / smoothedOutHz);
            // Widened by kHalfLenMultiplier in lockstep with the capture
            // side above - injection RATE (phaseOut's own wrap cadence,
            // unchanged) still defines pitch; this only widens how much
            // of the (correspondingly widened) grain gets summed into the
            // output at once. See the kOverlapFactor comment in the header.
            const int outHalf = juce::jlimit (16, maxGrainHalf, kHalfLenMultiplier * periodOutSamples);
            // Window spans k=-outHalf..+outHalf, a full width of 2*outHalf -
            // the cosine argument needs a full 2*pi sweep across that whole
            // span to produce a symmetric Hann bump (0 at both edges, 1.0 at
            // center), so the denominator here is outHalf, not 2*outHalf.
            // Using 2*outHalf (as this read for a while) only sweeps HALF a
            // cosine cycle across the window, turning the bump into a
            // one-sided RAMP (0 at one edge, full amplitude at the other) -
            // two overlapping ramps don't sum to a constant the way two
            // overlapping Hann bumps do, which breaks the constant-overlap-
            // add reconstruction this scheme depends on. Measured directly:
            // this produced visible cycle-to-cycle amplitude flutter on real
            // vocal material even at near-unity shift ratios (dry vs.
            // processed waveform comparison at a matched passage), audible
            // as a grainy/rough texture.
            const float denom = juce::jmax (1.0f, (float) outHalf);
            const float readStep = (float) (smoothedOutHz / smoothedInHz);
            const long long injectCenter = n + outputDelaySamples;

            const int grainMaxIdx = 2 * captureHalfLen;
            for (int k = -outHalf; k <= outHalf; ++k)
            {
                const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) (k + outHalf) / denom);

                const float readPos = juce::jlimit (0.0f, (float) grainMaxIdx,
                                                     (float) captureHalfLen + (float) k * readStep);
                const int i1 = (int) readPos;
                const float frac = readPos - (float) i1;

                // 4-point Catmull-Rom cubic, not linear - linear
                // interpolation between grain samples introduces audible
                // high-frequency roughness/aliasing whenever readStep
                // isn't close to 1.0 (i.e. on every real pitch shift, not
                // just extreme ones) - a well-documented source of the
                // "gritty"/rough texture classic to naive resampling-based
                // shifters, confirmed directly: a side-by-side spectrogram
                // against a reference plugin on the same real material
                // showed visibly fuzzier, less-defined harmonic bands here
                // where the reference's were crisp. Cubic uses the same
                // already-captured grain data - no new state, no change to
                // grain timing/placement, purely a smoother reconstruction
                // of the waveform between existing samples.
                const int i0 = juce::jmax (0, i1 - 1);
                const int i2 = juce::jmin (grainMaxIdx, i1 + 1);
                const int i3 = juce::jmin (grainMaxIdx, i1 + 2);
                const float p0 = grainBuf[(size_t) i0];
                const float p1 = grainBuf[(size_t) i1];
                const float p2 = grainBuf[(size_t) i2];
                const float p3 = grainBuf[(size_t) i3];
                const float a0 = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
                const float a1 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
                const float a2 = -0.5f * p0 + 0.5f * p2;
                const float a3 = p1;
                const float sample = ((a0 * frac + a1) * frac + a2) * frac + a3;

                const int idx = wrapIndex (injectCenter + k);
                outputBuf[(size_t) idx] += sample * w * kOverlapGainCompensation;
            }
        }

        const int readIdx = wrapIndex (n - outputDelaySamples);
        const float out = outputBuf[(size_t) readIdx];
        outputBuf[(size_t) readIdx] = 0.0f;

        ++n;
        return out;
    }
}
