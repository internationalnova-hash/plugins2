#include "PluginProcessorCore.h"
#include "ParameterIDs.hpp"

#include <cmath>

//==============================================================================
// EnvironmentChain
//==============================================================================

void EnvironmentChain::prepare (const juce::dsp::ProcessSpec& spec)
{
    preparedSpec = spec;
    lowCutFilter.prepare (spec);
    highCutFilter.prepare (spec);
    lowShelfFilter.prepare (spec);
    midPeakFilter.prepare (spec);
    highShelfFilter.prepare (spec);
    convolution.prepare (spec);
    reset();
}

void EnvironmentChain::reset()
{
    lowCutFilter.reset();
    highCutFilter.reset();
    lowShelfFilter.reset();
    midPeakFilter.reset();
    highShelfFilter.reset();
    convolution.reset();
    compressorEnvelope = 0.0f;
    compressorGainReductionDb = 0.0f;
    sideLowState = 0.0f;
    positionBiasSmoothed = 0.0f;
}

void EnvironmentChain::loadEnvironment (int environmentIndex, double sampleRate)
{
    const auto& profile = EnvironmentLibrary::getProfile (environmentIndex);

    const auto nyquist = static_cast<float> (sampleRate * 0.45);
    *lowCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (
        sampleRate, juce::jlimit (10.0f, nyquist, profile.lowCutHz));
    *highCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (
        sampleRate, juce::jlimit (200.0f, nyquist, profile.highCutHz));
    *lowShelfFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, profile.lowShelfFreq, 0.72f, juce::Decibels::decibelsToGain (profile.lowShelfGain));
    *midPeakFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, profile.midPeakFreq, profile.midPeakQ, juce::Decibels::decibelsToGain (profile.midPeakGain));
    *highShelfFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, profile.highShelfFreq, 0.72f, juce::Decibels::decibelsToGain (profile.highShelfGain));

    auto ir = EnvironmentLibrary::generateSyntheticIR (profile, sampleRate);
    convolution.loadImpulseResponse (std::move (ir), sampleRate,
                                      juce::dsp::Convolution::Stereo::yes,
                                      juce::dsp::Convolution::Trim::no,
                                      juce::dsp::Convolution::Normalise::no);

    lowCutFilter.reset();
    highCutFilter.reset();
    lowShelfFilter.reset();
    midPeakFilter.reset();
    highShelfFilter.reset();
    compressorEnvelope = 0.0f;
    compressorGainReductionDb = 0.0f;
    sideLowState = 0.0f;
    positionBiasSmoothed = 0.0f;

    loadedEnvironmentIndex = environmentIndex;
    ready = true;
}

void EnvironmentChain::process (juce::dsp::AudioBlock<float>& block, const NovaTranslateParams& p,
                                  double sampleRate, juce::AudioBuffer<float>& scratchBuffer)
{
    if (! ready)
        return;

    const auto& profile = EnvironmentLibrary::getProfile (loadedEnvironmentIndex);

    juce::dsp::ProcessContextReplacing<float> ctx (block);
    lowCutFilter.process (ctx);
    highCutFilter.process (ctx);
    lowShelfFilter.process (ctx);
    midPeakFilter.process (ctx);
    highShelfFilter.process (ctx);

    const auto numSamples = static_cast<int> (block.getNumSamples());
    const auto numChannels = juce::jmin (2, static_cast<int> (block.getNumChannels()));

    // ── Room / early-reflection convolution ────────────────────────────────
    const float roomMix = juce::jlimit (0.0f, 1.0f, p.roomAmount / 100.0f) * profile.roomAmount;
    if (roomMix > 0.001f)
    {
        scratchBuffer.setSize (numChannels, numSamples, false, false, true);
        for (int ch = 0; ch < numChannels; ++ch)
            scratchBuffer.copyFrom (ch, 0, block.getChannelPointer (static_cast<size_t> (ch)), numSamples);

        juce::dsp::AudioBlock<float> wetBlock (scratchBuffer);
        juce::dsp::ProcessContextReplacing<float> wetCtx (wetBlock);
        convolution.process (wetCtx);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* dst = block.getChannelPointer (static_cast<size_t> (ch));
            const auto* wet = scratchBuffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                dst[i] = (dst[i] * (1.0f - roomMix)) + (wet[i] * roomMix);
        }
    }

    // ── Stereo width / listening position ───────────────────────────────────
    if (numChannels > 1)
    {
        auto* left = block.getChannelPointer (0);
        auto* right = block.getChannelPointer (1);

        // Position handling is entirely data-driven from the active profile's
        // own `positions` map (see EnvironmentProfiles.hpp) - no hardcoded
        // per-index switch here, so a new environment's position behavior is
        // defined purely by the numbers in its profile, nothing in this file.
        // An out-of-range stored/automated index (e.g. recalled from a
        // preset made against a different environment with more positions)
        // clamps to this environment's own explicit defaultPositionIndex,
        // never to index 0 by assumption and never left unhandled.
        float targetPositionBias = profile.stereoOffset;
        if (profile.supportsPosition && profile.numPositions > 0)
        {
            const bool inRange = p.positionIndex >= 0 && p.positionIndex < profile.numPositions;
            const auto effectiveIndex = static_cast<size_t> (inRange ? p.positionIndex : profile.defaultPositionIndex);
            const auto& posDef = profile.positions[effectiveIndex];
            targetPositionBias += posDef.lateralBias;
            targetPositionBias *= (1.0f - posDef.diffuseAmount * 0.5f);
        }
        targetPositionBias = juce::jlimit (-1.0f, 1.0f, targetPositionBias);

        // ~40ms one-pole ramp toward the target bias, so a Position change
        // (or an environment switch that shifts the baseline bias) glides
        // rather than jumps - no click, no sudden image shift.
        const auto positionSmoothCoeff = std::exp (-1.0f / (0.04f * static_cast<float> (sampleRate)));

        for (int i = 0; i < numSamples; ++i)
        {
            positionBiasSmoothed = (positionSmoothCoeff * positionBiasSmoothed)
                                  + ((1.0f - positionSmoothCoeff) * targetPositionBias);
            const auto l = left[i], r = right[i];
            const auto mid = 0.5f * (l + r);
            const auto side = 0.5f * (l - r);
            const auto widenedSide = side * profile.stereoWidth;
            left[i] = mid + widenedSide + (positionBiasSmoothed * mid * 0.15f);
            right[i] = mid - widenedSide - (positionBiasSmoothed * mid * 0.15f);
        }
    }

    // ── Compressor (profile-driven amount) ──────────────────────────────────
    const float compAmount = juce::jlimit (0.0f, 1.0f, profile.compressionAmount);
    if (compAmount > 0.001f)
    {
        const float thresholdDb = -6.0f - (compAmount * 14.0f);
        const float ratio = 1.5f + (compAmount * 2.5f);
        constexpr float attackMs = 12.0f, releaseMs = 140.0f;
        const auto attackCoeff = std::exp (-1.0f / (0.001f * attackMs * static_cast<float> (sampleRate)));
        const auto releaseCoeff = std::exp (-1.0f / (0.001f * releaseMs * static_cast<float> (sampleRate)));

        std::array<float*, 2> chan { block.getChannelPointer (0),
                                      numChannels > 1 ? block.getChannelPointer (1) : block.getChannelPointer (0) };

        for (int i = 0; i < numSamples; ++i)
        {
            const auto peak = juce::jmax (std::abs (chan[0][i]), std::abs (chan[1][i])) + 1.0e-6f;

            if (peak > compressorEnvelope)
                compressorEnvelope = (attackCoeff * compressorEnvelope) + ((1.0f - attackCoeff) * peak);
            else
                compressorEnvelope = (releaseCoeff * compressorEnvelope) + ((1.0f - releaseCoeff) * peak);

            const auto envDb = juce::Decibels::gainToDecibels (compressorEnvelope, -100.0f);
            const auto overDb = envDb - thresholdDb;
            const auto targetGrDb = overDb > 0.0f ? overDb * (1.0f - (1.0f / ratio)) : 0.0f;
            compressorGainReductionDb += (targetGrDb - compressorGainReductionDb) * 0.2f;

            const auto gain = juce::Decibels::decibelsToGain (-compressorGainReductionDb);
            chan[0][i] *= gain;
            if (numChannels > 1)
                chan[1][i] *= gain;
        }
    }

    // ── Character: nonlinear speaker/device coloration ─────────────────────
    const float charAmount = juce::jlimit (0.0f, 1.0f, p.characterAmount / 100.0f) * profile.saturationAmount;
    if (charAmount > 0.001f)
    {
        const auto drive = 1.0f + (charAmount * 3.0f);
        const auto blend = juce::jlimit (0.0f, 0.6f, charAmount);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = block.getChannelPointer (static_cast<size_t> (ch));
            for (int i = 0; i < numSamples; ++i)
            {
                const auto dry = data[i];
                const auto sat = std::tanh (dry * drive);
                data[i] = (dry * (1.0f - blend)) + (sat * blend);
            }
        }
    }
}

//==============================================================================
// NovaTranslateEngine
//==============================================================================

NovaTranslateEngine::NovaTranslateEngine() = default;
NovaTranslateEngine::~NovaTranslateEngine()
{
    cancelPendingUpdate();
}

void NovaTranslateEngine::prepare (double sampleRate, int maximumBlockSize, int numChannels)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (maximumBlockSize);
    spec.numChannels = static_cast<juce::uint32> (juce::jmax (1, numChannels));

    for (auto& chain : chains)
        chain.prepare (spec);

    dryBuffer.setSize (static_cast<int> (spec.numChannels), maximumBlockSize, false, false, true);
    scratchA.setSize (static_cast<int> (spec.numChannels), maximumBlockSize, false, false, true);
    scratchB.setSize (static_cast<int> (spec.numChannels), maximumBlockSize, false, false, true);
    chainAScratch.setSize (static_cast<int> (spec.numChannels), maximumBlockSize, false, false, true);
    chainBScratch.setSize (static_cast<int> (spec.numChannels), maximumBlockSize, false, false, true);

    // Convolution IR includes a unity direct-sound tap at sample 0, so the
    // room/reflection path itself adds no additional latency beyond that.
    latencySamples = 0;

    reset();

    // Load the default environment synchronously here - prepare() is always
    // called off the audio thread (host calls it while setting up the graph).
    activeChain = 0;
    fadeTargetChain = 1;
    crossfading = false;
    crossfadeProgress = 0.0f;
    chains[static_cast<size_t> (activeChain)].loadEnvironment (requestedEnvironmentIndex.load(), sampleRate);
    lastSeenRequestedIndex = requestedEnvironmentIndex.load();
}

void NovaTranslateEngine::reset()
{
    for (auto& chain : chains)
        chain.reset();
    dryBuffer.clear();
    noiseLpStateL = 0.0f;
    noiseLpStateR = 0.0f;
    levelMatchGainDb = 0.0f;
    dryLoudnessEnvelope = 0.0f;
    wetLoudnessEnvelope = 0.0f;
}

void NovaTranslateEngine::loadEnvironmentSynchronously (int environmentIndex)
{
    const int targetSlot = (fadeTargetChain != activeChain) ? fadeTargetChain : (1 - activeChain);
    chains[static_cast<size_t> (targetSlot)].loadEnvironment (environmentIndex, currentSampleRate);
    fadeTargetChain = targetSlot;
    if (! crossfading)
    {
        crossfading = true;
        crossfadeProgress = 0.0f;
    }
    lastSeenRequestedIndex = environmentIndex;
}

void NovaTranslateEngine::handleAsyncUpdate()
{
    const int target = pendingLoadTarget.load();
    if (target < 0)
        return;

    const int targetSlot = (fadeTargetChain != activeChain) ? fadeTargetChain : (1 - activeChain);
    chains[static_cast<size_t> (targetSlot)].loadEnvironment (target, currentSampleRate);
    fadeTargetChain = targetSlot;
    pendingLoadReady.store (true);
}

void NovaTranslateEngine::process (juce::AudioBuffer<float>& buffer, const NovaTranslateParams& params)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    if (numSamples == 0)
        return;

    // ── Sanitize NaN/Inf from upstream (real-time-safe: no allocation) ──────
    // A single corrupt sample left unchecked would live forever in the IIR
    // filters' feedback state, silently breaking output/metering until the
    // plugin is reloaded - this sits last on the bus, so it can't assume
    // upstream plugins are well-behaved.
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            if (! std::isfinite (d[i]))
                d[i] = 0.0f;
    }

    // ── Detect environment changes (cheap int compare only - no allocation) ─
    const int wantedEnvironment = juce::jlimit (0, ParameterIDs::numEnvironments - 1, params.environmentIndex);
    if (wantedEnvironment != lastSeenRequestedIndex)
    {
        lastSeenRequestedIndex = wantedEnvironment;
        pendingLoadTarget.store (wantedEnvironment);
        triggerAsyncUpdate(); // posts to the message thread; safe from the audio thread
    }
    if (pendingLoadReady.exchange (false))
    {
        // The message thread finished loading into fadeTargetChain; begin/continue crossfade.
        if (! crossfading)
        {
            crossfading = true;
            crossfadeProgress = 0.0f;
        }
    }

    if (params.bypass)
    {
        // Fully transparent: don't even touch level-match/utility state.
        outputPeakLevel.store (inputPeakLevel.load());
        outputRmsLevel.store (inputRmsLevel.load());
        return;
    }

    // ── Dry copy (for wet/dry blend + level-match reference) ────────────────
    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // Input telemetry (measured on the dry signal).
    {
        float peak = 0.0f;
        double squared = 0.0;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* d = dryBuffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                peak = juce::jmax (peak, std::abs (d[i]));
                squared += static_cast<double> (d[i]) * static_cast<double> (d[i]);
            }
        }
        const auto rms = static_cast<float> (std::sqrt (squared / static_cast<double> (numSamples * juce::jmax (1, numChannels))));
        inputPeakLevel.store (peak);
        inputRmsLevel.store (rms);
    }

    // ── Process through the active chain (and the fade target, if crossfading) ─
    chainAScratch.setSize (numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        chainAScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    juce::dsp::AudioBlock<float> activeBlock (chainAScratch);
    chains[static_cast<size_t> (activeChain)].process (activeBlock, params, currentSampleRate, scratchA);

    if (crossfading)
    {
        chainBScratch.setSize (numChannels, numSamples, false, false, true);
        for (int ch = 0; ch < numChannels; ++ch)
            chainBScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        juce::dsp::AudioBlock<float> fadeBlock (chainBScratch);
        chains[static_cast<size_t> (fadeTargetChain)].process (fadeBlock, params, currentSampleRate, scratchB);

        constexpr float crossfadeSeconds = 0.15f;
        const auto step = static_cast<float> (numSamples) / static_cast<float> (crossfadeSeconds * currentSampleRate);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* a = chainAScratch.getWritePointer (ch);
            const auto* b = chainBScratch.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                const auto t = juce::jlimit (0.0f, 1.0f, crossfadeProgress + (step * (static_cast<float> (i) / numSamples)));
                a[i] = (a[i] * (1.0f - t)) + (b[i] * t);
            }
        }

        crossfadeProgress = juce::jlimit (0.0f, 1.0f, crossfadeProgress + step);
        if (crossfadeProgress >= 1.0f)
        {
            activeChain = fadeTargetChain;
            crossfading = false;
            crossfadeProgress = 0.0f;
        }
    }

    // ── Noise layer (per-environment ambience bed, hard-capped level) ──────
    if (params.noiseEnabled && chains[static_cast<size_t> (activeChain)].isReady())
    {
        const auto& profile = EnvironmentLibrary::getProfile (chains[static_cast<size_t> (activeChain)].getLoadedEnvironmentIndex());
        const auto noiseMix = juce::jlimit (0.0f, 1.0f, params.noiseAmount / 100.0f) * profile.noiseAmount;
        if (noiseMix > 0.001f)
        {
            constexpr float maxNoiseLinearGain = 0.05f; // hard cap so it can never become loud/distracting
            const auto gain = juce::jmin (maxNoiseLinearGain, noiseMix * maxNoiseLinearGain);
            auto* left = chainAScratch.getWritePointer (0);
            auto* right = numChannels > 1 ? chainAScratch.getWritePointer (1) : left;
            for (int i = 0; i < numSamples; ++i)
            {
                const auto nL = noiseRandom.nextFloat() * 2.0f - 1.0f;
                const auto nR = noiseRandom.nextFloat() * 2.0f - 1.0f;
                noiseLpStateL = (0.97f * noiseLpStateL) + (0.03f * nL);
                noiseLpStateR = (0.97f * noiseLpStateR) + (0.03f * nR);
                left[i] += noiseLpStateL * gain;
                right[i] += noiseLpStateR * gain;
            }
        }
    }

    // ── Wet/dry blend by environment_amount ─────────────────────────────────
    const auto wetMix = juce::jlimit (0.0f, 1.0f, params.environmentAmount / 100.0f);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* out = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        const auto* wet = chainAScratch.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            out[i] = (dry[i] * (1.0f - wetMix)) + (wet[i] * wetMix);
    }

    // ── Auto level match ─────────────────────────────────────────────────────
    {
        float dryPeak = 0.0f, wetPeak = 0.0f;
        double drySq = 0.0, wetSq = 0.0;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* d = dryBuffer.getReadPointer (ch);
            const auto* w = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                dryPeak = juce::jmax (dryPeak, std::abs (d[i]));
                wetPeak = juce::jmax (wetPeak, std::abs (w[i]));
                drySq += static_cast<double> (d[i]) * d[i];
                wetSq += static_cast<double> (w[i]) * w[i];
            }
        }
        const auto dryRms = std::sqrt (drySq / static_cast<double> (numSamples * juce::jmax (1, numChannels)));
        const auto wetRms = std::sqrt (wetSq / static_cast<double> (numSamples * juce::jmax (1, numChannels)));

        const auto dryDb = juce::Decibels::gainToDecibels (static_cast<float> (dryRms), -80.0f);
        const auto wetDb = juce::Decibels::gainToDecibels (static_cast<float> (wetRms), -80.0f);

        dryLoudnessEnvelope += (dryDb - dryLoudnessEnvelope) * 0.01f;
        wetLoudnessEnvelope += (wetDb - wetLoudnessEnvelope) * 0.01f;

        if (params.levelMatchEnabled)
        {
            const auto targetCompDb = juce::jlimit (-9.0f, 9.0f, dryLoudnessEnvelope - wetLoudnessEnvelope);
            const auto rate = targetCompDb > levelMatchGainDb ? 0.01f : 0.004f; // slower release avoids pumping
            levelMatchGainDb += (targetCompDb - levelMatchGainDb) * rate;
        }
        else
        {
            levelMatchGainDb += (0.0f - levelMatchGainDb) * 0.02f;
        }

        levelMatchCompensationDb.store (levelMatchGainDb);

        if (params.levelMatchEnabled && std::abs (levelMatchGainDb) > 0.01f)
        {
            const auto gain = juce::Decibels::decibelsToGain (levelMatchGainDb);
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.applyGain (ch, 0, numSamples, gain);
        }
    }

    // ── Utility routing ──────────────────────────────────────────────────────
    if (numChannels > 1)
    {
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        if (params.monoEnabled)
        {
            constexpr float monoCompensation = 1.4142135f; // +3dB, gain-compensated per spec
            for (int i = 0; i < numSamples; ++i)
            {
                const auto m = 0.5f * (left[i] + right[i]) * monoCompensation;
                left[i] = m;
                right[i] = m;
            }
        }
        else if (params.midOnly || params.sideOnly)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const auto mid = 0.5f * (left[i] + right[i]);
                const auto side = 0.5f * (left[i] - right[i]);
                const auto v = params.midOnly ? mid : side;
                left[i] = v;
                right[i] = v;
            }
        }
        else if (params.leftOnly)
        {
            for (int i = 0; i < numSamples; ++i) right[i] = left[i];
        }
        else if (params.rightOnly)
        {
            for (int i = 0; i < numSamples; ++i) left[i] = right[i];
        }

        if (params.phaseInvert)
        {
            for (int i = 0; i < numSamples; ++i) { left[i] = -left[i]; right[i] = -right[i]; }
        }

        if (params.dimEnabled)
        {
            constexpr float dimGain = 0.25119f; // -12dB
            for (int i = 0; i < numSamples; ++i) { left[i] *= dimGain; right[i] *= dimGain; }
        }
    }

    // ── Output trim ───────────────────────────────────────────────────────────
    if (std::abs (params.outputTrimDb) > 0.001f)
    {
        const auto gain = juce::Decibels::decibelsToGain (juce::jlimit (-12.0f, 12.0f, params.outputTrimDb));
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.applyGain (ch, 0, numSamples, gain);
    }

    // ── Output telemetry + correlation ───────────────────────────────────────
    {
        float peak = 0.0f;
        double squared = 0.0, sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;
        const auto* left = buffer.getReadPointer (0);
        const auto* right = numChannels > 1 ? buffer.getReadPointer (1) : left;
        for (int i = 0; i < numSamples; ++i)
        {
            const auto l = left[i], r = right[i];
            peak = juce::jmax (peak, std::abs (l), std::abs (r));
            squared += (static_cast<double> (l) * l) + (static_cast<double> (r) * r);
            sumLR += static_cast<double> (l) * r;
            sumLL += static_cast<double> (l) * l;
            sumRR += static_cast<double> (r) * r;
        }
        const auto rms = static_cast<float> (std::sqrt (squared / static_cast<double> (numSamples * 2)));
        outputPeakLevel.store (peak);
        outputRmsLevel.store (rms);

        const auto denom = std::sqrt (sumLL * sumRR);
        const auto corr = denom > 1.0e-9 ? static_cast<float> (sumLR / denom) : 1.0f;
        stereoCorrelation.store (juce::jlimit (-1.0f, 1.0f, corr));
    }
}
