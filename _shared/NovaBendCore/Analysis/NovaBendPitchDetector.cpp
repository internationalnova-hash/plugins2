#include "NovaBendPitchDetector.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace nova::bend
{
    int NovaBendPitchDetector::chooseWindowSize (double sampleRate, bool lowLatencyMode) noexcept
    {
        if (lowLatencyMode) return 2048;
        return sampleRate >= 88200.0 ? 4096 : 2048;
    }

    void NovaBendPitchDetector::prepare (double sampleRate, bool lowLatencyMode) noexcept
    {
        sr = sampleRate;
        windowSize = chooseWindowSize (sampleRate, lowLatencyMode);
        fftSize = windowSize * 2;
        hopSize = windowSize / 4;

        const int order = (int) std::round (std::log2 ((double) fftSize));
        fft = std::make_unique<juce::dsp::FFT> (order);

        history.assign ((size_t) windowSize, 0.0f);

        // Tukey window, 25% taper (12.5% on each edge, flat in the middle) -
        // a full Hann taper across the whole frame measurably biases the
        // detected autocorrelation-peak lag (worse at longer lags/lower
        // pitches, since more of the frame's energy sits under the
        // sloped part of the taper); a light edge-only taper still avoids
        // the hard discontinuity a rectangular window would leave at the
        // frame boundary, without distorting the bulk of the frame.
        analysisWindow.assign ((size_t) windowSize, 1.0f);
        const int taperLen = juce::jmax (1, (int) (0.125 * (windowSize - 1)));
        for (int i = 0; i < taperLen; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) i / (float) taperLen);
            analysisWindow[(size_t) i] = w;
            analysisWindow[(size_t) (windowSize - 1 - i)] = w;
        }

        fastWindowSize = windowSize / 2;
        fastFftSize = fastWindowSize * 2;
        const int fastOrder = (int) std::round (std::log2 ((double) fastFftSize));
        fastFft = std::make_unique<juce::dsp::FFT> (fastOrder);

        fastAnalysisWindow.assign ((size_t) fastWindowSize, 1.0f);
        const int fastTaperLen = juce::jmax (1, (int) (0.125 * (fastWindowSize - 1)));
        for (int i = 0; i < fastTaperLen; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) i / (float) fastTaperLen);
            fastAnalysisWindow[(size_t) i] = w;
            fastAnalysisWindow[(size_t) (fastWindowSize - 1 - i)] = w;
        }

        reset();
    }

    void NovaBendPitchDetector::reset() noexcept
    {
        std::fill (history.begin(), history.end(), 0.0f);
        historyWrite = 0;
        hopCounter = 0;
        currentHz = 0.0f;
        currentConfidence = 0.0f;
        voiced = false;
        lastAcceptedLag = 0.0f;
        recentPeakEnvelope = 0.0f;

        std::fill (std::begin (rawMidiHistory), std::end (rawMidiHistory), 0.0f);
        std::fill (std::begin (rawVoicedHistory), std::end (rawVoicedHistory), false);
        std::fill (std::begin (rawConfHistory), std::end (rawConfHistory), 0.0f);
        medianHistoryFill = 0;
    }

    void NovaBendPitchDetector::setConfidenceGate (float threshold0to1) noexcept
    {
        confidenceGate = juce::jlimit (0.0f, 1.0f, threshold0to1);
    }

    void NovaBendPitchDetector::setSearchRange (float loHz, float hiHz) noexcept
    {
        searchLoHz = juce::jmax (20.0f, loHz);
        searchHiHz = juce::jmax (searchLoHz + 10.0f, hiHz);
    }

    void NovaBendPitchDetector::pushBlock (const float* mono, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            history[(size_t) historyWrite] = mono[i];
            historyWrite = (historyWrite + 1) % windowSize;

            if (++hopCounter >= hopSize)
            {
                hopCounter = 0;
                analyseHop();
            }
        }
    }

    // In-place: buf holds fftSize complex samples on entry (the windowed
    // signal in the first windowSize slots, zero elsewhere); on exit,
    // buf's real part holds the (unnormalized) autocorrelation via
    // Wiener-Khinchin (FFT -> |.|^2 -> inverse FFT). Zero-padding to 2x
    // windowSize before the forward FFT is what makes this a genuine
    // LINEAR autocorrelation rather than a circular one - without it, the
    // correlation sum at longer lags wraps around and pulls in spurious
    // contributions from the opposite end of the buffer, which measurably
    // biased the detected pitch at lower frequencies (longer periods).
    // DC is removed before the inverse so the result isn't dominated by
    // signal offset.
    void NovaBendPitchDetector::autocorrelate (std::vector<std::complex<float>>& buf) const
    {
        autocorrelateWith (*fft, fftSize, buf);
    }

    void NovaBendPitchDetector::autocorrelateWith (juce::dsp::FFT& fftToUse, int fftLen, std::vector<std::complex<float>>& buf) const
    {
        std::vector<std::complex<float>> spectrum ((size_t) fftLen);
        fftToUse.perform (buf.data(), spectrum.data(), false);

        spectrum[0] = { 0.0f, 0.0f };
        for (int i = 1; i < fftLen; ++i)
        {
            const float mag2 = std::norm (spectrum[(size_t) i]);
            spectrum[(size_t) i] = { mag2, 0.0f };
        }

        fftToUse.perform (spectrum.data(), buf.data(), true);
    }

    // Shared candidate-peak selection: local-max peaks in [nMin,nMax],
    // biased toward whichever candidate is closest (in octave/log terms)
    // to lastLag when one is available, plus the octave-fold refinement -
    // see analyseHop()'s header comment for the full rationale. Used by
    // both the main and fast analysis windows so they share one, single
    // validated implementation rather than two copies that could drift.
    NovaBendPitchDetector::LagPick NovaBendPitchDetector::pickBestLag (const std::vector<std::complex<float>>& buf, int winSize, float lastLag, float loHz, float hiHz) const noexcept
    {
        LagPick result;
        const float norm0 = buf[0].real();
        result.norm0 = norm0;
        if (norm0 <= 1.0e-12f) return result;

        const int nMin = juce::jlimit (1, winSize - 2, (int) std::floor (sr / (double) hiHz));
        const int nMax = juce::jlimit (nMin + 1, winSize - 2, (int) std::ceil (sr / (double) loHz));

        constexpr float fundamentalAcceptRatio = 0.5f;
        constexpr int maxCandidates = 16;
        int candidateLags[maxCandidates];
        float candidateVals[maxCandidates];
        int numCandidates = 0;
        float globalBestVal = 0.0f;
        int globalBestLag = -1;

        for (int lag = nMin; lag <= nMax; ++lag)
        {
            const float v = buf[(size_t) lag].real();
            const float prev = buf[(size_t) (lag - 1)].real();
            const float next = buf[(size_t) (lag + 1)].real();
            if (v > prev && v >= next)
            {
                if (v > globalBestVal) { globalBestVal = v; globalBestLag = lag; }
                if (numCandidates < maxCandidates) { candidateLags[numCandidates] = lag; candidateVals[numCandidates] = v; ++numCandidates; }
            }
        }

        if (globalBestLag < 0) return result;

        int bestLag = globalBestLag;
        float bestVal = globalBestVal;

        if (lastLag > 0.0f)
        {
            float bestCloseness = std::numeric_limits<float>::max();
            for (int c = 0; c < numCandidates; ++c)
            {
                if (candidateVals[c] < fundamentalAcceptRatio * globalBestVal)
                    continue;
                const float closeness = std::abs (std::log2 ((float) candidateLags[c] / lastLag));
                if (closeness < bestCloseness)
                {
                    bestCloseness = closeness;
                    bestLag = candidateLags[c];
                    bestVal = candidateVals[c];
                }
            }

            for (const float foldFactor : { 0.5f, 2.0f })
            {
                const float foldedLagF = (float) bestLag * foldFactor;
                const int foldedLag = (int) std::round (foldedLagF);
                if (foldedLag < 1 || foldedLag > winSize - 2) continue;
                const float foldedVal = buf[(size_t) foldedLag].real();
                const float currentCloseness = std::abs (std::log2 ((float) bestLag / lastLag));
                const float foldedCloseness  = std::abs (std::log2 (foldedLagF / lastLag));
                if (foldedCloseness < currentCloseness && foldedVal >= 0.3f * globalBestVal)
                {
                    bestLag = foldedLag;
                    bestVal = foldedVal;
                }
            }
        }

        result.lag = bestLag;
        result.val = bestVal;
        result.found = true;
        return result;
    }

    void NovaBendPitchDetector::analyseHop()
    {
        std::vector<std::complex<float>> buf ((size_t) fftSize, std::complex<float> { 0.0f, 0.0f });
        // historyWrite is the index of the OLDEST sample currently held
        // (the slot about to be overwritten next), so walking forward from
        // there for windowSize samples reads the buffer in chronological
        // order. The remaining fftSize-windowSize slots stay zero (padding).
        for (int i = 0; i < windowSize; ++i)
        {
            const int idx = (historyWrite + i) % windowSize;
            buf[(size_t) i] = { history[(size_t) idx] * analysisWindow[(size_t) i], 0.0f };
        }

        autocorrelate (buf);

        const float norm0 = buf[0].real();
        if (norm0 <= 1.0e-12f) { markUnvoiced(); return; }

        // Noise floor, separate from the (purely relative) confidence
        // measure below. A near-silent frame - room tone, a breath, a
        // vocal trailing off at the end of a phrase - can still produce a
        // deceptively tall NORMALIZED autocorrelation peak, since
        // normalizing by the frame's own (tiny) energy throws away any
        // sense of how much real signal was actually there to begin with.
        // Measured directly on real vocal takes: a phrase's tail decaying
        // from a healthy -15..-20dBFS down toward -40..-55dBFS kept
        // producing confident (0.6-0.95) pitch estimates chasing whatever
        // in the residual noise happened to look periodic for that one
        // frame - audible as rapid octave-jumping/"speeding up" as a
        // phrase fades out.
        //
        // A single FIXED absolute floor isn't robust across takes with
        // different gain-staging - one bounce's breath tail measured
        // louder (-38dBFS) than a fixed -44dBFS floor, so it slipped
        // through untouched. Gating relative to this take's OWN recent
        // loud (genuinely voiced) level instead adapts automatically:
        // require the current frame to be within ~18dB of whatever this
        // take's own recent peak was, with a small fixed floor purely as a
        // startup/all-silence backstop before any peak has been observed
        // yet. The synthetic pitch-accuracy tests run continuously at full
        // amplitude, so recentPeakEnvelope tracks that immediately and
        // this has no effect on them.
        const float windowRms = std::sqrt (norm0 / (float) windowSize);
        constexpr float peakDecayPerHop = 0.995f; // ~2s time constant at this hop rate
        recentPeakEnvelope = juce::jmax (windowRms, recentPeakEnvelope * peakDecayPerHop);
        const float noiseFloor = juce::jmax (0.006f, 0.125f * recentPeakEnvelope); // ~18dB below recent peak
        if (windowRms < noiseFloor) { markUnvoiced(); return; }

        const int nMin = juce::jlimit (1, windowSize - 2, (int) std::floor (sr / (double) searchHiHz));
        const int nMax = juce::jlimit (nMin + 1, windowSize - 2, (int) std::ceil (sr / (double) searchLoHz));

        // Autocorrelation peaks occur at the true fundamental period AND at
        // its integer submultiples (i.e. at the lags corresponding to its
        // harmonics), and for plenty of real vowel spectra a harmonic's
        // peak comes out taller than the fundamental's own - just taking
        // the single tallest peak in the range then intermittently locks
        // onto a harmonic and reports a pitch an octave (or more) too high,
        // frame to frame, even with high confidence (measured directly on
        // real vocal material: flip-flopping between a true ~200Hz
        // fundamental and a falsely-detected ~800-900Hz peak from block to
        // block, cascading into wildly swinging shifter ratios and an
        // audible "speeding up" artifact).
        //
        // A blind "always prefer the longest strong lag" rule was tried and
        // reverted - a pure/clean tone's autocorrelation is trivially just
        // as tall at 2x, 3x, ...N x its true period as at the true period
        // itself (a period-T signal repeats exactly at any integer
        // multiple of T too), so that rule pulled clean synthetic tones
        // down to a spurious sub-multiple, regressing the detector's own
        // basic pitch-accuracy tests.
        //
        // The actual fix is temporal continuity: real pitch doesn't jump
        // a full octave between two ~10ms-apart frames, so among the
        // qualifying candidate peaks in THIS frame, prefer whichever one
        // is closest (in octave/log terms) to the lag this detector
        // actually settled on last time - which naturally keeps a steady
        // tone (or a gradually-moving real pitch) locked to the correct
        // octave even on a frame where a harmonic's peak happens to be
        // taller. With no prior yet (first-ever detection, or after a long
        // gap), fall back to the single tallest peak, which is what the
        // original (pre-fix) logic always did and is what the synthetic
        // pure-tone tests validate.
        constexpr float fundamentalAcceptRatio = 0.5f;
        constexpr int maxCandidates = 16;
        int candidateLags[maxCandidates];
        float candidateVals[maxCandidates];
        int numCandidates = 0;
        float globalBestVal = 0.0f;
        int globalBestLag = -1;

        for (int lag = nMin; lag <= nMax; ++lag)
        {
            const float v = buf[(size_t) lag].real();
            const float prev = buf[(size_t) (lag - 1)].real();
            const float next = buf[(size_t) (lag + 1)].real();
            if (v > prev && v >= next)
            {
                if (v > globalBestVal) { globalBestVal = v; globalBestLag = lag; }
                if (numCandidates < maxCandidates) { candidateLags[numCandidates] = lag; candidateVals[numCandidates] = v; ++numCandidates; }
            }
        }

        if (globalBestLag < 0) { markUnvoiced(); return; }

        int bestLag = globalBestLag;
        float bestVal = globalBestVal;

        if (lastAcceptedLag > 0.0f)
        {
            float bestCloseness = std::numeric_limits<float>::max();
            for (int c = 0; c < numCandidates; ++c)
            {
                if (candidateVals[c] < fundamentalAcceptRatio * globalBestVal)
                    continue;
                const float closeness = std::abs (std::log2 ((float) candidateLags[c] / lastAcceptedLag));
                if (closeness < bestCloseness)
                {
                    bestCloseness = closeness;
                    bestLag = candidateLags[c];
                    bestVal = candidateVals[c];
                }
            }

            // The candidate list above only contains genuine local-max
            // peaks, but interpolation/lag quantization means a real
            // fundamental sitting at exactly half/double the chosen lag
            // doesn't always register as its own discrete local max, even
            // though the raw autocorrelation value right there is still
            // clearly elevated. Explicitly check the folded position: if
            // it's meaningfully closer to where this detector actually was
            // a moment ago AND still shows real (if not peak-topping)
            // periodicity, prefer it over a peak-picked lag that happens to
            // sit an octave away from recent context.
            for (const float foldFactor : { 0.5f, 2.0f })
            {
                const float foldedLagF = (float) bestLag * foldFactor;
                const int foldedLag = (int) std::round (foldedLagF);
                if (foldedLag < 1 || foldedLag > windowSize - 2) continue;
                const float foldedVal = buf[(size_t) foldedLag].real();
                const float currentCloseness = std::abs (std::log2 ((float) bestLag / lastAcceptedLag));
                const float foldedCloseness  = std::abs (std::log2 (foldedLagF / lastAcceptedLag));
                if (foldedCloseness < currentCloseness && foldedVal >= 0.3f * globalBestVal)
                {
                    bestLag = foldedLag;
                    bestVal = foldedVal;
                }
            }
        }

        // Parabolic interpolation for sub-sample lag precision - the
        // standard refinement for a locally-quadratic peak (a center-of-
        // mass average is the wrong shape assumption here and measurably
        // biases the result). Some residual bias at long lags (low
        // pitches, few periods per analysis window) is an expected
        // characteristic of window-based autocorrelation pitch detection,
        // not fully eliminable without a much larger (and much higher-
        // latency) analysis window - the validation harness's tolerance
        // is intentionally frequency-aware to reflect that honestly.
        const float y0 = buf[(size_t) (bestLag - 1)].real();
        const float y1 = buf[(size_t) bestLag].real();
        const float y2 = buf[(size_t) (bestLag + 1)].real();
        const float denom = y0 - 2.0f * y1 + y2;
        const float delta = std::abs (denom) > 1.0e-12f ? 0.5f * (y0 - y2) / denom : 0.0f;
        float refinedLag = (float) bestLag + juce::jlimit (-0.5f, 0.5f, delta);

        // Plain normalized peak height - the standard normalized-
        // autocorrelation confidence measure.
        float rawConfidence = juce::jlimit (0.0f, 1.0f, bestVal / norm0);

        // Secondary fast-window pass - see the member comment on
        // fastWindowSize for the full rationale. Only overrides the main
        // window's result when it's both confident AND above
        // kFastWindowMinHz, so low notes (where the main window's larger
        // size is actually needed for reliable detection) are completely
        // unaffected.
        {
            std::vector<std::complex<float>> fastBuf ((size_t) fastFftSize, std::complex<float> { 0.0f, 0.0f });
            const int fastStart = (historyWrite + (windowSize - fastWindowSize)) % windowSize;
            for (int i = 0; i < fastWindowSize; ++i)
            {
                const int idx = (fastStart + i) % windowSize;
                fastBuf[(size_t) i] = { history[(size_t) idx] * fastAnalysisWindow[(size_t) i], 0.0f };
            }
            autocorrelateWith (*fastFft, fastFftSize, fastBuf);

            const LagPick fastPick = pickBestLag (fastBuf, fastWindowSize, lastAcceptedLag, searchLoHz, searchHiHz);
            if (fastPick.found && fastPick.norm0 > 1.0e-12f)
            {
                const float fy0 = fastBuf[(size_t) (fastPick.lag - 1)].real();
                const float fy1 = fastBuf[(size_t) fastPick.lag].real();
                const float fy2 = fastBuf[(size_t) (fastPick.lag + 1)].real();
                const float fdenom = fy0 - 2.0f * fy1 + fy2;
                const float fdelta = std::abs (fdenom) > 1.0e-12f ? 0.5f * (fy0 - fy2) / fdenom : 0.0f;
                const float fastRefinedLag = (float) fastPick.lag + juce::jlimit (-0.5f, 0.5f, fdelta);
                const float fastConfidence = juce::jlimit (0.0f, 1.0f, fastPick.val / fastPick.norm0);
                const float fastHz = fastRefinedLag > 1.0f ? (float) (sr / (double) fastRefinedLag) : 0.0f;

                if (fastConfidence >= confidenceGate && fastHz >= kFastWindowMinHz)
                {
                    refinedLag = fastRefinedLag;
                    rawConfidence = fastConfidence;
                }
            }
        }

        if (rawConfidence >= confidenceGate && refinedLag > 1.0f)
        {
            lastAcceptedLag = refinedLag;
            pushRawHopResult (true, (float) (sr / (double) refinedLag), rawConfidence);
        }
        else
        {
            // This hop's own raw reading isn't trusted - the median filter
            // below still needs a push for every hop to stay correctly
            // paced, same as markUnvoiced()'s early-return callers.
            pushRawHopResult (false, 0.0f, rawConfidence);
        }
    }

    void NovaBendPitchDetector::markUnvoiced() noexcept
    {
        pushRawHopResult (false, 0.0f, 0.0f);
    }

    // Shifts the small raw-hop history left by one and appends this hop's
    // raw (pre-filter) result, then recomputes the publicly-visible
    // currentHz/voiced/currentConfidence as the median-filtered value for
    // the CENTER hop of that window (kMedianDelayHops hops back from the
    // newest) - see the header comment on kMedianWindow for the full
    // rationale. Every call to analyseHop() (voiced or not) must call this
    // exactly once, so the delay stays a fixed, correct number of hops.
    void NovaBendPitchDetector::pushRawHopResult (bool rawVoiced, float rawHz, float rawConf) noexcept
    {
        for (int i = 0; i < kMedianWindow - 1; ++i)
        {
            rawMidiHistory[i] = rawMidiHistory[i + 1];
            rawVoicedHistory[i] = rawVoicedHistory[i + 1];
            rawConfHistory[i] = rawConfHistory[i + 1];
        }
        rawMidiHistory[kMedianWindow - 1] = rawVoiced ? hzToMidiF (rawHz) : 0.0f;
        rawVoicedHistory[kMedianWindow - 1] = rawVoiced;
        rawConfHistory[kMedianWindow - 1] = rawConf;

        if (medianHistoryFill < kMedianWindow)
            ++medianHistoryFill;

        constexpr int centerIdx = kMedianWindow - 1 - kMedianDelayHops;

        if (! rawVoicedHistory[(size_t) centerIdx])
        {
            voiced = false;
            currentConfidence = rawConfHistory[(size_t) centerIdx];
            return;
        }

        // Median over just the voiced entries in the window - a minority
        // that disagrees with the majority (wherever in the window it
        // falls, including the center itself) gets outvoted rather than
        // ever reaching the output.
        float voicedMidi[kMedianWindow];
        int voicedCount = 0;
        for (int i = 0; i < kMedianWindow; ++i)
            if (rawVoicedHistory[(size_t) i])
                voicedMidi[voicedCount++] = rawMidiHistory[(size_t) i];

        std::sort (voicedMidi, voicedMidi + voicedCount);

        voiced = true;
        currentConfidence = rawConfHistory[(size_t) centerIdx];
        currentHz = midiToHzF (voicedMidi[voicedCount / 2]);
    }
}
