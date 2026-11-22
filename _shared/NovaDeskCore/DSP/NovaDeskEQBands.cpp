#include "NovaDeskEQBands.h"

namespace nova::desk
{
    // Standard cascaded 4th-order (24dB/oct) Butterworth Q factors, used
    // for both HPF and LPF stages (spec sections 9-10's ~24dB/octave
    // target).
    static constexpr float kButterworthQ1 = 0.5411961f;
    static constexpr float kButterworthQ2 = 1.3065630f;

    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

        hpfStage1.prepare (spec);
        hpfStage2.prepare (spec);
        lpfStage1.prepare (spec);
        lpfStage2.prepare (spec);
        lowFilter.prepare (spec);
        lowMidFilter.prepare (spec);
        highMidFilter.prepare (spec);
        highFilter.prepare (spec);

        hpfDryScratch.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize, false, false, true);
        lpfDryScratch.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize, false, false, true);

        lowFreqSm.reset (sampleRate, kSmoothingSeconds);     lowFreqSm.setCurrentAndTargetValue (lowFreq);
        lowGainSm.reset (sampleRate, kSmoothingSeconds);     lowGainSm.setCurrentAndTargetValue (lowGainDb);
        lowMidFreqSm.reset (sampleRate, kSmoothingSeconds);  lowMidFreqSm.setCurrentAndTargetValue (lowMidFreq);
        lowMidGainSm.reset (sampleRate, kSmoothingSeconds);  lowMidGainSm.setCurrentAndTargetValue (lowMidGainDb);
        lowMidQSm.reset (sampleRate, kSmoothingSeconds);     lowMidQSm.setCurrentAndTargetValue (lowMidQ);
        highMidFreqSm.reset (sampleRate, kSmoothingSeconds); highMidFreqSm.setCurrentAndTargetValue (highMidFreq);
        highMidGainSm.reset (sampleRate, kSmoothingSeconds); highMidGainSm.setCurrentAndTargetValue (highMidGainDb);
        highMidQSm.reset (sampleRate, kSmoothingSeconds);    highMidQSm.setCurrentAndTargetValue (highMidQ);
        highFreqSm.reset (sampleRate, kSmoothingSeconds);    highFreqSm.setCurrentAndTargetValue (highFreq);
        highGainSm.reset (sampleRate, kSmoothingSeconds);    highGainSm.setCurrentAndTargetValue (highGainDb);

        personalityBlendSm.reset (sampleRate, kSmoothingSeconds);
        personalityBlendSm.setCurrentAndTargetValue (personality == EQPersonality::G ? 1.0f : 0.0f);

        hpfFreqSm.reset (sampleRate, kSmoothingSeconds);
        hpfFreqSm.setCurrentAndTargetValue (hpfIndex > 0 ? hpfFreqChoices[(size_t) hpfIndex] : hpfFreqChoices[1]);
        lpfFreqSm.reset (sampleRate, kSmoothingSeconds);
        lpfFreqSm.setCurrentAndTargetValue (lpfIndex > 0 ? lpfFreqChoices[(size_t) lpfIndex] : lpfFreqChoices[1]);

        hpfEngageSm.reset (sampleRate, kEngageSeconds);
        hpfEngageSm.setCurrentAndTargetValue ((hpfOn && hpfIndex > 0) ? 1.0f : 0.0f);
        lpfEngageSm.reset (sampleRate, kEngageSeconds);
        lpfEngageSm.setCurrentAndTargetValue ((lpfOn && lpfIndex > 0) ? 1.0f : 0.0f);

        coefficientsDirty = true;
        updateRealtimeState (1);
        reset();
    }

    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::reset() noexcept
    {
        hpfStage1.reset();
        hpfStage2.reset();
        lpfStage1.reset();
        lpfStage2.reset();
        lowFilter.reset();
        lowMidFilter.reset();
        highMidFilter.reset();
        highFilter.reset();
    }

    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setPersonality (EQPersonality mode) noexcept
    {
        personality = mode;
        personalityBlendSm.setTargetValue (mode == EQPersonality::G ? 1.0f : 0.0f);
        coefficientsDirty = true;
    }

    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setHpfFreqIndex (int index) noexcept
    {
        hpfIndex = juce::jlimit (0, (int) hpfFreqChoices.size() - 1, index);
        if (hpfIndex > 0)
            hpfFreqSm.setTargetValue (hpfFreqChoices[(size_t) hpfIndex]);
        hpfEngageSm.setTargetValue ((hpfOn && hpfIndex > 0) ? 1.0f : 0.0f);
        coefficientsDirty = true;
    }

    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setHpfIn (bool on) noexcept
    {
        hpfOn = on;
        hpfEngageSm.setTargetValue ((hpfOn && hpfIndex > 0) ? 1.0f : 0.0f);
        coefficientsDirty = true;
    }

    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setLpfFreqIndex (int index) noexcept
    {
        lpfIndex = juce::jlimit (0, (int) lpfFreqChoices.size() - 1, index);
        if (lpfIndex > 0)
            lpfFreqSm.setTargetValue (lpfFreqChoices[(size_t) lpfIndex]);
        lpfEngageSm.setTargetValue ((lpfOn && lpfIndex > 0) ? 1.0f : 0.0f);
        coefficientsDirty = true;
    }

    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setLpfIn (bool on) noexcept
    {
        lpfOn = on;
        lpfEngageSm.setTargetValue ((lpfOn && lpfIndex > 0) ? 1.0f : 0.0f);
        coefficientsDirty = true;
    }

    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setLowFreq (float hz) noexcept { lowFreq = hz; lowFreqSm.setTargetValue (hz); coefficientsDirty = true; }
    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setLowGainDb (float db) noexcept { lowGainDb = db; lowGainSm.setTargetValue (db); coefficientsDirty = true; }

    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setLowMidFreq (float hz) noexcept { lowMidFreq = hz; lowMidFreqSm.setTargetValue (hz); coefficientsDirty = true; }
    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setLowMidGainDb (float db) noexcept { lowMidGainDb = db; lowMidGainSm.setTargetValue (db); coefficientsDirty = true; }
    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setLowMidQ (float q) noexcept { lowMidQ = q; lowMidQSm.setTargetValue (q); coefficientsDirty = true; }

    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setHighMidFreq (float hz) noexcept { highMidFreq = hz; highMidFreqSm.setTargetValue (hz); coefficientsDirty = true; }
    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setHighMidGainDb (float db) noexcept { highMidGainDb = db; highMidGainSm.setTargetValue (db); coefficientsDirty = true; }
    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setHighMidQ (float q) noexcept { highMidQ = q; highMidQSm.setTargetValue (q); coefficientsDirty = true; }

    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setHighFreq (float hz) noexcept { highFreq = hz; highFreqSm.setTargetValue (hz); coefficientsDirty = true; }
    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::setHighGainDb (float db) noexcept { highGainDb = db; highGainSm.setTargetValue (db); coefficientsDirty = true; }

    template <typename SampleType>
    float NovaDeskEQBands<SampleType>::effectiveQBlended (float baseQ, float gainDb, float personalityBlend01) noexcept
    {
        const auto k = juce::jmap (std::abs (gainDb), 0.0f, 15.0f, 0.0f, 0.45f);
        const auto eqE = baseQ * (1.0f + k);
        const auto eqG = baseQ;
        return eqE + (eqG - eqE) * juce::jlimit (0.0f, 1.0f, personalityBlend01);
    }

    template <typename SampleType>
    typename NovaDeskEQBands<SampleType>::Coeffs::Ptr NovaDeskEQBands<SampleType>::buildLowCoeffs (double rate, float freq, float gainDb, float personalityBlend01, BandType type) const
    {
        const auto gain = (SampleType) juce::Decibels::decibelsToGain (gainDb);
        return type == BandType::Shelf
             ? Coeffs::makeLowShelf (rate, (SampleType) freq, (SampleType) effectiveQBlended (kShelfBaseQ, gainDb, personalityBlend01), gain)
             : Coeffs::makePeakFilter (rate, (SampleType) freq, (SampleType) effectiveQBlended (kBellBaseQ, gainDb, personalityBlend01), gain);
    }

    template <typename SampleType>
    typename NovaDeskEQBands<SampleType>::Coeffs::Ptr NovaDeskEQBands<SampleType>::buildLowMidCoeffs (double rate, float freq, float gainDb, float q, float personalityBlend01) const
    {
        const auto gain = (SampleType) juce::Decibels::decibelsToGain (gainDb);
        return Coeffs::makePeakFilter (rate, (SampleType) freq, (SampleType) effectiveQBlended (q, gainDb, personalityBlend01), gain);
    }

    template <typename SampleType>
    typename NovaDeskEQBands<SampleType>::Coeffs::Ptr NovaDeskEQBands<SampleType>::buildHighMidCoeffs (double rate, float freq, float gainDb, float q, float personalityBlend01) const
    {
        const auto gain = (SampleType) juce::Decibels::decibelsToGain (gainDb);
        return Coeffs::makePeakFilter (rate, (SampleType) freq, (SampleType) effectiveQBlended (q, gainDb, personalityBlend01), gain);
    }

    template <typename SampleType>
    typename NovaDeskEQBands<SampleType>::Coeffs::Ptr NovaDeskEQBands<SampleType>::buildHighCoeffs (double rate, float freq, float gainDb, float personalityBlend01, BandType type) const
    {
        const auto gain = (SampleType) juce::Decibels::decibelsToGain (gainDb);
        return type == BandType::Shelf
             ? Coeffs::makeHighShelf (rate, (SampleType) freq, (SampleType) effectiveQBlended (kShelfBaseQ, gainDb, personalityBlend01), gain)
             : Coeffs::makePeakFilter (rate, (SampleType) freq, (SampleType) effectiveQBlended (kBellBaseQ, gainDb, personalityBlend01), gain);
    }

    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::updateRealtimeState (int numSamplesInBlock)
    {
        // Engage smoothers (hpfEngageSm/lpfEngageSm) are deliberately NOT
        // included here or advanced below - they're a per-SAMPLE
        // crossfade weight applied directly in process(), not a
        // coefficient input. Advancing them at block rate here would
        // apply one weight for an entire block, producing a step at each
        // block boundary during the ~15ms engage ramp instead of a true
        // sample-accurate fade (this was a real bug, caught by the
        // click-safety regression test - see process()'s HPF/LPF
        // sections for the actual per-sample blend).
        const bool anySmoothing = lowFreqSm.isSmoothing() || lowGainSm.isSmoothing()
                                || lowMidFreqSm.isSmoothing() || lowMidGainSm.isSmoothing() || lowMidQSm.isSmoothing()
                                || highMidFreqSm.isSmoothing() || highMidGainSm.isSmoothing() || highMidQSm.isSmoothing()
                                || highFreqSm.isSmoothing() || highGainSm.isSmoothing()
                                || personalityBlendSm.isSmoothing()
                                || hpfFreqSm.isSmoothing() || lpfFreqSm.isSmoothing();

        if (! coefficientsDirty && ! anySmoothing)
            return;

        const auto advance = [numSamplesInBlock] (juce::SmoothedValue<float>& sm) -> float
        {
            if (numSamplesInBlock > 1)
                sm.skip (numSamplesInBlock - 1);
            return sm.getNextValue();
        };

        const auto curLowFreq = advance (lowFreqSm);
        const auto curLowGain = advance (lowGainSm);
        const auto curLowMidFreq = advance (lowMidFreqSm);
        const auto curLowMidGain = advance (lowMidGainSm);
        const auto curLowMidQ = advance (lowMidQSm);
        const auto curHighMidFreq = advance (highMidFreqSm);
        const auto curHighMidGain = advance (highMidGainSm);
        const auto curHighMidQ = advance (highMidQSm);
        const auto curHighFreq = advance (highFreqSm);
        const auto curHighGain = advance (highGainSm);
        const auto curBlend = advance (personalityBlendSm);
        const auto curHpfFreq = advance (hpfFreqSm);
        const auto curLpfFreq = advance (lpfFreqSm);

        *hpfStage1.state = *Coeffs::makeHighPass (sampleRate, (SampleType) curHpfFreq, (SampleType) kButterworthQ1);
        *hpfStage2.state = *Coeffs::makeHighPass (sampleRate, (SampleType) curHpfFreq, (SampleType) kButterworthQ2);
        *lpfStage1.state = *Coeffs::makeLowPass (sampleRate, (SampleType) curLpfFreq, (SampleType) kButterworthQ1);
        *lpfStage2.state = *Coeffs::makeLowPass (sampleRate, (SampleType) curLpfFreq, (SampleType) kButterworthQ2);

        *lowFilter.state = *buildLowCoeffs (sampleRate, curLowFreq, curLowGain, curBlend, lowType);
        *lowMidFilter.state = *buildLowMidCoeffs (sampleRate, curLowMidFreq, curLowMidGain, curLowMidQ, curBlend);
        *highMidFilter.state = *buildHighMidCoeffs (sampleRate, curHighMidFreq, curHighMidGain, curHighMidQ, curBlend);
        *highFilter.state = *buildHighCoeffs (sampleRate, curHighFreq, curHighGain, curBlend, highType);

        coefficientsDirty = false;
    }

    template <typename SampleType>
    void NovaDeskEQBands<SampleType>::process (juce::AudioBuffer<SampleType>& buffer) noexcept
    {
        const int N = buffer.getNumSamples();
        if (N == 0)
            return;

        updateRealtimeState (N);

        juce::dsp::AudioBlock<SampleType> block (buffer);
        juce::dsp::ProcessContextReplacing<SampleType> context (block);

        // HPF: always processed (keeps filter state warm so re-engaging
        // never resumes from stale/frozen state), blended against the
        // pre-filter dry signal by a PER-SAMPLE engage crossfade - this is
        // what makes the In button (and Off<->non-Off frequency changes)
        // click-free rather than a hard branch. The blend weight must be
        // sample-accurate, not one scalar per block: a block-constant
        // weight still steps discontinuously at block boundaries during
        // the ~15ms ramp (caught by the click-safety regression test).
        {
            hpfDryScratch.setSize (buffer.getNumChannels(), N, false, false, true);
            hpfDryScratch.makeCopyOf (buffer, true);
            hpfStage1.process (context);
            hpfStage2.process (context);

            const bool settledOn = ! hpfEngageSm.isSmoothing() && hpfEngageSm.getCurrentValue() >= 0.9999f;
            const bool settledOff = ! hpfEngageSm.isSmoothing() && hpfEngageSm.getCurrentValue() <= 0.0001f;

            if (settledOff)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.copyFrom (ch, 0, hpfDryScratch, ch, 0, N);
            }
            else if (! settledOn)
            {
                const int nCh = buffer.getNumChannels();
                for (int i = 0; i < N; ++i)
                {
                    const auto e = (SampleType) hpfEngageSm.getNextValue();
                    for (int ch = 0; ch < nCh; ++ch)
                    {
                        auto* w = buffer.getWritePointer (ch);
                        const auto* dry = hpfDryScratch.getReadPointer (ch);
                        w[i] = dry[i] * (SampleType (1) - e) + w[i] * e;
                    }
                }
            }
        }

        lowFilter.process (context);
        lowMidFilter.process (context);
        highMidFilter.process (context);
        highFilter.process (context);

        // LPF: same always-run-and-blend treatment as HPF.
        {
            lpfDryScratch.setSize (buffer.getNumChannels(), N, false, false, true);
            lpfDryScratch.makeCopyOf (buffer, true);
            lpfStage1.process (context);
            lpfStage2.process (context);

            const bool settledOn = ! lpfEngageSm.isSmoothing() && lpfEngageSm.getCurrentValue() >= 0.9999f;
            const bool settledOff = ! lpfEngageSm.isSmoothing() && lpfEngageSm.getCurrentValue() <= 0.0001f;

            if (settledOff)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.copyFrom (ch, 0, lpfDryScratch, ch, 0, N);
            }
            else if (! settledOn)
            {
                const int nCh = buffer.getNumChannels();
                for (int i = 0; i < N; ++i)
                {
                    const auto e = (SampleType) lpfEngageSm.getNextValue();
                    for (int ch = 0; ch < nCh; ++ch)
                    {
                        auto* w = buffer.getWritePointer (ch);
                        const auto* dry = lpfDryScratch.getReadPointer (ch);
                        w[i] = dry[i] * (SampleType (1) - e) + w[i] * e;
                    }
                }
            }
        }
    }

    template <typename SampleType>
    float NovaDeskEQBands<SampleType>::getLowMagnitudeResponseDb (double frequencyHz) const
    {
        return (float) juce::Decibels::gainToDecibels ((double) makeLowCoeffs (sampleRate)->getMagnitudeForFrequency (frequencyHz, sampleRate));
    }

    template <typename SampleType>
    float NovaDeskEQBands<SampleType>::getLowMidMagnitudeResponseDb (double frequencyHz) const
    {
        return (float) juce::Decibels::gainToDecibels ((double) makeLowMidCoeffs (sampleRate)->getMagnitudeForFrequency (frequencyHz, sampleRate));
    }

    template <typename SampleType>
    float NovaDeskEQBands<SampleType>::getHighMidMagnitudeResponseDb (double frequencyHz) const
    {
        return (float) juce::Decibels::gainToDecibels ((double) makeHighMidCoeffs (sampleRate)->getMagnitudeForFrequency (frequencyHz, sampleRate));
    }

    template <typename SampleType>
    float NovaDeskEQBands<SampleType>::getHighMagnitudeResponseDb (double frequencyHz) const
    {
        return (float) juce::Decibels::gainToDecibels ((double) makeHighCoeffs (sampleRate)->getMagnitudeForFrequency (frequencyHz, sampleRate));
    }

    template <typename SampleType>
    float NovaDeskEQBands<SampleType>::getMagnitudeResponseDb (double frequencyHz) const
    {
        double totalDb = 0.0;

        if (hpfOn && hpfIndex > 0)
        {
            const auto hz = (SampleType) hpfFreqChoices[(size_t) hpfIndex];
            auto stage1 = Coeffs::makeHighPass (sampleRate, hz, (SampleType) kButterworthQ1);
            auto stage2 = Coeffs::makeHighPass (sampleRate, hz, (SampleType) kButterworthQ2);
            totalDb += juce::Decibels::gainToDecibels ((double) stage1->getMagnitudeForFrequency (frequencyHz, sampleRate));
            totalDb += juce::Decibels::gainToDecibels ((double) stage2->getMagnitudeForFrequency (frequencyHz, sampleRate));
        }

        totalDb += getLowMagnitudeResponseDb (frequencyHz);
        totalDb += getLowMidMagnitudeResponseDb (frequencyHz);
        totalDb += getHighMidMagnitudeResponseDb (frequencyHz);
        totalDb += getHighMagnitudeResponseDb (frequencyHz);

        if (lpfOn && lpfIndex > 0)
        {
            const auto hz = (SampleType) lpfFreqChoices[(size_t) lpfIndex];
            auto stage1 = Coeffs::makeLowPass (sampleRate, hz, (SampleType) kButterworthQ1);
            auto stage2 = Coeffs::makeLowPass (sampleRate, hz, (SampleType) kButterworthQ2);
            totalDb += juce::Decibels::gainToDecibels ((double) stage1->getMagnitudeForFrequency (frequencyHz, sampleRate));
            totalDb += juce::Decibels::gainToDecibels ((double) stage2->getMagnitudeForFrequency (frequencyHz, sampleRate));
        }

        return (float) totalDb;
    }

    template class NovaDeskEQBands<float>;
    template class NovaDeskEQBands<double>;
}
