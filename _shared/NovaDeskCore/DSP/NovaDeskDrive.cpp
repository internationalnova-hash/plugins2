#include "NovaDeskDrive.h"

namespace nova::desk
{
    template <typename SampleType>
    void NovaDeskDrive<SampleType>::prepare (const juce::dsp::ProcessSpec& spec, int oversamplingStages)
    {
        preparedNumChannels = (int) spec.numChannels;
        oversamplingStagesRequested = oversamplingStages;

        oversampler = std::make_unique<juce::dsp::Oversampling<SampleType>> (
            spec.numChannels, oversamplingStages, juce::dsp::Oversampling<SampleType>::filterHalfBandFIREquiripple, true, false);
        oversampler->initProcessing (spec.maximumBlockSize);

        const int factor = 1 << oversamplingStages;
        innerSampleRate = spec.sampleRate * (double) factor;

        // One-pole DC blocker, ~20Hz corner at the INNER (oversampled) rate
        // - must track the rate the shaper actually runs at, same reasoning
        // as NovaOversampledColorStage's filter/envelope coefficients.
        dcBlockR = (SampleType) std::exp (-2.0 * juce::MathConstants<double>::pi * 20.0 / innerSampleRate);

        driveAmountSm.reset (innerSampleRate, kSmoothingSeconds);
        driveAmountSm.setCurrentAndTargetValue (driveAmount010);
        personalityBlendSm.reset (innerSampleRate, kSmoothingSeconds);
        personalityBlendSm.setCurrentAndTargetValue (personality == EQPersonality::G ? 1.0f : 0.0f);

        reset();
    }

    template <typename SampleType>
    void NovaDeskDrive<SampleType>::setDriveAmount (float amount010) noexcept
    {
        driveAmount010 = juce::jlimit (0.0f, 10.0f, amount010);
        driveAmountSm.setTargetValue (driveAmount010);
    }

    template <typename SampleType>
    void NovaDeskDrive<SampleType>::setPersonality (EQPersonality mode) noexcept
    {
        personality = mode;
        personalityBlendSm.setTargetValue (mode == EQPersonality::G ? 1.0f : 0.0f);
    }

    template <typename SampleType>
    void NovaDeskDrive<SampleType>::reset() noexcept
    {
        if (oversampler) oversampler->reset();
        for (auto& s : channelStates)
        {
            s.envelope = SampleType (0);
            s.dcX1 = SampleType (0);
            s.dcY1 = SampleType (0);
        }
    }

    template <typename SampleType>
    int NovaDeskDrive<SampleType>::getLatencySamples() const noexcept
    {
        // The oversampler now runs continuously regardless of drive
        // amount (see process()'s comment on why a hard bypass around the
        // oversampler is unsafe), so latency is a fixed property of the
        // oversampling factor, not something that changes with drive.
        return (oversampler != nullptr && oversamplingStagesRequested != 0) ? (int) oversampler->getLatencyInSamples() : 0;
    }

    template <typename SampleType>
    SampleType NovaDeskDrive<SampleType>::shapeSample (SampleType x, SampleType k, SampleType asymmetry) noexcept
    {
        // Rational ("Arraya"-style) soft clipper: y = x*(1+k)/(1+k*|x|).
        // Positive and negative excursions get slightly different k
        // (kPos/kNeg) so the curve is not point-symmetric - the source of
        // the stage's even-harmonic "console" coloration.
        const SampleType kPos = k * (SampleType (1) + asymmetry);
        const SampleType kNeg = k * (SampleType (1) - asymmetry);

        if (x >= SampleType (0))
            return x * (SampleType (1) + kPos) / (SampleType (1) + kPos * x);

        const SampleType ax = -x;
        return -(ax * (SampleType (1) + kNeg) / (SampleType (1) + kNeg * ax));
    }

    template <typename SampleType>
    void NovaDeskDrive<SampleType>::processInner (juce::AudioBuffer<SampleType>& buffer) noexcept
    {
        const int nCh = juce::jmin (buffer.getNumChannels(), preparedNumChannels, (int) channelStates.size());
        const int N = buffer.getNumSamples();

        // Outer loop over SAMPLES (not channels): driveAmountSm/
        // personalityBlendSm must be consumed exactly once per sample
        // index - consuming them inside a channel-outer loop would
        // advance the ramp once per (channel, sample) pair instead of
        // once per sample, corrupting the ramp's timing and producing a
        // different smoothed value per channel at the "same" sample.
        for (int i = 0; i < N; ++i)
        {
            const auto driveAmt = driveAmountSm.getNextValue();
            const auto blend = juce::jlimit (0.0f, 1.0f, personalityBlendSm.getNextValue());

            const auto driveFraction = driveAmt / 10.0f;
            const SampleType baseK = (SampleType) juce::jmap (driveFraction, 0.0f, 1.0f, 0.0f, 6.0f);

            // E: more asymmetry, faster envelope (punchier, more even
            // harmonics). G: less asymmetry, slower envelope (smoother,
            // more forgiving). Continuous blend, not a hard switch.
            const auto asymE = 0.14f * driveFraction, asymG = 0.04f * driveFraction;
            const SampleType asymmetryAmount = (SampleType) (asymE + (asymG - asymE) * blend);

            const auto envMs = 2.0f + (12.0f - 2.0f) * blend;
            const SampleType envCoeff = (SampleType) (1.0 - std::exp (-1.0 / (innerSampleRate * ((double) envMs / 1000.0))));

            // Drive settled at (essentially) zero: the waveshaper is
            // mathematically transparent at k=0 anyway (shapeSample(x,0,0)
            // == x), so skip the per-sample math entirely and pass the
            // sample straight through. This is the CPU-saving path now
            // that the oversampler itself always runs (see process()) -
            // cheap and correct, since it only ever engages once driveAmt
            // is genuinely above the threshold.
            if (driveAmt < 0.01f)
                continue;

            for (int ch = 0; ch < nCh; ++ch)
            {
                auto* w = buffer.getWritePointer (ch);
                auto& state = channelStates[(size_t) ch];

                const SampleType x = w[i];
                const SampleType level = std::abs (x);
                state.envelope += envCoeff * (level - state.envelope);

                // Louder input drives the shaper harder; quiet input stays
                // closer to the base amount - program-dependent response.
                const SampleType envNorm = juce::jlimit (SampleType (0), SampleType (1), state.envelope);
                const SampleType effectiveK = baseK * (SampleType (0.6) + envNorm * SampleType (0.7));

                const SampleType shaped = shapeSample (x, effectiveK, asymmetryAmount);

                const SampleType dcOut = shaped - state.dcX1 + dcBlockR * state.dcY1;
                state.dcX1 = shaped;
                state.dcY1 = dcOut;

                w[i] = dcOut;
            }
        }
    }

    template <typename SampleType>
    void NovaDeskDrive<SampleType>::process (juce::AudioBuffer<SampleType>& buffer) noexcept
    {
        // The oversampler runs UNCONDITIONALLY, even at drive=0 - this
        // deliberately gives up a true zero-cost bypass. An FIR-based
        // oversampler that sits idle and is later fed a live (non-silent)
        // input outputs near-silence for its first `latency` samples (its
        // delay line starts zero-filled) - jumping straight from
        // unprocessed audio into that cold-start silence is an audible
        // click, confirmed by the click-safety regression test (a ~0.42
        // sample-to-sample jump at the exact re-engage boundary before
        // this fix). Keeping the oversampler continuously fed with real
        // audio means its state is always warm, so there is no cold-start
        // transient when drive ramps up from zero. The actual CPU saving
        // now happens inside processInner(), which skips the (cheap)
        // waveshaping math per-sample once the smoothed drive amount is
        // ~0 - the oversampling itself is the expensive part and no
        // longer skippable without reintroducing the click.
        if (buffer.getNumSamples() == 0)
            return;

        if (oversampler == nullptr || oversamplingStagesRequested == 0)
        {
            processInner (buffer);
            return;
        }

        const int nCh = juce::jmin (buffer.getNumChannels(), preparedNumChannels);
        const int N = buffer.getNumSamples();
        if (nCh == 0)
            return;

        juce::dsp::AudioBlock<SampleType> block (buffer.getArrayOfWritePointers(), (size_t) nCh, (size_t) N);
        auto upBlock = oversampler->processSamplesUp (block);

        const size_t upNumCh = upBlock.getNumChannels();
        const size_t upNumSamples = upBlock.getNumSamples();

        std::array<SampleType*, 2> channelPtrs { nullptr, nullptr };
        for (size_t ch = 0; ch < upNumCh && ch < channelPtrs.size(); ++ch)
            channelPtrs[ch] = upBlock.getChannelPointer (ch);

        juce::AudioBuffer<SampleType> tempBuffer (channelPtrs.data(), (int) upNumCh, (int) upNumSamples);
        processInner (tempBuffer);

        oversampler->processSamplesDown (block);
    }

    template class NovaDeskDrive<float>;
    template class NovaDeskDrive<double>;
}
