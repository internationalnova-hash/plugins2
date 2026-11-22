#pragma once

#include "NovaBendTypes.h"
#include "Analysis/NovaBendPitchDetector.h"
#include "DSP/NovaBendFlattenTarget.h"
#include "DSP/NovaBendShifter.h"
#include "DSP/NovaBendFormantProcessor.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// Top-level facade tying PITCH/FORMANT/FLATTEN/LINK/MIX/BYPASS together -
// the only class PluginProcessor talks to for audio.
//
// ENGINE HISTORY / WHY THE SHIFTER'S DETECTOR DEPENDENCY CHANGED: the
// original design fed the pitch detector's live estimate into the shifter
// as an ALIGNMENT reference (a WSOLA cross-correlation search deciding
// exactly where each grain starts). On real vocal material (consonants,
// sibilance, transients) that live estimate degrades in ways clean
// synthetic test tones never exposed, and a bad match directly corrupted
// grain alignment - confirmed as the source of user-reported clicking/
// crackling, and independently confirmed present in Nova Pitch's own
// original shared DSP core too (not something introduced only by adapting
// it for Bend). NovaBendShifter was rewritten around Nova Voice's
// proven-clean dual-tap crossfade technique, which needs no alignment
// search at all - but a synthetic sine-sweep test found that design's own
// FIXED window size produces large (hundreds of cents) frequency errors on
// arbitrary real pitch content, since accuracy depends on the window being
// close to an integer multiple of the actual input period. The fix kept:
// the detector's estimate (safeInHz below) now only SIZES the shifter's
// crossfade window (smoothed inside NovaBendShifter), never controls
// alignment/search - a wrong estimate just makes accuracy gradually worse,
// it cannot desynchronize or click the way a bad WSOLA match could.
// Verified directly: even a drastic, instantaneous, wrong window-size hint
// produces zero instability, only reduced accuracy. The detector still
// runs UNCONDITIONALLY every sample - needed for Flatten's target-note
// tracking, the getDetectedHz() debug getter, AND now the shifter's window
// sizing in every mode - but the RATIO applied outside Flatten no longer
// depends on it at all (see the plain-mode branch in process() below).
//
// Per-sample flow:
//   mono-sum -> detector (always, but only READ by Flatten) -> [Flatten:
//   flatten-target ratio, else: direct pitch-knob ratio] -> formant pre
//   (L/R) -> delay-line shifter (L/R) -> formant post (L/R) -> bypass
//   crossfade -> dry/wet mix
//
// NEUTRAL PASSTHROUGH: the shifter/formant chain is an analysis-resynthesis
// process, not a literal sample passthrough - it always re-derives the
// output waveform from captured pitch periods, even when Pitch=0 and
// Formant=0 (there is no "ratio==1 means skip everything" shortcut in
// either stage). Measured directly on real vocal material: at Pitch=0/
// Formant=0/Flatten off, the processed output's RMS matched the dry
// signal's RMS almost exactly, yet the two waveforms were essentially
// UNCORRELATED (residual RMS ~= sqrt(2) x signal RMS, the textbook
// signature of same-energy-but-different-content) - and confirmed audible
// as artifacts on the real clip that motivated this fix. So "0/0" is
// crossfaded (via neutralSmoothed below) toward the SAME delay-matched dry
// tap already used for Mix, entirely independent of the Mix knob - at true
// neutral settings there is nothing to shift, so the output is the
// unmodified (delay-compensated) input regardless of where Mix is set.
//
// setParams() is real-time-safe (no allocation). quality (LIVE/STUDIO) is
// the one exception: it changes the detector's analysis window size AND
// the shifter's overlap factor (both reallocate), so it goes through
// reconfigureIfNeeded() instead - message-thread-only, mirrors Nova
// Pitch's own proven oversampling/quality-reconfigure pattern.
namespace nova::bend
{
    class NovaBendEngine
    {
    public:
        NovaBendEngine() = default;

        void prepare (double sampleRate, int maxBlockSize, bool highQuality) noexcept;
        void reset() noexcept;

        // Message-thread-only - never call from process()/setParams().
        void reconfigureIfNeeded (double sampleRate, bool desiredHighQuality) noexcept;

        struct Params
        {
            float pitchSemitones = 0.0f;     // APVTS "pitch", -12..+12
            float formantSemitones = 0.0f;   // APVTS "formant", -12..+12
            bool  flatten = false;
            int   flattenNoteIndex = 0;      // 0=C..11=B
            int   flattenOctave = 3;
            bool  link = false;
            float mixPercent = 100.0f;
            bool  bypassed = false;
        };

        void setParams (const Params& p) noexcept;

        void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

        float getDetectedHz() const noexcept { return lastDetectedHz; }
        float getEffectivePitchSemitonesDebug() const noexcept { return lastEffectivePitchSemitones; }
        int   getLatencySamples() const noexcept { return latencySamples; }

    private:
        NovaBendPitchDetector detector;
        NovaBendFlattenTarget flattenTarget;
        NovaBendShifter shifterL, shifterR;
        NovaBendFormantProcessor formantL, formantR;

        std::vector<float> monoScratch;
        std::vector<float> dryDelayL, dryDelayR;
        int dryDelayWrite = 0;
        int latencySamples = 0;

        bool appliedHighQuality = true;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed, bypassSmoothed, neutralSmoothed;

        Params params;

        float lastDetectedHz = 0.0f;
        float lastEffectivePitchSemitones = 0.0f;

        // Flatten-only: shift ratio releases toward unity while unvoiced,
        // same mechanism (and same rationale) as Nova Pitch's own engine -
        // a real vocal take's decaying tail (breath/room tone/reverb) is
        // never truly digital-silent, so holding the last active Flatten
        // ratio indefinitely would keep actively pitch-shifting that tail,
        // audible as a quiet but clearly-pitched ringing artifact riding on
        // top of the singer's own natural decay.
        float releasedOutHz = 0.0f;
        float releaseAlpha = 0.0f; // one-pole coeff, ~120ms time constant
    };
}
