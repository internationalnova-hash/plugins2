#pragma once

#include <juce_core/juce_core.h>
#include <vector>

// Stage 3 of the pitch chain: a dual-tap delay-line shifter, adapted from
// Nova Voice's own pitch-shift stage (Nova Voice/Source/PluginProcessor.cpp's
// pitchDelayLeft/Right block) rather than the PSOLA/WSOLA grain-capture
// design this class used previously.
//
// Why the rewrite: the previous design needed a continuously-accurate
// input-period estimate to align its grain CAPTURE POSITION (a
// cross-correlation search picking where, exactly, to start each grain) -
// real vocal material's consonants/sibilance/transients degrade that
// estimate in ways clean synthetic test tones never exposed, and a bad
// match directly corrupted grain alignment, audible as clicking/crackling.
//
// Why a period hint is still used (unlike Nova Voice's own fixed-window
// original): a fixed window size, independent of the input's actual pitch,
// was measured directly (synthetic sine sweep) to produce LARGE, real
// frequency errors - up to several hundred cents - whenever the window
// isn't close to an integer multiple of the input's own period; forcing
// that alignment in a test dropped the error to a couple tenths of a cent.
// Real voice pitch constantly moves, so a truly fixed window can never
// stay matched to it. periodHintHz below lets the window SIZE track the
// input's period - a much gentler dependency than the old design's grain
// ALIGNMENT search: a wrong hint just makes accuracy proportionally worse
// (smoothly, gradually), it cannot desynchronize or click the way a wrong
// WSOLA match could, since the crossfade math itself never depends on the
// hint being right.
//
// A structural benefit carried over from the Nova Voice design: because
// both taps only ever read BACKWARD from the current write position
// (unlike PSOLA's forward grain-injection, which needs the output queued
// and read out only once fully summed), this design's true latency is
// just its own fixed read-back distance, not an added output delay - and
// that reported latency stays a genuine constant even though the window's
// actual span adapts, because the window is always centered on the same
// fixed point (see .cpp) rather than growing outward from it.
//
// Real-time-safe: history buffer sized once in prepare() (message
// thread); processSample() never allocates.
namespace nova::bend
{
    class NovaBendShifter
    {
    public:
        NovaBendShifter() = default;

        // highQuality selects more periods per crossfade window (smoother,
        // more latency, more averaging so a noisy periodHintHz matters
        // less) - LIVE uses fewer periods (snappier, marginally more
        // sensitive to hint accuracy). Both are far below the old PSOLA
        // design's latency either way; see getLatencySamples().
        void prepare (double sampleRate, bool highQuality) noexcept;
        void reset() noexcept;

        // ratio: target/source frequency ratio (semitoneToRatio(pitch) in
        // plain mode, target/detected in Flatten mode). Recomputed and
        // passed in fresh every call - no frozen "locked reference".
        //
        // periodHintHz: the caller's current best estimate of the INPUT's
        // own pitch (NovaBendEngine's already-computed safeInHz - the same
        // detector output used everywhere else, held at its last voiced
        // value during unvoiced material by the detector's own design).
        // Only ever used to size the crossfade window (smoothed - see
        // .cpp - so a momentary bad estimate doesn't cause an abrupt
        // window-size jump); never used for grain alignment/search, so
        // there is no failure mode where a wrong hint corrupts anything
        // beyond gradually reducing accuracy.
        float processSample (float input, float ratio, float periodHintHz) noexcept;

        int   getLatencySamples() const noexcept { return latencySamples; }
        float getCurrentRatioDebug() const noexcept { return lastRatioDebug; }

    private:
        static int nextPowerOfTwo (int v) noexcept;
        float readHermite (float pos) const noexcept;

        double sr = 44100.0;

        std::vector<float> delayBuf;
        int delaySize = 0;
        int writeIndex = 0;

        // How many periods of the (hinted) input pitch each crossfade
        // window should span - the knob that made the window-vs-period
        // alignment test go from several-hundred-cent errors down to a
        // couple tenths of a cent. Set from Quality in prepare().
        float periodsPerWindow = 4.0f;

        // Window span in samples - now ADAPTIVE (periodHintHz-driven,
        // smoothed), not fixed. currentWindowSamplesF is the smoothed
        // float value actually used each sample; windowSmoothAlpha sets
        // how fast it tracks a changing hint (slow enough that real
        // pitch movement doesn't cause an abrupt window-size jump/click,
        // fast enough to track a genuine note change in a reasonable
        // time).
        float currentWindowSamplesF = 0.0f;
        float windowSmoothAlpha = 1.0f;
        int minWindowSamples = 192;
        int maxWindowSamples = 0; // sized in prepare() off the lowest supported pitch

        // Fixed center point every window is built around (see .cpp) -
        // this, not the (now-variable) window span, is what
        // getLatencySamples() reports, so the reported latency stays a
        // true constant regardless of how the window adapts.
        float centerDelaySamples = 0.0f;

        float readPos = 0.0f; // phase accumulator, wraps in [0,1)
        float lastRatioDebug = 1.0f;

        int latencySamples = 0;

        // Floor for window-sizing purposes (NOT a hard input-range limit -
        // input below this just gets clamped to maxWindowSamples, trading
        // some accuracy for staying within a bounded, reasonable latency).
        // 80Hz comfortably covers the vocal range this plugin targets
        // (typical male vocal fundamentals rarely go lower); the previous
        // 60Hz floor pushed STUDIO latency to 100ms, audibly laggy for
        // live monitoring, for headroom the plugin's actual use case
        // doesn't need.
        static constexpr double kMinPitchHzForSizing = 80.0;
    };
}
