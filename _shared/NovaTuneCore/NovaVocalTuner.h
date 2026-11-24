#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <complex>
#include <memory>
#include <vector>

// ── Real-time pitch tracker ──────────────────────────────────────────────
// YIN (cumulative-mean-normalised difference) computed with an FFT, first
// dip under threshold, octave-continuity check against the previous note,
// then a 3-hop median. All buffers are sized in prepare(); push() never
// allocates.
//
// Timing is explicit: getDelaySamples() is how far in the past the current
// estimate is centred. The tuner delays its audio by exactly that much so
// every correction is applied to the audio it was measured from - the
// misalignment that made Nova Pitch correct each new note with the previous
// note's correction.
class NovaPitchTracker
{
public:
    void prepare (double sampleRate, bool lowLatency = false);
    void reset() noexcept;

    // Voicing gate on YIN aperiodicity (lower = stricter). Default 0.25.
    void setVoicingThreshold (float t) noexcept { voicing = juce::jlimit (0.05f, 0.6f, t); }
    // Highest pitch searched for (lower ceilings avoid octave-up errors on low voices).
    void setMaxHz (float hz) noexcept { tauMin = juce::jmax (tauMinFloor, (int) std::floor (sr / juce::jmax (100.f, hz))); }

    // Feed one sample. Returns true when a new estimate was produced.
    bool push (float x) noexcept;

    float getHz() const noexcept       { return outHz; } // 0 = unvoiced
    int   getDelaySamples() const noexcept { return delay; }
    int   getMaxPeriodSamples() const noexcept { return tauMax; }
    int   getHopSize() const noexcept { return hop; }

private:
    void analyse() noexcept;

    double sr = 48000.0;
    int W = 768, tauMin = 44, tauMinFloor = 44, tauMax = 686, L = 1454, hop = 128, delay = 0, N = 2048;
    float voicing = 0.25f;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> ring;
    int ringPos = 0, hopCount = 0, filled = 0;

    std::vector<float> frame;
    std::vector<double> prefix;
    std::vector<std::complex<float>> a, b, A, B, r;
    std::vector<float> dp;

    float raw[3] { 0.f, 0.f, 0.f };
    int   rawIdx = 0;
    float lastTau = 0.f;
    int   unvoicedHops = 0;
    float outHz = 0.f;
};

// ── Tuner ────────────────────────────────────────────────────────────────
// Snaps the voice to the nearest note of the chosen scale.
//
// Shifter: pitch-synchronous resampling through a delay line - the classic
// hard-tune method. A read head moves through the delayed audio at the
// correction ratio; when it drifts half a pitch period from centre it jumps
// by exactly one (refined) period, crossfaded, so the waveform stays
// continuous. Corrections are at most half a semitone, so the formant shift
// this method causes is under 3 % - inaudible.
//
// Latency is constant (= the tracker's delay) and reported to the host.
class NovaVocalTuner
{
public:
    struct Params
    {
        bool  enabled   = false;
        float amount    = 0.f;   // 0..1 of the correction
        float retuneMs  = 0.f;   // 0 = instant (hard), larger = natural
        int   keyPc     = 0;
        int   scaleMask = 0xFFF; // 12-bit pitch-class mask relative to keyPc

        // Character controls (used by Nova Pitch). Defaults are all "off",
        // which is exactly the behaviour Nova AI Vocal was tuned with.
        float vibratoPreserve  = 0.f;    // 0..1: keep natural vibrato, correct the note centre
        float vibratoRateHz    = 5.f;    // vibrato speed to preserve
        float humanize         = 0.f;    // 0..1: let notes already close to pitch through
        float transientHold    = 0.f;    // 0..1: ease off correction on note attacks
        float voicingThreshold = 0.25f;  // pitch-confidence gate (lower = stricter)
        float maxHz            = 1100.f; // highest pitch to look for

        // Pitch-shift modes (used by Nova Bend). Defaults = off.
        float fixedShiftSemis  = 0.f;    // constant shift added on top (consonants too)
        int   flattenMidi      = -1;     // >= 0: lock every sung note onto this one note
    };

    // tracking = Tracking mode for recording: the audio is corrected as it arrives, using the
    // newest pitch reading, so it comes out a few ms late instead of a full detection window
    // (corrections at note changes land a little later). Off = the aligned, full-accuracy tuner.
    void prepare (double sampleRate, int maxBlockSize, bool lowLatency = false, bool tracking = false);
    void reset() noexcept;
    void setParams (const Params& p) noexcept { params = p; }
    void process (float* left, float* right, int numSamples) noexcept;

    int getLatencySamples() const noexcept { return latency; }

    // For the UI: what it hears and what it is snapping to (0 = unvoiced)
    float getInputMidi() const noexcept  { return inMidiMeter.load (std::memory_order_relaxed); }
    float getTargetMidi() const noexcept { return targetMeter.load (std::memory_order_relaxed); }
    float getCurrentRatio() const noexcept { return ratioMeter.load (std::memory_order_relaxed); }
    float getCurrentShiftSemis() const noexcept { return 12.f * std::log2 (juce::jmax (1.0e-3f, getCurrentRatio())); }

    // Key mapping helpers shared with the processor
    static int scaleMaskFor (bool minor) noexcept;

    // Tune knob (0..1) -> params. The first quarter fades the correction in;
    // above that the retune speeds up from natural (200 ms) to instant.
    static Params fromTuneKnob (float tune, int keyPc, int scaleMask) noexcept;

private:
    int    chooseNote (float midi) noexcept;
    float  readCubic (const std::vector<float>& buf, double pos) const noexcept;
    double refinePeriod (double fromPos, int dir, double period) const noexcept;
    void   startJump (double newPos, int fade) noexcept;

    NovaPitchTracker tracker;
    Params params;

    double sr = 48000.0;
    int latency = 0, size = 0, mask = 0;
    bool trackingMode = false;
    int trackMin = 0;     // Tracking mode: closest the read head may get to the live input
    std::vector<float> bufL, bufR, bufM;

    juce::int64 w = 0;    // absolute write count
    double rp = 0.0;      // head A read position (absolute)
    double rpB = 0.0;     // head B during a jump crossfade
    int fadeLen = 0, fadePos = -1;

    int   heldNote = -1000;
    int   pendingNote = -1000, pendingCount = 0, dwellHops = 8;
    int   unvoicedSamples = 0;
    float prevMidi = 0.f, curMidi = 0.f; // last two estimates, interpolated between
    int   sinceEstimate = 0, hopSize = 128, currentNote = 0;
    float corr = 0.f;        // smoothed semitones actually applied
    float period = 0.f;      // samples, last voiced

    // Character controls
    float midiSlow = 0.f, slowNote = -1000.f;
    float fastEnv = 0.f, slowEnv = 0.f, fastAtt = 0.f, fastRel = 0.f, slowAtt = 0.f, slowRel = 0.f;

    std::atomic<float> inMidiMeter { 0.f }, targetMeter { 0.f }, ratioMeter { 1.f };
};

// Offline helper for Listen: musical key from a captured vocal.
struct NovaKeyEstimate
{
    bool  valid = false;
    int   keyPc = 0;
    bool  minor = false;
    float confidence = 0.f;
};

NovaKeyEstimate estimateNovaKey (const float* mono, int numSamples, double sampleRate);
