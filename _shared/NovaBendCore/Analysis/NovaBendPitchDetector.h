#pragma once

#include "../NovaBendTypes.h"
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <complex>
#include <memory>
#include <vector>

// Stage 1 of the pitch-correction chain: real-time monophonic pitch
// detection via FFT-based autocorrelation (Wiener-Khinchin: window -> FFT
// -> magnitude-squared -> inverse FFT), with a confidence gate that holds
// the last VOICED pitch across quiet/unvoiced gaps instead of chasing
// noise. This replaces Nova Pitch's previous time-domain YIN detector -
// independently implemented from the general technique (this is not a
// port of any third-party source).
//
// Runs analysis every windowSize/4 samples (decoupled from the host's
// block size via an internal hop counter) on a windowSize-sample history
// (2048 samples, or 4096 above 88.2kHz unless lowLatencyMode forces 2048
// regardless of rate). All buffers are sized once in prepare() (message
// thread) - pushBlock() never allocates.
namespace nova::bend
{
    class NovaBendPitchDetector
    {
    public:
        NovaBendPitchDetector() = default;

        void prepare (double sampleRate, bool lowLatencyMode) noexcept;
        void reset() noexcept;

        // 0..1 normalized. Below this, a hop's estimate is treated as
        // unvoiced and getCurrentHz() keeps returning the last voiced value.
        void setConfidenceGate (float threshold0to1) noexcept;

        // Lag-search bounds in Hz - the "Range" control biases these
        // (narrower/lower ceiling reduces high-octave false positives on
        // low voices; wider ceiling covers soprano range).
        void setSearchRange (float loHz, float hiHz) noexcept;

        void pushBlock (const float* mono, int numSamples) noexcept;

        float getCurrentHz() const noexcept { return currentHz; }
        float getConfidence() const noexcept { return currentConfidence; }
        bool  isVoiced() const noexcept { return voiced; }
        int   getAnalysisWindowSize() const noexcept { return windowSize; }
        // Includes the median-filter's own lookahead delay (see
        // kMedianWindow below) - callers that align a dry signal against
        // this detector's output need the true total delay, not just the
        // analysis window's own inherent lag.
        int   getLatencySamples() const noexcept { return windowSize / 2 + kMedianDelayHops * hopSize; }

    private:
        static int chooseWindowSize (double sampleRate, bool lowLatencyMode) noexcept;

        void autocorrelate (std::vector<std::complex<float>>& buf) const;
        void autocorrelateWith (juce::dsp::FFT& fftToUse, int fftLen, std::vector<std::complex<float>>& buf) const;

        struct LagPick { int lag = -1; float val = 0.0f; float norm0 = 0.0f; bool found = false; };
        // Shared candidate-peak selection (continuity + octave-fold bias,
        // see analyseHop()'s header comment for the rationale) - factored
        // out so the secondary fast window (below) can reuse the exact
        // same, already-validated logic instead of a parallel copy.
        LagPick pickBestLag (const std::vector<std::complex<float>>& buf, int winSize, float lastLag, float loHz, float hiHz) const noexcept;

        void analyseHop();
        void markUnvoiced() noexcept;
        void pushRawHopResult (bool rawVoiced, float rawHz, float rawConf) noexcept;

        double sr = 44100.0;
        int windowSize = 2048;
        int fftSize = 4096; // 2x windowSize, zero-padded - avoids circular-autocorrelation wraparound
        int hopSize = 512;
        int hopCounter = 0;

        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> history;
        int historyWrite = 0;

        std::vector<float> analysisWindow;

        // Secondary, half-length analysis window - drawn from the most
        // recent fastWindowSize samples of the same history buffer (no
        // separate storage needed). The main window above is sized for
        // reliable detection down to searchLoHz (typically 70Hz), but that
        // same size means it straddles a genuine note-to-note transition
        // for a meaningful fraction of any note shorter than about its own
        // length - measured directly on real material: fast/melismatic
        // passages (notes as short as 60-100ms) landed on a smeared,
        // chromatic-passing pitch between the real notes instead of
        // confidently snapping to either one, while the same passage's
        // sustained notes (each comfortably longer than one window)
        // tracked cleanly. This half-length window resolves fast
        // transitions with roughly half the smearing, at the cost of
        // needing a higher minimum frequency to stay reliable with fewer
        // periods per window - see kFastWindowMinHz in analyseHop(). Below
        // that floor, the main window (already validated down to 70Hz)
        // still owns detection entirely unchanged.
        int fastWindowSize = 1024;
        int fastFftSize = 2048;
        std::unique_ptr<juce::dsp::FFT> fastFft;
        std::vector<float> fastAnalysisWindow;
        static constexpr float kFastWindowMinHz = 180.0f;

        float searchLoHz = 70.0f;
        float searchHiHz = 700.0f;
        float confidenceGate = 0.7f;

        float currentHz = 0.0f;
        float currentConfidence = 0.0f;
        bool  voiced = false;
        float lastAcceptedLag = 0.0f; // most recent chosen peak lag - used as a continuity prior to disambiguate octave-related candidate peaks (see analyseHop())
        float recentPeakEnvelope = 0.0f; // slow peak-hold of windowRms - the noise-floor gate in analyseHop() is relative to this, not a fixed absolute number, so it adapts to whatever gain-staging a given take happens to use

        // Short median filter over recent hops' RAW (pre-filter) results,
        // reporting a value delayed by kMedianDelayHops hops (a centered
        // median needs that many hops of "future" context before it can
        // report the center hop's result). Continuity-based candidate
        // selection in analyseHop() can only choose among whatever peaks
        // exist in ONE frame - if the true fundamental's peak is genuinely
        // absent from that single frame (common during a fast consonant
        // transition), there's no good candidate to pick, and that one
        // hop's raw result is wrong even with continuity biasing. A median
        // over a few hops is immune to that: as long as a majority of the
        // window is correct, one wrong hop - wherever it falls in the
        // window - gets outvoted rather than ever reaching the output.
        // This is the same principle commercial pitch correction uses to
        // avoid single-frame glitches; the tradeoff is a modest amount of
        // added reaction latency (kMedianDelayHops hops), reflected in
        // getLatencySamples() above.
        // 3 rather than the originally-tuned 5: still outvotes a single
        // bad hop (2-of-3 majority), which was the whole point (see the
        // header comment above), but at half the added lookahead latency -
        // real-material testing showed this latency was a meaningful
        // fraction of how long a genuine note-to-note transition takes to
        // fully snap, directly trading against how "tight"/decisive the
        // correction feels on fast passages. Less robust to two
        // CONSECUTIVE bad hops than the 5-tap version was, which is the
        // real tradeoff - re-verify the zero-single-frame-outlier result
        // this was built to guarantee before trusting this change.
        static constexpr int kMedianWindow = 3;
        static constexpr int kMedianDelayHops = kMedianWindow / 2; // = 2
        float rawMidiHistory[kMedianWindow] {};
        bool  rawVoicedHistory[kMedianWindow] {};
        float rawConfHistory[kMedianWindow] {};
        int   medianHistoryFill = 0; // how many real hops have been pushed since reset() (ramps up to kMedianWindow)
    };
}
