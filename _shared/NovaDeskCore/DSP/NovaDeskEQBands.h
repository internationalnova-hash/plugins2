#pragma once

#include <array>

#include <juce_dsp/juce_dsp.h>

#include "NovaDeskTypes.h"

namespace nova::desk
{
    // EQPersonality (E/G) is declared in NovaDeskTypes.h, shared with
    // NovaDeskDrive.
    //
    // The mechanism chosen here for the EQ bands is proportional-Q
    // shaping: as a band's |gain| increases, E tightens its effective Q (a
    // more focused, assertive curve - "cuts through a mix"), while G holds
    // its Q constant regardless of gain (stays broad and forgiving even
    // under heavy boost - "difficult to make harsh"). This is Nova's own
    // response curve, not a reproduction of any specific manufacturer's
    // circuit math (spec section 15's explicit instruction).

    enum class BandType { Bell, Shelf };

    // 4-band console EQ (Low/Low-Mid/High-Mid/High) plus stepped HPF/LPF,
    // matching the build spec's locked signal-flow section (HPF -> Low ->
    // Low-Mid -> High-Mid -> High -> LPF). Continuous freq/gain on all
    // bands; Low and High additionally choose Bell or Shelf; Low-Mid and
    // High-Mid are Bell-only with a continuous Q knob (matching the
    // approved mockup's control set exactly - spec sections 11-14).
    //
    // Templated on SampleType (float/double) so the plugin can genuinely
    // run its audio path at either precision (build spec's "64-bit double
    // precision processing" requirement) - explicitly instantiated for
    // float and double at the bottom of the .cpp. Control-rate parameters
    // (frequency/gain/Q) stay float regardless of SampleType: they're set
    // at most once per block from APVTS, not per-sample audio data, so
    // there's no precision benefit to widening them, and it keeps the
    // public setter API identical across both instantiations.
    //
    // CLICK-SAFETY DESIGN (gap-closure pass): setters update a "target"
    // value immediately, used by the analytic getXxxMagnitudeResponseDb()
    // methods (so a UI curve display tracks the knob instantly, per spec -
    // "UI follows mouse immediately; DSP smooths internally"). The
    // REALTIME coefficient path instead reads a block-rate-smoothed shadow
    // of each continuous parameter (~20ms ramp), avoiding zipper noise on
    // fast automation/knob drags without per-sample coefficient
    // recomputation cost. EQPersonality (E/G) is smoothed as a continuous
    // 0..1 blend rather than a hard switch, so E<->G transitions ramp
    // rather than jump. HPF/LPF "engaged" state (the In button AND
    // selecting/leaving the Off frequency choice) is smoothed as a
    // continuous crossfade between the filtered and unfiltered signal,
    // because unlike a plain coefficient swap, disengaging a stage
    // literally stops updating its internal filter state - crossfading
    // avoids the discontinuity that would otherwise cause on the
    // re-engage. Bell/Shelf type is an instant coefficient swap: the
    // filter keeps running (and its state stays continuous) in both
    // cases, so swapping coefficients produces at most a brief transient,
    // not the sample-domain discontinuity an engage/disengage toggle
    // risks - deliberately out of scope for the crossfade treatment here.
    template <typename SampleType>
    class NovaDeskEQBands
    {
    public:
        NovaDeskEQBands() = default;

        void prepare (const juce::dsp::ProcessSpec& spec);
        void reset() noexcept;

        void setPersonality (EQPersonality mode) noexcept;
        EQPersonality getPersonality() const noexcept { return personality; }

        // HPF/LPF: stepped rotary switches (spec sections 9-10). Index 0 =
        // Off for both. ~24dB/octave via two cascaded Butterworth sections.
        void setHpfFreqIndex (int index) noexcept;
        void setHpfIn (bool on) noexcept;
        void setLpfFreqIndex (int index) noexcept;
        void setLpfIn (bool on) noexcept;

        void setLowFreq (float hz) noexcept;
        void setLowGainDb (float db) noexcept;
        void setLowType (BandType type) noexcept { lowType = type; coefficientsDirty = true; }

        void setLowMidFreq (float hz) noexcept;
        void setLowMidGainDb (float db) noexcept;
        void setLowMidQ (float q) noexcept;

        void setHighMidFreq (float hz) noexcept;
        void setHighMidGainDb (float db) noexcept;
        void setHighMidQ (float q) noexcept;

        void setHighFreq (float hz) noexcept;
        void setHighGainDb (float db) noexcept;
        void setHighType (BandType type) noexcept { highType = type; coefficientsDirty = true; }

        void process (juce::AudioBuffer<SampleType>& buffer) noexcept;

        // Combined magnitude response in dB at a given frequency, computed
        // from the CURRENT TARGET parameter settings (not the realtime-
        // smoothed shadow) via fresh (non-audio-thread) coefficient
        // objects. Used by the validation harness and by the UI's small
        // display-only EQ curve (spec sections 26-28), which must track
        // the knob immediately rather than lagging behind the audio-path
        // smoothing. Always float precision regardless of SampleType - a
        // display/analysis value, not audio data.
        float getMagnitudeResponseDb (double frequencyHz) const;

        // Per-band magnitude response, for isolating one band's shape in
        // tests without the others contributing.
        float getLowMagnitudeResponseDb (double frequencyHz) const;
        float getLowMidMagnitudeResponseDb (double frequencyHz) const;
        float getHighMidMagnitudeResponseDb (double frequencyHz) const;
        float getHighMagnitudeResponseDb (double frequencyHz) const;

        static constexpr std::array<float, 6> hpfFreqChoices { 0.0f, 20.0f, 50.0f, 100.0f, 200.0f, 400.0f };
        static constexpr std::array<float, 6> lpfFreqChoices { 0.0f, 5000.0f, 8000.0f, 12000.0f, 16000.0f, 20000.0f };

    private:
        using Filter = juce::dsp::IIR::Filter<SampleType>;
        using Coeffs = juce::dsp::IIR::Coefficients<SampleType>;
        using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, Coeffs>;

        // E tightens Q as |gain| grows (more assertive/focused at extreme
        // settings); G holds Q constant (stays broad/forgiving). Roughly
        // +45% Q at the full +-15dB range for E - a real, audible, but not
        // gimmicky difference (spec section 8). personalityBlend01 is 0.0
        // for pure E, 1.0 for pure G, continuous in between (used by the
        // realtime path during an E<->G transition).
        static float effectiveQBlended (float baseQ, float gainDb, float personalityBlend01) noexcept;

        // Builds coefficients from EXPLICIT values (not member state), so
        // the same logic serves both the instant-target analysis path and
        // the smoothed realtime path.
        typename Coeffs::Ptr buildLowCoeffs (double rate, float freq, float gainDb, float personalityBlend01, BandType type) const;
        typename Coeffs::Ptr buildLowMidCoeffs (double rate, float freq, float gainDb, float q, float personalityBlend01) const;
        typename Coeffs::Ptr buildHighMidCoeffs (double rate, float freq, float gainDb, float q, float personalityBlend01) const;
        typename Coeffs::Ptr buildHighCoeffs (double rate, float freq, float gainDb, float personalityBlend01, BandType type) const;

        typename Coeffs::Ptr makeLowCoeffs (double rate) const { return buildLowCoeffs (rate, lowFreq, lowGainDb, personality == EQPersonality::G ? 1.0f : 0.0f, lowType); }
        typename Coeffs::Ptr makeLowMidCoeffs (double rate) const { return buildLowMidCoeffs (rate, lowMidFreq, lowMidGainDb, lowMidQ, personality == EQPersonality::G ? 1.0f : 0.0f); }
        typename Coeffs::Ptr makeHighMidCoeffs (double rate) const { return buildHighMidCoeffs (rate, highMidFreq, highMidGainDb, highMidQ, personality == EQPersonality::G ? 1.0f : 0.0f); }
        typename Coeffs::Ptr makeHighCoeffs (double rate) const { return buildHighCoeffs (rate, highFreq, highGainDb, personality == EQPersonality::G ? 1.0f : 0.0f, highType); }

        // Advances every smoothed shadow value by the block's sample
        // count (so ramp durations are real wall-clock time regardless of
        // host block size) and rebuilds realtime coefficients if anything
        // is still moving or a discrete (non-smoothed) parameter changed.
        void updateRealtimeState (int numSamplesInBlock);

        EQPersonality personality = EQPersonality::E;

        int hpfIndex = 0;
        bool hpfOn = true;
        int lpfIndex = 0;
        bool lpfOn = true;

        float lowFreq = 100.0f, lowGainDb = 0.0f;
        BandType lowType = BandType::Shelf;

        float lowMidFreq = 400.0f, lowMidGainDb = 0.0f, lowMidQ = 0.8f;
        float highMidFreq = 2000.0f, highMidGainDb = 0.0f, highMidQ = 0.8f;

        float highFreq = 8000.0f, highGainDb = 0.0f;
        BandType highType = BandType::Shelf;

        double sampleRate = 44100.0;
        bool coefficientsDirty = true;

        static constexpr double kSmoothingSeconds = 0.02;   // ~20ms, continuous params
        static constexpr double kEngageSeconds = 0.015;     // ~15ms, HPF/LPF engage crossfade

        juce::SmoothedValue<float> lowFreqSm, lowGainSm;
        juce::SmoothedValue<float> lowMidFreqSm, lowMidGainSm, lowMidQSm;
        juce::SmoothedValue<float> highMidFreqSm, highMidGainSm, highMidQSm;
        juce::SmoothedValue<float> highFreqSm, highGainSm;
        juce::SmoothedValue<float> personalityBlendSm; // 0 = E, 1 = G
        juce::SmoothedValue<float> hpfFreqSm, lpfFreqSm;
        juce::SmoothedValue<float> hpfEngageSm, lpfEngageSm; // 0 = bypassed, 1 = fully filtered

        StereoFilter hpfStage1, hpfStage2;
        StereoFilter lpfStage1, lpfStage2;
        StereoFilter lowFilter, lowMidFilter, highMidFilter, highFilter;

        juce::AudioBuffer<SampleType> hpfDryScratch, lpfDryScratch;

        static constexpr float kBellBaseQ = 0.9f;
        static constexpr float kShelfBaseQ = 0.9f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaDeskEQBands)
    };
}
