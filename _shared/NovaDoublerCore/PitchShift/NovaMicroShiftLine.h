#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>

// Per-voice micro-delay + micro-pitch shifter, mono. One component does
// BOTH jobs at once: the nominal read/write gap gives the micro-delay,
// and the read-head advance rate gives the micro-pitch - adapted from
// Nova Pitch's dual-head TD-PSOLA shifter core (Source/PluginProcessor.cpp
// :459-624 in that project), with its YIN pitch TRACKING removed. Nova
// Pitch as a product was abandoned because tracking a real vocal's pitch
// accurately through fast transitions was never solved - but this class
// never tracks anything: the jump-search "center" period comes from a
// fixed assumed vocal-range fundamental, because all we need is a
// plausible period to search near for an inaudible jump point, not a
// correct one. That sidesteps the exact problem that sank Nova Pitch.
//
// Catmull-Rom (not linear) interpolation is kept deliberately: Nova
// Pitch's own comment on this found that linear interpolation caused
// ~6-cent audible flutter at ratios near 1.0 - exactly the regime this
// class lives in (the whole pitch range here is +-3 to +-12 cents).
//
// Position tracking (writePos/readA/readB) uses double, not float: the
// original Nova Pitch code tracked these as float, which is only exact
// for integers up to 2^24 (~380s at 44.1kHz, ~175s at 96kHz) - past that,
// the gap/drift computation this class's jump logic depends on would
// silently degrade during any real-world session longer than a few
// minutes. double is exact to 2^53, i.e. effectively unbounded here.
//
// Realtime-safe: buffer sized once in prepare() (message thread), never
// resized in processSample().
class NovaMicroShiftLine
{
public:
    NovaMicroShiftLine() = default;

    // maxDelayMs: largest nominal delay this line will ever be asked to
    // hold (drives buffer size) - computed from sample rate, not a fixed
    // constant, so behavior is correct at any of 44.1/48/88.2/96kHz.
    void prepare (double sampleRate, float maxDelayMs) noexcept
    {
        sr = sampleRate;

        // Room for: max nominal delay + jump-search radius + crossfade
        // drift tolerance + a few samples of Catmull-Rom margin. Generous
        // 4x headroom keeps the write head from ever lapping the read
        // heads even under the largest Tightness/Width settings.
        const int neededSamples = (int) std::ceil ((double) maxDelayMs / 1000.0 * sampleRate) * 4 + 256;
        int size = 1;
        while (size < neededSamples) size <<= 1;
        bufSize = size;
        bufMask = size - 1;
        buf.assign ((size_t) bufSize, 0.0f);

        setAssumedFundamentalHz (180.0f); // mid vocal range default

        reset();
    }

    // The jump-search center period comes from this, not live tracking
    // (see class comment). 180Hz sits between typical male/female
    // fundamentals; callers may adjust per voice if desired.
    void setAssumedFundamentalHz (float hz) noexcept
    {
        assumedPeriodSamples = juce::jlimit (16, bufSize / 8, (int) std::round ((float) sr / hz));
    }

    void reset() noexcept
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        writePos = 0.0;
        xfading = false;
        xfade = 0.0f;
        xfadeInc = 0.0f;
        xfadeCooldown = 0;
        // Arbitrary placeholder gap - callers MUST follow reset() with
        // primeDelay(actualTargetSamples) before real signal flows (see
        // that method's comment for why this matters).
        readA = -(double) juce::jmin (assumedPeriodSamples * 8, bufSize / 4);
        readB = 0.0;
    }

    // Jumps the read head directly to the given target gap, with no
    // crossfade - intended to be called once, right after reset()/prepare(),
    // before any real signal has entered the line (silence is playing, so
    // a hard jump is inherently inaudible). Without this, the line starts
    // at reset()'s arbitrary placeholder gap and the normal jump-correction
    // path - designed only to chase SMALL, SLOW drift from the organic
    // modulators during normal playback - has to claw its way there via
    // many small sequential jumps (each corrects only about one assumed
    // pitch period, gated by a cooldown of a similar duration), which
    // measured as needing several hundred milliseconds to converge from a
    // large initial mismatch. Priming the gap directly avoids that
    // startup transient entirely.
    void primeDelay (float targetSamples) noexcept
    {
        readA = writePos - (double) targetSamples;
        readB = readA;
    }

    // targetDelaySamples: desired nominal read/write gap right now (time-
    // varying - this is what gives the micro-delay its slow organic
    // drift). pitchRatio: read-advance speed (1.0 = no shift; e.g.
    // 2^(cents/1200) for a cents-based offset).
    float processSample (float input, float targetDelaySamples, float pitchRatio) noexcept
    {
        buf[(size_t) ((long long) writePos & bufMask)] = input;
        writePos += 1.0;

        const float outA = readCatmullRom (readA);
        float output = outA;

        if (xfading)
        {
            const float outB = readCatmullRom (readB);
            const float w = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * xfade));
            output = outA * (1.0f - w) + outB * w;

            readB += (double) pitchRatio;
            xfade += xfadeInc;

            if (xfade >= 1.0f)
            {
                readA = readB;
                xfading = false;
                xfade = 0.0f;
            }
        }

        readA += (double) pitchRatio;

        if (xfadeCooldown > 0) --xfadeCooldown;

        if (! xfading && xfadeCooldown <= 0)
        {
            const double gap = writePos - readA;
            const double drift = gap - (double) targetDelaySamples;
            const double triggerDrift = (double) assumedPeriodSamples * 3.0;

            if (std::abs (drift) > triggerDrift)
            {
                const int jump = findBestJump (assumedPeriodSamples);
                readB = drift > 0.0 ? readA + (double) jump : readA - (double) jump;
                xfade = 0.0f;
                const int xfadeDur = juce::jmax (64, assumedPeriodSamples * 8);
                xfadeInc = 1.0f / (float) xfadeDur;
                xfading = true;
                readB += (double) pitchRatio;
                xfade += xfadeInc;
                xfadeCooldown = juce::jmax (xfadeDur, (int) (0.03 * sr));
            }
        }

        return output;
    }

    int getBufferSizeSamples() const noexcept { return bufSize; }

    // Diagnostics only (also handy for future debug telemetry).
    double getGapSamples() const noexcept { return writePos - readA; }
    bool isCrossfading() const noexcept { return xfading; }

private:
    float readCatmullRom (double readPos) const noexcept
    {
        const int i1 = ((int) std::floor (readPos)) & bufMask;
        const int i0 = (i1 - 1) & bufMask;
        const int i2 = (i1 + 1) & bufMask;
        const int i3 = (i1 + 2) & bufMask;
        const float t = (float) (readPos - std::floor (readPos));

        const float y0 = buf[(size_t) i0], y1 = buf[(size_t) i1], y2 = buf[(size_t) i2], y3 = buf[(size_t) i3];
        const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        const float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float a2 = -0.5f * y0 + 0.5f * y2;
        const float a3 = y1;
        return ((a0 * t + a1) * t + a2) * t + a3;
    }

    // Autocorrelation search for an inaudible jump point near `center`
    // samples away from the current read head - unchanged in spirit from
    // Nova Pitch's findBestJump(), just against this class's own buffer.
    int findBestJump (int center) const noexcept
    {
        const int radius = juce::jmax (1, center / 4);
        const int anchor = (int) readA;
        float bestCorr = -1.0e30f;
        int best = center;

        for (int d = center - radius; d <= center + radius; ++d)
        {
            if (d < 4) continue;
            float corr = 0.0f;
            const int half = d / 2;
            for (int k = -half; k < half; ++k)
                corr += buf[(size_t) ((anchor + k) & bufMask)] * buf[(size_t) ((anchor + k - d) & bufMask)];
            if (corr > bestCorr) { bestCorr = corr; best = d; }
        }
        return best;
    }

    std::vector<float> buf;
    int bufSize = 0;
    int bufMask = 0;
    double sr = 44100.0;
    int assumedPeriodSamples = 245; // ~180Hz at 44.1kHz, recomputed in setAssumedFundamentalHz

    double writePos = 0.0;
    double readA = 0.0;
    double readB = 0.0;
    bool xfading = false;
    float xfade = 0.0f;
    float xfadeInc = 0.0f;
    int xfadeCooldown = 0;
};
