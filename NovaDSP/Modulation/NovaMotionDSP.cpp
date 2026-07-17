#include "NovaMotionDSP.h"

void NovaMotionDSP::prepare (const juce::dsp::ProcessSpec& spec,
                             const NovaMotionParameters& initial)
{
    currentSR = spec.sampleRate;
    params = initial;

    filterL.prepare (spec); filterL.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filterR.prepare (spec); filterR.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    maxDelaySamples = (int)(spec.sampleRate * 2.0);
    delayLineL.prepare (spec); delayLineL.setMaximumDelayInSamples (maxDelaySamples);
    delayLineR.prepare (spec); delayLineR.setMaximumDelayInSamples (maxDelaySamples);

    reverbL.reset();

    auto init = [&](juce::SmoothedValue<float>& sv, float v)
    {
        sv.reset (spec.sampleRate, 0.02);
        sv.setCurrentAndTargetValue (v);
    };
    init (smCutoff,    20000.f);
    init (smResonance, 0.707f);
    init (smDrive,     0.f);
    init (smFeedback,  0.f);
    init (smDelayMix,  0.f);
    init (smSize,      0.6f);
    init (smReverbMix, 0.f);
    init (smInput,     1.f);
    init (smOutput,    1.f);
    init (smMix,       1.f);

    dryBuf.setSize (2, (int)spec.maximumBlockSize);

    peakL.store (0.f, std::memory_order_relaxed);
    peakR.store (0.f, std::memory_order_relaxed);

    prevFilterActive = false;
    prevDelayActive  = false;
    prevReverbActive = false;
}

void NovaMotionDSP::reset() noexcept
{
    filterL.reset();
    filterR.reset();
    delayLineL.reset();
    delayLineR.reset();
    reverbL.reset();
    peakL.store (0.f, std::memory_order_relaxed);
    peakR.store (0.f, std::memory_order_relaxed);
    prevFilterActive = false;
    prevDelayActive  = false;
    prevReverbActive = false;
}

void NovaMotionDSP::setParameters (const NovaMotionParameters& p) noexcept
{
    params = p;
}

void NovaMotionDSP::process (juce::AudioBuffer<float>& buf) noexcept
{
    const int N   = buf.getNumSamples();
    const int nCh = buf.getNumChannels();

    if (N == 0 || nCh == 0) return;

    float* dataL = buf.getWritePointer (0);
    float* dataR = nCh > 1 ? buf.getWritePointer (1) : dataL;

    const float inGain  = juce::Decibels::decibelsToGain (params.inputDb);
    const float outGain = juce::Decibels::decibelsToGain (params.outputDb);

    const float safeMaxCutoff = (float)(currentSR * 0.40);
    const bool  filterOn = params.cutoff < safeMaxCutoff - 100.f;
    const bool  delayOn  = params.delayMix  > 0.001f;
    const bool  reverbOn = params.reverbMix > 0.001f;

    // Reset on active→inactive transition only
    if (!filterOn && prevFilterActive) { filterL.reset(); filterR.reset(); }
    if (!delayOn  && prevDelayActive)  { delayLineL.reset(); delayLineR.reset(); }
    if (!reverbOn && prevReverbActive) { reverbL.reset(); }
    prevFilterActive = filterOn;
    prevDelayActive  = delayOn;
    prevReverbActive = reverbOn;

    // Input gain
    for (int ch = 0; ch < nCh; ++ch)
        juce::FloatVectorOperations::multiply (buf.getWritePointer (ch), inGain, N);

    // Save dry for master wet/dry blend
    dryBuf.setSize (nCh, N, false, false, true);
    dryBuf.makeCopyOf (buf, true);

    // Filter — coefficients set once per block, processSample per sample
    if (filterOn)
    {
        const float safeCut = juce::jmin (params.cutoff, safeMaxCutoff);
        const float safeRes = juce::jlimit (0.1f, 4.0f, params.resonance);
        filterL.setCutoffFrequency (safeCut); filterL.setResonance (safeRes);
        filterR.setCutoffFrequency (safeCut); filterR.setResonance (safeRes);
        for (int i = 0; i < N; ++i)
        {
            dataL[i] = filterL.processSample (0, dataL[i]);
            dataR[i] = filterR.processSample (0, dataR[i]);
        }
    }

    // Delay — per sample
    if (delayOn)
    {
        const float ds = juce::jmin ((float)maxDelaySamples - 1.f,
                                     (float)(currentSR * 0.25));
        for (int i = 0; i < N; ++i)
        {
            const float dL = delayLineL.popSample (0, ds);
            const float dR = delayLineR.popSample (0, ds);
            delayLineL.pushSample (0, dataL[i] + dL * params.feedback);
            delayLineR.pushSample (0, dataR[i] + dR * params.feedback);
            dataL[i] = dataL[i] * (1.f - params.delayMix) + dL * params.delayMix;
            dataR[i] = dataR[i] * (1.f - params.delayMix) + dR * params.delayMix;
        }
    }

    // Reverb — full block (juce::Reverb is not per-sample)
    if (reverbOn)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize   = params.size;
        rp.wetLevel   = params.reverbMix;
        rp.dryLevel   = 1.f - params.reverbMix;
        rp.damping    = 0.45f;
        rp.width      = 1.f;
        rp.freezeMode = 0.f;
        reverbL.setParameters (rp);

        if (nCh >= 2)
            reverbL.processStereo (dataL, dataR, N);
        else
            reverbL.processMono (dataL, N);
    }

    // Master wet/dry blend
    if (params.mix < 0.999f)
    {
        const float dry = 1.f - params.mix;
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto*       w = buf.getWritePointer (ch);
            const auto* d = dryBuf.getReadPointer (ch);
            for (int i = 0; i < N; ++i)
                w[i] = w[i] * params.mix + d[i] * dry;
        }
    }

    // Output gain + NaN-safe hard limiter
    for (int i = 0; i < N; ++i)
    {
        const float l = dataL[i] * outGain;
        const float r = dataR[i] * outGain;
        dataL[i] = std::isfinite (l) ? juce::jlimit (-1.f, 1.f, l) : 0.f;
        dataR[i] = std::isfinite (r) ? juce::jlimit (-1.f, 1.f, r) : 0.f;
    }

    peakL.store (buf.getMagnitude (0, 0, N), std::memory_order_relaxed);
    if (nCh > 1) peakR.store (buf.getMagnitude (1, 0, N), std::memory_order_relaxed);
}
