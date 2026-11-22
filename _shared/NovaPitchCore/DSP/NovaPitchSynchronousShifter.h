#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <functional>

// Stage 3 of the pitch-correction chain: pitch-SYNCHRONOUS time-domain
// overlap-add shifting (classic TD-PSOLA-style grain OLA - Moulines &
// Charpentier's well-known, widely-published technique, independently
// implemented here, not a port of any third-party source).
//
// Two independent phase accumulators track input and output pitch
// periods. Every time the INPUT phase wraps (one input period has
// elapsed), a fresh grain - centered one input-period behind "now", so
// it's always fully already-written history - is captured from the
// circular input buffer. Every time the OUTPUT phase wraps (one output
// period has elapsed), the most-recently-captured grain is Hann-windowed
// and overlap-added into the output buffer at a fixed constant delay
// ahead of "now". The pitch shift comes ENTIRELY from the difference
// between how often grains are captured (input rate) vs. how often they're
// repeated (output rate) - grain content itself is never resampled/
// interpolated, which is what keeps this free of the phase-vocoder-style
// phasiness/smearing that a frequency-domain shifter would introduce on
// monophonic material.
//
// Latency is a FIXED constant (declared via getLatencySamples(), set once
// in prepare() from the lowest supported pitch) - it does not vary with
// the actual input, so it's safe to report once via
// AudioProcessor::setLatencySamples() and never change it at runtime.
//
// Real-time-safe: all buffers sized once in prepare() (message thread);
// processSample() never allocates.
namespace nova::pitch
{
    class NovaPitchSynchronousShifter
    {
    public:
        NovaPitchSynchronousShifter() = default;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        // inputPitchHz/outputPitchHz: current input and target pitch,
        // recomputed and passed in fresh every call - there is no frozen
        // "locked reference" concept anywhere in this class, which is what
        // lets it track fast pitch movement (including the singer's own
        // vibrato) instead of only being able to shift a constant average.
        float processSample (float input, float inputPitchHz, float outputPitchHz) noexcept;

        int   getLatencySamples() const noexcept { return outputDelaySamples; }
        float getCurrentRatioDebug() const noexcept { return lastRatioDebug; }

        // Forensic instrumentation only - nullptr by default (a single
        // branch check per capture event, i.e. once per input period, not
        // per sample - negligible even in the worst case) and never set
        // by the real-time plugin path. Only NovaPitchOfflineDiag wires
        // this up, to log every individual grain-capture decision (WSOLA
        // offset chosen, its confidence score, and the captured grain's
        // OWN independently-measured pitch vs. what the phase accumulator
        // assumed) for direct inspection at flagged real-world timestamps
        // - added specifically because two separate real hosts (FL Studio/
        // VST3, Pro Tools/AAX) both reproduce a "chorus" artifact that a
        // clean, cold-started offline render of the identical settings and
        // source never does, and the reason why is not yet understood.
        struct CaptureDebugInfo
        {
            long long sampleIndex = 0;
            int periodInSamples = 0;
            int wsolaOffset = 0;
            float wsolaBestScore = 0.0f;
            float wsolaBaselineScore = 0.0f;
            float measuredGrainHz = 0.0f; // independently autocorrelated from the captured grain itself
            float assumedHz = 0.0f;       // sr / periodInSamples, i.e. what the phase accumulator assumed
        };
        std::function<void (const CaptureDebugInfo&)> onGrainCaptured;

    private:
        static int nextPowerOfTwo (int v) noexcept;
        int wrapIndex (long long v) const noexcept;

        double sr = 44100.0;

        std::vector<float> historyBuf;
        std::vector<float> outputBuf;
        std::vector<float> grainBuf;
        int bufSize = 0;

        int maxGrainHalf = 0;
        int outputDelaySamples = 0;
        int captureHalfLen = 0; // half-width of the grain currently held in grainBuf, at capture time

        // WSOLA-style capture-position refinement: the input-phase wrap
        // gives a mathematically-computed grain center (n - captureHalfLen),
        // but that position doesn't know where the real waveform's own
        // local cycle actually starts - well documented in the PSOLA
        // literature as a source of roughness/instability, since real
        // voice always has some cycle-to-cycle jitter the computed period
        // alone can't see. Before capturing, a small search around that
        // computed center looks for the offset that makes the new grain
        // most waveform-similar (via normalized cross-correlation) to the
        // previously captured grain, keeping consecutive grains phase-
        // continuous with each other without needing full glottal-epoch
        // detection.
        std::vector<float> prevGrainBuf;
        int prevGrainHalfLen = 0;
        bool havePrevGrain = false;

        double phaseIn = 0.0;
        double phaseOut = 0.0;
        long long n = 0;

        float lastRatioDebug = 1.0f;

        // Output-side smoothing: the quantizer can now (correctly) commit
        // to a new target within a single hop, but the OUTPUT phase
        // accumulator and grain-injection resample rate both read the
        // target pitch directly - a target that jumps abruptly makes the
        // injection cadence AND the per-grain resample ratio jump right
        // along with it, audible as a grainy/warbly "landing" on the new
        // note rather than a clean one (confirmed directly against user
        // report: correct note, correct timing, but not a clean landing
        // "like in Neptune Light"). A short one-pole smoother on the
        // OUTPUT pitch only - capture/input side is untouched, so grain
        // content itself stays fully faithful to the real input - lets the
        // injection cadence and resample rate change gradually over a few
        // milliseconds instead of instantly. Short enough (see prepare())
        // to not meaningfully reintroduce the commit-lag this session
        // already fixed.
        double smoothedOutHz = 0.0;
        float outHzSmoothAlpha = 1.0f;

        // Capture-side smoothing, much lighter than the output-side one
        // above: during active pitch movement (a real glide/portamento),
        // consecutive captured grains are drawn from genuinely different
        // points in a moving pitch, and since two grains always overlap
        // during OLA reconstruction, summing two legitimately different
        // frequencies together is mechanically the same thing that
        // produces a chorus/beating effect - reported directly by the
        // user as "a little bit of chorus" specifically during vocal
        // movement, absent on sustained notes (where consecutive grains
        // barely differ, so this doesn't apply). This doesn't touch grain
        // CONTENT - historyBuf is still read raw/unsmoothed - only the
        // capture-rate phase accumulator and the resulting grain-width
        // estimate are smoothed, so consecutive grains' assumed period
        // drifts a little less abruptly from one capture to the next.
        // Much shorter than the output smoother (that one absorbs a full
        // note-change; this one only needs to round off within-glide
        // jitter) so real pitch tracking speed is barely affected.
        double smoothedInHz = 0.0;
        float inHzSmoothAlpha = 1.0f;

        static constexpr double kMinPitchHz = 50.0;
        static constexpr double kMaxPitchHz = 1200.0;

        // Overlap factor R: how many grains are alive (summed) at any
        // instant. R=2 (window width = 2x hop) is the original/minimum
        // COLA-compliant Hann configuration - hop here is fixed at exactly
        // one period (one injection per output-phase wrap, one capture per
        // input-phase wrap; THAT rate is what defines pitch and must never
        // change). Raising R only widens the window (and, symmetrically,
        // the captured grain) so more overlapping copies sum together,
        // which is the standard, well-documented way to reduce OLA
        // amplitude-modulation "beating" between grains that carry
        // genuinely different frequency content (i.e. during real,
        // continuously-moving pitch, not just abrupt note changes) -
        // proven directly via spectrogram comparison against a reference
        // plugin as this scheme's residual "chorus"/"grainy" artifact.
        //
        // For a Hann window w(t)=0.5-0.5cos(2*pi*t/T), summing R copies
        // spaced at hop H=T/R (R integer >= 2) sums to the EXACT constant
        // 0.5*R at every point (the R hop-phases are the R-th roots of
        // unity, which cancel identically in the cosine sum, for ANY
        // integer R - not just R=2) - so raising R needs a compensating
        // output gain of exactly 2/R to keep amplitude unity; the R=2 case
        // (gain=1, i.e. no compensation) is why the original code never
        // needed one.
        //
        // A first attempt at "more overlap" instead doubled the INJECTION
        // RATE (how often phaseOut wraps) while leaving the window width
        // alone - that's a different, incorrect axis: injection rate IS
        // literally what defines output pitch here (grain content is never
        // resampled to a different rate, only repeated at the target
        // rate), so doubling it doubled the fundamental frequency outright
        // (measured: catastrophic multi-hundred-cent pitch errors). This
        // R-based design only ever changes WINDOW WIDTH (and, to keep the
        // read-step traversal valid, the CAPTURED GRAIN width by the same
        // factor) - both the capture rate and the injection rate are
        // completely untouched, so pitch accuracy is structurally
        // unaffected by R.
        static constexpr int kOverlapFactor = 2;               // R, must be even (>=2)
        static constexpr int kHalfLenMultiplier = kOverlapFactor / 2; // widens window+grain vs. R=2 baseline
        static constexpr float kOverlapGainCompensation = 2.0f / (float) kOverlapFactor;
    };
}
