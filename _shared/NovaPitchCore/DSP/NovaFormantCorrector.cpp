#include "NovaFormantCorrector.h"
#include <cmath>
#include <algorithm>

namespace nova::pitch
{
    void NovaFormantCorrector::prepare (double sampleRate) noexcept
    {
        sr = sampleRate;

        analysisFrame.assign ((size_t) kFrameSize, 0.0f);
        analysisWindowLPC.assign ((size_t) kFrameSize, 0.0f);
        orderedFrame.assign ((size_t) kFrameSize, 0.0f);
        resampleScratch.assign ((size_t) kFrameSize, 0.0f);

        for (int i = 0; i < kFrameSize; ++i)
            analysisWindowLPC[(size_t) i] = 0.54f - 0.46f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                                        * (float) i / (float) (kFrameSize - 1));

        reset();
    }

    void NovaFormantCorrector::reset() noexcept
    {
        std::fill (analysisFrame.begin(), analysisFrame.end(), 0.0f);
        frameWrite = 0;
        hopCounter = 0;

        std::fill (std::begin (analysisCoeffs), std::end (analysisCoeffs), 0.0f);
        std::fill (std::begin (synthesisReflection), std::end (synthesisReflection), 0.0f);
        std::fill (std::begin (preHistory), std::end (preHistory), 0.0f);
        std::fill (std::begin (latticeState), std::end (latticeState), 0.0f);
    }

    void NovaFormantCorrector::setFormantAmount (float amount0to1) noexcept
    {
        amount = juce::jlimit (0.0f, 1.0f, amount0to1);
    }

    void NovaFormantCorrector::setFormantWarp (float warpNormalizedMinus1to1) noexcept
    {
        const float w = juce::jlimit (-1.0f, 1.0f, warpNormalizedMinus1to1);
        // Bounded to roughly a third of an octave (~4 semitones) either
        // way at the extremes - a creative-range control, not a wide
        // spectral remap.
        warpFactor = std::pow (2.0f, w / 3.0f);
    }

    float NovaFormantCorrector::sampleChronological (float pos) const noexcept
    {
        pos = juce::jlimit (0.0f, (float) (kFrameSize - 1), pos);
        const int i0 = (int) pos;
        const int i1 = juce::jmin (kFrameSize - 1, i0 + 1);
        const float frac = pos - (float) i0;
        const float s0 = analysisFrame[(size_t) ((frameWrite + i0) % kFrameSize)];
        const float s1 = analysisFrame[(size_t) ((frameWrite + i1) % kFrameSize)];
        return s0 + frac * (s1 - s0);
    }

    void NovaFormantCorrector::autocorrelateFrame (const float* frame, float* outAc) noexcept
    {
        for (int lag = 0; lag <= kOrder; ++lag)
        {
            float sum = 0.0f;
            for (int i = 0; i + lag < kFrameSize; ++i)
                sum += frame[i] * frame[i + lag];
            outAc[lag] = sum;
        }
    }

    // Standard Levinson-Durbin recursion (textbook algorithm, independent
    // of any third-party source): converts an autocorrelation sequence
    // into linear-prediction coefficients minimizing prediction error.
    // Outputs both the direct-form coefficients (outCoeffs, used by the
    // FIR analysis/whitening filter in processPre - unconditionally stable
    // regardless of coefficient magnitude) and the raw reflection
    // coefficients (outReflection, used by the LATTICE synthesis filter in
    // processPost). |k|<1 alone only guarantees stability for the direct
    // form in exact arithmetic - a real vocal transient can still produce
    // direct-form coefficients large enough to make that recursive filter
    // numerically fragile in float32 (measured directly: coefficient
    // magnitude ~18 was enough to ring processPost() out to Inf/NaN within
    // a quarter second of real material). The lattice form doesn't have
    // that failure mode: each stage's stability depends only on that
    // stage's own clamped |k|<1, with no equivalent of a coefficient
    // magnitude to blow up, so processPost() is built on the reflection
    // coefficients directly instead of converting to direct form.
    void NovaFormantCorrector::levinsonDurbin (const float* autocorr, float* outCoeffs, float* outReflection) noexcept
    {
        float a[kOrder] {};
        float tmp[kOrder] {};
        float k_all[kOrder] {};
        float error = autocorr[0];

        if (error <= 1.0e-12f)
        {
            std::fill (outCoeffs, outCoeffs + kOrder, 0.0f);
            std::fill (outReflection, outReflection + kOrder, 0.0f);
            return;
        }

        for (int i = 1; i <= kOrder; ++i)
        {
            float acc = autocorr[i];
            for (int j = 1; j < i; ++j)
                acc -= a[j - 1] * autocorr[i - j];

            float k = error > 1.0e-12f ? acc / error : 0.0f;
            // Clamped well short of +-1: the lattice form built from these
            // reflection coefficients is unconditionally stable for any
            // |k|<1 in exact arithmetic, but a k sitting right at the edge
            // is still an extremely high-Q resonance - measured directly
            // on real vocal material at full Formant amount, a k that
            // would otherwise land near 0.999 produced a technically
            // stable (finite, no NaN) but 500x+ gain spike on a sustained
            // note. Tightening the clamp itself (rather than uniformly
            // shrinking every k afterwards) only pulls back the few
            // frames that would otherwise sit right on the dangerous edge
            // - typical, well-conditioned coefficients are far enough from
            // +-0.97 that this clamp never engages for them at all, so
            // round-trip fidelity on normal material is essentially
            // untouched.
            k = juce::jlimit (-0.97f, 0.97f, k);
            k_all[i - 1] = k;

            std::copy (std::begin (a), std::end (a), std::begin (tmp));
            a[i - 1] = k;
            for (int j = 1; j < i; ++j)
                a[j - 1] = tmp[j - 1] - k * tmp[i - j - 1];

            error *= (1.0f - k * k);
            if (error <= 1.0e-9f) error = 1.0e-9f;
        }

        std::copy (std::begin (a), std::end (a), outCoeffs);
        std::copy (std::begin (k_all), std::end (k_all), outReflection);
    }

    void NovaFormantCorrector::updateCoefficients() noexcept
    {
        for (int i = 0; i < kFrameSize; ++i)
            orderedFrame[(size_t) i] = analysisFrame[(size_t) ((frameWrite + i) % kFrameSize)] * analysisWindowLPC[(size_t) i];

        float ac[kOrder + 1];
        autocorrelateFrame (orderedFrame.data(), ac);

        float unusedReflection[kOrder];
        levinsonDurbin (ac, analysisCoeffs, unusedReflection);

        if (std::abs (warpFactor - 1.0f) < 1.0e-4f)
        {
            // Neutral warp: synthesis reflection coefficients come from the
            // same (unwarped) frame as the analysis coefficients, so
            // processPost(processPre(x)) is a clean, unwarped
            // analysis/resynthesis round trip.
            std::copy (std::begin (unusedReflection), std::end (unusedReflection), std::begin (synthesisReflection));
            return;
        }

        for (int i = 0; i < kFrameSize; ++i)
            resampleScratch[(size_t) i] = sampleChronological ((float) i * warpFactor) * analysisWindowLPC[(size_t) i];

        float ac2[kOrder + 1];
        autocorrelateFrame (resampleScratch.data(), ac2);

        float unusedCoeffs[kOrder];
        levinsonDurbin (ac2, unusedCoeffs, synthesisReflection);
    }

    void NovaFormantCorrector::pushAnalysisSample (float x) noexcept
    {
        analysisFrame[(size_t) frameWrite] = x;
        frameWrite = (frameWrite + 1) % kFrameSize;

        if (++hopCounter >= kHopSize)
        {
            hopCounter = 0;
            updateCoefficients();
        }
    }

    float NovaFormantCorrector::processPre (float input) noexcept
    {
        pushAnalysisSample (input);

        float prediction = 0.0f;
        for (int j = 0; j < kOrder; ++j)
            prediction += analysisCoeffs[(size_t) j] * amount * preHistory[(size_t) j];

        const float residual = input - prediction;

        for (int j = kOrder - 1; j > 0; --j)
            preHistory[(size_t) j] = preHistory[(size_t) (j - 1)];
        preHistory[0] = input;

        return residual;
    }

    float NovaFormantCorrector::processPost (float shiftedInput) noexcept
    {
        // All-pole LATTICE synthesis, built directly from the reflection
        // coefficients (not the direct-form a[] used by processPre) -
        // see the levinsonDurbin() comment for why. Standard textbook
        // recursion (e.g. Rabiner & Schafer, "Digital Processing of Speech
        // Signals"): running the analysis lattice
        //   f[i] = f[i-1] - k[i]*g[i-1](prev)
        //   g[i] =        -k[i]*f[i]     + g[i-1](prev)
        // in reverse (i = order..1) and solving for f[i-1] inverts it
        // exactly, reconstructing x[n] from the residual e[n]:
        //   f[i-1] = f[i] + k[i]*g[i-1](prev)
        //   g[i]   = -k[i]*f[i-1] + g[i-1](prev)
        // x[n] = f[0]. latticeState[m] holds g[m][n-1] between calls - each
        // stage's own single-sample delay, no direct-form coefficient
        // feedback anywhere, so this is unconditionally stable for any
        // |k|<1 regardless of how the reflection coefficients happen to
        // land on a given frame.
        // gNew[m] will hold g[m] for this sample (m = 1..kOrder from the loop,
        // plus g[0] = output, assigned separately below) - note the state
        // produced at stage m is g[m], not g[m-1], even though the loop
        // reads g[m-1] as its "previous" input each iteration.
        float f = shiftedInput; // f[kOrder]
        float gNew[kOrder + 1];
        for (int m = kOrder; m >= 1; --m)
        {
            const float k = synthesisReflection[(size_t) (m - 1)] * amount;
            const float gPrev = latticeState[(size_t) (m - 1)]; // g[m-1] from the previous sample
            const float fPrev = f + k * gPrev;                  // f[m-1]
            gNew[m] = -k * fPrev + gPrev;                       // g[m] for this sample
            f = fPrev;
        }

        float output = f; // f[0] = x[n]
        gNew[0] = output; // g[0] = x[n], by definition

        // Belt-and-suspenders: the lattice is unconditionally stable by
        // construction given |k|<0.999 (already enforced in
        // levinsonDurbin()), so this should never fire - but a real-time
        // audio filter should never be able to latch into a non-finite
        // state regardless of how confident that guarantee is.
        // "Unconditionally stable" only holds in exact arithmetic - a
        // reflection coefficient sitting right at the clamp boundary can
        // still produce a technically-finite but enormous resonance spike
        // (measured directly: 500x+ gain on real material even after the
        // bandwidth expansion above). A threshold relative to the
        // instantaneous input doesn't work here - a resonant filter is
        // SUPPOSED to keep ringing through the quiet gaps between an
        // impulsive excitation's pulses, briefly outputting far more than
        // whatever the input happens to be at that exact sample - so this
        // is a fixed absolute ceiling instead, well above any legitimate
        // peak seen on either real vocal material or the synthetic
        // stress-test signal, but well below a genuine blow-up.
        if (! std::isfinite (output) || std::abs (output) > 20.0f)
        {
            output = shiftedInput;
            std::fill (std::begin (latticeState), std::end (latticeState), 0.0f);
            return output;
        }

        // Only g[0..kOrder-1] are ever read back as "previous" state
        // (g[kOrder] would only matter to a stage kOrder+1, which doesn't
        // exist), so that's all that needs to persist.
        for (int m = 0; m < kOrder; ++m)
            latticeState[(size_t) m] = gNew[m];

        return output;
    }
}
