#include "NovaBendShifter.h"
#include <cmath>

namespace nova::bend
{
    int NovaBendShifter::nextPowerOfTwo (int v) noexcept
    {
        int p = 1;
        while (p < v) p <<= 1;
        return p;
    }

    void NovaBendShifter::prepare (double sampleRate, bool highQuality) noexcept
    {
        sr = sampleRate;

        // STUDIO: more periods per window (smoother, more latency, more
        // averaging so a noisy periodHintHz matters less). LIVE: fewer
        // (snappier, marginally more sensitive to hint accuracy) - gives
        // Quality a real, audible meaning (matches the "Live < Studio,
        // real DSP difference" contract this plugin's own validation
        // suite already expects).
        //
        // MUST stay an even integer: tap B is offset from tap A by half a
        // window, so an ODD period count puts B's own read a non-integer
        // number of periods away from A's - measured directly: odd values
        // (3, 5) produced 100+ cent errors even with an exactly-correct
        // period hint, while adjacent even values (4, 6, 8) all measured
        // within a few hundredths of a cent. Even values keep the
        // half-window offset an integer number of periods too, so both
        // taps stay phase-aligned with each other, not just individually
        // with the source.
        periodsPerWindow = highQuality ? 6.0f : 4.0f;

        maxWindowSamples = juce::jmax (minWindowSamples,
            (int) std::round (periodsPerWindow * sr / kMinPitchHzForSizing));

        // Generous headroom so the offset range (see processSample()) never
        // gets clamped even at maxWindowSamples.
        delaySize = nextPowerOfTwo ((int) (2.5 * maxWindowSamples) + 4096);
        if (delaySize < 8192) delaySize = 8192;
        delayBuf.assign ((size_t) delaySize, 0.0f);

        // Fixed center every window is built around regardless of its
        // current (adaptive) span - this is what makes getLatencySamples()
        // a true constant. See the class comment.
        centerDelaySamples = (float) maxWindowSamples;
        latencySamples = (int) std::round (centerDelaySamples);

        // ~20ms - fast enough to track a genuine note change in a
        // reasonable time, slow enough that real pitch movement (vibrato,
        // natural jitter) doesn't yank the window size around abruptly.
        windowSmoothAlpha = 1.0f - std::exp (-1.0f / (0.02f * (float) sr));

        reset();
    }

    void NovaBendShifter::reset() noexcept
    {
        std::fill (delayBuf.begin(), delayBuf.end(), 0.0f);
        writeIndex = 0;
        readPos = 0.0f;
        lastRatioDebug = 1.0f;
        currentWindowSamplesF = juce::jlimit ((float) minWindowSamples, (float) maxWindowSamples,
            periodsPerWindow * (float) sr / 220.0f); // sane default until the first real hint arrives
    }

    float NovaBendShifter::readHermite (float pos) const noexcept
    {
        const int base = (int) pos;
        const float frac = pos - (float) base;

        const auto wrapped = [this] (int idx) -> float
        {
            while (idx < 0) idx += delaySize;
            while (idx >= delaySize) idx -= delaySize;
            return delayBuf[(size_t) idx];
        };

        const float xm1 = wrapped (base - 1);
        const float x0  = wrapped (base);
        const float x1  = wrapped (base + 1);
        const float x2  = wrapped (base + 2);

        // Standard 4-point Hermite/Catmull-Rom-style interpolation -
        // smoother than linear, avoids the "gritty" resampling roughness
        // linear interpolation produces whenever the read step isn't
        // exactly 1.0 (i.e. on every real pitch shift).
        const float c0 = x0;
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    float NovaBendShifter::processSample (float input, float ratio, float periodHintHz) noexcept
    {
        delayBuf[(size_t) writeIndex] = input;
        lastRatioDebug = ratio;

        // Window size tracks the hinted period (see class comment) -
        // smoothed so a momentary bad estimate doesn't yank it around.
        const float safeHintHz = periodHintHz > 1.0f ? periodHintHz : 220.0f;
        const float targetWindow = juce::jlimit ((float) minWindowSamples, (float) maxWindowSamples,
            periodsPerWindow * (float) sr / safeHintHz);
        currentWindowSamplesF += windowSmoothAlpha * (targetWindow - currentWindowSamplesF);
        const float windowSamples = currentWindowSamplesF;

        const auto wrap01 = [] (float v) noexcept
        {
            while (v < 0.0f) v += 1.0f;
            while (v >= 1.0f) v -= 1.0f;
            return v;
        };

        // Two taps, half a cycle apart in the crossfade's own phase -
        // while one fades in the other fades out, so summing them (with
        // a raised-cosine/Hann weighting, normalized) reconstructs a
        // continuous signal even though each tap's own read position
        // jumps by windowSamples once per cycle.
        const float phaseA = wrap01 (readPos);
        const float phaseB = wrap01 (phaseA + 0.5f);

        // Centered on the FIXED centerDelaySamples regardless of the
        // current (adaptive) window span - keeps getLatencySamples() a
        // true constant. See class comment.
        const float halfWindow = windowSamples * 0.5f;
        const float minDelay = halfWindow + 24.0f;
        const float maxDelay = (float) delaySize - halfWindow - 24.0f;

        const float offsetA = juce::jlimit (minDelay, maxDelay, centerDelaySamples - halfWindow + phaseA * windowSamples);
        const float offsetB = juce::jlimit (minDelay, maxDelay, centerDelaySamples - halfWindow + phaseB * windowSamples);

        auto readPosA = (float) writeIndex - offsetA;
        auto readPosB = (float) writeIndex - offsetB;
        while (readPosA < 0.0f) readPosA += (float) delaySize;
        while (readPosB < 0.0f) readPosB += (float) delaySize;

        const float sampA = readHermite (readPosA);
        const float sampB = readHermite (readPosB);

        const float winA = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phaseA);
        const float winB = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phaseB);
        const float norm = 1.0f / juce::jmax (1.0e-5f, winA + winB);

        const float output = (sampA * winA + sampB * winB) * norm;

        // Phase advance rate directly encodes the shift: at ratio==1 the
        // phase freezes (0 step) and the two taps settle into a fixed,
        // stable (non-clicking) blend of two constant delay positions -
        // this is fine as a steady-state; the literal Pitch=0/Formant=0
        // case is handled separately at the engine level (see
        // NovaBendEngine.h's NEUTRAL PASSTHROUGH comment).
        const float phaseStep = (1.0f - ratio) / windowSamples;
        readPos = wrap01 (phaseA + phaseStep);

        writeIndex = (writeIndex + 1) % delaySize;
        return output;
    }
}
