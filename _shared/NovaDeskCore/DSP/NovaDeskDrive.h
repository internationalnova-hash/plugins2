#pragma once

#include <array>
#include <memory>

#include <juce_dsp/juce_dsp.h>

#include "NovaDeskTypes.h"

namespace nova::desk
{
    // Console-coloration "Drive" stage (build spec's Drive knob, 0-10).
    //
    // Deliberately NOT the two-piece asymmetric-tanh shape used by
    // NovaClassAAmp (Nova 7even) / Nova Heat - this is Nova Desk EQ's own
    // curve, built from different math so it is a genuinely distinct
    // sound, not a reskin:
    //
    //  - Waveshaper: a rational ("Arraya"-style) soft clipper
    //    y = x * (1+k) / (1 + k*|x|), no exponentials, a softer initial
    //    knee and a harder asymptotic ceiling than tanh at matched drive.
    //  - Controlled asymmetry: positive and negative excursions use
    //    slightly different k (kPos > kNeg), producing even-harmonic
    //    "console" coloration rather than tanh's purely odd-harmonic
    //    symmetric clipping.
    //  - Level-dependent response: an envelope follower scales the
    //    effective k with the input's instantaneous level, so quiet
    //    passages stay cleaner and loud passages saturate harder
    //    (program-dependent, distinct from ClassAAmp's fixed-drive sag
    //    envelope).
    //  - E/G character (subtle, not a different curve family): E uses
    //    more asymmetry and a faster envelope (punchier, more even
    //    harmonics); G uses less asymmetry and a slower envelope
    //    (smoother, more forgiving).
    //  - DC blocking: the asymmetric shaping introduces a small DC
    //    component; a one-pole DC blocker removes it post-shaping.
    //
    // Wrapped in selectable 1x/2x/4x oversampling (default 2x) using the
    // same juce::dsp::Oversampling shape proven by
    // NovaSevenCore/DSP/NovaOversampledColorStage. The oversampler runs
    // UNCONDITIONALLY, even at drive=0 - an FIR filter that sits idle and
    // is later fed live audio outputs near-silence for its first
    // `latency` samples (a real, audible click on re-engage, caught by
    // the click-safety regression test). The CPU saving at drive~0
    // instead happens inside the per-sample waveshaping math, which is
    // skipped once the smoothed drive amount settles near zero - the
    // oversampling itself is the expensive part and isn't skippable
    // without reintroducing the click.
    //
    // Templated on SampleType (float/double) for genuine double-precision
    // processing (build spec's "64-bit double precision processing"
    // requirement) - explicitly instantiated for float and double at the
    // bottom of the .cpp. juce::dsp::Oversampling itself is templated on
    // SampleType, so the oversampled path is natively double when
    // SampleType=double, not a float-converted approximation.
    template <typename SampleType>
    class NovaDeskDrive
    {
    public:
        NovaDeskDrive() = default;

        // spec is the BASE (un-oversampled) processing spec; the class
        // builds its own oversampler and computes the inner stage's
        // filter/envelope time constants against the oversampled rate.
        void prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages = 1);
        void reset() noexcept;

        // Drive amount and E/G character are smoothed PER-SAMPLE (cheap
        // here - the waveshaper already runs a per-sample loop, unlike
        // NovaDeskEQBands where filter-coefficient rebuilds make per-
        // sample smoothing too expensive). This is genuinely click-free
        // on fast automation and rapid E<->G toggling, not just block-
        // rate-approximated.
        void setDriveAmount (float amount010) noexcept;
        void setPersonality (EQPersonality mode) noexcept;

        void process (juce::AudioBuffer<SampleType>& buffer) noexcept;

        int getLatencySamples() const noexcept;

        // Exposed for the validation harness: applies the static
        // waveshaper curve to one sample with no envelope/DC-block state,
        // so the curve's own shape (knee, asymmetry, ceiling) can be
        // measured in isolation from the dynamic/level-dependent parts.
        static SampleType shapeSample (SampleType x, SampleType k, SampleType asymmetry) noexcept;

    private:
        struct ChannelState
        {
            SampleType envelope = SampleType (0);
            SampleType dcX1 = SampleType (0);
            SampleType dcY1 = SampleType (0);
        };

        void processInner (juce::AudioBuffer<SampleType>& buffer) noexcept;

        float driveAmount010 = 0.0f;
        EQPersonality personality = EQPersonality::E;

        static constexpr double kSmoothingSeconds = 0.02; // ~20ms
        juce::SmoothedValue<float> driveAmountSm;
        juce::SmoothedValue<float> personalityBlendSm; // 0 = E, 1 = G

        double innerSampleRate = 44100.0;
        SampleType dcBlockR = SampleType (0.999);

        std::array<ChannelState, 2> channelStates;

        std::unique_ptr<juce::dsp::Oversampling<SampleType>> oversampler;
        int oversamplingStagesRequested = 1;
        int preparedNumChannels = 2;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaDeskDrive)
    };
}
