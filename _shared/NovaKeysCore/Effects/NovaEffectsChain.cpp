#include "NovaEffectsChain.h"
#include <cmath>

namespace
{
    // Soft-knee gain-reduction curve, same technique as
    // _shared/NovaAIMasterCore/DSP/NovaGlueCompressor.cpp (originally from
    // Nova Master's PluginProcessor.cpp:29-45), reimplemented here against
    // this module's own plain params.
    float computeGainReductionDb (float overDb, float ratio, float kneeDb) noexcept
    {
        if (overDb <= 0.0f) return 0.0f;
        const auto slope = 1.0f - (1.0f / juce::jmax (1.0f, ratio));
        if (kneeDb <= 0.0f) return slope * overDb;
        const auto kneeStart = -0.5f * kneeDb;
        const auto kneeEnd = 0.5f * kneeDb;
        if (overDb <= kneeStart) return 0.0f;
        if (overDb < kneeEnd)
        {
            const auto x = overDb - kneeStart;
            return slope * (x * x) / (2.0f * kneeDb);
        }
        return slope * overDb;
    }

    // Soft-ceiling safety curve, same technique as NovaLimiter/NovaSoftClipper
    // (identity below the ceiling, tanh-shaped above it).
    float applySoftCeilingCurve (float sample, float ceilingLinear) noexcept
    {
        const auto safeCeiling = juce::jmax (0.05f, ceilingLinear);
        const auto normalised = sample / safeCeiling;
        const auto shaped = std::tanh (normalised * 0.82f) / std::tanh (0.82f);
        return shaped * safeCeiling;
    }

    float applySoftCeiling (float sample, float ceilingLinear) noexcept
    {
        return std::abs (sample) >= ceilingLinear ? applySoftCeilingCurve (sample, ceilingLinear) : sample;
    }

    constexpr float kCompKneeDb = 6.f;
}

void NovaEffectsChain::prepare (double outputSampleRate, int maxBlockSize, int numChannels)
{
    sampleRate = outputSampleRate;
    preparedNumChannels = juce::jmax (1, numChannels);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) preparedNumChannels };

    for (auto& channelBands : eqBands)
        for (auto& band : channelBands)
            band.filter.prepare (spec);

    saturationOversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) preparedNumChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
    saturationOversampler->initProcessing ((size_t) maxBlockSize);

    chorus.prepare (spec);
    delayLine.prepare (spec);
    reverb.setSampleRate (sampleRate);

    updateEQCoefficients();
    reset();
}

void NovaEffectsChain::reset() noexcept
{
    for (auto& channelBands : eqBands)
        for (auto& band : channelBands)
            band.filter.reset();

    if (saturationOversampler) saturationOversampler->reset();
    compEnvelope = 0.f;
    compGainReductionDb = 0.f;
    outputLimiterGrDb = 0.f;
    chorus.reset();
    delayLine.reset();
    reverb.reset();
}

void NovaEffectsChain::setParams (const NovaEffectsParams& p) noexcept
{
    params = p;
    updateEQCoefficients();

    const float attackMs = juce::jmax (0.1f, params.compAttackMs);
    const float releaseMs = juce::jmax (1.0f, params.compReleaseMs);
    compAttackCoeff = std::exp (-1.0f / (0.001f * attackMs * (float) sampleRate));
    compReleaseCoeff = std::exp (-1.0f / (0.001f * releaseMs * (float) sampleRate));

    chorus.setRate (juce::jlimit (0.01f, 20.f, params.chorusRateHz));
    chorus.setDepth (juce::jlimit (0.f, 1.f, params.chorusDepth));
    chorus.setMix (juce::jlimit (0.f, 1.f, params.chorusMix));

    juce::Reverb::Parameters rp;
    rp.roomSize = juce::jlimit (0.f, 1.f, params.reverbRoomSize);
    rp.damping = juce::jlimit (0.f, 1.f, params.reverbDamping);
    rp.wetLevel = juce::jlimit (0.f, 1.f, params.reverbMix);
    rp.dryLevel = 1.f - rp.wetLevel;
    rp.width = juce::jlimit (0.f, 1.f, params.reverbWidth);
    reverb.setParameters (rp);
}

void NovaEffectsChain::setMacros (const NovaMacroValues& macros) noexcept
{
    macroValues = macros;

    // TONE: EQ tilt - brighter tilts high up / low down, darker the reverse.
    const float tiltDb = juce::jmap (macros.tone, 0.f, 1.f, -6.f, 6.f);
    params.eqLowGainDb = -tiltDb * 0.5f;
    params.eqHighGainDb = tiltDb * 0.5f;

    // MOTION: chorus is the "movement" effect - rate/depth/amount all scale
    // with it, so "still" (0) is effectively inert without needing a
    // separate bypass.
    params.chorusRateHz = juce::jmap (macros.motion, 0.f, 1.f, 0.1f, 5.0f);
    params.chorusDepth = juce::jmap (macros.motion, 0.f, 1.f, 0.02f, 0.5f);
    params.chorusMix = juce::jmap (macros.motion, 0.f, 1.f, 0.f, 0.4f);

    // SPACE: reverb size/amount, plus a touch of delay for a sense of huge
    // ambience at the top end.
    params.reverbRoomSize = juce::jmap (macros.space, 0.f, 1.f, 0.1f, 0.95f);
    params.reverbMix = juce::jmap (macros.space, 0.f, 1.f, 0.f, 0.5f);
    params.delayMix = juce::jmap (macros.space, 0.f, 1.f, 0.f, 0.25f);

    // CHARACTER: saturation + compression both add "richness"/coloration.
    params.saturationDrive = juce::jmap (macros.character, 0.f, 1.f, 0.f, 0.7f);
    params.saturationMix = juce::jmap (macros.character, 0.f, 1.f, 0.f, 0.6f);
    params.compRatio = juce::jmap (macros.character, 0.f, 1.f, 1.5f, 4.0f);
    params.compThresholdDb = juce::jmap (macros.character, 0.f, 1.f, -6.f, -18.f);

    setParams (params); // re-run coefficient/param updates with the new derived values
}

void NovaEffectsChain::updateEQCoefficients() noexcept
{
    const auto low = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, juce::jlimit (20.f, 20000.f, params.eqLowFreq), 0.707f, juce::Decibels::decibelsToGain (params.eqLowGainDb));
    const auto mid = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, juce::jlimit (20.f, 20000.f, params.eqMidFreq), 1.0f, juce::Decibels::decibelsToGain (params.eqMidGainDb));
    const auto high = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, juce::jlimit (20.f, 20000.f, params.eqHighFreq), 0.707f, juce::Decibels::decibelsToGain (params.eqHighGainDb));

    for (auto& channelBands : eqBands)
    {
        channelBands[0].filter.coefficients = low;
        channelBands[1].filter.coefficients = mid;
        channelBands[2].filter.coefficients = high;
    }
}

void NovaEffectsChain::processEQ (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! params.eqActive) return;

    const int nCh = juce::jmin (buffer.getNumChannels(), preparedNumChannels);
    const int N = buffer.getNumSamples();

    for (int ch = 0; ch < nCh; ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < N; ++i)
        {
            float s = d[i];
            for (auto& band : eqBands[(size_t) ch])
                s = band.filter.processSample (s);
            d[i] = s;
        }
    }
}

void NovaEffectsChain::processSaturation (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! params.saturationActive || saturationOversampler == nullptr) return;

    const float drive = 1.f + juce::jlimit (0.f, 1.f, params.saturationDrive) * 2.f;
    const float blend = juce::jlimit (0.f, 1.f, params.saturationMix);

    const int nCh = juce::jmin (buffer.getNumChannels(), preparedNumChannels);
    const int N = buffer.getNumSamples();
    if (nCh == 0 || N == 0) return;

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) nCh, (size_t) N);
    auto upBlock = saturationOversampler->processSamplesUp (block);

    for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch)
    {
        float* d = upBlock.getChannelPointer (ch);
        for (size_t i = 0; i < upBlock.getNumSamples(); ++i)
        {
            const float dry = d[i];
            const float saturated = std::tanh (dry * drive);
            d[i] = dry + (saturated - dry) * blend;
        }
    }

    saturationOversampler->processSamplesDown (block);
}

void NovaEffectsChain::processCompression (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! params.compressionActive) return;

    const int nCh = buffer.getNumChannels();
    const int N = buffer.getNumSamples();
    if (nCh == 0 || N == 0) return;

    float* l = buffer.getWritePointer (0);
    float* r = nCh > 1 ? buffer.getWritePointer (1) : l;
    const float makeupGain = juce::Decibels::decibelsToGain (params.compMakeupDb);

    for (int i = 0; i < N; ++i)
    {
        const float detector = juce::jmax (std::abs (l[i]), std::abs (r[i])) + 1.0e-6f;
        if (detector > compEnvelope)
            compEnvelope = (compAttackCoeff * compEnvelope) + ((1.f - compAttackCoeff) * detector);
        else
            compEnvelope = (compReleaseCoeff * compEnvelope) + ((1.f - compReleaseCoeff) * detector);

        const float detectorDb = juce::Decibels::gainToDecibels (compEnvelope, -160.f);
        const float targetGrDb = computeGainReductionDb (detectorDb - params.compThresholdDb, params.compRatio, kCompKneeDb);

        if (targetGrDb > compGainReductionDb)
            compGainReductionDb += (targetGrDb - compGainReductionDb) * 0.14f;
        else
            compGainReductionDb += (targetGrDb - compGainReductionDb) * 0.025f;

        const float gain = juce::Decibels::decibelsToGain (-compGainReductionDb) * makeupGain;
        l[i] *= gain;
        if (nCh > 1) r[i] *= gain;
    }
}

void NovaEffectsChain::processDelay (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! params.delayActive) return;

    const int nCh = juce::jmin (buffer.getNumChannels(), preparedNumChannels);
    const int N = buffer.getNumSamples();
    if (nCh == 0 || N == 0) return;

    const float delaySamples = juce::jlimit (1.f, (float) delayLine.getMaximumDelayInSamples() - 1.f,
        (float) (params.delayTimeMs * 0.001 * sampleRate));
    delayLine.setDelay (delaySamples);

    const float feedback = juce::jlimit (0.f, 0.95f, params.delayFeedback);
    const float mix = juce::jlimit (0.f, 1.f, params.delayMix);

    for (int ch = 0; ch < nCh; ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < N; ++i)
        {
            const float dry = d[i];
            const float delayed = delayLine.popSample (ch);
            delayLine.pushSample (ch, dry + delayed * feedback);
            d[i] = dry + delayed * mix;
        }
    }
}

void NovaEffectsChain::processOutputCeiling (juce::AudioBuffer<float>& buffer) noexcept
{
    // The soft-ceiling curve alone is NOT a limiter - it's a fixed tanh
    // shape that only guarantees identity below the ceiling; for input far
    // above it, the curve's own asymptote can still exceed the ceiling
    // (e.g. ~1.48x for very hot signals). It was always meant to run AFTER
    // a real gain-reduction stage (see NovaLimiter), not stand alone. A
    // fast peak limiter here does the actual ceiling-holding; the soft
    // curve is the secondary safety net for whatever residual overshoot
    // slips through between attack updates.
    const float ceilingLinear = juce::Decibels::decibelsToGain (params.outputCeilingDb);
    const int nCh = buffer.getNumChannels();
    const int N = buffer.getNumSamples();
    if (nCh == 0 || N == 0) return;

    float* l = buffer.getWritePointer (0);
    float* r = nCh > 1 ? buffer.getWritePointer (1) : l;

    for (int i = 0; i < N; ++i)
    {
        const float peakSample = juce::jmax (std::abs (l[i]), std::abs (r[i]));
        float targetGrDb = 0.f;
        if (peakSample > ceilingLinear)
        {
            const float overDb = juce::Decibels::gainToDecibels (peakSample / ceilingLinear, 0.f);
            targetGrDb = juce::jlimit (0.f, 48.f, computeGainReductionDb (overDb, 20.f, 1.f));
        }

        if (targetGrDb > outputLimiterGrDb)
            outputLimiterGrDb += (targetGrDb - outputLimiterGrDb) * 0.5f;  // fast attack
        else
            outputLimiterGrDb += (targetGrDb - outputLimiterGrDb) * 0.05f; // moderate release

        const float gain = juce::Decibels::decibelsToGain (-outputLimiterGrDb);
        l[i] *= gain;
        if (nCh > 1) r[i] *= gain;

        l[i] = applySoftCeiling (l[i], ceilingLinear);
        if (nCh > 1) r[i] = applySoftCeiling (r[i], ceilingLinear);
    }
}

void NovaEffectsChain::process (juce::AudioBuffer<float>& buffer) noexcept
{
    processEQ (buffer);
    processSaturation (buffer);
    processCompression (buffer);

    if (params.chorusActive)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        chorus.process (context);
    }

    processDelay (buffer);

    if (params.reverbActive)
    {
        const int nCh = buffer.getNumChannels();
        const int N = buffer.getNumSamples();
        if (nCh > 1)
            reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), N);
        else if (nCh == 1)
            reverb.processMono (buffer.getWritePointer (0), N);
    }

    processOutputCeiling (buffer);
}
